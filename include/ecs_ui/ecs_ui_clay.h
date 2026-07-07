#ifndef ECS_UI_ECS_UI_CLAY_H
#define ECS_UI_ECS_UI_CLAY_H

#include <clay.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_CLAY_INTERACTION_TARGET_MAX 4096u

/*
 * Clay bridge inputs are physical pixels. `bounds` positions the tree's physical
 * root box in window coordinates; emitted EcsUiEvent pointer coordinates are
 * converted back to window-origin logical units with the tree scale. Placement
 * point anchors are authored in tree-root logical units; for a full-window root
 * this is the same as event coordinates, while bounded emits require callers to
 * subtract `bounds.xy / tree->scale` from window-logical event coordinates.
 */
typedef struct EcsUiClayLayoutOptions {
    Clay_BoundingBox bounds;
    Clay_FloatingAttachPoints attach_points;
    int16_t z_index;
    bool capture_pointer;
} EcsUiClayLayoutOptions;

/* Physical window pointer state consumed by the Clay bridge. */
typedef struct EcsUiClayPointerState {
    float x;
    float y;
    double time;
    bool down;
    bool pressed;
    bool released;
    bool secondary_down;
    bool secondary_pressed;
    bool secondary_released;
    bool middle_down;
    bool middle_pressed;
    bool middle_released;
    float scroll_x;
    float scroll_y;
} EcsUiClayPointerState;

typedef struct EcsUiClayInteractionTarget {
    Clay_ElementId clay_id;
    Clay_ElementId wrapper_id;
    ecs_entity_t tree;
    ecs_entity_t node;
    ecs_entity_t action;
    uint64_t payload;
    const EcsUiTreeSnapshot *tree_snapshot;
    char node_id[ECS_UI_ID_MAX];
    uint32_t node_index;
    uint32_t emit_order;
    uint32_t depth;
    float scale;
    bool area;
    bool pressable;
    bool blocking;
    bool scroll_container;
    bool scroll_subscribed;
    uint32_t scroll_axes;
    bool disabled;
    bool inside;
} EcsUiClayInteractionTarget;

typedef struct EcsUiClayPointerCapture {
    bool active;
    ecs_entity_t tree;
    ecs_entity_t node;
    ecs_entity_t action;
    uint64_t payload;
    char node_id[ECS_UI_ID_MAX];
    float scale;
    /* Logical start coordinates for emitted drag events. */
    float start_x;
    float start_y;
    /* Physical start coordinates kept for bridge drag-threshold tests. */
    float physical_start_x;
    float physical_start_y;
    double start_time;
    EcsUiPointerButton button;
} EcsUiClayPointerCapture;

typedef struct EcsUiClayInteractionState {
    EcsUiClayPointerCapture capture;
} EcsUiClayInteractionState;

typedef struct EcsUiClayInteractionFrame {
    EcsUiClayInteractionState *state;
    EcsUiClayInteractionTarget targets[ECS_UI_CLAY_INTERACTION_TARGET_MAX];
    uint32_t target_count;
    uint32_t inside_target_count;
    uint32_t pressable_target_count;
    ecs_entity_t resolved_tree;
    ecs_entity_t resolved_node;
    ecs_entity_t resolved_action;
    uint64_t resolved_payload;
    char resolved_node_id[ECS_UI_ID_MAX];
    bool resolved_pressable;
    /*
     * True only when direct scroll-container routing mutated Clay's retained
     * scroll position. Routed SCROLLED events do not set this; hosts should
     * still use the event list to schedule follow-up work for subscribers.
     */
    bool scroll_consumed;
    bool truncated;
    bool capture_missing_target;
    ecs_entity_t capture_missing_node;
    ecs_entity_t capture_missing_action;
    uint64_t capture_missing_payload;
    char capture_missing_node_id[ECS_UI_ID_MAX];
    bool capture_missed_release;
    ecs_entity_t capture_missed_release_node;
    ecs_entity_t capture_missed_release_action;
    uint64_t capture_missed_release_payload;
    char capture_missed_release_node_id[ECS_UI_ID_MAX];
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
/*
 * Enrich a snapshot with Clay post-layout rectangles from the current Clay
 * context. Call after EcsUiClayEmitTreeEx and Clay_EndLayout for the same
 * frame. Fills every node whose Clay element was emitted this frame; nodes
 * skipped by emission (for example fully transparent subtrees) remain
 * has_layout=false.
 *
 * Filled rectangles are logical units, tree-root relative. For bounded emits,
 * the Clay viewport origin is subtracted before dividing by tree->scale.
 */
uint32_t EcsUiClayEnrichSnapshotLayout(
    EcsUiTreeSnapshot *tree,
    const EcsUiClayLayoutOptions *options);
/*
 * `pointer` is physical window input matching Clay's pointer state. The emitted
 * EcsUiEvent coordinates, starts, deltas, and velocities are logical,
 * window-origin values divided by the owning tree's scale.
 */
void EcsUiClayCollectFrameEvents(
    EcsUiClayInteractionFrame *frame,
    EcsUiClayPointerState pointer,
    EcsUiEventList *events);
bool EcsUiClayApplyInteractionFrame(
    ecs_world_t *world,
    const EcsUiClayInteractionFrame *frame);
bool EcsUiClayInteractionFrameTreePointerInside(
    const EcsUiClayInteractionFrame *frame,
    ecs_entity_t tree);

#ifdef __cplusplus
}
#endif

#endif
