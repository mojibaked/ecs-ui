#include "ecs_ui_solver.h"
#include "ecs_ui_style.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ECS_UI_SOLVER_ICON_SIZE 16.0f

typedef struct EcsUiSolverRect {
    float x;
    float y;
    float width;
    float height;
} EcsUiSolverRect;

typedef struct EcsUiSolverLayout {
    EcsUiSolverRect rect;
    bool emitted;
} EcsUiSolverLayout;

typedef struct EcsUiSolverElementIdEntry {
    char id[ECS_UI_ID_MAX * 2u];
    uint32_t index;
} EcsUiSolverElementIdEntry;

typedef struct EcsUiSolverContext {
    EcsUiTreeSnapshot *tree;
    EcsUiSolverArena *arena;
    EcsUiSolverElementIdEntry *element_ids;
    EcsUiSolverLayout *layouts;
    struct EcsUiSolverMetrics *metrics;
    struct EcsUiSolverVirtualFlow *virtual_entries;
    uint32_t virtual_count;
    uint32_t virtual_capacity;
    struct EcsUiSolverFlowItem *scratch_items;
    uint32_t scratch_item_capacity;
    const EcsUiFrameLayoutOptions *layout_options;
    float surface_width;
    float surface_height;
    EcsUiMeasureTextFn measure_text;
    void *measure_user_data;
    const EcsUiSolverScrollOffset *scroll_offsets;
    uint32_t scroll_offset_count;
    EcsUiSolverScrollContent *scroll_contents;
    uint32_t scroll_content_count;
    uint32_t text_line_capacity;
    EcsUiFrameErrorKind *error_kind;
    char *error_message;
    size_t error_message_size;
    bool failed;
} EcsUiSolverContext;

static float EcsUiSolverMaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static float EcsUiSolverMinFloat(float a, float b)
{
    return a < b ? a : b;
}

static float EcsUiSolverClampPositive(float value)
{
    return value > 0.0f ? value : 0.0f;
}

static float EcsUiSolverClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float EcsUiSolverScale(const EcsUiTreeSnapshot *tree)
{
    return tree != NULL && tree->scale > 0.0f ? tree->scale : 1.0f;
}

static float EcsUiSolverPhysicalEpsilon(const EcsUiTreeSnapshot *tree)
{
    return 0.01f / EcsUiSolverScale(tree);
}

static uint16_t EcsUiSolverU16(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= 65535.0f) {
        return 65535u;
    }
    return (uint16_t)value;
}

static float EcsUiSolverScaledU16Logical(
    const EcsUiTreeSnapshot *tree,
    float value)
{
    const float scale = EcsUiSolverScale(tree);
    return (float)EcsUiSolverU16(value * scale) / scale;
}

static float EcsUiSolverLogicalRootWidth(
    const EcsUiTreeSnapshot *tree,
    const EcsUiFrameLayoutOptions *options)
{
    if (options != NULL) {
        return options->physical_bounds.width / EcsUiSolverScale(tree);
    }
    if (tree != NULL && tree->count > 0u) {
        return EcsUiSolverClampPositive(tree->nodes[0].stack.preferred_width);
    }
    return 0.0f;
}

static float EcsUiSolverLogicalRootHeight(
    const EcsUiTreeSnapshot *tree,
    const EcsUiFrameLayoutOptions *options)
{
    if (options != NULL) {
        return options->physical_bounds.height / EcsUiSolverScale(tree);
    }
    if (tree != NULL && tree->count > 0u) {
        return EcsUiSolverClampPositive(tree->nodes[0].stack.preferred_height);
    }
    return 0.0f;
}

static float EcsUiSolverLogicalSurfaceWidth(const EcsUiSolverContext *ctx)
{
    if (ctx != NULL && ctx->surface_width > 0.0f) {
        return ctx->surface_width / EcsUiSolverScale(ctx->tree);
    }
    return EcsUiSolverLogicalRootWidth(
        ctx != NULL ? ctx->tree : NULL,
        ctx != NULL ? ctx->layout_options : NULL);
}

static float EcsUiSolverLogicalSurfaceHeight(const EcsUiSolverContext *ctx)
{
    if (ctx != NULL && ctx->surface_height > 0.0f) {
        return ctx->surface_height / EcsUiSolverScale(ctx->tree);
    }
    return EcsUiSolverLogicalRootHeight(
        ctx != NULL ? ctx->tree : NULL,
        ctx != NULL ? ctx->layout_options : NULL);
}

static bool EcsUiSolverNodeIsStack(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL &&
        (node->kind == ECS_UI_NODE_ROOT ||
         node->kind == ECS_UI_NODE_VSTACK ||
         node->kind == ECS_UI_NODE_HSTACK ||
         node->kind == ECS_UI_NODE_ZSTACK);
}

static float EcsUiSolverStackPaddingSide(float side, float uniform)
{
    return EcsUiSolverClampPositive(side > 0.0f ? side : uniform);
}

static float EcsUiSolverPaddingLeft(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return EcsUiSolverScaledU16Logical(tree, 14.0f);
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        const float padding = node->has_box_style && node->box_style.padding > 0.0f ?
            node->box_style.padding :
            12.0f;
        return EcsUiSolverScaledU16Logical(tree, padding);
    }
    return EcsUiSolverScaledU16Logical(
        tree,
        EcsUiSolverStackPaddingSide(
            node->stack.padding_left,
            node->stack.padding));
}

static float EcsUiSolverPaddingTop(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiSolverScaledU16Logical(
        tree,
        EcsUiSolverStackPaddingSide(
            node->stack.padding_top,
            node->stack.padding));
}

static float EcsUiSolverPaddingRight(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return EcsUiSolverScaledU16Logical(tree, 14.0f);
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        const float padding = node->has_box_style && node->box_style.padding > 0.0f ?
            node->box_style.padding :
            12.0f;
        return EcsUiSolverScaledU16Logical(tree, padding);
    }
    return EcsUiSolverScaledU16Logical(
        tree,
        EcsUiSolverStackPaddingSide(
            node->stack.padding_right,
            node->stack.padding));
}

static float EcsUiSolverPaddingBottom(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiSolverScaledU16Logical(
        tree,
        EcsUiSolverStackPaddingSide(
            node->stack.padding_bottom,
            node->stack.padding));
}

static float EcsUiSolverGap(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return EcsUiSolverScaledU16Logical(tree, 8.0f);
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return node->has_text_field_view ?
            0.0f :
            EcsUiSolverScaledU16Logical(tree, 8.0f);
    }
    if (node->kind == ECS_UI_NODE_ZSTACK) {
        return 0.0f;
    }
    return EcsUiSolverScaledU16Logical(
        tree,
        EcsUiSolverClampPositive(node->stack.gap));
}

static float EcsUiSolverButtonHeight(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->button.preferred_height > 0.0f ?
        node->button.preferred_height :
        46.0f;
}

static float EcsUiSolverPressableHeight(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->pressable.preferred_height > 0.0f ?
        node->pressable.preferred_height :
        46.0f;
}

static float EcsUiSolverCustomHeight(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->custom.preferred_height > 0.0f ?
        node->custom.preferred_height :
        96.0f;
}

static float EcsUiSolverInheritedTextSize(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    return EcsUiStyleInheritedTextSize(tree, index);
}

static float EcsUiSolverLocalTextSize(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL ?
        EcsUiStyleTextSize(
            node->text.role,
            node->text_style,
            node->has_text_style) :
        EcsUiStyleRoleTextSize(ECS_UI_TEXT_BODY);
}

static void EcsUiSolverArenaReset(EcsUiSolverArena *arena)
{
    if (arena != NULL) {
        arena->used = 0u;
    }
}

void EcsUiSolverArenaRelease(EcsUiSolverArena *arena)
{
    if (arena == NULL) {
        return;
    }
    free(arena->data);
    *arena = (EcsUiSolverArena){0};
}

static void *EcsUiSolverArenaAlloc(
    EcsUiSolverArena *arena,
    size_t size,
    size_t align)
{
    if (arena == NULL || size == 0u) {
        return NULL;
    }
    if (align == 0u) {
        align = sizeof(void *);
    }
    const size_t mask = align - 1u;
    size_t offset = (arena->used + mask) & ~mask;
    if (offset > SIZE_MAX - size) {
        return NULL;
    }
    const size_t needed = offset + size;
    if (needed > arena->capacity) {
        size_t next_capacity = arena->capacity > 0u ? arena->capacity : 4096u;
        while (next_capacity < needed) {
            if (next_capacity > SIZE_MAX / 2u) {
                next_capacity = needed;
                break;
            }
            next_capacity *= 2u;
        }
        unsigned char *data = (unsigned char *)realloc(arena->data, next_capacity);
        if (data == NULL) {
            return NULL;
        }
        arena->data = data;
        arena->capacity = next_capacity;
    }
    void *out = arena->data + offset;
    arena->used = needed;
    memset(out, 0, size);
    return out;
}

static void EcsUiSolverSetErrorKind(
    EcsUiSolverContext *ctx,
    EcsUiFrameErrorKind kind,
    const char *message)
{
    if (ctx == NULL) {
        return;
    }
    ctx->failed = true;
    if (ctx->error_kind != NULL) {
        *ctx->error_kind = kind;
    }
    if (ctx->error_message == NULL || ctx->error_message_size == 0u ||
            message == NULL) {
        return;
    }
    (void)snprintf(
        ctx->error_message,
        ctx->error_message_size,
        "%s",
        message);
}

static void EcsUiSolverSetError(
    EcsUiSolverContext *ctx,
    const char *message)
{
    EcsUiSolverSetErrorKind(
        ctx,
        ECS_UI_FRAME_ERROR_INTERNAL,
        message);
}

static void EcsUiSolverElementId(
    const EcsUiTreeNodeSnapshot *node,
    char *out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (node == NULL) {
        return;
    }

    const char *authored_id = node->id[0] != '\0' ? node->id : "Node";
    (void)snprintf(
        out,
        out_size,
        "%s_%llu",
        authored_id,
        (unsigned long long)node->entity);
}

