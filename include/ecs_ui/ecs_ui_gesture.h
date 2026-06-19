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
} EcsUiGestureEventType;

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

typedef struct EcsUiGestureArena {
    bool active;
    bool accepted;
    ecs_entity_t target;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
    double start_time;
    double last_time;
} EcsUiGestureArena;

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

#ifdef __cplusplus
}
#endif

#endif
