#include "ecs_ui/ecs_ui_runner.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct EcsUiWakeSourceSlot {
    atomic_uint generation;
    atomic_bool active;
    atomic_uint kind;
    atomic_bool posted;
    bool pending;
    bool deadline_armed;
    uint64_t deadline_ns;
    int fd;
    uint32_t fd_interests;
    char label[ECS_UI_ID_MAX];
} EcsUiWakeSourceSlot;

struct EcsUiWakeRegistry {
    EcsUiWakeSourceSlot slots[ECS_UI_WAKE_SOURCE_MAX];
};

static EcsUiWakeHandle EcsUiWakeInvalidHandle(void)
{
    return (EcsUiWakeHandle){0};
}

static void EcsUiWakeCopyLabel(char *out, size_t out_size, const char *label)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *source = label != NULL ? label : "";
    size_t i = 0u;
    for (; i + 1u < out_size && source[i] != '\0'; i += 1u) {
        out[i] = source[i];
    }
    out[i] = '\0';
}

static bool EcsUiWakeLabelValid(const char *label)
{
    return label != NULL && label[0] != '\0';
}

static uint32_t EcsUiWakeNormalizeFdInterests(uint32_t interests)
{
    return interests &
        (ECS_UI_WAKE_FD_INTEREST_READ | ECS_UI_WAKE_FD_INTEREST_WRITE);
}

static EcsUiWakeSourceSlot *EcsUiWakeSlotForHandle(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle)
{
    if (registry == NULL || handle.index == 0u ||
        handle.index > ECS_UI_WAKE_SOURCE_MAX || handle.generation == 0u) {
        return NULL;
    }

    EcsUiWakeSourceSlot *slot = &registry->slots[handle.index - 1u];
    const uint32_t generation =
        (uint32_t)atomic_load_explicit(
            &slot->generation,
            memory_order_acquire);
    const bool active =
        atomic_load_explicit(&slot->active, memory_order_acquire);
    if (!active || generation != handle.generation) {
        return NULL;
    }
    return slot;
}

static EcsUiWakeSourceSlot *EcsUiWakeSlotForKind(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    EcsUiWakeSourceKind kind)
{
    EcsUiWakeSourceSlot *slot = EcsUiWakeSlotForHandle(registry, handle);
    if (slot == NULL) {
        return NULL;
    }
    const EcsUiWakeSourceKind slot_kind =
        (EcsUiWakeSourceKind)atomic_load_explicit(
            &slot->kind,
            memory_order_acquire);
    return slot_kind == kind ? slot : NULL;
}

static EcsUiWakeHandle EcsUiWakeRegisterSource(
    EcsUiWakeRegistry *registry,
    EcsUiWakeSourceKind kind,
    const char *label)
{
    if (registry == NULL || kind == ECS_UI_WAKE_SOURCE_NONE ||
        !EcsUiWakeLabelValid(label)) {
        return EcsUiWakeInvalidHandle();
    }

    for (uint32_t i = 0u; i < ECS_UI_WAKE_SOURCE_MAX; i += 1u) {
        EcsUiWakeSourceSlot *slot = &registry->slots[i];
        const bool active =
            atomic_load_explicit(&slot->active, memory_order_acquire);
        if (active) {
            continue;
        }

        uint32_t generation =
            (uint32_t)atomic_load_explicit(
                &slot->generation,
                memory_order_relaxed);
        generation += 1u;
        if (generation == 0u) {
            generation = 1u;
        }

        slot->pending = false;
        slot->deadline_armed = false;
        slot->deadline_ns = 0u;
        slot->fd = -1;
        slot->fd_interests = ECS_UI_WAKE_FD_INTEREST_NONE;
        EcsUiWakeCopyLabel(slot->label, sizeof(slot->label), label);
        atomic_store_explicit(&slot->posted, false, memory_order_release);
        atomic_store_explicit(&slot->generation, generation, memory_order_release);
        atomic_store_explicit(
            &slot->kind,
            (unsigned int)kind,
            memory_order_release);
        atomic_store_explicit(&slot->active, true, memory_order_release);
        return (EcsUiWakeHandle){
            .index = i + 1u,
            .generation = generation,
        };
    }

    return EcsUiWakeInvalidHandle();
}