static int EcsUiSolverCompareElementIdEntries(
    const void *a,
    const void *b)
{
    const EcsUiSolverElementIdEntry *entry_a = a;
    const EcsUiSolverElementIdEntry *entry_b = b;
    const int cmp = strcmp(entry_a->id, entry_b->id);
    if (cmp != 0) {
        return cmp;
    }
    if (entry_a->index < entry_b->index) {
        return -1;
    }
    if (entry_a->index > entry_b->index) {
        return 1;
    }
    return 0;
}

static bool EcsUiSolverValidateDuplicateElementIds(EcsUiSolverContext *ctx)
{
    if (ctx == NULL || ctx->tree == NULL || ctx->element_ids == NULL) {
        return false;
    }
    const EcsUiTreeSnapshot *tree = ctx->tree;
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        ctx->element_ids[i] = (EcsUiSolverElementIdEntry){.index = i};
        EcsUiSolverElementId(
            &tree->nodes[i],
            ctx->element_ids[i].id,
            sizeof(ctx->element_ids[i].id));
    }
    qsort(
        ctx->element_ids,
        tree->count,
        sizeof(ctx->element_ids[0]),
        EcsUiSolverCompareElementIdEntries);
    for (uint32_t i = 1u; i < tree->count; i += 1u) {
        const char *previous = ctx->element_ids[i - 1u].id;
        const char *current = ctx->element_ids[i].id;
        if (previous[0] != '\0' && strcmp(previous, current) == 0) {
            EcsUiSolverSetErrorKind(
                ctx,
                ECS_UI_FRAME_ERROR_DUPLICATE_ID,
                "native layout solver duplicate element id");
            return false;
        }
    }
    return true;
}

static bool EcsUiSolverValidatePlacementParents(EcsUiSolverContext *ctx)
{
    if (ctx == NULL || ctx->tree == NULL) {
        return false;
    }
    const EcsUiTreeSnapshot *tree = ctx->tree;
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        const EcsUiTreeNodeSnapshot *node = &tree->nodes[i];
        if (!node->has_placement) {
            continue;
        }
        if (node->parent == 0 ||
                node->parent_index == ECS_UI_TREE_INVALID_INDEX ||
                node->parent_index >= tree->count) {
            EcsUiSolverSetErrorKind(
                ctx,
                ECS_UI_FRAME_ERROR_FLOATING_PARENT_NOT_FOUND,
                "native layout solver placement parent not found");
            return false;
        }
    }
    return true;
}

static bool EcsUiSolverValidateSupported(EcsUiSolverContext *ctx)
{
    if (ctx == NULL || ctx->tree == NULL) {
        return false;
    }
    return EcsUiSolverValidateDuplicateElementIds(ctx) &&
        EcsUiSolverValidatePlacementParents(ctx);
}

typedef enum EcsUiSolverSizingKind {
    ECS_UI_SOLVER_SIZE_FIXED = 0,
    ECS_UI_SOLVER_SIZE_FIT = 1,
    ECS_UI_SOLVER_SIZE_GROW = 2,
} EcsUiSolverSizingKind;

typedef struct EcsUiSolverSizing {
    EcsUiSolverSizingKind kind;
    float fixed;
} EcsUiSolverSizing;

typedef struct EcsUiSolverMetrics {
    EcsUiSolverSizing width_sizing;
    EcsUiSolverSizing height_sizing;
    float width;
    float height;
    float min_width;
    float min_height;
    float flow_width;
    float flow_height;
    float flow_min_width;
    float flow_min_height;
    uint32_t virtual_first;
    uint32_t virtual_count;
    bool visible;
    bool offset_wrapper;
    bool text_field_value;
} EcsUiSolverMetrics;

typedef struct EcsUiSolverVirtualFlow {
    EcsUiSolverMetrics metrics;
} EcsUiSolverVirtualFlow;

typedef struct EcsUiSolverFlowItem {
    bool virtual_item;
    uint32_t index;
} EcsUiSolverFlowItem;

static bool EcsUiSolverLayoutHorizontal(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (node->kind == ECS_UI_NODE_ZSTACK) {
        return true;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->stack.axis == ECS_UI_AXIS_HORIZONTAL;
    }
    return node->kind == ECS_UI_NODE_BUTTON ||
        node->kind == ECS_UI_NODE_PRESSABLE;
}

static bool EcsUiSolverZStackChildInFlow(
    const EcsUiTreeSnapshot *tree,
    uint32_t parent,
    uint32_t child)
{
    if (tree == NULL || parent >= tree->count || child >= tree->count ||
            tree->nodes[parent].kind != ECS_UI_NODE_ZSTACK) {
        return true;
    }
    const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
    return tree->nodes[parent].first_child == child && !child_node->has_placement;
}

static bool EcsUiSolverChildInParentFlow(
    const EcsUiTreeSnapshot *tree,
    uint32_t parent,
    uint32_t child)
{
    return EcsUiSolverZStackChildInFlow(tree, parent, child);
}

static bool EcsUiSolverIsTextFieldValue(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    if (tree == NULL || index >= tree->count ||
            tree->nodes[index].kind != ECS_UI_NODE_TEXT) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (node->parent_index == ECS_UI_TREE_INVALID_INDEX ||
            node->parent_index >= tree->count) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *parent = &tree->nodes[node->parent_index];
    return parent->kind == ECS_UI_NODE_PRESSABLE &&
        parent->has_text_field_view &&
        parent->text_field_view.value_node == node->entity;
}

static bool EcsUiSolverScrollClipsAxis(
    const EcsUiTreeNodeSnapshot *node,
    bool x_axis)
{
    if (node == NULL || !node->has_scroll_view) {
        return false;
    }
    const uint32_t axis = x_axis ? ECS_UI_SCROLL_AXIS_X : ECS_UI_SCROLL_AXIS_Y;
    return (node->scroll_view.axes & axis) != 0u;
}

static EcsUiSolverScrollOffset EcsUiSolverScrollOffsetForNode(
    const EcsUiSolverContext *ctx,
    uint32_t index)
{
    if (ctx == NULL) {
        return (EcsUiSolverScrollOffset){0};
    }
    if (ctx->scroll_offsets != NULL) {
        for (uint32_t i = 0u; i < ctx->scroll_offset_count; i += 1u) {
            const EcsUiSolverScrollOffset *offset = &ctx->scroll_offsets[i];
            if (offset->node_index == index) {
                return *offset;
            }
        }
    }
    if (ctx->tree != NULL && index < ctx->tree->count &&
            ctx->tree->nodes[index].has_scroll_state) {
        const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
        return (EcsUiSolverScrollOffset){
            .node_index = index,
            .offset_x = node->scroll_state.offset_x,
            .offset_y = node->scroll_state.offset_y,
        };
    }
    return (EcsUiSolverScrollOffset){0};
}

static void EcsUiSolverReportScrollContent(
    EcsUiSolverContext *ctx,
    uint32_t index,
    float width,
    float height)
{
    if (ctx == NULL || ctx->scroll_contents == NULL) {
        return;
    }
    for (uint32_t i = 0u; i < ctx->scroll_content_count; i += 1u) {
        EcsUiSolverScrollContent *content = &ctx->scroll_contents[i];
        if (content->node_index == index) {
            content->width = width;
            content->height = height;
            content->valid = true;
            return;
        }
    }
}

static bool EcsUiSolverHasVisualOffset(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL &&
        (node->visual.offset_x > 0.01f || node->visual.offset_x < -0.01f ||
            node->visual.offset_y > 0.01f || node->visual.offset_y < -0.01f);
}

static float EcsUiSolverRawPaddingTop(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiSolverStackPaddingSide(node->stack.padding_top, node->stack.padding);
}

static float EcsUiSolverRawPaddingBottom(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiSolverStackPaddingSide(node->stack.padding_bottom, node->stack.padding);
}

static float EcsUiSolverRawGap(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiSolverClampPositive(node->stack.gap);
}

static float EcsUiSolverPreferredWalkHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index);

static float EcsUiSolverPreferredWalkHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node) &&
            node->stack.preferred_height > 0.0f &&
            node->stack.height_sizing == ECS_UI_SIZE_AUTO) {
        return node->stack.preferred_height;
    }

    switch (node->kind) {
    case ECS_UI_NODE_TEXT:
        return EcsUiSolverLocalTextSize(node) + 8.0f;
    case ECS_UI_NODE_ICON:
        return ECS_UI_SOLVER_ICON_SIZE;
    case ECS_UI_NODE_BUTTON:
        return EcsUiSolverButtonHeight(node);
    case ECS_UI_NODE_PRESSABLE:
        return EcsUiSolverPressableHeight(node);
    case ECS_UI_NODE_CUSTOM:
        if (node->custom.height_sizing == ECS_UI_SIZE_GROW) {
            return 0.0f;
        }
        return EcsUiSolverCustomHeight(node);
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        break;
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }

    if (node->kind == ECS_UI_NODE_ZSTACK) {
        float height =
            EcsUiSolverRawPaddingTop(node) +
            EcsUiSolverRawPaddingBottom(node);
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            height += EcsUiSolverPreferredWalkHeight(tree, child);
        }
        return height;
    }

    if (EcsUiSolverLayoutHorizontal(node)) {
        float height = 0.0f;
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            height = EcsUiSolverMaxFloat(
                height,
                EcsUiSolverPreferredWalkHeight(tree, child));
        }
        return EcsUiSolverRawPaddingTop(node) +
            EcsUiSolverRawPaddingBottom(node) +
            height;
    } else {
        float height =
            EcsUiSolverRawPaddingTop(node) +
            EcsUiSolverRawPaddingBottom(node);
        uint32_t child_count = 0u;
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            if (child_count > 0u && node->kind != ECS_UI_NODE_ZSTACK) {
                height += EcsUiSolverRawGap(node);
            }
            height += EcsUiSolverPreferredWalkHeight(tree, child);
            child_count += 1u;
        }
        return height;
    }
}

