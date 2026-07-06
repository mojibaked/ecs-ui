#define _POSIX_C_SOURCE 200809L

#include "ecs_ui/ecs_ui_raylib.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(ECS_UI_RAYLIB_HAS_GLFW_POST)
#include <GLFW/glfw3.h>
#endif

#if !defined(_WIN32)
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#endif

#define ECS_UI_RAYLIB_POST_LABEL "post"
#define ECS_UI_RAYLIB_CAPABILITY_LABEL "glfwPostEmptyEvent unavailable"

struct EcsUiRaylibParker {
    EcsUiRaylibParkerCapabilities capabilities;
    EcsUiRaylibPostEmptyEventFn post_empty_event;
    void *post_empty_event_ctx;
    EcsUiRaylibWakeReason last_wake;
    uint64_t wake_sequence;
#if !defined(_WIN32)
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int control_pipe[2];
    bool mutex_initialized;
    bool cond_initialized;
    bool thread_started;
    bool stop;
    bool has_spec;
    uint64_t spec_generation;
    EcsUiWakeWaitSpec spec;
    char pending_post_label[ECS_UI_ID_MAX];
#endif
};

static void EcsUiRaylibCopyLabel(
    char *out,
    size_t out_size,
    const char *label)
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

static EcsUiRaylibWakeReason EcsUiRaylibWakeReasonForSource(
    EcsUiRaylibWakeReasonKind kind,
    const EcsUiWakeSourceInfo *source)
{
    EcsUiRaylibWakeReason reason = {
        .kind = kind,
        .handle = source != NULL ? source->handle : (EcsUiWakeHandle){0},
        .fd = source != NULL ? source->fd : -1,
        .fd_interests = source != NULL ?
            source->fd_interests :
            ECS_UI_WAKE_FD_INTEREST_NONE,
    };
    if (source != NULL) {
        EcsUiRaylibCopyLabel(
            reason.label,
            sizeof(reason.label),
            source->label);
    }
    return reason;
}

static EcsUiRaylibWakeReason EcsUiRaylibWakeReasonForFd(
    const EcsUiWakeFdWait *fd,
    uint32_t interests)
{
    EcsUiRaylibWakeReason reason = {
        .kind = ECS_UI_RAYLIB_WAKE_FD,
        .handle = fd != NULL ? fd->handle : (EcsUiWakeHandle){0},
        .fd = fd != NULL ? fd->fd : -1,
        .fd_interests = interests,
    };
    if (fd != NULL) {
        EcsUiRaylibCopyLabel(reason.label, sizeof(reason.label), fd->label);
    }
    return reason;
}

static EcsUiRaylibWakeReason EcsUiRaylibWakeReasonForDeadline(
    const EcsUiWakeWaitSpec *spec)
{
    EcsUiRaylibWakeReason reason = {
        .kind = ECS_UI_RAYLIB_WAKE_DEADLINE,
        .handle = spec != NULL ? spec->deadline_handle : (EcsUiWakeHandle){0},
        .fd = -1,
        .fd_interests = ECS_UI_WAKE_FD_INTEREST_NONE,
    };
    if (spec != NULL) {
        EcsUiRaylibCopyLabel(
            reason.label,
            sizeof(reason.label),
            spec->deadline_label);
    }
    return reason;
}

static EcsUiRaylibWakeReason EcsUiRaylibWakeReasonWithLabel(
    EcsUiRaylibWakeReasonKind kind,
    const char *label)
{
    EcsUiRaylibWakeReason reason = {
        .kind = kind,
        .fd = -1,
        .fd_interests = ECS_UI_WAKE_FD_INTEREST_NONE,
    };
    EcsUiRaylibCopyLabel(reason.label, sizeof(reason.label), label);
    return reason;
}

#if defined(ECS_UI_RAYLIB_HAS_GLFW_POST)
static void EcsUiRaylibGlfwPostEmptyEvent(void *ctx)
{
    (void)ctx;
    glfwPostEmptyEvent();
}
#endif

