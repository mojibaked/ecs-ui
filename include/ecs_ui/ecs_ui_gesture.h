#ifndef ECS_UI_ECS_UI_GESTURE_H
#define ECS_UI_ECS_UI_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

#include <flecs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EcsUiGestureEventType {
    ECS_UI_GESTURE_EVENT_NONE = 0,
    ECS_UI_GESTURE_EVENT_PAN_STARTED = 1,
    ECS_UI_GESTURE_EVENT_PAN_UPDATED = 2,
    ECS_UI_GESTURE_EVENT_PAN_ENDED = 3,
    ECS_UI_GESTURE_EVENT_CANCELLED = 4,
    ECS_UI_GESTURE_EVENT_PRESS_STARTED = 5,
    ECS_UI_GESTURE_EVENT_PRESS_MOVED = 6,
    ECS_UI_GESTURE_EVENT_PRESS_RELEASED = 7,
    ECS_UI_GESTURE_EVENT_PRESS_CANCELLED = 8,
} EcsUiGestureEventType;

typedef enum EcsUiGestureKind {
    ECS_UI_GESTURE_KIND_NONE = 0,
    ECS_UI_GESTURE_KIND_PAN = 1,
    ECS_UI_GESTURE_KIND_PRESS = 2,
} EcsUiGestureKind;

/* Public gesture coordinates are logical, window-origin units. */
typedef struct EcsUiPointerSample {
    float x;
    float y;
    double time;
    bool down;
    bool pressed;
    bool released;
} EcsUiPointerSample;

typedef struct EcsUiPanRecognizerDesc {
    ecs_entity_t target;
    bool enabled;
    bool hit;
    bool start_immediately;
    float threshold;
} EcsUiPanRecognizerDesc;

/* Press recognizer bounds are logical, window-origin units. */
typedef struct EcsUiPressRecognizerDesc {
    ecs_entity_t target;
    bool enabled;
    bool hit;
    bool has_bounds;
    float x;
    float y;
    float width;
    float height;
} EcsUiPressRecognizerDesc;

typedef struct EcsUiGestureArena {
    bool active;
    bool accepted;
    EcsUiGestureKind kind;
    ecs_entity_t target;
    bool has_local;
    float origin_x;
    float origin_y;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
    double start_time;
    double last_time;
} EcsUiGestureArena;

/* Gesture event coordinates, deltas, and velocities use logical units. */
typedef struct EcsUiGestureEvent {
    EcsUiGestureEventType type;
    ecs_entity_t target;
    float x;
    float y;
    float start_x;
    float start_y;
    float delta_x;
    float delta_y;
    float frame_delta_x;
    float frame_delta_y;
    bool has_local;
    float local_x;
    float local_y;
    float start_local_x;
    float start_local_y;
    float elapsed;
    float velocity_x;
    float velocity_y;
} EcsUiGestureEvent;

void EcsUiGestureArenaInit(EcsUiGestureArena *arena);
bool EcsUiGestureArenaCancel(
    EcsUiGestureArena *arena,
    EcsUiGestureEvent *out);
bool EcsUiGestureArenaUpdatePan(
    EcsUiGestureArena *arena,
    EcsUiPointerSample pointer,
    EcsUiPanRecognizerDesc desc,
    EcsUiGestureEvent *out);
bool EcsUiGestureArenaUpdatePress(
    EcsUiGestureArena *arena,
    EcsUiPointerSample pointer,
    EcsUiPressRecognizerDesc desc,
    EcsUiGestureEvent *out);

#ifdef __cplusplus
}
#endif

#endif