static void EcsUiWakeFillSourceInfo(
    const EcsUiWakeSourceSlot *slot,
    EcsUiWakeHandle handle,
    EcsUiWakeSourceKind kind,
    EcsUiWakeSourceInfo *out)
{
    if (out == NULL) {
        return;
    }
    *out = (EcsUiWakeSourceInfo){
        .handle = handle,
        .kind = kind,
        .fd = slot != NULL ? slot->fd : -1,
        .fd_interests = slot != NULL ?
            slot->fd_interests :
            ECS_UI_WAKE_FD_INTEREST_NONE,
        .deadline_ns = slot != NULL ? slot->deadline_ns : 0u,
    };
    if (slot != NULL) {
        EcsUiWakeCopyLabel(out->label, sizeof(out->label), slot->label);
    }
}

static void EcsUiWakeAddActive(
    EcsUiWakeWaitSpec *spec,
    const EcsUiWakeSourceSlot *slot,
    EcsUiWakeHandle handle,
    EcsUiWakeSourceKind kind)
{
    if (spec == NULL) {
        return;
    }
    spec->hot = true;
    if (spec->active_count >= ECS_UI_WAKE_SOURCE_MAX) {
        spec->active_truncated = true;
        return;
    }

    EcsUiWakeFillSourceInfo(
        slot,
        handle,
        kind,
        &spec->active[spec->active_count]);
    spec->active_count += 1u;
}

static void EcsUiWakeMaybeSetNearestDeadline(
    EcsUiWakeWaitSpec *spec,
    const EcsUiWakeSourceSlot *slot,
    EcsUiWakeHandle handle)
{
    if (spec == NULL || slot == NULL || !slot->deadline_armed) {
        return;
    }
    if (!spec->has_deadline || slot->deadline_ns < spec->deadline_ns) {
        spec->has_deadline = true;
        spec->deadline_ns = slot->deadline_ns;
        spec->deadline_handle = handle;
        EcsUiWakeCopyLabel(
            spec->deadline_label,
            sizeof(spec->deadline_label),
            slot->label);
    }
}

bool EcsUiWakeHandleIsValid(EcsUiWakeHandle handle)
{
    return handle.index != 0u && handle.generation != 0u;
}

EcsUiWakeRegistry *EcsUiWakeRegistryCreate(void)
{
    EcsUiWakeRegistry *registry =
        (EcsUiWakeRegistry *)calloc(1u, sizeof(EcsUiWakeRegistry));
    if (registry == NULL) {
        return NULL;
    }

    for (uint32_t i = 0u; i < ECS_UI_WAKE_SOURCE_MAX; i += 1u) {
        EcsUiWakeSourceSlot *slot = &registry->slots[i];
        atomic_init(&slot->generation, 0u);
        atomic_init(&slot->active, false);
        atomic_init(&slot->kind, (unsigned int)ECS_UI_WAKE_SOURCE_NONE);
        atomic_init(&slot->posted, false);
        slot->fd = -1;
    }
    return registry;
}

void EcsUiWakeRegistryDestroy(EcsUiWakeRegistry *registry)
{
    free(registry);
}

EcsUiWakeHandle EcsUiWakeRegisterPending(
    EcsUiWakeRegistry *registry,
    const char *label)
{
    return EcsUiWakeRegisterSource(
        registry,
        ECS_UI_WAKE_SOURCE_PENDING,
        label);
}

EcsUiWakeHandle EcsUiWakeRegisterDeadline(
    EcsUiWakeRegistry *registry,
    const char *label)
{
    return EcsUiWakeRegisterSource(
        registry,
        ECS_UI_WAKE_SOURCE_DEADLINE,
        label);
}