#if !defined(_WIN32)
static uint64_t EcsUiRaylibMonotonicNs(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0u;
    }
    return ((uint64_t)now.tv_sec * 1000000000u) + (uint64_t)now.tv_nsec;
}

static uint64_t EcsUiRaylibRealtimeNs(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return 0u;
    }
    return ((uint64_t)now.tv_sec * 1000000000u) + (uint64_t)now.tv_nsec;
}

static struct timespec EcsUiRaylibRealtimeAfter(uint64_t timeout_ns)
{
    const uint64_t deadline = EcsUiRaylibRealtimeNs() + timeout_ns;
    return (struct timespec){
        .tv_sec = (time_t)(deadline / 1000000000u),
        .tv_nsec = (long)(deadline % 1000000000u),
    };
}

static int EcsUiRaylibDeadlineTimeoutMs(const EcsUiWakeWaitSpec *spec)
{
    if (spec == NULL || !spec->has_deadline) {
        return -1;
    }

    const uint64_t now = EcsUiRaylibMonotonicNs();
    if (now == 0u || spec->deadline_ns <= now) {
        return 0;
    }

    uint64_t delta_ns = spec->deadline_ns - now;
    uint64_t timeout_ms = (delta_ns + 999999u) / 1000000u;
    if (timeout_ms > (uint64_t)INT_MAX) {
        timeout_ms = (uint64_t)INT_MAX;
    }
    return (int)timeout_ms;
}

static short EcsUiRaylibPollEventsForInterests(uint32_t interests)
{
    short events = 0;
    if ((interests & ECS_UI_WAKE_FD_INTEREST_READ) != 0u) {
        events = (short)(events | POLLIN);
    }
    if ((interests & ECS_UI_WAKE_FD_INTEREST_WRITE) != 0u) {
        events = (short)(events | POLLOUT);
    }
    return events;
}

static uint32_t EcsUiRaylibInterestsForRevents(short revents)
{
    uint32_t interests = ECS_UI_WAKE_FD_INTEREST_NONE;
    if ((revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) != 0) {
        interests |= ECS_UI_WAKE_FD_INTEREST_READ;
    }
    if ((revents & (POLLOUT | POLLERR | POLLNVAL)) != 0) {
        interests |= ECS_UI_WAKE_FD_INTEREST_WRITE;
    }
    return interests;
}

static void EcsUiRaylibSetNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static void EcsUiRaylibDrainControlPipe(int fd, bool *out_post)
{
    unsigned char buffer[64];
    for (;;) {
        const ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) {
            return;
        }
        for (ssize_t i = 0; i < n; i += 1) {
            if (buffer[i] == (unsigned char)'p' && out_post != NULL) {
                *out_post = true;
            }
        }
    }
}

static void EcsUiRaylibSignalControlPipe(
    EcsUiRaylibParker *parker,
    unsigned char code)
{
    if (parker == NULL || parker->control_pipe[1] < 0) {
        return;
    }
    const ssize_t n = write(parker->control_pipe[1], &code, sizeof(code));
    (void)n;
}

static void EcsUiRaylibRecordWake(
    EcsUiRaylibParker *parker,
    EcsUiRaylibWakeReason reason,
    bool post_empty_event)
{
    if (parker == NULL) {
        return;
    }

    EcsUiRaylibPostEmptyEventFn post = NULL;
    void *post_ctx = NULL;
#if !defined(_WIN32)
    if (!parker->mutex_initialized) {
        parker->last_wake = reason;
        parker->wake_sequence += 1u;
        if (post_empty_event && parker->post_empty_event != NULL) {
            parker->post_empty_event(parker->post_empty_event_ctx);
        }
        return;
    }
#endif
    pthread_mutex_lock(&parker->mutex);
    parker->last_wake = reason;
    parker->wake_sequence += 1u;
    post = parker->post_empty_event;
    post_ctx = parker->post_empty_event_ctx;
    pthread_cond_broadcast(&parker->cond);
    pthread_mutex_unlock(&parker->mutex);

    if (post_empty_event && post != NULL) {
        post(post_ctx);
    }
}

