#include "ecs_ui_solver.h"

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

typedef struct EcsUiSolverContext {
    EcsUiTreeSnapshot *tree;
    EcsUiSolverArena *arena;
    EcsUiSolverLayout *layouts;
    char *error_message;
    size_t error_message_size;
    bool failed;
} EcsUiSolverContext;

static float EcsUiSolverMaxFloat(float a, float b)
{
    return a > b ? a : b;
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

static void EcsUiSolverSetError(
    EcsUiSolverContext *ctx,
    const char *message)
{
    if (ctx == NULL) {
        return;
    }
    ctx->failed = true;
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

static bool EcsUiSolverNodeUsesFit(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->stack.width_sizing == ECS_UI_SIZE_FIT ||
            node->stack.height_sizing == ECS_UI_SIZE_FIT;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        return node->custom.width_sizing == ECS_UI_SIZE_FIT ||
            node->custom.height_sizing == ECS_UI_SIZE_FIT;
    }
    return false;
}

static bool EcsUiSolverValidateSupported(EcsUiSolverContext *ctx)
{
    if (ctx == NULL || ctx->tree == NULL) {
        return false;
    }
    for (uint32_t i = 0u; i < ctx->tree->count; i += 1u) {
        const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[i];
        char message[128] = {0};
        if (node->kind == ECS_UI_NODE_TEXT) {
            (void)snprintf(
                message,
                sizeof(message),
                "unsupported node kind %d -- stage 5",
                (int)node->kind);
            EcsUiSolverSetError(ctx, message);
            return false;
        }
        if (node->kind == ECS_UI_NODE_ZSTACK) {
            (void)snprintf(
                message,
                sizeof(message),
                "unsupported node kind %d -- stage 6",
                (int)node->kind);
            EcsUiSolverSetError(ctx, message);
            return false;
        }
        if (EcsUiSolverNodeUsesFit(node)) {
            (void)snprintf(
                message,
                sizeof(message),
                "unsupported FIT sizing on node kind %d -- stage 3",
                (int)node->kind);
            EcsUiSolverSetError(ctx, message);
            return false;
        }
    }
    return true;
}

static float EcsUiSolverPreferredWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index);

static float EcsUiSolverPreferredHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index);

static float EcsUiSolverPreferredWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node) &&
            node->stack.preferred_width > 0.0f &&
            node->stack.width_sizing != ECS_UI_SIZE_GROW) {
        return node->stack.preferred_width;
    }

    switch (node->kind) {
    case ECS_UI_NODE_ICON:
        return ECS_UI_SOLVER_ICON_SIZE;
    case ECS_UI_NODE_BUTTON:
        return node->button.preferred_width > 0.0f ?
            node->button.preferred_width :
            0.0f;
    case ECS_UI_NODE_CUSTOM:
        if (node->custom.width_sizing == ECS_UI_SIZE_GROW) {
            return 0.0f;
        }
        return node->custom.preferred_width > 0.0f ?
            node->custom.preferred_width :
            0.0f;
    case ECS_UI_NODE_HSTACK: {
        float width =
            EcsUiSolverPaddingLeft(tree, node) +
            EcsUiSolverPaddingRight(tree, node);
        uint32_t child_count = 0u;
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            if (child_count > 0u) {
                width += EcsUiSolverGap(tree, node);
            }
            width += EcsUiSolverPreferredWidth(tree, child);
            child_count += 1u;
        }
        return width;
    }
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_ZSTACK: {
        float width =
            EcsUiSolverPaddingLeft(tree, node) +
            EcsUiSolverPaddingRight(tree, node);
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            width = EcsUiSolverMaxFloat(
                width,
                EcsUiSolverPaddingLeft(tree, node) +
                    EcsUiSolverPaddingRight(tree, node) +
                    EcsUiSolverPreferredWidth(tree, child));
        }
        return width;
    }
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_PRESSABLE:
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }
}

static float EcsUiSolverPreferredHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node) &&
            node->stack.preferred_height > 0.0f &&
            node->stack.height_sizing != ECS_UI_SIZE_GROW) {
        return node->stack.preferred_height;
    }

    switch (node->kind) {
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
    case ECS_UI_NODE_TEXT:
        return 0.0f;
    case ECS_UI_NODE_HSTACK: {
        float height = 0.0f;
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            height = EcsUiSolverMaxFloat(
                height,
                EcsUiSolverPreferredHeight(tree, child));
        }
        return EcsUiSolverPaddingTop(tree, node) +
            EcsUiSolverPaddingBottom(tree, node) +
            height;
    }
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_ZSTACK: {
        float height =
            EcsUiSolverPaddingTop(tree, node) +
            EcsUiSolverPaddingBottom(tree, node);
        uint32_t child_count = 0u;
        for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            if (child_count > 0u && node->kind != ECS_UI_NODE_ZSTACK) {
                height += EcsUiSolverGap(tree, node);
            }
            height += EcsUiSolverPreferredHeight(tree, child);
            child_count += 1u;
        }
        return height;
    }
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }
}

