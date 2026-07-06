#define _POSIX_C_SOURCE 200809L

#include "ecs_ui/ecs_ui_raylib.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

#if !defined(_WIN32)
static uint64_t TestMonotonicNs(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0u;
    }
    return ((uint64_t)now.tv_sec * 1000000000u) + (uint64_t)now.tv_nsec;
}

typedef struct TestPostCtx {
    atomic_uint count;
} TestPostCtx;

static void TestPostEmptyEvent(void *ctx)
{
    TestPostCtx *post = (TestPostCtx *)ctx;
    atomic_fetch_add_explicit(&post->count, 1u, memory_order_acq_rel);
}

static unsigned int TestPostCount(const TestPostCtx *ctx)
{
    return atomic_load_explicit(&ctx->count, memory_order_acquire);
}

static EcsUiRaylibParker *TestCreateParker(TestPostCtx *post)
{
    atomic_init(&post->count, 0u);
    return EcsUiRaylibParkerCreate(
        &(EcsUiRaylibParkerDesc){
            .post_empty_event = TestPostEmptyEvent,
            .post_empty_event_ctx = post,
        });
}

static int TestFdWake(void)
{
    int result = 0;
    int fds[2] = {-1, -1};
    TestPostCtx post = {0};
    EcsUiRaylibParker *parker = TestCreateParker(&post);
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeWaitSpec spec = {0};
    EcsUiRaylibWakeReason reason = {0};

    result |= Require(
        socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0,
        "socketpair failed");
    EcsUiWakeHandle fd_handle =
        EcsUiWakeRegisterPosixFd(
            registry,
            "attach-fd",
            fds[0],
            ECS_UI_WAKE_FD_INTEREST_READ);
    result |= Require(parker != NULL, "fd parker missing");
    result |= Require(
        EcsUiWakeHandleIsValid(fd_handle),
        "fd handle invalid");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, TestMonotonicNs(), &spec),
        "fd wait spec failed");
    const uint64_t sequence = EcsUiRaylibParkerWakeSequence(parker);
    result |= Require(
        EcsUiRaylibParkerArm(parker, &spec),
        "fd parker arm failed");

    const unsigned char byte = 7u;
    result |= Require(
        write(fds[1], &byte, sizeof(byte)) == (ssize_t)sizeof(byte),
        "socket write failed");
    result |= Require(
        EcsUiRaylibParkerWaitForWake(
            parker,
            sequence,
            250000000u,
            &reason),
        "fd wake timed out");
    result |= Require(
        reason.kind == ECS_UI_RAYLIB_WAKE_FD &&
            strcmp(reason.label, "attach-fd") == 0 &&
            reason.fd == fds[0] &&
            (reason.fd_interests & ECS_UI_WAKE_FD_INTEREST_READ) != 0u,
        "fd wake reason mismatch");
    result |= Require(
        TestPostCount(&post) == 1u,
        "fd wake should post empty event once");

    EcsUiRaylibParkerDestroy(parker);
    EcsUiWakeRegistryDestroy(registry);
    if (fds[0] >= 0) {
        (void)close(fds[0]);
    }
    if (fds[1] >= 0) {
        (void)close(fds[1]);
    }
    return result;
}

typedef struct TestWorkerPostCtx {
    EcsUiRaylibParker *parker;
    bool ok;
} TestWorkerPostCtx;

static void *TestWorkerPostMain(void *arg)
{
    TestWorkerPostCtx *ctx = (TestWorkerPostCtx *)arg;
    ctx->ok = EcsUiRaylibParkerPost(ctx->parker, "worker-post");
    return NULL;
}

static int TestWorkerPostWake(void)
{
    int result = 0;
    TestPostCtx post = {0};
    EcsUiRaylibParker *parker = TestCreateParker(&post);
    EcsUiWakeWaitSpec spec = {0};
    EcsUiRaylibWakeReason reason = {0};
    TestWorkerPostCtx worker = {
        .parker = parker,
        .ok = false,
    };
    pthread_t thread;

    result |= Require(parker != NULL, "post parker missing");
    const uint64_t sequence = EcsUiRaylibParkerWakeSequence(parker);
    result |= Require(
        EcsUiRaylibParkerArm(parker, &spec),
        "post parker arm failed");
    result |= Require(
        pthread_create(&thread, NULL, TestWorkerPostMain, &worker) == 0,
        "post pthread create failed");
    result |= Require(
        pthread_join(thread, NULL) == 0,
        "post pthread join failed");
    result |= Require(worker.ok, "worker post returned false");
    result |= Require(
        EcsUiRaylibParkerWaitForWake(
            parker,
            sequence,
            250000000u,
            &reason),
        "worker post wake timed out");
    result |= Require(
        reason.kind == ECS_UI_RAYLIB_WAKE_POST &&
            strcmp(reason.label, "worker-post") == 0,
        "worker post reason mismatch");
    result |= Require(
        TestPostCount(&post) == 1u,
        "worker post should post empty event once");
    result |= Require(
        !EcsUiRaylibParkerWaitForWake(
            parker,
            EcsUiRaylibParkerWakeSequence(parker),
            10000000u,
            &reason),
        "worker post should not wake twice");

    EcsUiRaylibParkerDestroy(parker);
    return result;
}