static EcsUiSolverSizing EcsUiSolverFixedSizing(float value)
{
    return (EcsUiSolverSizing){
        .kind = ECS_UI_SOLVER_SIZE_FIXED,
        .fixed = EcsUiSolverClampPositive(value),
    };
}

static EcsUiSolverSizing EcsUiSolverGrowSizing(void)
{
    return (EcsUiSolverSizing){.kind = ECS_UI_SOLVER_SIZE_GROW};
}

static EcsUiSolverSizing EcsUiSolverFitSizing(void)
{
    return (EcsUiSolverSizing){.kind = ECS_UI_SOLVER_SIZE_FIT};
}

static EcsUiSolverSizing EcsUiSolverWidthSizing(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node)) {
        EcsUiSolverSizing sizing = EcsUiSolverGrowSizing();
        if (node->stack.preferred_width > 0.0f) {
            sizing = EcsUiSolverFixedSizing(node->stack.preferred_width);
        }
        if (node->stack.width_sizing == ECS_UI_SIZE_GROW) {
            sizing = EcsUiSolverGrowSizing();
        } else if (node->stack.width_sizing == ECS_UI_SIZE_FIT) {
            sizing = EcsUiSolverFitSizing();
        }
        return sizing;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        EcsUiSolverSizing sizing = node->custom.preferred_width > 0.0f ?
            EcsUiSolverFixedSizing(node->custom.preferred_width) :
            EcsUiSolverGrowSizing();
        if (node->custom.width_sizing == ECS_UI_SIZE_GROW) {
            sizing = EcsUiSolverGrowSizing();
        }
        return sizing;
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return node->button.preferred_width > 0.0f ?
            EcsUiSolverFixedSizing(node->button.preferred_width) :
            EcsUiSolverGrowSizing();
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return EcsUiSolverGrowSizing();
    }
    if (node->kind == ECS_UI_NODE_TEXT) {
        return EcsUiSolverGrowSizing();
    }
    if (node->kind == ECS_UI_NODE_ICON) {
        return EcsUiSolverFixedSizing(ECS_UI_SOLVER_ICON_SIZE);
    }
    return EcsUiSolverFixedSizing(0.0f);
}

static EcsUiSolverSizing EcsUiSolverHeightSizing(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node)) {
        EcsUiSolverSizing sizing = EcsUiSolverGrowSizing();
        if (node->kind != ECS_UI_NODE_ROOT &&
                node->parent_index != ECS_UI_TREE_INVALID_INDEX &&
                node->parent_index < tree->count &&
                tree->nodes[node->parent_index].kind != ECS_UI_NODE_ZSTACK) {
            sizing = EcsUiSolverFixedSizing(
                EcsUiSolverPreferredWalkHeight(tree, index));
        }
        if (node->stack.preferred_height > 0.0f) {
            sizing = EcsUiSolverFixedSizing(node->stack.preferred_height);
        }
        if (node->stack.height_sizing == ECS_UI_SIZE_GROW) {
            sizing = EcsUiSolverGrowSizing();
        } else if (node->stack.height_sizing == ECS_UI_SIZE_FIT) {
            sizing = EcsUiSolverFitSizing();
        }
        return sizing;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        EcsUiSolverSizing sizing =
            EcsUiSolverFixedSizing(EcsUiSolverCustomHeight(node));
        if (node->custom.height_sizing == ECS_UI_SIZE_GROW) {
            sizing = EcsUiSolverGrowSizing();
        }
        return sizing;
    }
    if (node->kind == ECS_UI_NODE_ICON) {
        return EcsUiSolverFixedSizing(ECS_UI_SOLVER_ICON_SIZE);
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return EcsUiSolverFixedSizing(EcsUiSolverButtonHeight(node));
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return EcsUiSolverFixedSizing(EcsUiSolverPressableHeight(node));
    }
    if (node->kind == ECS_UI_NODE_TEXT) {
        return EcsUiSolverFixedSizing(
            EcsUiSolverInheritedTextSize(tree, index) + 8.0f);
    }
    return EcsUiSolverFixedSizing(0.0f);
}

static void EcsUiSolverClampToSizing(
    float *size,
    float *min_size,
    EcsUiSolverSizing sizing)
{
    if (sizing.kind != ECS_UI_SOLVER_SIZE_FIXED) {
        return;
    }
    *size = sizing.fixed;
    *min_size = sizing.fixed;
}

static float EcsUiSolverOffsetSlotSize(EcsUiSolverSizing sizing)
{
    return sizing.kind == ECS_UI_SOLVER_SIZE_FIXED ? sizing.fixed : 0.0f;
}

static float *EcsUiSolverAxisSize(EcsUiSolverMetrics *metrics, bool x_axis)
{
    return x_axis ? &metrics->width : &metrics->height;
}

static float *EcsUiSolverAxisMin(EcsUiSolverMetrics *metrics, bool x_axis)
{
    return x_axis ? &metrics->min_width : &metrics->min_height;
}

static float *EcsUiSolverAxisFlowSize(
    EcsUiSolverMetrics *metrics,
    bool x_axis)
{
    return x_axis ? &metrics->flow_width : &metrics->flow_height;
}

static float *EcsUiSolverAxisFlowMin(
    EcsUiSolverMetrics *metrics,
    bool x_axis)
{
    return x_axis ? &metrics->flow_min_width : &metrics->flow_min_height;
}

static void EcsUiSolverSetAxisFlowSize(
    EcsUiSolverMetrics *metrics,
    bool x_axis,
    float value)
{
    *EcsUiSolverAxisFlowSize(metrics, x_axis) = value;
    if (!metrics->offset_wrapper) {
        *EcsUiSolverAxisSize(metrics, x_axis) = value;
    }
}

static EcsUiSolverSizing EcsUiSolverAxisSizing(
    const EcsUiSolverMetrics *metrics,
    bool x_axis)
{
    return x_axis ? metrics->width_sizing : metrics->height_sizing;
}

static bool EcsUiSolverAxisResizable(
    const EcsUiSolverMetrics *metrics,
    bool x_axis)
{
    if (metrics != NULL && metrics->text_field_value) {
        return false;
    }
    return EcsUiSolverAxisSizing(metrics, x_axis).kind !=
        ECS_UI_SOLVER_SIZE_FIXED;
}

static EcsUiSolverMetrics *EcsUiSolverFlowMetrics(
    EcsUiSolverContext *ctx,
    EcsUiSolverFlowItem item)
{
    return item.virtual_item ?
        &ctx->virtual_entries[item.index].metrics :
        &ctx->metrics[item.index];
}

static EcsUiSolverFlowItem EcsUiSolverRealFlowItem(uint32_t index)
{
    return (EcsUiSolverFlowItem){.virtual_item = false, .index = index};
}

static EcsUiSolverFlowItem EcsUiSolverVirtualFlowItem(uint32_t index)
{
    return (EcsUiSolverFlowItem){.virtual_item = true, .index = index};
}

static bool EcsUiSolverAppendScratchFlowItem(
    EcsUiSolverContext *ctx,
    EcsUiSolverFlowItem item,
    uint32_t *count)
{
    if (ctx == NULL || count == NULL || ctx->scratch_items == NULL ||
            *count >= ctx->scratch_item_capacity) {
        EcsUiSolverSetError(ctx, "native layout solver flow scratch overflow");
        return false;
    }
    ctx->scratch_items[*count] = item;
    *count += 1u;
    return true;
}

static bool EcsUiSolverAppendChildFlowItems(
    EcsUiSolverContext *ctx,
    uint32_t parent,
    uint32_t child,
    uint32_t *count)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
    if (!child_metrics->visible ||
            !EcsUiSolverChildInParentFlow(tree, parent, child)) {
        return true;
    }
    if (child_metrics->text_field_value) {
        const uint32_t first = child_metrics->virtual_first;
        const uint32_t end = first + child_metrics->virtual_count;
        for (uint32_t i = first; i < end; i += 1u) {
            if (!EcsUiSolverAppendScratchFlowItem(
                    ctx,
                    EcsUiSolverVirtualFlowItem(i),
                    count)) {
                return false;
            }
        }
        return true;
    }
    return EcsUiSolverAppendScratchFlowItem(
        ctx,
        EcsUiSolverRealFlowItem(child),
        count);
}

static uint32_t EcsUiSolverCollectParentFlowItems(
    EcsUiSolverContext *ctx,
    uint32_t parent)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    uint32_t count = 0u;
    for (uint32_t child = tree->nodes[parent].first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        if (!EcsUiSolverAppendChildFlowItems(ctx, parent, child, &count)) {
            return count;
        }
    }
    return count;
}

typedef struct EcsUiSolverTextMeasure {
    float width;
    float height;
    float min_width;
} EcsUiSolverTextMeasure;

static uint32_t EcsUiSolverClampTextIndex(uint32_t index, size_t length)
{
    return index <= length ? index : (uint32_t)length;
}

static EcsUiSolverTextMeasure EcsUiSolverMeasureTextRange(
    EcsUiSolverContext *ctx,
    const char *text_or_null,
    uint32_t start,
    uint32_t end,
    uint16_t font_size)
{
    EcsUiSolverTextMeasure measured = {0};
    if (ctx->measure_text == NULL) {
        EcsUiSolverSetErrorKind(
            ctx,
            ECS_UI_FRAME_ERROR_MEASURE_TEXT_MISSING,
            "native layout solver text measure callback missing");
        return measured;
    }

    const EcsUiStyleTextRangeMeasure shared =
        EcsUiStyleMeasureTextRangeWithCapacity(
            ctx->measure_text,
            ctx->measure_user_data,
            text_or_null,
            start,
            end,
            font_size,
            EcsUiSolverScale(ctx->tree),
            ctx->text_line_capacity);
    if (shared.truncated) {
        EcsUiSolverSetErrorKind(
            ctx,
            ECS_UI_FRAME_ERROR_TEXT_MEASURE_CAPACITY,
            "native layout solver text measure line capacity exceeded");
        return measured;
    }
    measured.width = shared.width;
    measured.height = shared.height;
    measured.min_width = shared.min_width;
    return measured;
}