static bool EcsUiRaylibCopyWatcherSpec(
    EcsUiRaylibParker *parker,
    EcsUiWakeWaitSpec *out,
    bool *out_stop,
    uint64_t *out_generation)
{
    if (parker == NULL || out == NULL || out_stop == NULL) {
        return false;
    }

    pthread_mutex_lock(&parker->mutex);
    *out_stop = parker->stop;
    if (parker->has_spec) {
        *out = parker->spec;
    } else {
        *out = (EcsUiWakeWaitSpec){0};
    }
    if (out_generation != NULL) {
        *out_generation = parker->spec_generation;
    }
    pthread_mutex_unlock(&parker->mutex);
    return true;
}

static void EcsUiRaylibClearWatcherSpec(
    EcsUiRaylibParker *parker,
    uint64_t generation)
{
    if (parker == NULL) {
        return;
    }

    pthread_mutex_lock(&parker->mutex);
    if (parker->spec_generation == generation) {
        parker->has_spec = false;
        parker->spec = (EcsUiWakeWaitSpec){0};
    }
    pthread_mutex_unlock(&parker->mutex);
}

static void EcsUiRaylibTakePostLabel(
    EcsUiRaylibParker *parker,
    char *out,
    size_t out_size)
{
    if (parker == NULL || out == NULL || out_size == 0u) {
        return;
    }

    pthread_mutex_lock(&parker->mutex);
    EcsUiRaylibCopyLabel(out, out_size, parker->pending_post_label);
    parker->pending_post_label[0] = '\0';
    pthread_mutex_unlock(&parker->mutex);
}

static void *EcsUiRaylibWatcherMain(void *arg)
{
    EcsUiRaylibParker *parker = (EcsUiRaylibParker *)arg;
    for (;;) {
        EcsUiWakeWaitSpec spec = {0};
        bool stop = false;
        uint64_t spec_generation = 0u;
        (void)EcsUiRaylibCopyWatcherSpec(
            parker,
            &spec,
            &stop,
            &spec_generation);
        if (stop) {
            return NULL;
        }

        struct pollfd fds[ECS_UI_WAKE_FD_MAX + 1u];
        fds[0] = (struct pollfd){
            .fd = parker->control_pipe[0],
            .events = POLLIN,
            .revents = 0,
        };
        uint32_t fd_count = 1u;
        for (uint32_t i = 0u; i < spec.fd_count &&
                fd_count < ECS_UI_WAKE_FD_MAX + 1u; i += 1u) {
            fds[fd_count] = (struct pollfd){
                .fd = spec.fds[i].fd,
                .events = EcsUiRaylibPollEventsForInterests(
                    spec.fds[i].interests),
                .revents = 0,
            };
            fd_count += 1u;
        }

        const int timeout_ms = EcsUiRaylibDeadlineTimeoutMs(&spec);
        const int ready = poll(fds, (nfds_t)fd_count, timeout_ms);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            EcsUiRaylibRecordWake(
                parker,
                EcsUiRaylibWakeReasonWithLabel(
                    ECS_UI_RAYLIB_WAKE_ERROR,
                    "poll failed"),
                true);
            continue;
        }

        if (ready == 0) {
            if (spec.has_deadline) {
                EcsUiRaylibClearWatcherSpec(parker, spec_generation);
                EcsUiRaylibRecordWake(
                    parker,
                    EcsUiRaylibWakeReasonForDeadline(&spec),
                    true);
            }
            continue;
        }

        bool saw_post = false;
        if ((fds[0].revents & POLLIN) != 0) {
            EcsUiRaylibDrainControlPipe(parker->control_pipe[0], &saw_post);
        }
        if (saw_post) {
            char post_label[ECS_UI_ID_MAX] = {0};
            EcsUiRaylibTakePostLabel(
                parker,
                post_label,
                sizeof(post_label));
            EcsUiRaylibRecordWake(
                parker,
                EcsUiRaylibWakeReasonWithLabel(
                    ECS_UI_RAYLIB_WAKE_POST,
                    post_label[0] != '\0' ?
                        post_label :
                        ECS_UI_RAYLIB_POST_LABEL),
                true);
            continue;
        }

        for (uint32_t i = 1u; i < fd_count; i += 1u) {
            const uint32_t ready_interests =
                EcsUiRaylibInterestsForRevents(fds[i].revents) &
                spec.fds[i - 1u].interests;
            if (ready_interests == 0u) {
                continue;
            }
            EcsUiRaylibClearWatcherSpec(parker, spec_generation);
            EcsUiRaylibRecordWake(
                parker,
                EcsUiRaylibWakeReasonForFd(
                    &spec.fds[i - 1u],
                    ready_interests),
                true);
            break;
        }
    }
}
#endif

