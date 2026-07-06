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

static bool StepPresent(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'E');
    return false;
}

static bool StepAfterPresent(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'A');
    return false;
}

static bool StepPark(void *ctx)
{
    StepOrderAppend((StepOrderCtx *)ctx, 'K');
    return false;
}

static int TestStepPhaseOrdering(void)
{
    int result = 0;
    EcsUiRaylibStepState state;
    EcsUiRaylibStepResult step = {0};
    StepOrderCtx order = {0};
    EcsUiFrameSignalAccumulator *signals =
        EcsUiFrameSignalAccumulatorCreate();

    EcsUiRaylibStepStateInit(&state);
    result |= Require(
        EcsUiRaylibStep(
            &state,
            &(EcsUiRaylibStepDesc){
                .frame_signals = signals,
                .dt = 0.125,
                .hooks = {
                    .pre_input_pump = StepPreInput,
                    .input_pump = StepInput,
                    .tick = StepTick,
                    .render = StepRender,
                    .post_blit_screenshot = StepScreenshot,
                    .present = StepPresent,
                    .after_present_cleanup = StepAfterPresent,
                    .ctx = &order,
                },
            },
            &step),
        "step failed");
    result |= Require(
        strcmp(order.order, "PITRSEA") == 0,
        "step phase order mismatch");
    result |= Require(order.dt == 0.125, "step dt mismatch");
    result |= Require(step.rendered, "step should report render");
    result |= Require(
        step.frame_classification == ECS_UI_FRAME_RENDER_AND_PRESENT &&
            step.frame_reason.kind == ECS_UI_FRAME_REASON_FIRST_FRAME,
        "step should expose render classification");
    result |= Require(!step.park_armed, "step should not park without spec");
    result |= Require(
        step.counters.steps == 1u &&
            step.counters.rendered == 1u &&
            step.counters.continued == 1u,
        "step counters mismatch");
    EcsUiFrameSignalAccumulatorDestroy(signals);
    return result;
}