static EcsUiSolverTextMeasure EcsUiSolverMeasureText(
    EcsUiSolverContext *ctx,
    uint32_t index)
{
    const EcsUiTreeSnapshot *tree = ctx->tree;
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float scale = EcsUiSolverScale(tree);
    const uint16_t font_size =
        EcsUiSolverU16(EcsUiSolverInheritedTextSize(tree, index) * scale);
    return EcsUiSolverMeasureTextRange(
        ctx,
        node->text.text,
        0u,
        (uint32_t)strlen(node->text.text),
        font_size);
}

static EcsUiSolverMetrics *EcsUiSolverAppendTextFieldVirtualEntry(
    EcsUiSolverContext *ctx,
    EcsUiSolverMetrics *value_metrics,
    EcsUiSolverSizing width_sizing,
    EcsUiSolverSizing height_sizing,
    float width,
    float height,
    float min_width,
    float min_height)
{
    if (ctx == NULL || value_metrics == NULL ||
            ctx->virtual_count >= ctx->virtual_capacity) {
        EcsUiSolverSetError(ctx, "native layout solver virtual flow overflow");
        return NULL;
    }

    EcsUiSolverVirtualFlow *entry = &ctx->virtual_entries[ctx->virtual_count];
    ctx->virtual_count += 1u;
    EcsUiSolverMetrics *metrics = &entry->metrics;
    metrics->width_sizing = width_sizing;
    metrics->height_sizing = height_sizing;
    metrics->width = width;
    metrics->height = height;
    metrics->min_width = min_width;
    metrics->min_height = min_height;
    metrics->flow_width = width;
    metrics->flow_height = height;
    metrics->flow_min_width = min_width;
    metrics->flow_min_height = min_height;
    metrics->visible = true;

    value_metrics->width += width;
    value_metrics->height = EcsUiSolverMaxFloat(value_metrics->height, height);
    value_metrics->min_width += min_width;
    value_metrics->min_height =
        EcsUiSolverMaxFloat(value_metrics->min_height, min_height);
    value_metrics->flow_width = value_metrics->width;
    value_metrics->flow_height = value_metrics->height;
    value_metrics->flow_min_width = value_metrics->min_width;
    value_metrics->flow_min_height = value_metrics->min_height;
    return metrics;
}

static void EcsUiSolverAddTextFieldRange(
    EcsUiSolverContext *ctx,
    EcsUiSolverMetrics *value_metrics,
    const EcsUiTreeNodeSnapshot *value_node,
    uint16_t font_size,
    uint32_t start,
    uint32_t end,
    bool selection_wrapper)
{
    if (value_node == NULL || start == end) {
        return;
    }
    const EcsUiSolverTextMeasure text = EcsUiSolverMeasureTextRange(
        ctx,
        value_node->text.text,
        start,
        end,
        font_size);
    const EcsUiSolverSizing width_sizing = selection_wrapper ?
        EcsUiSolverFitSizing() :
        EcsUiSolverFixedSizing(text.width);
    const EcsUiSolverSizing height_sizing = selection_wrapper ?
        EcsUiSolverFitSizing() :
        EcsUiSolverFixedSizing(text.height);
    (void)EcsUiSolverAppendTextFieldVirtualEntry(
        ctx,
        value_metrics,
        width_sizing,
        height_sizing,
        text.width,
        text.height,
        text.min_width,
        text.height);
}

static void EcsUiSolverAddTextFieldCaret(
    const EcsUiTreeSnapshot *tree,
    EcsUiSolverContext *ctx,
    EcsUiSolverMetrics *value_metrics,
    const EcsUiTreeNodeSnapshot *field_node,
    float resolved_text_size)
{
    (void)tree;
    const float caret_width =
        field_node != NULL && field_node->text_field_view.caret_width > 0.0f ?
            field_node->text_field_view.caret_width :
            2.0f;
    const float caret_height = resolved_text_size + 8.0f;
    (void)EcsUiSolverAppendTextFieldVirtualEntry(
        ctx,
        value_metrics,
        EcsUiSolverFixedSizing(caret_width),
        EcsUiSolverFixedSizing(caret_height),
        caret_width,
        caret_height,
        caret_width,
        caret_height);
}

static void EcsUiSolverBuildTextFieldVirtualFlow(
    EcsUiSolverContext *ctx,
    uint32_t value_index)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    const EcsUiTreeNodeSnapshot *value_node = &tree->nodes[value_index];
    const EcsUiTreeNodeSnapshot *field_node =
        &tree->nodes[value_node->parent_index];
    EcsUiSolverMetrics *value_metrics = &ctx->metrics[value_index];
    const EcsUiTextFieldView *view = &field_node->text_field_view;
    const char *text = value_node->text.text;
    const size_t length = strlen(text);
    const uint32_t text_end = (uint32_t)length;
    const float scale = EcsUiSolverScale(tree);
    const float resolved_size =
        EcsUiSolverInheritedTextSize(tree, value_index);
    const uint16_t font_size = EcsUiSolverU16(resolved_size * scale);
    const uint32_t cursor = EcsUiSolverClampTextIndex(view->cursor, length);
    uint32_t selection_start =
        EcsUiSolverClampTextIndex(view->selection_anchor, length);
    uint32_t selection_end =
        EcsUiSolverClampTextIndex(view->selection_focus, length);
    if (selection_start > selection_end) {
        uint32_t swap = selection_start;
        selection_start = selection_end;
        selection_end = swap;
    }
    const bool has_selection =
        view->focused && selection_start < selection_end;

    value_metrics->virtual_first = ctx->virtual_count;
    value_metrics->virtual_count = 0u;
    value_metrics->width = 0.0f;
    value_metrics->height = 0.0f;
    value_metrics->min_width = 0.0f;
    value_metrics->min_height = 0.0f;
    value_metrics->flow_width = 0.0f;
    value_metrics->flow_height = 0.0f;
    value_metrics->flow_min_width = 0.0f;
    value_metrics->flow_min_height = 0.0f;

    if (!view->focused) {
        EcsUiSolverAddTextFieldRange(
            ctx,
            value_metrics,
            value_node,
            font_size,
            0u,
            text_end,
            false);
        value_metrics->virtual_count =
            ctx->virtual_count - value_metrics->virtual_first;
        return;
    }

    if (!has_selection) {
        EcsUiSolverAddTextFieldRange(
            ctx,
            value_metrics,
            value_node,
            font_size,
            0u,
            cursor,
            false);
        EcsUiSolverAddTextFieldCaret(
            tree,
            ctx,
            value_metrics,
            field_node,
            resolved_size);
        EcsUiSolverAddTextFieldRange(
            ctx,
            value_metrics,
            value_node,
            font_size,
            cursor,
            text_end,
            false);
        value_metrics->virtual_count =
            ctx->virtual_count - value_metrics->virtual_first;
        return;
    }

    EcsUiSolverAddTextFieldRange(
        ctx,
        value_metrics,
        value_node,
        font_size,
        0u,
        selection_start,
        false);
    if (cursor == selection_start) {
        EcsUiSolverAddTextFieldCaret(
            tree,
            ctx,
            value_metrics,
            field_node,
            resolved_size);
    }
    EcsUiSolverAddTextFieldRange(
        ctx,
        value_metrics,
        value_node,
        font_size,
        selection_start,
        selection_end,
        true);
    if (cursor != selection_start) {
        EcsUiSolverAddTextFieldCaret(
            tree,
            ctx,
            value_metrics,
            field_node,
            resolved_size);
    }
    EcsUiSolverAddTextFieldRange(
        ctx,
        value_metrics,
        value_node,
        font_size,
        selection_end,
        text_end,
        false);
    value_metrics->virtual_count =
        ctx->virtual_count - value_metrics->virtual_first;
}

static void EcsUiSolverAccumulateContainerFlow(
    const EcsUiSolverMetrics *child_metrics,
    bool horizontal,
    bool clips_x,
    bool clips_y,
    float gap,
    float *width,
    float *height,
    float *min_width,
    float *min_height,
    uint32_t *visible_child_count)
{
    const float child_width = child_metrics->flow_width;
    const float child_height = child_metrics->flow_height;
    const float child_min_width = child_metrics->flow_min_width;
    const float child_min_height = child_metrics->flow_min_height;
    if (horizontal) {
        if (*visible_child_count > 0u) {
            *width += gap;
            if (!clips_x) {
                *min_width += gap;
            }
        }
        *width += child_width;
        if (!clips_x) {
            *min_width += child_min_width;
        }
        *height = EcsUiSolverMaxFloat(*height, child_height);
        if (!clips_y) {
            *min_height = EcsUiSolverMaxFloat(*min_height, child_min_height);
        }
    } else {
        if (*visible_child_count > 0u) {
            *height += gap;
            if (!clips_y) {
                *min_height += gap;
            }
        }
        *height += child_height;
        if (!clips_y) {
            *min_height += child_min_height;
        }
        *width = EcsUiSolverMaxFloat(*width, child_width);
        if (!clips_x) {
            *min_width = EcsUiSolverMaxFloat(*min_width, child_min_width);
        }
    }
    *visible_child_count += 1u;
}