uint64_t EcsUiRaylibNowNs(void)
{
#if !defined(_WIN32)
    return EcsUiRaylibMonotonicNs();
#else
    return (uint64_t)(GetTime() * 1000000000.0);
#endif
}

static void EcsUiRaylibRecordCapabilityDisabled(EcsUiRaylibParker *parker)
{
    if (parker == NULL) {
        return;
    }
    parker->capabilities.capability_disabled = true;
#if !defined(_WIN32)
    EcsUiRaylibRecordWake(
        parker,
        EcsUiRaylibWakeReasonWithLabel(
            ECS_UI_RAYLIB_WAKE_CAPABILITY_DISABLED,
            ECS_UI_RAYLIB_CAPABILITY_LABEL),
        false);
#else
    parker->last_wake =
        EcsUiRaylibWakeReasonWithLabel(
            ECS_UI_RAYLIB_WAKE_CAPABILITY_DISABLED,
            ECS_UI_RAYLIB_CAPABILITY_LABEL);
    parker->wake_sequence += 1u;
#endif
}

EcsUiRaylibParker *EcsUiRaylibParkerCreate(
    const EcsUiRaylibParkerDesc *desc)
{
    EcsUiRaylibParker *parker =
        (EcsUiRaylibParker *)calloc(1u, sizeof(EcsUiRaylibParker));
    if (parker == NULL) {
        return NULL;
    }

    parker->post_empty_event = desc != NULL ? desc->post_empty_event : NULL;
    parker->post_empty_event_ctx =
        desc != NULL ? desc->post_empty_event_ctx : NULL;
    parker->last_wake =
        EcsUiRaylibWakeReasonWithLabel(ECS_UI_RAYLIB_WAKE_NONE, "");
#if !defined(_WIN32)
    parker->control_pipe[0] = -1;
    parker->control_pipe[1] = -1;
#endif

    if (parker->post_empty_event == NULL) {
        parker->capabilities = (EcsUiRaylibParkerCapabilities){
            .post_empty_event_available = false,
            .post_wake_enabled = false,
            .fd_wake_enabled = false,
            .deadline_wake_enabled = false,
            .capability_disabled = true,
        };
        return parker;
    }

    parker->capabilities = (EcsUiRaylibParkerCapabilities){
        .post_empty_event_available = true,
        .post_wake_enabled = true,
        .fd_wake_enabled = true,
        .deadline_wake_enabled = true,
        .capability_disabled = false,
    };

#if !defined(_WIN32)
    if (pipe(parker->control_pipe) != 0) {
        EcsUiRaylibParkerDestroy(parker);
        return NULL;
    }
    EcsUiRaylibSetNonBlocking(parker->control_pipe[0]);
    EcsUiRaylibSetNonBlocking(parker->control_pipe[1]);
    if (pthread_mutex_init(&parker->mutex, NULL) != 0) {
        EcsUiRaylibParkerDestroy(parker);
        return NULL;
    }
    parker->mutex_initialized = true;
    if (pthread_cond_init(&parker->cond, NULL) != 0) {
        EcsUiRaylibParkerDestroy(parker);
        return NULL;
    }
    parker->cond_initialized = true;
    if (pthread_create(&parker->thread, NULL, EcsUiRaylibWatcherMain, parker) !=
            0) {
        EcsUiRaylibParkerDestroy(parker);
        return NULL;
    }
    parker->thread_started = true;
#else
    parker->capabilities.post_wake_enabled = false;
    parker->capabilities.fd_wake_enabled = false;
    parker->capabilities.deadline_wake_enabled = false;
    parker->capabilities.capability_disabled = true;
#endif
    return parker;
}

