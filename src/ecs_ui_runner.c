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

typedef struct EcsUiFrameSignalSlot {
    uint32_t generation;
    bool active;
    bool has_previous;
    uint64_t id;
    uint64_t revision;
    uint64_t previous_revision;
    char label[ECS_UI_ID_MAX];
} EcsUiFrameSignalSlot;

struct EcsUiWakeRegistry {
    EcsUiWakeSourceSlot slots[ECS_UI_WAKE_SOURCE_MAX];
};

struct EcsUiFrameSignalAccumulator {
    EcsUiFrameSignalSlot slots[ECS_UI_FRAME_SIGNAL_MAX];
    uint32_t transient_marks;
    bool has_classified_frame;
};

static EcsUiWakeHandle EcsUiWakeInvalidHandle(void)
{
    return (EcsUiWakeHandle){0};
}

static EcsUiFrameSignalHandle EcsUiFrameSignalInvalidHandle(void)
{
    return (EcsUiFrameSignalHandle){0};
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

static bool EcsUiFrameSignalLabelValid(const char *label)
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

static EcsUiFrameSignalSlot *EcsUiFrameSignalSlotForHandle(
    EcsUiFrameSignalAccumulator *accumulator,
    EcsUiFrameSignalHandle handle)
{
    if (accumulator == NULL || handle.index == 0u ||
        handle.index > ECS_UI_FRAME_SIGNAL_MAX || handle.generation == 0u) {
        return NULL;
    }

    EcsUiFrameSignalSlot *slot = &accumulator->slots[handle.index - 1u];
    if (!slot->active || slot->generation != handle.generation) {
        return NULL;
    }
    return slot;
}

static bool EcsUiFrameSignalIdActive(
    const EcsUiFrameSignalAccumulator *accumulator,
    uint64_t id)
{
    if (accumulator == NULL) {
        return false;
    }
    for (uint32_t i = 0u; i < ECS_UI_FRAME_SIGNAL_MAX; i += 1u) {
        const EcsUiFrameSignalSlot *slot = &accumulator->slots[i];
        if (slot->active && slot->id == id) {
            return true;
        }
    }
    return false;
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

static EcsUiFrameReason EcsUiFrameReasonWithLabel(
    EcsUiFrameReasonKind kind,
    const char *label)
{
    EcsUiFrameReason reason = {
        .kind = kind,
    };
    EcsUiWakeCopyLabel(reason.label, sizeof(reason.label), label);
    return reason;
}

static EcsUiFrameReason EcsUiFrameReasonForStable(
    const EcsUiFrameSignalSlot *slot,
    EcsUiFrameSignalHandle handle)
{
    EcsUiFrameReason reason = {
        .kind = ECS_UI_FRAME_REASON_STABLE_REVISION,
        .signal = handle,
        .stable_id = slot != NULL ? slot->id : 0u,
        .previous_revision = slot != NULL ? slot->previous_revision : 0u,
        .revision = slot != NULL ? slot->revision : 0u,
    };
    if (slot != NULL) {
        EcsUiWakeCopyLabel(reason.label, sizeof(reason.label), slot->label);
    }
    return reason;
}

static EcsUiFrameReason EcsUiFrameReasonForWake(
    const EcsUiWakeSourceInfo *source)
{
    EcsUiFrameReason reason = {
        .kind = ECS_UI_FRAME_REASON_WAKE_HOT,
        .wake = source != NULL ? source->handle : (EcsUiWakeHandle){0},
    };
    if (source != NULL) {
        EcsUiWakeCopyLabel(reason.label, sizeof(reason.label), source->label);
    }
    return reason;
}

static void EcsUiFrameClassifySetResult(
    EcsUiFrameClassifyResult *out,
    EcsUiFrameClassification classification,
    EcsUiFrameReason reason,
    bool screenshot_requested)
{
    if (out == NULL) {
        return;
    }
    *out = (EcsUiFrameClassifyResult){
        .classification = classification,
        .reason = reason,
        .should_render = classification == ECS_UI_FRAME_RENDER_AND_PRESENT,
        .should_present = classification != ECS_UI_FRAME_PARK,
        .should_park = classification == ECS_UI_FRAME_PARK,
        .screenshot_requested = screenshot_requested,
    };
}

static void EcsUiFrameSignalCommit(EcsUiFrameSignalAccumulator *accumulator)
{
    if (accumulator == NULL) {
        return;
    }
    for (uint32_t i = 0u; i < ECS_UI_FRAME_SIGNAL_MAX; i += 1u) {
        EcsUiFrameSignalSlot *slot = &accumulator->slots[i];
        if (!slot->active) {
            continue;
        }
        slot->previous_revision = slot->revision;
        slot->has_previous = true;
    }
    accumulator->transient_marks = ECS_UI_FRAME_MARK_NONE;
    accumulator->has_classified_frame = true;
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

bool EcsUiFrameSignalHandleIsValid(EcsUiFrameSignalHandle handle)
{
    return handle.index != 0u && handle.generation != 0u;
}

EcsUiFrameSignalAccumulator *EcsUiFrameSignalAccumulatorCreate(void)
{
    return (EcsUiFrameSignalAccumulator *)calloc(
        1u,
        sizeof(EcsUiFrameSignalAccumulator));
}

void EcsUiFrameSignalAccumulatorDestroy(
    EcsUiFrameSignalAccumulator *accumulator)
{
    free(accumulator);
}

EcsUiFrameSignalHandle EcsUiFrameSignalRegisterStable(
    EcsUiFrameSignalAccumulator *accumulator,
    uint64_t id,
    const char *label,
    uint64_t revision)
{
    if (accumulator == NULL || !EcsUiFrameSignalLabelValid(label) ||
        EcsUiFrameSignalIdActive(accumulator, id)) {
        return EcsUiFrameSignalInvalidHandle();
    }

    for (uint32_t i = 0u; i < ECS_UI_FRAME_SIGNAL_MAX; i += 1u) {
        EcsUiFrameSignalSlot *slot = &accumulator->slots[i];
        if (slot->active) {
            continue;
        }
        slot->generation += 1u;
        if (slot->generation == 0u) {
            slot->generation = 1u;
        }
        slot->active = true;
        slot->has_previous = false;
        slot->id = id;
        slot->revision = revision;
        slot->previous_revision = 0u;
        EcsUiWakeCopyLabel(slot->label, sizeof(slot->label), label);
        return (EcsUiFrameSignalHandle){
            .index = i + 1u,
            .generation = slot->generation,
        };
    }
    return EcsUiFrameSignalInvalidHandle();
}

bool EcsUiFrameSignalSetRevision(
    EcsUiFrameSignalAccumulator *accumulator,
    EcsUiFrameSignalHandle handle,
    uint64_t revision)
{
    EcsUiFrameSignalSlot *slot =
        EcsUiFrameSignalSlotForHandle(accumulator, handle);
    if (slot == NULL) {
        return false;
    }
    slot->revision = revision;
    return true;
}

bool EcsUiFrameSignalRemove(
    EcsUiFrameSignalAccumulator *accumulator,
    EcsUiFrameSignalHandle handle)
{
    EcsUiFrameSignalSlot *slot =
        EcsUiFrameSignalSlotForHandle(accumulator, handle);
    if (slot == NULL) {
        return false;
    }
    slot->active = false;
    slot->has_previous = false;
    slot->id = 0u;
    slot->revision = 0u;
    slot->previous_revision = 0u;
    slot->label[0] = '\0';
    return true;
}

bool EcsUiFrameSignalMark(
    EcsUiFrameSignalAccumulator *accumulator,
    uint32_t marks)
{
    if (accumulator == NULL) {
        return false;
    }
    accumulator->transient_marks |= marks &
        (ECS_UI_FRAME_MARK_INPUT |
            ECS_UI_FRAME_MARK_WINDOW_EVENT |
            ECS_UI_FRAME_MARK_FORCE_RENDER |
            ECS_UI_FRAME_MARK_SCREENSHOT_REQUEST);
    return true;
}

bool EcsUiFrameSignalClearMarks(EcsUiFrameSignalAccumulator *accumulator)
{
    if (accumulator == NULL) {
        return false;
    }
    accumulator->transient_marks = ECS_UI_FRAME_MARK_NONE;
    return true;
}

bool EcsUiFrameClassify(
    EcsUiFrameSignalAccumulator *accumulator,
    const EcsUiFrameClassifyDesc *desc,
    EcsUiFrameClassifyResult *out)
{
    if (accumulator == NULL || out == NULL) {
        return false;
    }

    const uint32_t marks = accumulator->transient_marks;
    const bool screenshot_requested =
        (marks & ECS_UI_FRAME_MARK_SCREENSHOT_REQUEST) != 0u;
    EcsUiFrameClassification classification = ECS_UI_FRAME_PARK;
    EcsUiFrameReason reason =
        EcsUiFrameReasonWithLabel(ECS_UI_FRAME_REASON_NONE, "clean");

    if (!accumulator->has_classified_frame) {
        classification = ECS_UI_FRAME_RENDER_AND_PRESENT;
        reason =
            EcsUiFrameReasonWithLabel(
                ECS_UI_FRAME_REASON_FIRST_FRAME,
                "first-frame");
    } else if (screenshot_requested) {
        classification = ECS_UI_FRAME_RENDER_AND_PRESENT;
        reason =
            EcsUiFrameReasonWithLabel(
                ECS_UI_FRAME_REASON_SCREENSHOT_REQUEST,
                "screenshot-request");
    } else if ((marks & ECS_UI_FRAME_MARK_FORCE_RENDER) != 0u) {
        classification = ECS_UI_FRAME_RENDER_AND_PRESENT;
        reason =
            EcsUiFrameReasonWithLabel(
                ECS_UI_FRAME_REASON_FORCE_RENDER,
                "force-render");
    } else if ((marks & ECS_UI_FRAME_MARK_INPUT) != 0u) {
        classification = ECS_UI_FRAME_RENDER_AND_PRESENT;
        reason =
            EcsUiFrameReasonWithLabel(ECS_UI_FRAME_REASON_INPUT, "input");
    } else if ((marks & ECS_UI_FRAME_MARK_WINDOW_EVENT) != 0u) {
        classification = ECS_UI_FRAME_RENDER_AND_PRESENT;
        reason =
            EcsUiFrameReasonWithLabel(
                ECS_UI_FRAME_REASON_WINDOW_EVENT,
                "window-event");
    } else {
        for (uint32_t i = 0u; i < ECS_UI_FRAME_SIGNAL_MAX; i += 1u) {
            EcsUiFrameSignalSlot *slot = &accumulator->slots[i];
            if (!slot->active) {
                continue;
            }
            if (!slot->has_previous ||
                    slot->previous_revision != slot->revision) {
                classification = ECS_UI_FRAME_RENDER_AND_PRESENT;
                reason =
                    EcsUiFrameReasonForStable(
                        slot,
                        (EcsUiFrameSignalHandle){
                            .index = i + 1u,
                            .generation = slot->generation,
                        });
                break;
            }
        }
        if (classification == ECS_UI_FRAME_PARK &&
                desc != NULL && desc->wake != NULL && desc->wake->hot) {
            classification = ECS_UI_FRAME_PRESENT_ONLY;
            reason =
                EcsUiFrameReasonForWake(
                    desc->wake->active_count > 0u ?
                        &desc->wake->active[0] :
                        NULL);
        }
        if (classification == ECS_UI_FRAME_PARK &&
                desc != NULL && desc->present_when_clean) {
            classification = ECS_UI_FRAME_PRESENT_ONLY;
            reason =
                EcsUiFrameReasonWithLabel(
                    ECS_UI_FRAME_REASON_PRESENT_POLICY,
                    desc->present_policy_label != NULL &&
                            desc->present_policy_label[0] != '\0' ?
                        desc->present_policy_label :
                        "present-policy");
        }
    }

    EcsUiFrameClassifySetResult(
        out,
        classification,
        reason,
        screenshot_requested);
    EcsUiFrameSignalCommit(accumulator);
    return true;
}
