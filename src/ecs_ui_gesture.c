#include "ecs_ui/ecs_ui_gesture.h"

#include <string.h>

static float EcsUiGestureAbs(float value)
{
    return value < 0.0f ? -value : value;
}

static float EcsUiGestureMax(float left, float right)
{
    return left > right ? left : right;
}

static EcsUiGestureEventType EcsUiGestureCancelEventType(
    EcsUiGestureKind kind)
{
    switch (kind) {
    case ECS_UI_GESTURE_KIND_PRESS:
        return ECS_UI_GESTURE_EVENT_PRESS_CANCELLED;
    case ECS_UI_GESTURE_KIND_PAN:
    case ECS_UI_GESTURE_KIND_NONE:
    default:
        return ECS_UI_GESTURE_EVENT_CANCELLED;
    }
}

static float EcsUiGestureThreshold(EcsUiPanRecognizerDesc desc)
{
    return desc.threshold > 0.0f ? desc.threshold : 6.0f;
}

static bool EcsUiGestureAcceptsImmediately(EcsUiPanRecognizerDesc desc)
{
    return desc.start_immediately || desc.threshold <= 0.0f;
}

static bool EcsUiGesturePastThreshold(
    EcsUiPointerSample pointer,
    const EcsUiGestureArena *arena,
    EcsUiPanRecognizerDesc desc)
{
    if (arena == NULL) {
        return false;
    }

    const float dx = pointer.x - arena->start_x;
    const float dy = pointer.y - arena->start_y;
    const float threshold = EcsUiGestureThreshold(desc);
    return (dx * dx) + (dy * dy) >= threshold * threshold;
}

static void EcsUiGestureFillEvent(
    const EcsUiGestureArena *arena,
    EcsUiPointerSample pointer,
    EcsUiGestureEventType type,
    EcsUiGestureEvent *out)
{
    if (arena == NULL || out == NULL) {
        return;
    }

    float elapsed = (float)(pointer.time - arena->start_time);
    elapsed = EcsUiGestureMax(elapsed, 0.001f);
    const float delta_x = pointer.x - arena->start_x;
    const float delta_y = pointer.y - arena->start_y;
    *out = (EcsUiGestureEvent){
        .type = type,
        .target = arena->target,
        .x = pointer.x,
        .y = pointer.y,
        .start_x = arena->start_x,
        .start_y = arena->start_y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .frame_delta_x = pointer.x - arena->last_x,
        .frame_delta_y = pointer.y - arena->last_y,
        .has_local = arena->has_local,
        .local_x = pointer.x - arena->origin_x,
        .local_y = pointer.y - arena->origin_y,
        .start_local_x = arena->start_x - arena->origin_x,
        .start_local_y = arena->start_y - arena->origin_y,
        .elapsed = elapsed,
        .velocity_x = delta_x / elapsed,
        .velocity_y = delta_y / elapsed,
    };
}

static void EcsUiGestureStart(
    EcsUiGestureArena *arena,
    EcsUiPointerSample pointer,
    ecs_entity_t target,
    bool accepted,
    EcsUiGestureKind kind,
    bool has_local,
    float origin_x,
    float origin_y)
{
    if (arena == NULL) {
        return;
    }

    *arena = (EcsUiGestureArena){
        .active = true,
        .accepted = accepted,
        .kind = kind,
        .target = target,
        .has_local = has_local,
        .origin_x = origin_x,
        .origin_y = origin_y,
        .start_x = pointer.x,
        .start_y = pointer.y,
        .last_x = pointer.x,
        .last_y = pointer.y,
        .start_time = pointer.time,
        .last_time = pointer.time,
    };
}

static void EcsUiGestureRememberLast(
    EcsUiGestureArena *arena,
    EcsUiPointerSample pointer)
{
    if (arena == NULL) {
        return;
    }

    arena->last_x = pointer.x;
    arena->last_y = pointer.y;
    arena->last_time = pointer.time;
}

void EcsUiGestureArenaInit(EcsUiGestureArena *arena)
{
    if (arena == NULL) {
        return;
    }

    memset(arena, 0, sizeof(*arena));
}

bool EcsUiGestureArenaCancel(
    EcsUiGestureArena *arena,
    EcsUiGestureEvent *out)
{
    if (arena == NULL || !arena->active) {
        return false;
    }

    if (arena->accepted && out != NULL) {
        EcsUiGestureFillEvent(
            arena,
            (EcsUiPointerSample){
                .x = arena->last_x,
                .y = arena->last_y,
                .time = arena->last_time,
            },
            EcsUiGestureCancelEventType(arena->kind),
            out);
    }
    const bool emitted = arena->accepted;
    *arena = (EcsUiGestureArena){0};
    return emitted;
}