static void EcsUiSolverComputeBottomUp(
    EcsUiSolverContext *ctx,
    uint32_t index,
    float parent_opacity)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiSolverMetrics *metrics = &ctx->metrics[index];
    metrics->text_field_value = EcsUiSolverIsTextFieldValue(tree, index);
    const float node_opacity = metrics->text_field_value ?
        parent_opacity :
        parent_opacity * EcsUiSolverClamp01(node->visual.opacity);
    if (node_opacity <= 0.01f) {
        return;
    }
    metrics->visible = true;
    metrics->width_sizing = EcsUiSolverWidthSizing(tree, index);
    metrics->height_sizing = EcsUiSolverHeightSizing(tree, index);

    const float padding_left = EcsUiSolverPaddingLeft(tree, node);
    const float padding_top = EcsUiSolverPaddingTop(tree, node);
    const float padding_right = EcsUiSolverPaddingRight(tree, node);
    const float padding_bottom = EcsUiSolverPaddingBottom(tree, node);
    const float gap = EcsUiSolverGap(tree, node);
    const bool container = EcsUiSolverNodeIsStack(node) ||
        node->kind == ECS_UI_NODE_BUTTON ||
        node->kind == ECS_UI_NODE_PRESSABLE;
    const bool horizontal = EcsUiSolverLayoutHorizontal(node);
    const bool clips_x = EcsUiSolverScrollClipsAxis(node, true);
    const bool clips_y = EcsUiSolverScrollClipsAxis(node, false);

    float width = 0.0f;
    float height = 0.0f;
    float min_width = 0.0f;
    float min_height = 0.0f;
    uint32_t visible_child_count = 0u;

    if (metrics->text_field_value) {
        EcsUiSolverBuildTextFieldVirtualFlow(ctx, index);
        if (ctx->failed) {
            return;
        }
        width = metrics->width;
        height = metrics->height;
        min_width = metrics->min_width;
        min_height = metrics->min_height;
        metrics->width_sizing = EcsUiSolverFitSizing();
        metrics->height_sizing = EcsUiSolverFitSizing();
    } else if (node->kind == ECS_UI_NODE_TEXT) {
        const EcsUiSolverTextMeasure text = EcsUiSolverMeasureText(ctx, index);
        width = text.width;
        height = text.height;
        min_width = text.min_width;
        min_height = text.height;
    } else if (container) {
        for (uint32_t child = node->first_child;
             child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            EcsUiSolverComputeBottomUp(ctx, child, node_opacity);
            if (ctx->failed) {
                return;
            }
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            if (!child_metrics->visible ||
                    !EcsUiSolverChildInParentFlow(tree, index, child)) {
                continue;
            }
            if (child_metrics->text_field_value) {
                const uint32_t first = child_metrics->virtual_first;
                const uint32_t end = first + child_metrics->virtual_count;
                for (uint32_t i = first; i < end; i += 1u) {
                    EcsUiSolverAccumulateContainerFlow(
                        &ctx->virtual_entries[i].metrics,
                        horizontal,
                        clips_x,
                        clips_y,
                        gap,
                        &width,
                        &height,
                        &min_width,
                        &min_height,
                        &visible_child_count);
                }
                continue;
            }
            EcsUiSolverAccumulateContainerFlow(
                child_metrics,
                horizontal,
                clips_x,
                clips_y,
                gap,
                &width,
                &height,
                &min_width,
                &min_height,
                &visible_child_count);
        }
        if (horizontal) {
            width += padding_left + padding_right;
            min_width += padding_left + padding_right;
            if (visible_child_count > 0u) {
                height += padding_top + padding_bottom;
                if (!clips_y) {
                    min_height += padding_top + padding_bottom;
                }
            }
        } else {
            height += padding_top + padding_bottom;
            min_height += padding_top + padding_bottom;
            if (visible_child_count > 0u) {
                width += padding_left + padding_right;
                if (!clips_x) {
                    min_width += padding_left + padding_right;
                }
            }
        }
    }

    metrics->width = width;
    metrics->height = height;
    metrics->min_width = min_width;
    metrics->min_height = min_height;
    EcsUiSolverClampToSizing(
        &metrics->width,
        &metrics->min_width,
        metrics->width_sizing);
    EcsUiSolverClampToSizing(
        &metrics->height,
        &metrics->min_height,
        metrics->height_sizing);
    metrics->offset_wrapper =
        !metrics->text_field_value &&
        node->kind != ECS_UI_NODE_ROOT &&
        EcsUiSolverHasVisualOffset(node);
    if (metrics->offset_wrapper) {
        metrics->flow_width = EcsUiSolverOffsetSlotSize(metrics->width_sizing);
        metrics->flow_min_width = metrics->flow_width;
        metrics->flow_height = node->kind == ECS_UI_NODE_TEXT ?
            EcsUiSolverLocalTextSize(node) + 8.0f :
            EcsUiSolverOffsetSlotSize(metrics->height_sizing);
        metrics->flow_min_height = metrics->flow_height;
    } else {
        metrics->flow_width = metrics->width;
        metrics->flow_height = metrics->height;
        metrics->flow_min_width = metrics->min_width;
        metrics->flow_min_height = metrics->min_height;
    }
}

static bool EcsUiSolverFloatEqual(
    const EcsUiTreeSnapshot *tree,
    float a,
    float b)
{
    const float delta = a - b;
    const float epsilon = EcsUiSolverPhysicalEpsilon(tree);
    return delta < epsilon && delta > -epsilon;
}

static uint32_t EcsUiSolverCollectAxisChildren(
    EcsUiSolverContext *ctx,
    uint32_t parent,
    bool x_axis,
    bool grow_only)
{
    uint32_t count = 0u;
    const uint32_t flow_count = EcsUiSolverCollectParentFlowItems(ctx, parent);
    if (ctx->failed) {
        return 0u;
    }
    for (uint32_t i = 0u; i < flow_count; i += 1u) {
        EcsUiSolverFlowItem item = ctx->scratch_items[i];
        EcsUiSolverMetrics *child_metrics = EcsUiSolverFlowMetrics(ctx, item);
        if (!child_metrics->visible ||
            !EcsUiSolverAxisResizable(child_metrics, x_axis)) {
            continue;
        }
        if (grow_only &&
                EcsUiSolverAxisSizing(child_metrics, x_axis).kind !=
                    ECS_UI_SOLVER_SIZE_GROW) {
            continue;
        }
        ctx->scratch_items[count] = item;
        count += 1u;
    }
    return count;
}

static void EcsUiSolverWaterFillGrow(
    EcsUiSolverContext *ctx,
    uint32_t parent,
    bool x_axis,
    float free_space)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    const float epsilon = EcsUiSolverPhysicalEpsilon(tree);
    uint32_t active_count =
        EcsUiSolverCollectAxisChildren(ctx, parent, x_axis, true);
    if (ctx->failed) {
        return;
    }
    while (free_space > epsilon && active_count > 0u) {
        float smallest = 3.4e38f;
        float second_smallest = 3.4e38f;
        float amount = free_space;
        for (uint32_t i = 0u; i < active_count; i += 1u) {
            EcsUiSolverMetrics *child_metrics =
                EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
            const float size = *EcsUiSolverAxisFlowSize(child_metrics, x_axis);
            if (EcsUiSolverFloatEqual(tree, size, smallest)) {
                continue;
            }
            if (size < smallest) {
                second_smallest = smallest;
                smallest = size;
            }
            if (size > smallest) {
                second_smallest = EcsUiSolverMinFloat(second_smallest, size);
                amount = second_smallest - smallest;
            }
        }

        amount = EcsUiSolverMinFloat(amount, free_space / (float)active_count);
        for (uint32_t i = 0u; i < active_count; i += 1u) {
            EcsUiSolverMetrics *child_metrics =
                EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
            float *size = EcsUiSolverAxisFlowSize(child_metrics, x_axis);
            if (EcsUiSolverFloatEqual(tree, *size, smallest)) {
                EcsUiSolverSetAxisFlowSize(child_metrics, x_axis, *size + amount);
                free_space -= amount;
            }
        }
    }
}

static void EcsUiSolverCompressResizable(
    EcsUiSolverContext *ctx,
    uint32_t parent,
    bool x_axis,
    float free_space)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    const float epsilon = EcsUiSolverPhysicalEpsilon(tree);
    uint32_t active_count =
        EcsUiSolverCollectAxisChildren(ctx, parent, x_axis, false);
    if (ctx->failed) {
        return;
    }
    uint32_t iteration_limit =
        active_count > 0u ? (active_count * active_count * 8u) + 8u : 0u;

    /* Keep dropout order stable so text and scroll stages share the same
     * compression behavior. */
    while (free_space < -epsilon && active_count > 0u) {
        if (iteration_limit == 0u) {
            EcsUiSolverSetError(
                ctx,
                "native layout solver compression iteration limit exceeded");
            return;
        }
        iteration_limit -= 1u;
        float largest = 0.0f;
        float second_largest = 0.0f;
        float amount = free_space;
        for (uint32_t i = 0u; i < active_count; i += 1u) {
            EcsUiSolverMetrics *child_metrics =
                EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
            const float size = *EcsUiSolverAxisFlowSize(child_metrics, x_axis);
            if (EcsUiSolverFloatEqual(tree, size, largest)) {
                continue;
            }
            if (size > largest) {
                second_largest = largest;
                largest = size;
            }
            if (size < largest) {
                second_largest = EcsUiSolverMaxFloat(second_largest, size);
                amount = second_largest - largest;
            }
        }

        amount = EcsUiSolverMaxFloat(amount, free_space / (float)active_count);
        for (uint32_t i = 0u; i < active_count; i += 1u) {
            EcsUiSolverMetrics *child_metrics =
                EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
            float *size = EcsUiSolverAxisFlowSize(child_metrics, x_axis);
            const float min_size =
                *EcsUiSolverAxisFlowMin(child_metrics, x_axis);
            if (EcsUiSolverFloatEqual(tree, *size, largest)) {
                const float previous = *size;
                EcsUiSolverSetAxisFlowSize(child_metrics, x_axis, *size + amount);
                if (*size <= min_size) {
                    EcsUiSolverSetAxisFlowSize(child_metrics, x_axis, min_size);
                    active_count -= 1u;
                    ctx->scratch_items[i] = ctx->scratch_items[active_count];
                    i -= 1u;
                }
                free_space -= *size - previous;
            }
        }
    }
}