EcsUiWakeHandle EcsUiWakeRegisterPosixFd(
    EcsUiWakeRegistry *registry,
    const char *label,
    int fd,
    uint32_t interests)
{
    if (fd < 0) {
        return EcsUiWakeInvalidHandle();
    }

    EcsUiWakeHandle handle =
        EcsUiWakeRegisterSource(
            registry,
            ECS_UI_WAKE_SOURCE_POSIX_FD,
            label);
    EcsUiWakeSourceSlot *slot =
        EcsUiWakeSlotForKind(
            registry,
            handle,
            ECS_UI_WAKE_SOURCE_POSIX_FD);
    if (slot == NULL) {
        return EcsUiWakeInvalidHandle();
    }

    slot->fd = fd;
    slot->fd_interests = EcsUiWakeNormalizeFdInterests(interests);
    return handle;
}

bool EcsUiWakeRemove(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle)
{
    EcsUiWakeSourceSlot *slot = EcsUiWakeSlotForHandle(registry, handle);
    if (slot == NULL) {
        return false;
    }

    slot->pending = false;
    slot->deadline_armed = false;
    slot->deadline_ns = 0u;
    slot->fd = -1;
    slot->fd_interests = ECS_UI_WAKE_FD_INTEREST_NONE;
    slot->label[0] = '\0';
    atomic_store_explicit(&slot->posted, false, memory_order_release);
    atomic_store_explicit(
        &slot->kind,
        (unsigned int)ECS_UI_WAKE_SOURCE_NONE,
        memory_order_release);
    atomic_store_explicit(&slot->active, false, memory_order_release);
    return true;
}

bool EcsUiWakeSetPending(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    bool pending)
{
    EcsUiWakeSourceSlot *slot =
        EcsUiWakeSlotForKind(
            registry,
            handle,
            ECS_UI_WAKE_SOURCE_PENDING);
    if (slot == NULL) {
        return false;
    }
    slot->pending = pending;
    return true;
}

bool EcsUiWakePost(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle)
{
    if (registry == NULL || handle.index == 0u ||
        handle.index > ECS_UI_WAKE_SOURCE_MAX || handle.generation == 0u) {
        return false;
    }

    EcsUiWakeSourceSlot *slot = &registry->slots[handle.index - 1u];
    const bool active =
        atomic_load_explicit(&slot->active, memory_order_acquire);
    const uint32_t generation =
        (uint32_t)atomic_load_explicit(
            &slot->generation,
            memory_order_acquire);
    const EcsUiWakeSourceKind kind =
        (EcsUiWakeSourceKind)atomic_load_explicit(
            &slot->kind,
            memory_order_acquire);
    if (!active || generation != handle.generation ||
        kind != ECS_UI_WAKE_SOURCE_PENDING) {
        return false;
    }

    atomic_store_explicit(&slot->posted, true, memory_order_release);
    return true;
}

uint32_t EcsUiWakeConsumePosts(EcsUiWakeRegistry *registry)
{
    if (registry == NULL) {
        return 0u;
    }

    uint32_t consumed = 0u;
    for (uint32_t i = 0u; i < ECS_UI_WAKE_SOURCE_MAX; i += 1u) {
        EcsUiWakeSourceSlot *slot = &registry->slots[i];
        const bool active =
            atomic_load_explicit(&slot->active, memory_order_acquire);
        const EcsUiWakeSourceKind kind =
            (EcsUiWakeSourceKind)atomic_load_explicit(
                &slot->kind,
                memory_order_acquire);
        if (!active || kind != ECS_UI_WAKE_SOURCE_PENDING) {
            continue;
        }
        if (atomic_exchange_explicit(
                &slot->posted,
                false,
                memory_order_acq_rel)) {
            consumed += 1u;
        }
    }
    return consumed;
}

bool EcsUiWakeArmDeadline(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    uint64_t deadline_ns)
{
    EcsUiWakeSourceSlot *slot =
        EcsUiWakeSlotForKind(
            registry,
            handle,
            ECS_UI_WAKE_SOURCE_DEADLINE);
    if (slot == NULL) {
        return false;
    }
    slot->deadline_ns = deadline_ns;
    slot->deadline_armed = true;
    return true;
}