static int TestDeadlineWake(void)
{
    int result = 0;
    TestPostCtx post = {0};
    EcsUiRaylibParker *parker = TestCreateParker(&post);
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle deadline =
        EcsUiWakeRegisterDeadline(registry, "animation-deadline");
    EcsUiWakeWaitSpec spec = {0};
    EcsUiRaylibWakeReason reason = {0};
    const uint64_t now = TestMonotonicNs();

    result |= Require(parker != NULL, "deadline parker missing");
    result |= Require(now != 0u, "monotonic clock failed");
    result |= Require(
        EcsUiWakeArmDeadline(registry, deadline, now + 30000000u),
        "arm deadline failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, now, &spec),
        "deadline spec failed");
    const uint64_t sequence = EcsUiRaylibParkerWakeSequence(parker);
    result |= Require(
        EcsUiRaylibParkerArm(parker, &spec),
        "deadline parker arm failed");
    result |= Require(
        EcsUiRaylibParkerWaitForWake(
            parker,
            sequence,
            300000000u,
            &reason),
        "deadline wake timed out");
    result |= Require(
        reason.kind == ECS_UI_RAYLIB_WAKE_DEADLINE &&
            strcmp(reason.label, "animation-deadline") == 0,
        "deadline wake reason mismatch");
    result |= Require(
        TestPostCount(&post) == 1u,
        "deadline wake should post empty event once");

    EcsUiRaylibParkerDestroy(parker);
    EcsUiWakeRegistryDestroy(registry);
    return result;
}
#endif

typedef struct StepOrderCtx {
    char order[16];
    uint32_t count;
    double dt;
} StepOrderCtx;

static void StepOrderAppend(StepOrderCtx *ctx, char phase)
{
    if (ctx->count + 1u < sizeof(ctx->order)) {
        ctx->order[ctx->count] = phase;
        ctx->count += 1u;
        ctx->order[ctx->count] = '\0';
    }
}

static bool StepPreInput(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'P');
    return false;
}

static bool StepInput(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'I');
    return false;
}

static bool StepTick(double dt, void *ctx)
{
    StepOrderCtx *order = (StepOrderCtx *)ctx;
    order->dt = dt;
    StepOrderAppend(order, 'T');
    return false;
}

static bool StepRender(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'R');
    return false;
}

static bool StepScreenshot(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'S');
    return false;
}

static bool StepAfterPresent(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'A');
    return false;
}

static int TestStepPhaseOrdering(void)
{
    int result = 0;
    EcsUiRaylibStepState state;
    EcsUiRaylibStepResult step = {0};
    StepOrderCtx order = {0};

    EcsUiRaylibStepStateInit(&state);
    result |= Require(
        EcsUiRaylibStep(
            &state,
            &(EcsUiRaylibStepDesc){
                .dt = 0.125,
                .should_render = true,
                .hooks = {
                    .pre_input_pump = StepPreInput,
                    .input_pump = StepInput,
                    .tick = StepTick,
                    .render = StepRender,
                    .post_blit_screenshot = StepScreenshot,
                    .after_present_cleanup = StepAfterPresent,
                    .ctx = &order,
                },
            },
            &step),
        "step failed");
    result |= Require(
        strcmp(order.order, "PITRSA") == 0,
        "step phase order mismatch");
    result |= Require(order.dt == 0.125, "step dt mismatch");
    result |= Require(step.rendered, "step should report render");
    result |= Require(!step.park_armed, "step should not park without spec");
    result |= Require(
        step.counters.steps == 1u &&
            step.counters.rendered == 1u &&
            step.counters.continued == 1u,
        "step counters mismatch");
    return result;
}

static int TestCapabilityDisabledIsVisible(void)
{
    int result = 0;
    EcsUiRaylibParker *parker = EcsUiRaylibParkerCreate(NULL);
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle deadline =
        EcsUiWakeRegisterDeadline(registry, "needs-post");
    EcsUiWakeWaitSpec spec = {0};
    EcsUiRaylibWakeReason reason = {0};

    result |= Require(parker != NULL, "capability parker missing");
    EcsUiRaylibParkerCapabilities caps =
        EcsUiRaylibParkerGetCapabilities(parker);
    result |= Require(
        !caps.post_empty_event_available &&
            !caps.fd_wake_enabled &&
            caps.capability_disabled,
        "missing post capability should be visible");
    result |= Require(
        EcsUiWakeArmDeadline(registry, deadline, 1u),
        "capability deadline arm failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "capability wait spec failed");
    result |= Require(
        !EcsUiRaylibParkerArm(parker, &spec),
        "capability-disabled arm should fail");
    result |= Require(
        EcsUiRaylibParkerLastWake(parker, &reason),
        "capability last wake failed");
    result |= Require(
        reason.kind == ECS_UI_RAYLIB_WAKE_CAPABILITY_DISABLED,
        "capability disabled reason mismatch");

    EcsUiRaylibParkerDestroy(parker);
    EcsUiWakeRegistryDestroy(registry);
    return result;
}

int main(void)
{
    int result = 0;
    result |= TestStepPhaseOrdering();
    result |= TestCapabilityDisabledIsVisible();
#if !defined(_WIN32)
    result |= TestFdWake();
    result |= TestWorkerPostWake();
    result |= TestDeadlineWake();
#endif
    return result;
}