EcsUiRaylibParker *EcsUiRaylibParkerCreateDefault(void)
{
#if defined(ECS_UI_RAYLIB_HAS_GLFW_POST)
    return EcsUiRaylibParkerCreate(
        &(EcsUiRaylibParkerDesc){
            .post_empty_event = EcsUiRaylibGlfwPostEmptyEvent,
        });
#else
    return EcsUiRaylibParkerCreate(NULL);
#endif
}

void EcsUiRaylibParkerDestroy(EcsUiRaylibParker *parker)
{
    if (parker == NULL) {
        return;
    }
#if !defined(_WIN32)
    if (parker->thread_started) {
        pthread_mutex_lock(&parker->mutex);
        parker->stop = true;
        pthread_mutex_unlock(&parker->mutex);
        EcsUiRaylibSignalControlPipe(parker, (unsigned char)'u');
        (void)pthread_join(parker->thread, NULL);
    }
    if (parker->control_pipe[0] >= 0) {
        (void)close(parker->control_pipe[0]);
    }
    if (parker->control_pipe[1] >= 0) {
        (void)close(parker->control_pipe[1]);
    }
    if (parker->cond_initialized) {
        (void)pthread_cond_destroy(&parker->cond);
    }
    if (parker->mutex_initialized) {
        (void)pthread_mutex_destroy(&parker->mutex);
    }
#endif
    free(parker);
}

EcsUiRaylibParkerCapabilities EcsUiRaylibParkerGetCapabilities(
    const EcsUiRaylibParker *parker)
{
    return parker != NULL ?
        parker->capabilities :
        (EcsUiRaylibParkerCapabilities){0};
}

bool EcsUiRaylibParkerArm(
    EcsUiRaylibParker *parker,
    const EcsUiWakeWaitSpec *spec)
{
    if (parker == NULL || spec == NULL) {
        return false;
    }

    if (spec->hot) {
        const EcsUiWakeSourceInfo *source =
            spec->active_count > 0u ? &spec->active[0] : NULL;
#if !defined(_WIN32)
        EcsUiRaylibRecordWake(
            parker,
            EcsUiRaylibWakeReasonForSource(
                ECS_UI_RAYLIB_WAKE_HOT,
                source),
            false);
#else
        parker->last_wake =
            EcsUiRaylibWakeReasonForSource(ECS_UI_RAYLIB_WAKE_HOT, source);
        parker->wake_sequence += 1u;
#endif
        return true;
    }

    if ((spec->fd_count > 0u && !parker->capabilities.fd_wake_enabled) ||
            (spec->has_deadline &&
                !parker->capabilities.deadline_wake_enabled)) {
        EcsUiRaylibRecordCapabilityDisabled(parker);
        return false;
    }

#if !defined(_WIN32)
    if (!parker->capabilities.post_empty_event_available) {
        EcsUiRaylibRecordCapabilityDisabled(parker);
        return false;
    }
    pthread_mutex_lock(&parker->mutex);
    parker->spec = *spec;
    parker->has_spec = true;
    parker->spec_generation += 1u;
    pthread_mutex_unlock(&parker->mutex);
    EcsUiRaylibSignalControlPipe(parker, (unsigned char)'u');
    return true;
#else
    EcsUiRaylibRecordCapabilityDisabled(parker);
    return false;
#endif
}