bool EcsUiWakeDisarmDeadline(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle)
{
    EcsUiWakeSourceSlot *slot =
        EcsUiWakeSlotForKind(
            registry,
            handle,
            ECS_UI_WAKE_SOURCE_DEADLINE);
    if (slot == NULL) {
        return false;
    }
    slot->deadline_ns = 0u;
    slot->deadline_armed = false;
    return true;
}

bool EcsUiWakeSetFdInterests(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    uint32_t interests)
{
    EcsUiWakeSourceSlot *slot =
        EcsUiWakeSlotForKind(
            registry,
            handle,
            ECS_UI_WAKE_SOURCE_POSIX_FD);
    if (slot == NULL) {
        return false;
    }
    slot->fd_interests = EcsUiWakeNormalizeFdInterests(interests);
    return true;
}

bool EcsUiWakeSetPosixFd(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    int fd,
    uint32_t interests)
{
    if (fd < 0) {
        return false;
    }

    EcsUiWakeSourceSlot *slot =
        EcsUiWakeSlotForKind(
            registry,
            handle,
            ECS_UI_WAKE_SOURCE_POSIX_FD);
    if (slot == NULL) {
        return false;
    }
    slot->fd = fd;
    slot->fd_interests = EcsUiWakeNormalizeFdInterests(interests);
    return true;
}

bool EcsUiWakeBuildWaitSpec(
    const EcsUiWakeRegistry *registry,
    uint64_t now_ns,
    EcsUiWakeWaitSpec *out)
{
    if (registry == NULL || out == NULL) {
        return false;
    }

    *out = (EcsUiWakeWaitSpec){0};
    for (uint32_t i = 0u; i < ECS_UI_WAKE_SOURCE_MAX; i += 1u) {
        const EcsUiWakeSourceSlot *slot = &registry->slots[i];
        const bool active =
            atomic_load_explicit(&slot->active, memory_order_acquire);
        if (!active) {
            continue;
        }

        const EcsUiWakeSourceKind kind =
            (EcsUiWakeSourceKind)atomic_load_explicit(
                &slot->kind,
                memory_order_acquire);
        const uint32_t generation =
            (uint32_t)atomic_load_explicit(
                &slot->generation,
                memory_order_acquire);
        const EcsUiWakeHandle handle = {
            .index = i + 1u,
            .generation = generation,
        };

        switch (kind) {
        case ECS_UI_WAKE_SOURCE_PENDING:
            if (slot->pending ||
                    atomic_load_explicit(
                        &slot->posted,
                        memory_order_acquire)) {
                EcsUiWakeAddActive(out, slot, handle, kind);
            }
            break;
        case ECS_UI_WAKE_SOURCE_DEADLINE:
            if (slot->deadline_armed) {
                if (slot->deadline_ns <= now_ns) {
                    EcsUiWakeAddActive(out, slot, handle, kind);
                } else {
                    EcsUiWakeMaybeSetNearestDeadline(out, slot, handle);
                }
            }
            break;
        case ECS_UI_WAKE_SOURCE_POSIX_FD:
            if (slot->fd >= 0 && slot->fd_interests != 0u) {
                if (out->fd_count >= ECS_UI_WAKE_FD_MAX) {
                    out->fd_truncated = true;
                    break;
                }
                EcsUiWakeFdWait *fd = &out->fds[out->fd_count];
                *fd = (EcsUiWakeFdWait){
                    .handle = handle,
                    .fd = slot->fd,
                    .interests = slot->fd_interests,
                };
                EcsUiWakeCopyLabel(
                    fd->label,
                    sizeof(fd->label),
                    slot->label);
                out->fd_count += 1u;
            }
            break;
        case ECS_UI_WAKE_SOURCE_NONE:
        default:
            break;
        }
    }

    return true;
}
