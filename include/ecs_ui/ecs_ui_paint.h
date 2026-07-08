#ifndef ECS_UI_ECS_UI_PAINT_H
#define ECS_UI_ECS_UI_PAINT_H

#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shared frame pool sized for current and near-term roles: box, border, clip,
 * bevel edges, and several text/subpart items per source node. Widen this with
 * the stage that proves it needs more via stress coverage.
 */
#define ECS_UI_PAINT_ITEMS_PER_NODE_HEADROOM 16u
#define ECS_UI_PAINT_ITEM_MAX \
    (ECS_UI_TREE_NODE_MAX * ECS_UI_PAINT_ITEMS_PER_NODE_HEADROOM)

typedef struct EcsUiColorF {
    float r;
    float g;
    float b;
    float a;
} EcsUiColorF;

typedef enum EcsUiPaintRole {
    ECS_UI_PAINT_ROLE_NONE = 0,
    ECS_UI_PAINT_ROLE_BOX = 1,
    ECS_UI_PAINT_ROLE_BORDER = 2,
    ECS_UI_PAINT_ROLE_TEXT_RUN = 3,
    ECS_UI_PAINT_ROLE_BEVEL_EDGE = 4,
    ECS_UI_PAINT_ROLE_CARET = 5,
    ECS_UI_PAINT_ROLE_SELECTION = 6,
    ECS_UI_PAINT_ROLE_NINE_SLICE = 7,
    ECS_UI_PAINT_ROLE_CUSTOM = 8,
    ECS_UI_PAINT_ROLE_ICON = 9,
    ECS_UI_PAINT_ROLE_CLIP_SCOPE = 10,
} EcsUiPaintRole;

typedef enum EcsUiPaintPrimitive {
    ECS_UI_PAINT_PRIMITIVE_NONE = 0,
    ECS_UI_PAINT_PRIMITIVE_BOX = 1,
    ECS_UI_PAINT_PRIMITIVE_CUSTOM = 2,
    ECS_UI_PAINT_PRIMITIVE_TEXT_RUN = 3,
    ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE = 4,
    ECS_UI_PAINT_PRIMITIVE_BORDER = 5,
} EcsUiPaintPrimitive;

typedef enum EcsUiPaintBevelEdgePart {
    ECS_UI_PAINT_BEVEL_EDGE_TOP = 0,
    ECS_UI_PAINT_BEVEL_EDGE_LEFT = 1,
    ECS_UI_PAINT_BEVEL_EDGE_BOTTOM = 2,
    ECS_UI_PAINT_BEVEL_EDGE_RIGHT = 3,
} EcsUiPaintBevelEdgePart;

typedef enum EcsUiPaintClipScopePart {
    ECS_UI_PAINT_CLIP_SCOPE_START = 0,
    ECS_UI_PAINT_CLIP_SCOPE_END = 1,
} EcsUiPaintClipScopePart;

typedef struct EcsUiPaintKey {
    ecs_entity_t source;
    uint16_t role;
    uint16_t part;
    uint32_t generation;
} EcsUiPaintKey;

typedef struct EcsUiPaintRect {
    float x;
    float y;
    float width;
    float height;
} EcsUiPaintRect;

typedef struct EcsUiPaintClip {
    uint32_t scope;
    EcsUiPaintRect rect;
    bool enabled;
} EcsUiPaintClip;

typedef struct EcsUiPaintCornerRadius {
    float top_left;
    float top_right;
    float bottom_left;
    float bottom_right;
} EcsUiPaintCornerRadius;

typedef struct EcsUiPaintBorder {
    EcsUiColorF color;
    EcsUiPaintCornerRadius radius;
    float left;
    float top;
    float right;
    float bottom;
    bool has_border;
} EcsUiPaintBorder;

typedef struct EcsUiPaintBox {
    EcsUiColorF fill;
    EcsUiPaintCornerRadius radius;
} EcsUiPaintBox;

typedef struct EcsUiPaintBevelEdge {
    EcsUiColorF color;
} EcsUiPaintBevelEdge;

typedef struct EcsUiPaintCustom {
    ecs_entity_t source;
    EcsUiColorF color;
} EcsUiPaintCustom;

typedef struct EcsUiPaintTextRun {
    const char *text;
    uint32_t byte_start;
    uint32_t byte_end;
    uint16_t font_id;
    uint16_t font_size;
    uint16_t letter_spacing;
    EcsUiColorF color;
} EcsUiPaintTextRun;

typedef struct EcsUiPaintItem {
    EcsUiPaintKey key;
    uint16_t primitive;
    int16_t z_index;
    EcsUiPaintRect rect;
    EcsUiPaintClip clip;
    float opacity;
    uint32_t root_order;
    uint32_t order;
    union {
        EcsUiPaintBox box;
        EcsUiPaintBorder border;
        EcsUiPaintBevelEdge bevel_edge;
        EcsUiPaintCustom custom;
        EcsUiPaintTextRun text_run;
    } payload;
} EcsUiPaintItem;

/*
 * Renderer-neutral frame artifact. Clay render commands are a transition-only
 * adapter format; renderers should consume EcsUiPaintList as the durable
 * semantic paint surface.
 */
typedef struct EcsUiPaintList {
    ecs_entity_t tree;
    uint32_t generation;
    uint32_t count;
    bool truncated;
    EcsUiPaintItem items[ECS_UI_PAINT_ITEM_MAX];
} EcsUiPaintList;

typedef struct EcsUiFrameArtifacts {
    const EcsUiPaintList *paint;
    uint32_t generation;
} EcsUiFrameArtifacts;

void EcsUiPaintListReset(
    EcsUiPaintList *list,
    ecs_entity_t tree,
    uint32_t generation);
bool EcsUiPaintListBuild(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
    int16_t base_z_index);

extern ECS_COMPONENT_DECLARE(EcsUiFrameArtifacts);

#ifdef __cplusplus
}
#endif

#endif