static float EcsUiSolverChildWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    float parent_content_width)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node)) {
        if (node->stack.preferred_width > 0.0f &&
                node->stack.width_sizing != ECS_UI_SIZE_GROW) {
            return node->stack.preferred_width;
        }
        return parent_content_width;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        if (node->custom.width_sizing == ECS_UI_SIZE_GROW) {
            return parent_content_width;
        }
        return node->custom.preferred_width > 0.0f ?
            node->custom.preferred_width :
            parent_content_width;
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return node->button.preferred_width > 0.0f ?
            node->button.preferred_width :
            parent_content_width;
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return parent_content_width;
    }
    if (node->kind == ECS_UI_NODE_TEXT) {
        return 0.0f;
    }
    if (node->kind == ECS_UI_NODE_ICON) {
        return ECS_UI_SOLVER_ICON_SIZE;
    }
    return EcsUiSolverPreferredWidth(tree, index);
}

static bool EcsUiSolverWidthGrows(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->stack.width_sizing == ECS_UI_SIZE_GROW ||
            node->stack.preferred_width <= 0.0f;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        return node->custom.width_sizing == ECS_UI_SIZE_GROW ||
            node->custom.preferred_width <= 0.0f;
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return node->button.preferred_width <= 0.0f;
    }
    return node->kind == ECS_UI_NODE_PRESSABLE;
}

static bool EcsUiSolverHeightGrows(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->kind == ECS_UI_NODE_ROOT ||
            node->stack.height_sizing == ECS_UI_SIZE_GROW;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        return node->custom.height_sizing == ECS_UI_SIZE_GROW;
    }
    return false;
}

static float EcsUiSolverChildHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    float parent_content_height)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiSolverNodeIsStack(node)) {
        if (node->stack.preferred_height > 0.0f &&
                node->stack.height_sizing != ECS_UI_SIZE_GROW) {
            return node->stack.preferred_height;
        }
        if (node->stack.height_sizing == ECS_UI_SIZE_GROW) {
            return parent_content_height;
        }
        return EcsUiSolverPreferredHeight(tree, index);
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        return node->custom.height_sizing == ECS_UI_SIZE_GROW ?
            parent_content_height :
            EcsUiSolverCustomHeight(node);
    }
    return EcsUiSolverPreferredHeight(tree, index);
}

static bool EcsUiSolverChildMainGrows(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    bool horizontal)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    return horizontal ? EcsUiSolverWidthGrows(node) : EcsUiSolverHeightGrows(node);
}