bool EcsUiRaylibParkerPost(
    EcsUiRaylibParker *parker,
    const char *label)
{
    if (parker == NULL || !parker->capabilities.post_wake_enabled) {
        EcsUiRaylibRecordCapabilityDisabled(parker);
        return false;
    }
#if !defined(_WIN32)
    pthread_mutex_lock(&parker->mutex);
    EcsUiRaylibCopyLabel(
        parker->pending_post_label,
        sizeof(parker->pending_post_label),
        label != NULL && label[0] != '\0' ?
            label :
            ECS_UI_RAYLIB_POST_LABEL);
    pthread_mutex_unlock(&parker->mutex);
    EcsUiRaylibSignalControlPipe(parker, (unsigned char)'p');
    return true;
#else
    EcsUiRaylibRecordCapabilityDisabled(parker);
    return false;
#endif
}

uint64_t EcsUiRaylibParkerWakeSequence(const EcsUiRaylibParker *parker)
{
    if (parker == NULL) {
        return 0u;
    }
#if !defined(_WIN32)
    if (!parker->mutex_initialized) {
        return parker->wake_sequence;
    }
    pthread_mutex_lock((pthread_mutex_t *)&parker->mutex);
    const uint64_t sequence = parker->wake_sequence;
    pthread_mutex_unlock((pthread_mutex_t *)&parker->mutex);
    return sequence;
#else
    return parker->wake_sequence;
#endif
}

bool EcsUiRaylibParkerWaitForWake(
    EcsUiRaylibParker *parker,
    uint64_t after_sequence,
    uint64_t timeout_ns,
    EcsUiRaylibWakeReason *out)
{
    if (parker == NULL) {
        return false;
    }
#if !defined(_WIN32)
    if (!parker->mutex_initialized) {
        if (parker->wake_sequence <= after_sequence) {
            return false;
        }
        if (out != NULL) {
            *out = parker->last_wake;
        }
        return true;
    }
    const struct timespec deadline = EcsUiRaylibRealtimeAfter(timeout_ns);
    pthread_mutex_lock(&parker->mutex);
    while (parker->wake_sequence <= after_sequence) {
        const int wait_result =
            pthread_cond_timedwait(&parker->cond, &parker->mutex, &deadline);
        if (wait_result == ETIMEDOUT) {
            pthread_mutex_unlock(&parker->mutex);
            return false;
        }
    }
    if (out != NULL) {
        *out = parker->last_wake;
    }
    pthread_mutex_unlock(&parker->mutex);
    return true;
#else
    if (parker->wake_sequence <= after_sequence) {
        return false;
    }
    if (out != NULL) {
        *out = parker->last_wake;
    }
    return true;
#endif
}

bool EcsUiRaylibParkerLastWake(
    const EcsUiRaylibParker *parker,
    EcsUiRaylibWakeReason *out)
{
    if (parker == NULL || out == NULL) {
        return false;
    }
#if !defined(_WIN32)
    if (!parker->mutex_initialized) {
        *out = parker->last_wake;
        return true;
    }
    pthread_mutex_lock((pthread_mutex_t *)&parker->mutex);
    *out = parker->last_wake;
    pthread_mutex_unlock((pthread_mutex_t *)&parker->mutex);
#else
    *out = parker->last_wake;
#endif
    return true;
}

static bool EcsUiRaylibStepRunHook(EcsUiRaylibStepHook hook, void *ctx)
{
    return hook != NULL && hook(ctx);
}

static bool EcsUiRaylibStepRunTick(
    EcsUiRaylibStepTickHook hook,
    double dt,
    void *ctx)
{
    return hook != NULL && hook(dt, ctx);
}