static void EcsUiSolverSolveAxisTopDown(
    EcsUiSolverContext *ctx,
    uint32_t index,
    bool x_axis)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiSolverMetrics *metrics = &ctx->metrics[index];
    if (!metrics->visible) {
        return;
    }
    const bool container = EcsUiSolverNodeIsStack(node) ||
        node->kind == ECS_UI_NODE_BUTTON ||
        node->kind == ECS_UI_NODE_PRESSABLE;
    if (!container) {
        return;
    }

    const bool along_axis =
        (x_axis && EcsUiSolverLayoutHorizontal(node)) ||
        (!x_axis && !EcsUiSolverLayoutHorizontal(node));
    const float parent_size = *EcsUiSolverAxisSize(metrics, x_axis);
    const float parent_padding = x_axis ?
        EcsUiSolverPaddingLeft(tree, node) + EcsUiSolverPaddingRight(tree, node) :
        EcsUiSolverPaddingTop(tree, node) + EcsUiSolverPaddingBottom(tree, node);
    const float gap = EcsUiSolverGap(tree, node);

    if (along_axis) {
        float inner = 0.0f;
        uint32_t visible_child_count = 0u;
        uint32_t grow_count = 0u;
        const uint32_t flow_count =
            EcsUiSolverCollectParentFlowItems(ctx, index);
        if (ctx->failed) {
            return;
        }
        for (uint32_t i = 0u; i < flow_count; i += 1u) {
            EcsUiSolverMetrics *child_metrics =
                EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
            inner += *EcsUiSolverAxisFlowSize(child_metrics, x_axis);
            if (visible_child_count > 0u) {
                inner += gap;
            }
            if (EcsUiSolverAxisSizing(child_metrics, x_axis).kind ==
                    ECS_UI_SOLVER_SIZE_GROW) {
                grow_count += 1u;
            }
            visible_child_count += 1u;
        }
        const float free_space = parent_size - parent_padding - inner;
        if (free_space > 0.0f && grow_count > 0u) {
            EcsUiSolverWaterFillGrow(ctx, index, x_axis, free_space);
        } else if (free_space < 0.0f &&
                !EcsUiSolverScrollClipsAxis(node, x_axis)) {
            EcsUiSolverCompressResizable(ctx, index, x_axis, free_space);
            if (ctx->failed) {
                return;
            }
        }
    } else {
        float max_size = parent_size - parent_padding;
        if (EcsUiSolverScrollClipsAxis(node, x_axis)) {
            float inner_content_size = 0.0f;
            const uint32_t flow_count =
                EcsUiSolverCollectParentFlowItems(ctx, index);
            if (ctx->failed) {
                return;
            }
            for (uint32_t i = 0u; i < flow_count; i += 1u) {
                EcsUiSolverMetrics *child_metrics =
                    EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
                inner_content_size = EcsUiSolverMaxFloat(
                    inner_content_size,
                    *EcsUiSolverAxisFlowSize(child_metrics, x_axis));
            }
            max_size = EcsUiSolverMaxFloat(max_size, inner_content_size);
        }
        const uint32_t flow_count =
            EcsUiSolverCollectParentFlowItems(ctx, index);
        if (ctx->failed) {
            return;
        }
        for (uint32_t i = 0u; i < flow_count; i += 1u) {
            EcsUiSolverMetrics *child_metrics =
                EcsUiSolverFlowMetrics(ctx, ctx->scratch_items[i]);
            if (!child_metrics->visible ||
                    !EcsUiSolverAxisResizable(child_metrics, x_axis)) {
                continue;
            }
            float *size = EcsUiSolverAxisFlowSize(child_metrics, x_axis);
            const float min_size =
                *EcsUiSolverAxisFlowMin(child_metrics, x_axis);
            if (EcsUiSolverAxisSizing(child_metrics, x_axis).kind ==
                    ECS_UI_SOLVER_SIZE_GROW) {
                EcsUiSolverSetAxisFlowSize(child_metrics, x_axis, max_size);
            }
            if (*size > max_size) {
                EcsUiSolverSetAxisFlowSize(child_metrics, x_axis, max_size);
            }
            if (*size < min_size) {
                EcsUiSolverSetAxisFlowSize(child_metrics, x_axis, min_size);
            }
        }
    }

    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        if (!EcsUiSolverChildInParentFlow(tree, index, child)) {
            continue;
        }
        if (ctx->metrics[child].text_field_value ||
                ctx->metrics[child].offset_wrapper) {
            continue;
        }
        EcsUiSolverSolveAxisTopDown(ctx, child, x_axis);
        if (ctx->failed) {
            return;
        }
    }
}

static void EcsUiSolverApplyRootContainer(
    EcsUiSolverContext *ctx,
    const EcsUiFrameLayoutOptions *layout)
{
    if (ctx == NULL || ctx->tree == NULL || ctx->tree->count == 0u ||
            !ctx->metrics[0].visible) {
        return;
    }

    EcsUiSolverMetrics *root = &ctx->metrics[0];
    const float viewport_width = EcsUiSolverLogicalRootWidth(ctx->tree, layout);
    const float viewport_height = EcsUiSolverLogicalRootHeight(ctx->tree, layout);

    if (root->width_sizing.kind == ECS_UI_SOLVER_SIZE_GROW) {
        root->width = EcsUiSolverMaxFloat(root->min_width, viewport_width);
    } else if (root->width_sizing.kind == ECS_UI_SOLVER_SIZE_FIT &&
            root->width > viewport_width) {
        root->width = EcsUiSolverMaxFloat(root->min_width, viewport_width);
    }

    if (root->height_sizing.kind == ECS_UI_SOLVER_SIZE_GROW) {
        root->height = EcsUiSolverMaxFloat(root->min_height, viewport_height);
    } else if (root->height_sizing.kind == ECS_UI_SOLVER_SIZE_FIT) {
        root->height = EcsUiSolverMaxFloat(
            root->min_height,
            EcsUiSolverMinFloat(root->height, viewport_height));
    }
}

static EcsUiAlign EcsUiSolverAlignX(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return ECS_UI_ALIGN_START;
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return ECS_UI_ALIGN_CENTER;
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return ECS_UI_ALIGN_START;
    }
    if (node->kind == ECS_UI_NODE_ZSTACK) {
        return ECS_UI_ALIGN_START;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->stack.align_x;
    }
    return ECS_UI_ALIGN_START;
}

static EcsUiAlign EcsUiSolverAlignY(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return ECS_UI_ALIGN_START;
    }
    if (node->kind == ECS_UI_NODE_BUTTON ||
            node->kind == ECS_UI_NODE_PRESSABLE) {
        return ECS_UI_ALIGN_CENTER;
    }
    if (node->kind == ECS_UI_NODE_ZSTACK) {
        return ECS_UI_ALIGN_START;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->stack.align_y;
    }
    return ECS_UI_ALIGN_START;
}

static float EcsUiSolverAlignmentOffset(EcsUiAlign align, float extra)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return extra * 0.5f;
    case ECS_UI_ALIGN_END:
        return extra;
    case ECS_UI_ALIGN_START:
    default:
        return 0.0f;
    }
}

static float EcsUiSolverMainAxisOffset(
    const EcsUiTreeNodeSnapshot *node,
    bool horizontal,
    float extra)
{
    const EcsUiAlign align = horizontal ?
        EcsUiSolverAlignX(node) :
        EcsUiSolverAlignY(node);
    float offset = EcsUiSolverAlignmentOffset(align, extra);
    if (!horizontal && offset < 0.0f) {
        offset = 0.0f;
    }
    return offset;
}

static float EcsUiSolverChildrenMainSize(
    EcsUiSolverContext *ctx,
    uint32_t index,
    bool horizontal)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float gap = EcsUiSolverGap(tree, node);
    float size = 0.0f;
    uint32_t visible_child_count = 0u;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
        if (!child_metrics->visible ||
                !EcsUiSolverChildInParentFlow(tree, index, child)) {
            continue;
        }
        if (child_metrics->text_field_value) {
            const uint32_t first = child_metrics->virtual_first;
            const uint32_t end = first + child_metrics->virtual_count;
            for (uint32_t i = first; i < end; i += 1u) {
                if (visible_child_count > 0u) {
                    size += gap;
                }
                EcsUiSolverMetrics *virtual_metrics =
                    &ctx->virtual_entries[i].metrics;
                size += horizontal ?
                    virtual_metrics->flow_width :
                    virtual_metrics->flow_height;
                visible_child_count += 1u;
            }
            continue;
        }
        if (visible_child_count > 0u) {
            size += gap;
        }
        size += horizontal ? child_metrics->flow_width : child_metrics->flow_height;
        visible_child_count += 1u;
    }
    return size;
}

static float EcsUiSolverChildrenOffAxisSize(
    EcsUiSolverContext *ctx,
    uint32_t index,
    bool horizontal)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    float size = 0.0f;
    for (uint32_t child = tree->nodes[index].first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
        if (!child_metrics->visible ||
                !EcsUiSolverChildInParentFlow(tree, index, child)) {
            continue;
        }
        if (child_metrics->text_field_value) {
            const uint32_t first = child_metrics->virtual_first;
            const uint32_t end = first + child_metrics->virtual_count;
            for (uint32_t i = first; i < end; i += 1u) {
                EcsUiSolverMetrics *virtual_metrics =
                    &ctx->virtual_entries[i].metrics;
                size = EcsUiSolverMaxFloat(
                    size,
                    horizontal ?
                        virtual_metrics->flow_height :
                        virtual_metrics->flow_width);
            }
            continue;
        }
        size = EcsUiSolverMaxFloat(
            size,
            horizontal ? child_metrics->flow_height : child_metrics->flow_width);
    }
    return size;
}

static float EcsUiSolverAttachAxis(EcsUiAlign align, float origin, float size)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return origin + size * 0.5f;
    case ECS_UI_ALIGN_END:
        return origin + size;
    case ECS_UI_ALIGN_START:
    default:
        return origin;
    }
}

