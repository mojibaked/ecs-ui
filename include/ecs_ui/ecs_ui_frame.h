#ifndef ECS_UI_ECS_UI_FRAME_H
#define ECS_UI_ECS_UI_FRAME_H

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_INTERACTION_TARGET_MAX 4096u

typedef struct EcsUiPaintList EcsUiPaintList;

typedef struct EcsUiSize {
    float width;
    float height;
} EcsUiSize;

typedef struct EcsUiTextMeasureSpec {
    uint16_t font_id;
    float font_size;
    float letter_spacing;
    float line_height;
} EcsUiTextMeasureSpec;

typedef EcsUiSize (*EcsUiMeasureTextFn)(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data);

typedef enum EcsUiFrameErrorKind {
    ECS_UI_FRAME_ERROR_NONE = 0,
    ECS_UI_FRAME_ERROR_MEASURE_TEXT_MISSING = 1,
    ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY = 3,
    ECS_UI_FRAME_ERROR_TEXT_MEASURE_CAPACITY = 4,
    ECS_UI_FRAME_ERROR_DUPLICATE_ID = 5,
    ECS_UI_FRAME_ERROR_FLOATING_PARENT_NOT_FOUND = 6,
    ECS_UI_FRAME_ERROR_INTERNAL = 8,
    ECS_UI_FRAME_ERROR_NOT_INITIALIZED = 10,
    ECS_UI_FRAME_ERROR_ALREADY_INITIALIZED = 11,
    ECS_UI_FRAME_ERROR_ALLOCATION_FAILED = 12,
    ECS_UI_FRAME_ERROR_INVALID_ARGUMENT = 13,
    ECS_UI_FRAME_ERROR_STALE_INTERACTION_FRAME = 14,
} EcsUiFrameErrorKind;

typedef void (*EcsUiFrameErrorFn)(
    EcsUiFrameErrorKind kind,
    const char *message,
    void *user_data);

typedef struct EcsUiFrameBackendDesc {
    float surface_width;
    float surface_height;
    EcsUiMeasureTextFn measure_text;
    void *measure_user_data;
    EcsUiFrameErrorFn error;
    void *error_user_data;
} EcsUiFrameBackendDesc;

/*
 * Physical window pointer state consumed by the frame backend. Emitted
 * EcsUiEvent pointer coordinates are converted back to window-origin logical
 * units with the owning tree scale.
 */
typedef struct EcsUiPointerState {
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
} EcsUiPointerState;

typedef struct EcsUiFrameRect {
    float x;
    float y;
    float width;
    float height;
} EcsUiFrameRect;

typedef struct EcsUiFrameAttachPoints {
    EcsUiAlign element_x;
    EcsUiAlign element_y;
    EcsUiAlign parent_x;
    EcsUiAlign parent_y;
} EcsUiFrameAttachPoints;

/*
 * Frame layout inputs are physical pixels. `physical_bounds` positions the
 * tree's physical root box in window coordinates; enriched snapshot layout is
 * logical tree-root relative. Attachment anchors are authored in tree-root
 * logical units. For a full-window root this is the same as event coordinates;
 * bounded emits require callers to subtract
 * `physical_bounds.xy / tree->scale` from window-logical event coordinates
 * before storing root-relative anchor points.
 */
typedef struct EcsUiFrameLayoutOptions {
    EcsUiFrameRect physical_bounds;
    EcsUiFrameAttachPoints attach_points;
    int16_t z_index;
    bool capture_pointer;
} EcsUiFrameLayoutOptions;

typedef struct EcsUiInteractionTarget {
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
    float physical_x;
    float physical_y;
    float physical_width;
    float physical_height;
    bool clip_enabled;
    float clip_x;
    float clip_y;
    float clip_width;
    float clip_height;
    float scroll_content_width;
    float scroll_content_height;
    float scroll_viewport_width;
    float scroll_viewport_height;
    int16_t z_index;
    uint32_t root_order;
    uint32_t order;
} EcsUiInteractionTarget;

typedef struct EcsUiPointerCapture {
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
} EcsUiPointerCapture;

typedef struct EcsUiInteractionState {
    EcsUiPointerCapture capture;
} EcsUiInteractionState;

typedef struct EcsUiInteractionFrame {
    EcsUiInteractionState *state;
    EcsUiInteractionTarget targets[ECS_UI_INTERACTION_TARGET_MAX];
    uint32_t target_count;
    uint32_t inside_target_count;
    uint32_t pressable_target_count;
    ecs_entity_t resolved_tree;
    ecs_entity_t resolved_node;
    ecs_entity_t resolved_action;
    uint64_t resolved_payload;
    char resolved_node_id[ECS_UI_ID_MAX];
    bool resolved_pressable;
    EcsUiScrollUpdate pending_scrolls[ECS_UI_SCROLL_UPDATE_MAX];
    uint32_t pending_scroll_count;
    /*
     * True only when direct scroll-container routing produced a pending ECS
     * scroll-state update. Routed SCROLLED events do not set this.
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
} EcsUiInteractionFrame;

/*
 * Initialize the single active frame backend. The backend is main-thread-only:
 * all frame, collect, settle, surface-size, culling, and shutdown calls must be
 * made on the same thread that initialized it.
 */
bool EcsUiFrameBackendInit(const EcsUiFrameBackendDesc *desc);
void EcsUiFrameBackendShutdown(void);
void EcsUiFrameBackendSetSurfaceSize(float width, float height);
void EcsUiFrameBackendSetCullingEnabled(bool enabled);

/*
 * Run one frame through the active backend. The returned paint list is owned by
 * the backend and remains valid only until the next frame run or shutdown, and
 * only while the source snapshot remains alive and unmodified.
 *
 * A run also invalidates the previous run's interaction frame. If events are
 * needed, call EcsUiFrameCollectEvents for the frame produced by this run
 * before any other EcsUiFrameRun call, including headless NULL/NULL runs.
 */
const EcsUiPaintList *EcsUiFrameRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options,
    const EcsUiPointerState *pointer_or_null,
    EcsUiInteractionFrame *frame_or_null);

/*
 * Return the paint list produced by the most recent successful frame run.
 * Returns NULL when the most recent run did not produce paint, including paint
 * emission failures. The returned list must be rendered only with the snapshot
 * whose generation matches paint->generation.
 */
const EcsUiPaintList *EcsUiFramePaintList(void);

/*
 * Advance backend scroll housekeeping and reconcile ECS scroll components from
 * the most recent frame. `world` is required because EcsUiScrollState is the
 * only authority for offsets/content dimensions.
 */
void EcsUiFrameSettleScroll(ecs_world_t *world, double dt);

void EcsUiFrameInteractionStateInit(EcsUiInteractionState *state);
void EcsUiFrameCollectEvents(
    EcsUiInteractionFrame *frame,
    EcsUiPointerState pointer,
    EcsUiEventList *events);
bool EcsUiFrameApply(
    ecs_world_t *world,
    const EcsUiInteractionFrame *frame);
bool EcsUiFrameTreePointerInside(
    const EcsUiInteractionFrame *frame,
    ecs_entity_t tree);

#ifdef __cplusplus
}
#endif

#endif
