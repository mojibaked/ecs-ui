#include "ecs_ui/ecs_ui_runner.h"

#include <stdio.h>
#include <string.h>

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static int RequireFrame(
    EcsUiFrameSignalAccumulator *accumulator,
    const EcsUiFrameClassifyDesc *desc,
    EcsUiFrameClassification classification,
    EcsUiFrameReasonKind reason,
    const char *label,
    const char *message)
{
    EcsUiFrameClassifyResult result = {0};
    int status = 0;
    status |= Require(
        EcsUiFrameClassify(accumulator, desc, &result),
        "classification call failed");
    status |= Require(result.classification == classification, message);
    status |= Require(
        result.reason.kind == reason,
        "classification reason kind mismatch");
    status |= Require(
        strcmp(result.reason.label, label) == 0,
        "classification reason label mismatch");
    status |= Require(
        result.should_render ==
            (classification == ECS_UI_FRAME_RENDER_AND_PRESENT),
        "should_render mismatch");
    status |= Require(
        result.should_present == (classification != ECS_UI_FRAME_PARK),
        "should_present mismatch");
    status |= Require(
        result.should_park == (classification == ECS_UI_FRAME_PARK),
        "should_park mismatch");
    return status;
}

static int TestFrameDirtyMatrix(void)
{
    int result = 0;
    EcsUiFrameSignalAccumulator *accumulator =
        EcsUiFrameSignalAccumulatorCreate();
    EcsUiFrameSignalHandle projection =
        EcsUiFrameSignalRegisterStable(
            accumulator,
            1u,
            "projection",
            1u);
    EcsUiFrameSignalHandle canvas =
        EcsUiFrameSignalRegisterStable(
            accumulator,
            2u,
            "canvas",
            1u);
    EcsUiFrameSignalHandle selection =
        EcsUiFrameSignalRegisterStable(
            accumulator,
            3u,
            "selection-mask",
            1u);

    result |= Require(accumulator != NULL, "classifier missing");
    result |= Require(
        EcsUiFrameSignalHandleIsValid(projection) &&
            EcsUiFrameSignalHandleIsValid(canvas) &&
            EcsUiFrameSignalHandleIsValid(selection),
        "stable signal handles should be valid");

    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_FIRST_FRAME,
        "first-frame",
        "initial frame should render");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "unchanged frame should park");
    result |= RequireFrame(
        accumulator,
        &(EcsUiFrameClassifyDesc){
            .present_when_clean = true,
            .present_policy_label = "unoccluded",
        },
        ECS_UI_FRAME_PRESENT_ONLY,
        ECS_UI_FRAME_REASON_PRESENT_POLICY,
        "unoccluded",
        "unchanged frame should present when policy requires it");

    result |= Require(
        EcsUiFrameSignalMark(accumulator, ECS_UI_FRAME_MARK_INPUT),
        "input mark failed");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_INPUT,
        "input",
        "input should render once");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "cleared input should not render next frame");

    result |= Require(
        EcsUiFrameSignalSetRevision(accumulator, projection, 2u),
        "projection revision set failed");
    EcsUiFrameClassifyResult classified = {0};
    result |= Require(
        EcsUiFrameClassify(accumulator, NULL, &classified),
        "projection classify failed");
    result |= Require(
        classified.classification == ECS_UI_FRAME_RENDER_AND_PRESENT &&
            classified.reason.kind == ECS_UI_FRAME_REASON_STABLE_REVISION &&
            strcmp(classified.reason.label, "projection") == 0 &&
            classified.reason.previous_revision == 1u &&
            classified.reason.revision == 2u,
        "projection revision reason mismatch");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "same projection revision should be clean");

    result |= Require(
        EcsUiFrameSignalSetRevision(accumulator, projection, 3u),
        "projection revision second set failed");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_STABLE_REVISION,
        "projection",
        "another projection revision should render");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "same aggregate projection revision should be clean");

    result |= Require(
        EcsUiFrameSignalSetRevision(accumulator, canvas, 2u),
        "canvas revision set failed");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_STABLE_REVISION,
        "canvas",
        "canvas revision change should render");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "same canvas revision should be clean");

    result |= Require(
        EcsUiFrameSignalSetRevision(accumulator, selection, 2u),
        "selection revision set failed");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_STABLE_REVISION,
        "selection-mask",
        "selection revision change should render");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "same selection revision should be clean");

    result |= Require(
        EcsUiFrameSignalMark(accumulator, ECS_UI_FRAME_MARK_WINDOW_EVENT),
        "window event mark failed");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_WINDOW_EVENT,
        "window-event",
        "window event should render once");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "cleared window event should be clean");

    result |= Require(
        EcsUiFrameSignalMark(accumulator, ECS_UI_FRAME_MARK_FORCE_RENDER),
        "force render mark failed");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_FORCE_RENDER,
        "force-render",
        "force-render should render once");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "force-render should clear after rendering");

    result |= Require(
        EcsUiFrameSignalMark(
            accumulator,
            ECS_UI_FRAME_MARK_SCREENSHOT_REQUEST),
        "screenshot mark failed");
    result |= Require(
        EcsUiFrameClassify(accumulator, NULL, &classified),
        "screenshot classify failed");
    result |= Require(
        classified.classification == ECS_UI_FRAME_RENDER_AND_PRESENT &&
            classified.reason.kind ==
                ECS_UI_FRAME_REASON_SCREENSHOT_REQUEST &&
            strcmp(classified.reason.label, "screenshot-request") == 0 &&
            classified.screenshot_requested,
        "screenshot request should force render");
    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_PARK,
        ECS_UI_FRAME_REASON_NONE,
        "clean",
        "screenshot request should clear after rendering");

    EcsUiFrameSignalAccumulatorDestroy(accumulator);
    return result;
}

static int TestWakeHotPresentsOnly(void)
{
    int result = 0;
    EcsUiFrameSignalAccumulator *accumulator =
        EcsUiFrameSignalAccumulatorCreate();
    EcsUiWakeWaitSpec wake = {
        .hot = true,
        .active_count = 1u,
        .active = {
            {
                .kind = ECS_UI_WAKE_SOURCE_PENDING,
                .label = "pending-job",
            },
        },
    };

    result |= RequireFrame(
        accumulator,
        NULL,
        ECS_UI_FRAME_RENDER_AND_PRESENT,
        ECS_UI_FRAME_REASON_FIRST_FRAME,
        "first-frame",
        "wake test first frame should render");
    result |= RequireFrame(
        accumulator,
        &(EcsUiFrameClassifyDesc){
            .wake = &wake,
        },
        ECS_UI_FRAME_PRESENT_ONLY,
        ECS_UI_FRAME_REASON_WAKE_HOT,
        "pending-job",
        "hot wake should present without parking");

    EcsUiFrameSignalAccumulatorDestroy(accumulator);
    return result;
}

int main(void)
{
    int result = 0;
    result |= TestFrameDirtyMatrix();
    result |= TestWakeHotPresentsOnly();
    return result;
}