static float EcsUiSolverElementAttachOffset(EcsUiAlign align, float size)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return size * 0.5f;
    case ECS_UI_ALIGN_END:
        return size;
    case ECS_UI_ALIGN_START:
    default:
        return 0.0f;
    }
}

static float EcsUiSolverResolvePointAnchorAxis(
    float point,
    float size,
    float root_size)
{
    float resolved = point;
    if (size > 0.0f && root_size > 0.0f && resolved + size > root_size) {
        resolved = point - size;
    }
    if (resolved < 0.0f) {
        resolved = 0.0f;
    }
    if (root_size > 0.0f && size > 0.0f && size <= root_size &&
            resolved + size > root_size) {
        resolved = root_size - size;
    }
    return resolved;
}

static float EcsUiSolverFloatingWrapperSize(
    EcsUiSolverContext *ctx,
    const EcsUiTreeNodeSnapshot *child_node,
    const EcsUiSolverMetrics *child_metrics,
    bool x_axis,
    float attach_size)
{
    (void)ctx;
    (void)child_metrics;
    const float authored = x_axis ?
        child_node->placement.width :
        child_node->placement.height;
    return child_node->has_placement && authored > 0.0f ?
        authored :
        attach_size;
}

static void EcsUiSolverSizeFloatingChildAxis(
    EcsUiSolverContext *ctx,
    uint32_t child,
    bool x_axis,
    float wrapper_size)
{
    EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
    if (!child_metrics->visible ||
            !EcsUiSolverAxisResizable(child_metrics, x_axis)) {
        return;
    }

    EcsUiSolverSizing sizing = EcsUiSolverAxisSizing(child_metrics, x_axis);
    float *size = EcsUiSolverAxisSize(child_metrics, x_axis);
    const float min_size = *EcsUiSolverAxisMin(child_metrics, x_axis);
    if (sizing.kind == ECS_UI_SOLVER_SIZE_GROW) {
        *size = wrapper_size;
    }
    if (*size > wrapper_size) {
        *size = wrapper_size;
    }
    if (*size < min_size) {
        *size = min_size;
    }
}

static void EcsUiSolverPlaceNode(
    EcsUiSolverContext *ctx,
    uint32_t index,
    EcsUiSolverRect rect);

static void EcsUiSolverPlaceZStackFloatingChildren(
    EcsUiSolverContext *ctx,
    uint32_t index,
    EcsUiSolverRect rect)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    for (uint32_t child = tree->nodes[index].first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        if (EcsUiSolverChildInParentFlow(tree, index, child)) {
            continue;
        }
        EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
        if (!child_metrics->visible) {
            continue;
        }

        const bool placed = child_node->has_placement;
        const bool point_anchored = placed &&
            child_node->placement.mode == ECS_UI_PLACEMENT_POINT;
        const bool placed_text = placed &&
            child_node->kind == ECS_UI_NODE_TEXT;
        const float point_root_width =
            EcsUiSolverLogicalRootWidth(tree, ctx->layout_options);
        const float point_root_height =
            EcsUiSolverLogicalRootHeight(tree, ctx->layout_options);
        const float attach_width = point_anchored ?
            EcsUiSolverLogicalSurfaceWidth(ctx) :
            rect.width;
        const float attach_height = point_anchored ?
            EcsUiSolverLogicalSurfaceHeight(ctx) :
            rect.height;
        const float wrapper_width = EcsUiSolverFloatingWrapperSize(
            ctx,
            child_node,
            child_metrics,
            true,
            attach_width);
        const float wrapper_height = EcsUiSolverFloatingWrapperSize(
            ctx,
            child_node,
            child_metrics,
            false,
            attach_height);

        EcsUiSolverRect wrapper_rect = {
            .width = wrapper_width,
            .height = wrapper_height,
        };
        if (point_anchored) {
            const float point_x =
                child_node->placement.point_x + child_node->visual.offset_x;
            const float point_y =
                child_node->placement.point_y + child_node->visual.offset_y;
            wrapper_rect.x = EcsUiSolverResolvePointAnchorAxis(
                point_x,
                child_node->placement.width,
                point_root_width);
            wrapper_rect.y = EcsUiSolverResolvePointAnchorAxis(
                point_y,
                child_node->placement.height,
                point_root_height);
        } else {
            const EcsUiAlign parent_x = placed ?
                child_node->placement.parent_x :
                ECS_UI_ALIGN_START;
            const EcsUiAlign parent_y = placed ?
                child_node->placement.parent_y :
                ECS_UI_ALIGN_START;
            const EcsUiAlign child_x = placed ?
                child_node->placement.child_x :
                ECS_UI_ALIGN_START;
            const EcsUiAlign child_y = placed ?
                child_node->placement.child_y :
                ECS_UI_ALIGN_START;
            const float offset_x =
                child_node->visual.offset_x +
                (placed ? child_node->placement.offset_x : 0.0f);
            const float offset_y =
                child_node->visual.offset_y +
                (placed ? child_node->placement.offset_y : 0.0f);
            wrapper_rect.x =
                EcsUiSolverAttachAxis(parent_x, rect.x, rect.width) -
                EcsUiSolverElementAttachOffset(child_x, wrapper_width) +
                offset_x;
            wrapper_rect.y =
                EcsUiSolverAttachAxis(parent_y, rect.y, rect.height) -
                EcsUiSolverElementAttachOffset(child_y, wrapper_height) +
                offset_y;
        }

        if (placed_text) {
            EcsUiSolverPlaceNode(ctx, child, wrapper_rect);
            continue;
        }

        EcsUiSolverSizeFloatingChildAxis(ctx, child, true, wrapper_width);
        EcsUiSolverSizeFloatingChildAxis(ctx, child, false, wrapper_height);
        EcsUiSolverSolveAxisTopDown(ctx, child, true);
        if (ctx->failed) {
            return;
        }
        EcsUiSolverSolveAxisTopDown(ctx, child, false);
        if (ctx->failed) {
            return;
        }
        EcsUiSolverPlaceNode(
            ctx,
            child,
            (EcsUiSolverRect){
                .x = wrapper_rect.x,
                .y = wrapper_rect.y,
                .width = child_metrics->width,
                .height = child_metrics->height,
            });
    }
}

static void EcsUiSolverPlaceNode(
    EcsUiSolverContext *ctx,
    uint32_t index,
    EcsUiSolverRect rect)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiSolverMetrics *metrics = &ctx->metrics[index];
    if (!metrics->visible) {
        return;
    }
    if (metrics->text_field_value) {
        return;
    }
    ctx->layouts[index] = (EcsUiSolverLayout){
        .rect = rect,
        .emitted = true,
    };
    const bool container = EcsUiSolverNodeIsStack(node) ||
        node->kind == ECS_UI_NODE_BUTTON ||
        node->kind == ECS_UI_NODE_PRESSABLE;
    if (!container) {
        return;
    }

    const float padding_left = EcsUiSolverPaddingLeft(tree, node);
    const float padding_top = EcsUiSolverPaddingTop(tree, node);
    const float padding_right = EcsUiSolverPaddingRight(tree, node);
    const float padding_bottom = EcsUiSolverPaddingBottom(tree, node);
    const float content_x = rect.x + padding_left;
    const float content_y = rect.y + padding_top;
    const bool horizontal = EcsUiSolverLayoutHorizontal(node);
    const float gap = EcsUiSolverGap(tree, node);
    const float main_children_size =
        EcsUiSolverChildrenMainSize(ctx, index, horizontal);
    if (node->has_scroll_view) {
        const float off_axis_size =
            EcsUiSolverChildrenOffAxisSize(ctx, index, horizontal);
        EcsUiSolverReportScrollContent(
            ctx,
            index,
            (horizontal ? main_children_size : off_axis_size) +
                padding_left + padding_right,
            (horizontal ? off_axis_size : main_children_size) +
                padding_top + padding_bottom);
    }
    const float main_inner_size = horizontal ?
        rect.width - padding_left - padding_right :
        rect.height - padding_top - padding_bottom;
    const float main_offset =
        EcsUiSolverMainAxisOffset(node, horizontal, main_inner_size - main_children_size);
    float cursor_x = content_x + (horizontal ? main_offset : 0.0f);
    float cursor_y = content_y + (!horizontal ? main_offset : 0.0f);
    EcsUiSolverScrollOffset scroll_offset =
        EcsUiSolverScrollOffsetForNode(ctx, index);
    if (!EcsUiSolverScrollClipsAxis(node, true)) {
        scroll_offset.offset_x = 0.0f;
    }
    if (!EcsUiSolverScrollClipsAxis(node, false)) {
        scroll_offset.offset_y = 0.0f;
    }
    uint32_t child_count = 0u;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
        if (!child_metrics->visible ||
                !EcsUiSolverChildInParentFlow(tree, index, child)) {
            continue;
        }
        if (child_metrics->text_field_value) {
            const uint32_t first = child_metrics->virtual_first;
            const uint32_t end = first + child_metrics->virtual_count;
            for (uint32_t i = first; i < end; i += 1u) {
                EcsUiSolverMetrics *virtual_metrics =
                    &ctx->virtual_entries[i].metrics;
                if (child_count > 0u) {
                    if (horizontal) {
                        cursor_x += gap;
                    } else {
                        cursor_y += gap;
                    }
                }
                if (horizontal) {
                    cursor_x += virtual_metrics->flow_width;
                } else {
                    cursor_y += virtual_metrics->flow_height;
                }
                child_count += 1u;
            }
            continue;
        }
        if (child_count > 0u) {
            if (horizontal) {
                cursor_x += gap;
            } else {
                cursor_y += gap;
            }
        }
        const float child_offset_x = EcsUiSolverAlignmentOffset(
            EcsUiSolverAlignX(node),
            rect.width - padding_left - padding_right - child_metrics->flow_width);
        const float child_offset_y = EcsUiSolverAlignmentOffset(
            EcsUiSolverAlignY(node),
            rect.height - padding_top - padding_bottom - child_metrics->flow_height);
        EcsUiSolverRect child_rect = {
            .x = horizontal ? cursor_x : content_x + child_offset_x,
            .y = horizontal ? content_y + child_offset_y : cursor_y,
            .width = child_metrics->flow_width,
            .height = child_metrics->flow_height,
        };
        child_rect.x += scroll_offset.offset_x;
        child_rect.y += scroll_offset.offset_y;
        if (child_metrics->offset_wrapper) {
            EcsUiSolverSizeFloatingChildAxis(
                ctx,
                child,
                true,
                child_metrics->flow_width);
            EcsUiSolverSizeFloatingChildAxis(
                ctx,
                child,
                false,
                child_metrics->flow_height);
            EcsUiSolverSolveAxisTopDown(ctx, child, true);
            if (ctx->failed) {
                return;
            }
            EcsUiSolverSolveAxisTopDown(ctx, child, false);
            if (ctx->failed) {
                return;
            }
            child_rect.x += tree->nodes[child].visual.offset_x;
            child_rect.y += tree->nodes[child].visual.offset_y;
            child_rect.width = child_metrics->width;
            child_rect.height = child_metrics->height;
        }
        EcsUiSolverPlaceNode(ctx, child, child_rect);
        if (ctx->failed) {
            return;
        }
        if (horizontal) {
            cursor_x += child_metrics->flow_width;
        } else {
            cursor_y += child_metrics->flow_height;
        }
        child_count += 1u;
    }

    if (node->kind == ECS_UI_NODE_ZSTACK) {
        EcsUiSolverPlaceZStackFloatingChildren(ctx, index, rect);
    }
}