bool EcsUiGestureArenaUpdatePan(
    EcsUiGestureArena *arena,
    EcsUiPointerSample pointer,
    EcsUiPanRecognizerDesc desc,
    EcsUiGestureEvent *out)
{
    if (out != NULL) {
        *out = (EcsUiGestureEvent){0};
    }
    if (arena == NULL) {
        return false;
    }

    if (arena->active &&
        (!desc.enabled || desc.target == 0 || desc.target != arena->target ||
            arena->kind != ECS_UI_GESTURE_KIND_PAN)) {
        return EcsUiGestureArenaCancel(arena, out);
    }

    if (!arena->active) {
        if (!desc.enabled || desc.target == 0 || !desc.hit ||
            !pointer.pressed || !pointer.down) {
            return false;
        }

        EcsUiGestureStart(
            arena,
            pointer,
            desc.target,
            EcsUiGestureAcceptsImmediately(desc),
            ECS_UI_GESTURE_KIND_PAN,
            false,
            0.0f,
            0.0f);
        if (!arena->accepted) {
            return false;
        }

        EcsUiGestureFillEvent(
            arena,
            pointer,
            ECS_UI_GESTURE_EVENT_PAN_STARTED,
            out);
        return true;
    }

    if (!pointer.down || pointer.released) {
        if (!arena->accepted) {
            *arena = (EcsUiGestureArena){0};
            return false;
        }

        EcsUiGestureFillEvent(
            arena,
            pointer,
            ECS_UI_GESTURE_EVENT_PAN_ENDED,
            out);
        *arena = (EcsUiGestureArena){0};
        return true;
    }

    if (!arena->accepted) {
        if (!EcsUiGesturePastThreshold(pointer, arena, desc)) {
            EcsUiGestureRememberLast(arena, pointer);
            return false;
        }

        arena->accepted = true;
        EcsUiGestureFillEvent(
            arena,
            pointer,
            ECS_UI_GESTURE_EVENT_PAN_STARTED,
            out);
        EcsUiGestureRememberLast(arena, pointer);
        return true;
    }

    const float frame_dx = pointer.x - arena->last_x;
    const float frame_dy = pointer.y - arena->last_y;
    if (EcsUiGestureAbs(frame_dx) <= 0.0f &&
        EcsUiGestureAbs(frame_dy) <= 0.0f) {
        return false;
    }

    EcsUiGestureFillEvent(
        arena,
        pointer,
        ECS_UI_GESTURE_EVENT_PAN_UPDATED,
        out);
    EcsUiGestureRememberLast(arena, pointer);
    return true;
}

bool EcsUiGestureArenaUpdatePress(
    EcsUiGestureArena *arena,
    EcsUiPointerSample pointer,
    EcsUiPressRecognizerDesc desc,
    EcsUiGestureEvent *out)
{
    if (out != NULL) {
        *out = (EcsUiGestureEvent){0};
    }
    if (arena == NULL) {
        return false;
    }

    if (arena->active &&
        (!desc.enabled || desc.target == 0 || desc.target != arena->target ||
            arena->kind != ECS_UI_GESTURE_KIND_PRESS)) {
        return EcsUiGestureArenaCancel(arena, out);
    }

    if (!arena->active) {
        if (!desc.enabled || desc.target == 0 || !desc.hit ||
            !pointer.pressed || !pointer.down) {
            return false;
        }

        EcsUiGestureStart(
            arena,
            pointer,
            desc.target,
            true,
            ECS_UI_GESTURE_KIND_PRESS,
            desc.has_bounds,
            desc.x,
            desc.y);
        EcsUiGestureFillEvent(
            arena,
            pointer,
            ECS_UI_GESTURE_EVENT_PRESS_STARTED,
            out);
        return true;
    }

    if (!pointer.down || pointer.released) {
        EcsUiGestureFillEvent(
            arena,
            pointer,
            ECS_UI_GESTURE_EVENT_PRESS_RELEASED,
            out);
        *arena = (EcsUiGestureArena){0};
        return true;
    }

    const float frame_dx = pointer.x - arena->last_x;
    const float frame_dy = pointer.y - arena->last_y;
    if (EcsUiGestureAbs(frame_dx) <= 0.0f &&
        EcsUiGestureAbs(frame_dy) <= 0.0f) {
        return false;
    }

    EcsUiGestureFillEvent(
        arena,
        pointer,
        ECS_UI_GESTURE_EVENT_PRESS_MOVED,
        out);
    EcsUiGestureRememberLast(arena, pointer);
    return true;
}