static void EcsUiRaylibStepFillWakeReasonFromFrame(
    const EcsUiFrameReason *frame_reason,
    EcsUiRaylibWakeReason *out)
{
    if (frame_reason == NULL || out == NULL ||
        frame_reason->kind != ECS_UI_FRAME_REASON_WAKE_HOT) {
        return;
    }
    *out = (EcsUiRaylibWakeReason){
        .kind = ECS_UI_RAYLIB_WAKE_HOT,
        .handle = frame_reason->wake,
        .fd = -1,
        .fd_interests = ECS_UI_WAKE_FD_INTEREST_NONE,
    };
    EcsUiRaylibCopyLabel(
        out->label,
        sizeof(out->label),
        frame_reason->label);
}

void EcsUiRaylibStepStateInit(EcsUiRaylibStepState *state)
{
    if (state != NULL) {
        *state = (EcsUiRaylibStepState){0};
    }
}

bool EcsUiRaylibStep(
    EcsUiRaylibStepState *state,
    const EcsUiRaylibStepDesc *desc,
    EcsUiRaylibStepResult *out)
{
    if (state == NULL || desc == NULL || out == NULL) {
        return false;
    }

    *out = (EcsUiRaylibStepResult){0};
    state->counters.steps += 1u;

    bool immediate = false;
    void *ctx = desc->hooks.ctx;
    immediate = EcsUiRaylibStepRunHook(
        desc->hooks.pre_input_pump,
        ctx) || immediate;
    immediate = EcsUiRaylibStepRunHook(desc->hooks.input_pump, ctx) || immediate;
    immediate = EcsUiRaylibStepRunTick(desc->hooks.tick, desc->dt, ctx) ||
        immediate;

    EcsUiWakeWaitSpec spec = {0};
    const bool has_spec =
        desc->wake_registry != NULL &&
        EcsUiWakeBuildWaitSpec(desc->wake_registry, desc->now_ns, &spec);
    EcsUiFrameClassifyResult frame = {0};
    if (desc->frame_signals != NULL) {
        (void)EcsUiFrameClassify(
            desc->frame_signals,
            &(EcsUiFrameClassifyDesc){
                .wake = has_spec ? &spec : NULL,
                .present_when_clean = desc->present_when_clean,
                .present_policy_label = desc->present_policy_label,
            },
            &frame);
    }
    out->frame_classification = frame.classification;
    out->frame_reason = frame.reason;
    out->presented = frame.should_present;

    if (frame.classification == ECS_UI_FRAME_PRESENT_ONLY) {
        state->counters.present_only += 1u;
    } else if (frame.classification == ECS_UI_FRAME_PARK) {
        state->counters.classified_park += 1u;
    }

    if (frame.should_render) {
        state->counters.rendered += 1u;
        out->rendered = true;
        immediate = EcsUiRaylibStepRunHook(desc->hooks.render, ctx) || immediate;
        immediate =
            EcsUiRaylibStepRunHook(
                desc->hooks.post_blit_screenshot,
                ctx) ||
            immediate;
    } else {
        state->counters.skipped_render += 1u;
    }

    if (frame.should_present) {
        immediate = EcsUiRaylibStepRunHook(desc->hooks.present, ctx) || immediate;
        immediate =
            EcsUiRaylibStepRunHook(desc->hooks.after_present_cleanup, ctx) ||
            immediate;
    }

    if (immediate) {
        state->counters.immediate += 1u;
        state->counters.continued += 1u;
    } else if (frame.classification == ECS_UI_FRAME_PRESENT_ONLY) {
        state->counters.continued += 1u;
        EcsUiRaylibStepFillWakeReasonFromFrame(
            &frame.reason,
            &out->wake_reason);
    } else if (frame.classification == ECS_UI_FRAME_RENDER_AND_PRESENT) {
        state->counters.continued += 1u;
    } else if (has_spec && desc->parker != NULL) {
        if (EcsUiRaylibParkerArm(desc->parker, &spec)) {
            state->counters.park_armed += 1u;
            out->park_armed = true;
            immediate =
                EcsUiRaylibStepRunHook(desc->hooks.park, ctx) || immediate;
            if (immediate) {
                state->counters.immediate += 1u;
                state->counters.continued += 1u;
            }
            (void)EcsUiRaylibParkerLastWake(desc->parker, &out->wake_reason);
        } else {
            state->counters.capability_disabled += 1u;
            state->counters.continued += 1u;
            (void)EcsUiRaylibParkerLastWake(desc->parker, &out->wake_reason);
        }
    } else {
        state->counters.continued += 1u;
    }

    out->immediate_next_step = immediate;
    out->counters = state->counters;
    return true;
}