static void EcsUiSolverClearSnapshotLayout(EcsUiTreeSnapshot *tree)
{
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        tree->nodes[i].layout_x = 0.0f;
        tree->nodes[i].layout_y = 0.0f;
        tree->nodes[i].layout_width = 0.0f;
        tree->nodes[i].layout_height = 0.0f;
        tree->nodes[i].has_layout = false;
    }
}

static void EcsUiSolverApplyLayouts(EcsUiSolverContext *ctx)
{
    for (uint32_t i = 0u; i < ctx->tree->count; i += 1u) {
        if (!ctx->layouts[i].emitted) {
            continue;
        }
        EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[i];
        const EcsUiSolverRect rect = ctx->layouts[i].rect;
        node->layout_x = rect.x;
        node->layout_y = rect.y;
        node->layout_width = rect.width;
        node->layout_height = rect.height;
        node->has_layout = true;
    }
}

static void EcsUiSolverApplyDeepDivergence(EcsUiSolverContext *ctx)
{
    if (ctx == NULL || ctx->tree == NULL || ctx->layouts == NULL) {
        return;
    }
    uint32_t best = ECS_UI_TREE_INVALID_INDEX;
    uint32_t best_depth = 0u;
    for (uint32_t i = 1u; i < ctx->tree->count; i += 1u) {
        if (!ctx->layouts[i].emitted) {
            continue;
        }
        if (best == ECS_UI_TREE_INVALID_INDEX ||
                ctx->tree->nodes[i].depth > best_depth) {
            best = i;
            best_depth = ctx->tree->nodes[i].depth;
        }
    }
    if (best != ECS_UI_TREE_INVALID_INDEX) {
        ctx->layouts[best].rect.width += 11.0f;
    }
}

static bool EcsUiSolverMulSize(size_t a, size_t b, size_t *out)
{
    if (out == NULL) {
        return false;
    }
    if (a != 0u && b > SIZE_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}

static bool EcsUiSolverAlignSize(size_t value, size_t align, size_t *out)
{
    if (out == NULL || align == 0u) {
        return false;
    }
    const size_t mask = align - 1u;
    if (value > SIZE_MAX - mask) {
        return false;
    }
    *out = (value + mask) & ~mask;
    return true;
}

bool EcsUiSolverRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiSolverRunOptions *options,
    EcsUiSolverArena *arena)
{
    if (tree == NULL || tree->count == 0u || arena == NULL) {
        return false;
    }

    EcsUiSolverArenaReset(arena);
    if (options != NULL && options->error_kind != NULL) {
        *options->error_kind = ECS_UI_FRAME_ERROR_NONE;
    }
    EcsUiSolverContext ctx = {
        .tree = tree,
        .arena = arena,
        .layout_options = options != NULL ? options->layout : NULL,
        .surface_width = options != NULL ? options->surface_width : 0.0f,
        .surface_height = options != NULL ? options->surface_height : 0.0f,
        .measure_text = options != NULL ? options->measure_text : NULL,
        .measure_user_data =
            options != NULL ? options->measure_user_data : NULL,
        .scroll_offsets =
            options != NULL ? options->scroll_offsets : NULL,
        .scroll_offset_count =
            options != NULL ? options->scroll_offset_count : 0u,
        .scroll_contents =
            options != NULL ? options->scroll_contents : NULL,
        .scroll_content_count =
            options != NULL ? options->scroll_content_count : 0u,
        .text_line_capacity =
            options != NULL ? options->text_line_capacity : 0u,
        .error_kind = options != NULL ? options->error_kind : NULL,
        .error_message = options != NULL ? options->error_message : NULL,
        .error_message_size =
            options != NULL ? options->error_message_size : 0u,
    };
    size_t layout_bytes = 0u;
    size_t element_id_bytes = 0u;
    size_t metrics_bytes = 0u;
    size_t virtual_bytes = 0u;
    size_t scratch_bytes = 0u;
    size_t element_id_offset = 0u;
    size_t metrics_offset = 0u;
    size_t virtual_offset = 0u;
    size_t scratch_offset = 0u;
    size_t total_bytes = 0u;
    if (tree->count > UINT32_MAX / 5u) {
        EcsUiSolverSetErrorKind(
            &ctx,
            ECS_UI_FRAME_ERROR_ALLOCATION_FAILED,
            "native layout solver allocation failed");
        return false;
    }
    const uint32_t flow_capacity = tree->count * 5u;
    if (!EcsUiSolverMulSize(sizeof(EcsUiSolverLayout), tree->count, &layout_bytes) ||
            !EcsUiSolverMulSize(
                sizeof(EcsUiSolverElementIdEntry),
                tree->count,
                &element_id_bytes) ||
            !EcsUiSolverMulSize(sizeof(EcsUiSolverMetrics), tree->count, &metrics_bytes) ||
            !EcsUiSolverMulSize(
                sizeof(EcsUiSolverVirtualFlow),
                flow_capacity,
                &virtual_bytes) ||
            !EcsUiSolverMulSize(
                sizeof(EcsUiSolverFlowItem),
                flow_capacity,
                &scratch_bytes) ||
            !EcsUiSolverAlignSize(
                layout_bytes,
                sizeof(void *),
                &element_id_offset) ||
            element_id_offset > SIZE_MAX - element_id_bytes ||
            !EcsUiSolverAlignSize(
                element_id_offset + element_id_bytes,
                sizeof(void *),
                &metrics_offset) ||
            metrics_offset > SIZE_MAX - metrics_bytes ||
            !EcsUiSolverAlignSize(
                metrics_offset + metrics_bytes,
                sizeof(void *),
                &virtual_offset) ||
            virtual_offset > SIZE_MAX - virtual_bytes ||
            !EcsUiSolverAlignSize(
                virtual_offset + virtual_bytes,
                sizeof(void *),
                &scratch_offset) ||
            scratch_offset > SIZE_MAX - scratch_bytes) {
        EcsUiSolverSetErrorKind(
            &ctx,
            ECS_UI_FRAME_ERROR_ALLOCATION_FAILED,
            "native layout solver allocation failed");
        return false;
    }
    total_bytes = scratch_offset + scratch_bytes;
    unsigned char *work = (unsigned char *)EcsUiSolverArenaAlloc(
        arena,
        total_bytes,
        sizeof(void *));
    if (work == NULL) {
        EcsUiSolverSetErrorKind(
            &ctx,
            ECS_UI_FRAME_ERROR_ALLOCATION_FAILED,
        "native layout solver allocation failed");
        return false;
    }
    ctx.layouts = (EcsUiSolverLayout *)work;
    ctx.element_ids =
        (EcsUiSolverElementIdEntry *)(void *)(work + element_id_offset);
    ctx.metrics = (EcsUiSolverMetrics *)(void *)(work + metrics_offset);
    ctx.virtual_entries =
        (EcsUiSolverVirtualFlow *)(void *)(work + virtual_offset);
    ctx.virtual_capacity = flow_capacity;
    ctx.scratch_items = (EcsUiSolverFlowItem *)(void *)(work + scratch_offset);
    ctx.scratch_item_capacity = flow_capacity;
    if (!EcsUiSolverValidateSupported(&ctx)) {
        return false;
    }
    const EcsUiFrameLayoutOptions *layout =
        options != NULL ? options->layout : NULL;
    EcsUiSolverClearSnapshotLayout(tree);
    EcsUiSolverComputeBottomUp(&ctx, 0u, 1.0f);
    if (ctx.failed) {
        return false;
    }
    EcsUiSolverApplyRootContainer(&ctx, layout);
    EcsUiSolverSolveAxisTopDown(&ctx, 0u, true);
    if (ctx.failed) {
        return false;
    }
    EcsUiSolverSolveAxisTopDown(&ctx, 0u, false);
    if (ctx.failed) {
        return false;
    }
    EcsUiSolverPlaceNode(
        &ctx,
        0u,
        (EcsUiSolverRect){
            .x = 0.0f,
            .y = 0.0f,
            .width = ctx.metrics[0].width,
            .height = ctx.metrics[0].height,
        });
    if (options != NULL && options->force_divergence && ctx.layouts[0].emitted) {
        ctx.layouts[0].rect.x += 7.0f;
    }
    if (options != NULL && options->force_deep_divergence) {
        EcsUiSolverApplyDeepDivergence(&ctx);
    }
    EcsUiSolverApplyLayouts(&ctx);
    return true;
}
