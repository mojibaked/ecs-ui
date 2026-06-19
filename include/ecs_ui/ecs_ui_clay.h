#ifndef ECS_UI_ECS_UI_CLAY_H
#define ECS_UI_ECS_UI_CLAY_H

#include <clay.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_CLAY_INTERACTION_TARGET_MAX 4096u

typedef struct EcsUiClayLayoutOptions {
    Clay_BoundingBox bounds;
    Clay_FloatingAttachPoints attach_points;
    int16_t z_index;
    bool capture_pointer;
} EcsUiClayLayoutOptions;

typedef struct EcsUiClayPointerState {
    float x;
    float y;
    double time;
    bool down;
    bool pressed;
    bool released;
} EcsUiClayPointerState;

typedef struct EcsUiClayInteractionTarget {
    Clay_ElementId clay_id;
    Clay_ElementId wrapper_id;
    ecs_entity_t tree;
    ecs_entity_t node;
    ecs_entity_t action;
    char node_id[ECS_UI_ID_MAX];
    uint32_t node_index;
    uint32_t emit_order;
    uint32_t depth;
    bool area;
    bool pressable;
    bool blocking;
    bool disabled;
    bool inside;
} EcsUiClayInteractionTarget;

typedef struct EcsUiClayPointerCapture {
    bool active;
    ecs_entity_t tree;
    ecs_entity_t node;
    ecs_entity_t action;
    char node_id[ECS_UI_ID_MAX];
    float start_x;
    float start_y;
    double start_time;
} EcsUiClayPointerCapture;

typedef struct EcsUiClayInteractionState {
    EcsUiClayPointerCapture capture;
} EcsUiClayInteractionState;

typedef struct EcsUiClayInteractionFrame {
    EcsUiClayInteractionState *state;
    EcsUiClayInteractionTarget targets[ECS_UI_CLAY_INTERACTION_TARGET_MAX];
    uint32_t target_count;
    ecs_entity_t resolved_tree;
    ecs_entity_t resolved_node;
    bool truncated;
} EcsUiClayInteractionFrame;

void EcsUiClayInteractionStateInit(EcsUiClayInteractionState *state);
void EcsUiClayInteractionFrameBegin(
    EcsUiClayInteractionFrame *frame,
    EcsUiClayInteractionState *state);
void EcsUiClayEmitTree(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    EcsUiClayInteractionFrame *frame);
void EcsUiClayEmitTreeEx(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiClayLayoutOptions *options,
    EcsUiClayInteractionFrame *frame);
void EcsUiClayCollectFrameEvents(
    EcsUiClayInteractionFrame *frame,
    EcsUiClayPointerState pointer,
    EcsUiEventList *events);
bool EcsUiClayInteractionFrameTreePointerInside(
    const EcsUiClayInteractionFrame *frame,
    ecs_entity_t tree);

#ifdef __cplusplus
}
#endif

#endif