static int TestStepSkipsRenderForParkAndPresentOnly(void)
{
    int result = 0;
    EcsUiFrameSignalAccumulator *signals =
        EcsUiFrameSignalAccumulatorCreate();
    EcsUiFrameClassifyResult seed = {0};
    EcsUiRaylibStepState state;
    EcsUiRaylibStepResult step = {0};
    StepOrderCtx order = {0};

    result |= Require(
        EcsUiFrameClassify(signals, NULL, &seed),
        "step skip seed classify failed");
    EcsUiRaylibStepStateInit(&state);
    result |= Require(
        EcsUiRaylibStep(
            &state,
            &(EcsUiRaylibStepDesc){
                .frame_signals = signals,
                .dt = 0.25,
                .hooks = {
                    .pre_input_pump = StepPreInput,
                    .input_pump = StepInput,
                    .tick = StepTick,
                    .render = StepRender,
                    .post_blit_screenshot = StepScreenshot,
                    .present = StepPresent,
                    .after_present_cleanup = StepAfterPresent,
                    .park = StepPark,
                    .ctx = &order,
                },
            },
            &step),
        "park step failed");
    result |= Require(
        strcmp(order.order, "PIT") == 0,
        "park step should skip present phases");
    result |= Require(
        step.frame_classification == ECS_UI_FRAME_PARK &&
            !step.rendered &&
            !step.presented,
        "park step classification mismatch");

    order = (StepOrderCtx){0};
    result |= Require(
        EcsUiRaylibStep(
            &state,
            &(EcsUiRaylibStepDesc){
                .frame_signals = signals,
                .dt = 0.25,
                .present_when_clean = true,
                .present_policy_label = "present-required",
                .hooks = {
                    .pre_input_pump = StepPreInput,
                    .input_pump = StepInput,
                    .tick = StepTick,
                    .render = StepRender,
                    .post_blit_screenshot = StepScreenshot,
                    .present = StepPresent,
                    .after_present_cleanup = StepAfterPresent,
                    .park = StepPark,
                    .ctx = &order,
                },
            },
            &step),
        "present-only step failed");
    result |= Require(
        strcmp(order.order, "PITEA") == 0,
        "present-only step should skip render phases");
    result |= Require(
        step.frame_classification == ECS_UI_FRAME_PRESENT_ONLY &&
            step.frame_reason.kind == ECS_UI_FRAME_REASON_PRESENT_POLICY &&
            strcmp(step.frame_reason.label, "present-required") == 0 &&
            !step.rendered &&
            step.presented,
        "present-only classification mismatch");

    result |= Require(
        EcsUiFrameSignalMark(signals, ECS_UI_FRAME_MARK_SCREENSHOT_REQUEST),
        "step screenshot mark failed");
    order = (StepOrderCtx){0};
    result |= Require(
        EcsUiRaylibStep(
            &state,
            &(EcsUiRaylibStepDesc){
                .frame_signals = signals,
                .dt = 0.25,
                .hooks = {
                    .pre_input_pump = StepPreInput,
                    .input_pump = StepInput,
                    .tick = StepTick,
                    .render = StepRender,
                    .post_blit_screenshot = StepScreenshot,
                    .present = StepPresent,
                    .after_present_cleanup = StepAfterPresent,
                    .park = StepPark,
                    .ctx = &order,
                },
            },
            &step),
        "screenshot step failed");
    result |= Require(
        strcmp(order.order, "PITRSEA") == 0,
        "screenshot step should run render and screenshot phases");
    result |= Require(
        step.frame_classification == ECS_UI_FRAME_RENDER_AND_PRESENT &&
            step.frame_reason.kind ==
                ECS_UI_FRAME_REASON_SCREENSHOT_REQUEST &&
            step.rendered &&
            step.presented,
        "screenshot step classification mismatch");

    EcsUiFrameSignalAccumulatorDestroy(signals);
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

typedef struct RunLifecycleCtx {
    uint32_t ticks;
    bool quit;
    double dt;
} RunLifecycleCtx;

static bool RunLifecycleShouldQuit(void *ctx)
{
    return ((RunLifecycleCtx *)ctx)->quit;
}

static bool RunLifecycleWindowShouldClose(void *ctx)
{
    (void)ctx;
    return false;
}

static double RunLifecycleDeltaTime(void *ctx)
{
    (void)ctx;
    return 0.016;
}

static uint64_t RunLifecycleNowNs(void *ctx)
{
    RunLifecycleCtx *run = (RunLifecycleCtx *)ctx;
    return 1000000000u + ((uint64_t)run->ticks * 16000000u);
}

static bool RunLifecycleHook(void *ctx)
{
    (void)ctx;
    return false;
}

static bool RunLifecycleTick(double dt, void *ctx)
{
    RunLifecycleCtx *run = (RunLifecycleCtx *)ctx;
    run->dt = dt;
    run->ticks += 1u;
    if (run->ticks >= 3u) {
        run->quit = true;
    }
    return false;
}

static int TestRunLifecycle(void)
{
    int result = 0;
    EcsUiFrameSignalAccumulator *signals =
        EcsUiFrameSignalAccumulatorCreate();
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    RunLifecycleCtx ctx = {0};
    EcsUiRaylibRunResult run = {0};

    result |= Require(signals != NULL, "run signals missing");
    result |= Require(registry != NULL, "run registry missing");
    result |= Require(
        EcsUiRaylibRun(
            &(EcsUiRaylibRunConfig){
                .frame_signals = signals,
                .wake_registry = registry,
                .present_when_clean = true,
                .present_policy_label = "test-present",
            },
            &(EcsUiRaylibRunCallbacks){
                .step = {
                    .pre_input_pump = RunLifecycleHook,
                    .input_pump = RunLifecycleHook,
                    .tick = RunLifecycleTick,
                    .render = RunLifecycleHook,
                    .present = RunLifecycleHook,
                    .after_present_cleanup = RunLifecycleHook,
                    .ctx = &ctx,
                },
                .should_quit = RunLifecycleShouldQuit,
                .window_should_close = RunLifecycleWindowShouldClose,
                .delta_time = RunLifecycleDeltaTime,
                .now_ns = RunLifecycleNowNs,
            },
            &run),
        "run lifecycle failed");
    result |= Require(run.quit_requested, "run should honor quit request");
    result |= Require(!run.window_should_close, "run should not close window");
    result |= Require(run.steps == 3u, "run should execute three steps");
    result |= Require(ctx.ticks == 3u, "run tick count mismatch");
    result |= Require(ctx.dt == 0.016, "run dt mismatch");
    result |= Require(
        run.counters.rendered == 1u &&
            run.counters.present_only == 2u,
        "run counters mismatch");

    EcsUiWakeRegistryDestroy(registry);
    EcsUiFrameSignalAccumulatorDestroy(signals);
    return result;
}

int main(void)
{
    int result = 0;
    result |= TestStepPhaseOrdering();
    result |= TestStepSkipsRenderForParkAndPresentOnly();
    result |= TestCapabilityDisabledIsVisible();
    result |= TestRunLifecycle();
#if !defined(_WIN32)
    result |= TestFdWake();
    result |= TestWorkerPostWake();
    result |= TestDeadlineWake();
#endif
    return result;
}