static float EcsUiSolverChildMainBaseSize(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    bool horizontal,
    float parent_content_width,
    float parent_content_height)
{
    if (horizontal) {
        const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
        if (node->kind == ECS_UI_NODE_CUSTOM &&
                EcsUiSolverWidthGrows(node)) {
            return 0.0f;
        }
        return EcsUiSolverWidthGrows(node) ?
            EcsUiSolverPreferredWidth(tree, index) :
            EcsUiSolverChildWidth(tree, index, parent_content_width);
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (node->kind == ECS_UI_NODE_CUSTOM &&
            EcsUiSolverHeightGrows(node)) {
        return 0.0f;
    }
    return EcsUiSolverHeightGrows(node) ?
        EcsUiSolverPreferredHeight(tree, index) :
        EcsUiSolverChildHeight(tree, index, parent_content_height);
}

static void EcsUiSolverSolveNode(
    EcsUiSolverContext *ctx,
    uint32_t index,
    EcsUiSolverRect rect,
    float parent_opacity)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float node_opacity =
        parent_opacity * EcsUiSolverClamp01(node->visual.opacity);
    if (node_opacity <= 0.01f) {
        return;
    }
    ctx->layouts[index] = (EcsUiSolverLayout){
        .rect = rect,
        .emitted = true,
    };
    if (!EcsUiSolverNodeIsStack(node) &&
            node->kind != ECS_UI_NODE_BUTTON &&
            node->kind != ECS_UI_NODE_PRESSABLE) {
        return;
    }

    const float padding_left = EcsUiSolverPaddingLeft(tree, node);
    const float padding_top = EcsUiSolverPaddingTop(tree, node);
    const float padding_right = EcsUiSolverPaddingRight(tree, node);
    const float padding_bottom = EcsUiSolverPaddingBottom(tree, node);
    const float content_x = rect.x + padding_left;
    const float content_y = rect.y + padding_top;
    const float content_width = EcsUiSolverMaxFloat(
        rect.width - padding_left - padding_right,
        0.0f);
    const float content_height = EcsUiSolverMaxFloat(
        rect.height - padding_top - padding_bottom,
        0.0f);
    const bool horizontal =
        node->kind == ECS_UI_NODE_HSTACK ||
        node->kind == ECS_UI_NODE_BUTTON ||
        node->kind == ECS_UI_NODE_PRESSABLE;

    const float available_main = horizontal ? content_width : content_height;
    const float gap = EcsUiSolverGap(tree, node);
    float main_sum = 0.0f;
    uint32_t visible_child_count = 0u;
    uint32_t grow_child_count = 0u;
    for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        const float child_opacity =
            node_opacity * EcsUiSolverClamp01(child_node->visual.opacity);
        if (child_opacity <= 0.01f) {
            continue;
        }
        main_sum += EcsUiSolverChildMainBaseSize(
            tree,
            child,
            horizontal,
            content_width,
            content_height);
        if (EcsUiSolverChildMainGrows(tree, child, horizontal)) {
            grow_child_count += 1u;
        }
        visible_child_count += 1u;
    }
    const float gap_sum =
        node->kind != ECS_UI_NODE_ZSTACK && visible_child_count > 1u ?
            gap * (float)(visible_child_count - 1u) :
            0.0f;
    const float grow_add =
        grow_child_count > 0u && available_main > main_sum + gap_sum ?
            (available_main - main_sum - gap_sum) / (float)grow_child_count :
            0.0f;

    float cursor_x = content_x;
    float cursor_y = content_y;
    uint32_t child_count = 0u;
    for (uint32_t child = node->first_child; child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        const float child_opacity =
            node_opacity * EcsUiSolverClamp01(child_node->visual.opacity);
        if (child_opacity <= 0.01f) {
            continue;
        }
        if (child_count > 0u && node->kind != ECS_UI_NODE_ZSTACK) {
            if (horizontal) {
                cursor_x += gap;
            } else {
                cursor_y += gap;
            }
        }
        float child_width = EcsUiSolverChildWidth(tree, child, content_width);
        float child_height = EcsUiSolverChildHeight(tree, child, content_height);
        if (horizontal) {
            child_width = EcsUiSolverChildMainBaseSize(
                tree,
                child,
                true,
                content_width,
                content_height);
            if (EcsUiSolverChildMainGrows(tree, child, true)) {
                child_width += grow_add;
            }
        } else {
            child_height = EcsUiSolverChildMainBaseSize(
                tree,
                child,
                false,
                content_width,
                content_height);
            if (EcsUiSolverChildMainGrows(tree, child, false)) {
                child_height += grow_add;
            }
        }
        EcsUiSolverRect child_rect = {
            .x = cursor_x,
            .y = cursor_y,
            .width = child_width,
            .height = child_height,
        };
        EcsUiSolverSolveNode(ctx, child, child_rect, node_opacity);
        if (horizontal) {
            cursor_x += child_width;
        } else if (node->kind != ECS_UI_NODE_ZSTACK) {
            cursor_y += child_height;
        }
        child_count += 1u;
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

bool EcsUiSolverRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiSolverRunOptions *options,
    EcsUiSolverArena *arena)
{
    if (tree == NULL || tree->count == 0u || arena == NULL) {
        return false;
    }

    EcsUiSolverArenaReset(arena);
    EcsUiSolverContext ctx = {
        .tree = tree,
        .arena = arena,
        .error_message = options != NULL ? options->error_message : NULL,
        .error_message_size =
            options != NULL ? options->error_message_size : 0u,
    };
    if (!EcsUiSolverValidateSupported(&ctx)) {
        return false;
    }
    EcsUiSolverLayout *layouts = (EcsUiSolverLayout *)EcsUiSolverArenaAlloc(
        arena,
        sizeof(layouts[0]) * tree->count,
        sizeof(void *));
    if (layouts == NULL) {
        EcsUiSolverSetError(&ctx, "native layout solver allocation failed");
        return false;
    }
    ctx.layouts = layouts;
    EcsUiSolverClearSnapshotLayout(tree);
    const EcsUiFrameLayoutOptions *layout =
        options != NULL ? options->layout : NULL;
    EcsUiSolverRect root_rect = {
        .x = 0.0f,
        .y = 0.0f,
        .width = EcsUiSolverLogicalRootWidth(tree, layout),
        .height = EcsUiSolverLogicalRootHeight(tree, layout),
    };
    const EcsUiTreeNodeSnapshot *root = &tree->nodes[0];
    const bool root_width_fixed =
        root->stack.preferred_width > 0.0f &&
        root->stack.width_sizing != ECS_UI_SIZE_GROW;
    const bool root_height_fixed =
        root->stack.preferred_height > 0.0f &&
        root->stack.height_sizing != ECS_UI_SIZE_GROW;
    root_rect.width = root_width_fixed ?
        root->stack.preferred_width :
        EcsUiSolverMaxFloat(root_rect.width, EcsUiSolverPreferredWidth(tree, 0u));
    root_rect.height = root_height_fixed ?
        root->stack.preferred_height :
        EcsUiSolverMaxFloat(
            root_rect.height,
            EcsUiSolverPreferredHeight(tree, 0u));
    EcsUiSolverSolveNode(&ctx, 0u, root_rect, 1.0f);
    if (options != NULL && options->force_divergence && ctx.layouts[0].emitted) {
        ctx.layouts[0].rect.x += 7.0f;
    }
    if (options != NULL && options->force_deep_divergence) {
        EcsUiSolverApplyDeepDivergence(&ctx);
    }
    EcsUiSolverApplyLayouts(&ctx);
    return true;
}