static bool EcsUiRaylibRunDefaultWindowShouldClose(void *ctx)
{
    (void)ctx;
    return WindowShouldClose();
}

static double EcsUiRaylibRunDefaultDeltaTime(void *ctx)
{
    (void)ctx;
    return (double)GetFrameTime();
}

static uint64_t EcsUiRaylibRunDefaultNowNs(void *ctx)
{
    (void)ctx;
    return EcsUiRaylibNowNs();
}

bool EcsUiRaylibRun(
    const EcsUiRaylibRunConfig *config,
    const EcsUiRaylibRunCallbacks *callbacks,
    EcsUiRaylibRunResult *out)
{
    if (config == NULL || callbacks == NULL || out == NULL) {
        return false;
    }

    *out = (EcsUiRaylibRunResult){0};
    EcsUiRaylibParker *parker = EcsUiRaylibParkerCreateDefault();
    if (parker == NULL) {
        return false;
    }
    out->parker_capabilities = EcsUiRaylibParkerGetCapabilities(parker);

    if (config->enable_event_waiting) {
        EnableEventWaiting();
    }

    EcsUiRaylibStepState step_state;
    EcsUiRaylibStepStateInit(&step_state);
    bool ok = true;
    for (;;) {
        EcsUiRaylibRunHook window_should_close =
            callbacks->window_should_close != NULL ?
                callbacks->window_should_close :
                EcsUiRaylibRunDefaultWindowShouldClose;
        if (window_should_close(callbacks->step.ctx)) {
            out->window_should_close = true;
            break;
        }

        if (callbacks->should_quit != NULL &&
                callbacks->should_quit(callbacks->step.ctx)) {
            out->quit_requested = true;
            break;
        }

        EcsUiRaylibRunDeltaTimeFn delta_time =
            callbacks->delta_time != NULL ?
                callbacks->delta_time :
                EcsUiRaylibRunDefaultDeltaTime;
        EcsUiRaylibRunNowNsFn now_ns =
            callbacks->now_ns != NULL ?
                callbacks->now_ns :
                EcsUiRaylibRunDefaultNowNs;
        EcsUiRaylibStepResult step = {0};
        if (!EcsUiRaylibStep(
                &step_state,
                &(EcsUiRaylibStepDesc){
                    .frame_signals = config->frame_signals,
                    .wake_registry = config->wake_registry,
                    .parker = parker,
                    .now_ns = now_ns(callbacks->step.ctx),
                    .dt = delta_time(callbacks->step.ctx),
                    .present_when_clean = config->present_when_clean,
                    .present_policy_label = config->present_policy_label,
                    .hooks = callbacks->step,
                },
                &step)) {
            ok = false;
            break;
        }
        out->last_step = step;
        out->counters = step.counters;
        out->steps = step.counters.steps;

        if (callbacks->should_quit != NULL &&
                callbacks->should_quit(callbacks->step.ctx)) {
            out->quit_requested = true;
            break;
        }
    }

    if (config->enable_event_waiting) {
        DisableEventWaiting();
    }
    EcsUiRaylibParkerDestroy(parker);
    return ok;
}
