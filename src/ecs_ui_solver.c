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
    struct EcsUiSolverMetrics *metrics;
    uint32_t *scratch_indices;
    EcsUiMeasureTextFn measure_text;
    void *measure_user_data;
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

static float EcsUiSolverRoleTextSize(EcsUiTextRole role)
{
    switch (role) {
    case ECS_UI_TEXT_TITLE:
        return 28.0f;
    case ECS_UI_TEXT_BUTTON:
    case ECS_UI_TEXT_LABEL:
        return 18.0f;
    case ECS_UI_TEXT_CAPTION:
        return 13.0f;
    case ECS_UI_TEXT_BODY:
    default:
        return 18.0f;
    }
}

static float EcsUiSolverTextStyleSize(
    EcsUiTextRole role,
    EcsUiTextStyle text_style)
{
    switch (role) {
    case ECS_UI_TEXT_TITLE:
        return text_style.title_size;
    case ECS_UI_TEXT_LABEL:
        return text_style.label_size;
    case ECS_UI_TEXT_BUTTON:
        return text_style.button_size;
    case ECS_UI_TEXT_CAPTION:
        return text_style.caption_size;
    case ECS_UI_TEXT_BODY:
    default:
        return text_style.body_size;
    }
}

static float EcsUiSolverTextSize(
    EcsUiTextRole role,
    EcsUiTextStyle text_style,
    bool has_text_style)
{
    const float styled_size =
        has_text_style ? EcsUiSolverTextStyleSize(role, text_style) : 0.0f;
    return styled_size > 0.0f ? styled_size : EcsUiSolverRoleTextSize(role);
}

static float EcsUiSolverInheritedTextSize(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    if (tree == NULL || index >= tree->count) {
        return EcsUiSolverRoleTextSize(ECS_UI_TEXT_BODY);
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiTextStyle text_style = {0};
    bool has_text_style = false;
    uint32_t current = index;
    while (current != ECS_UI_TREE_INVALID_INDEX && current < tree->count) {
        const EcsUiTreeNodeSnapshot *candidate = &tree->nodes[current];
        if (candidate->has_text_style) {
            text_style = candidate->text_style;
            has_text_style = true;
            break;
        }
        current = candidate->parent_index;
    }
    return EcsUiSolverTextSize(node->text.role, text_style, has_text_style);
}

static float EcsUiSolverLocalTextSize(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL ?
        EcsUiSolverTextSize(
            node->text.role,
            node->text_style,
            node->has_text_style) :
        EcsUiSolverRoleTextSize(ECS_UI_TEXT_BODY);
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

static bool EcsUiSolverValidateSupported(EcsUiSolverContext *ctx)
{
    if (ctx == NULL || ctx->tree == NULL) {
        return false;
    }
    for (uint32_t i = 0u; i < ctx->tree->count; i += 1u) {
        const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[i];
        char message[128] = {0};
        if (node->kind == ECS_UI_NODE_PRESSABLE && node->has_text_field_view) {
            (void)snprintf(
                message,
                sizeof(message),
                "unsupported text-field view on node kind %d -- stage 6",
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
        if (node->has_placement) {
            (void)snprintf(
                message,
                sizeof(message),
                "unsupported placement on node kind %d -- stage 6",
                (int)node->kind);
            EcsUiSolverSetError(ctx, message);
            return false;
        }
        if (node->has_scroll_view) {
            (void)snprintf(
                message,
                sizeof(message),
                "unsupported scroll/clip on node kind %d -- stage 6",
                (int)node->kind);
            EcsUiSolverSetError(ctx, message);
            return false;
        }
    }
    return true;
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
    bool visible;
} EcsUiSolverMetrics;

static bool EcsUiSolverLayoutHorizontal(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (EcsUiSolverNodeIsStack(node)) {
        return node->stack.axis == ECS_UI_AXIS_HORIZONTAL;
    }
    return node->kind == ECS_UI_NODE_BUTTON ||
        node->kind == ECS_UI_NODE_PRESSABLE;
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

static float *EcsUiSolverAxisSize(EcsUiSolverMetrics *metrics, bool x_axis)
{
    return x_axis ? &metrics->width : &metrics->height;
}

static float *EcsUiSolverAxisMin(EcsUiSolverMetrics *metrics, bool x_axis)
{
    return x_axis ? &metrics->min_width : &metrics->min_height;
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
    return EcsUiSolverAxisSizing(metrics, x_axis).kind !=
        ECS_UI_SOLVER_SIZE_FIXED;
}

typedef struct EcsUiSolverTextMeasure {
    float width;
    float height;
    float min_width;
} EcsUiSolverTextMeasure;

static EcsUiSize EcsUiSolverMeasureTextSlice(
    EcsUiSolverContext *ctx,
    const char *text,
    int32_t length,
    uint16_t font_size)
{
    if (ctx == NULL || ctx->measure_text == NULL) {
        return (EcsUiSize){0};
    }

    const EcsUiTextMeasureSpec spec = {
        .font_id = 0u,
        .font_size = (float)font_size,
        .letter_spacing = 0.0f,
        .line_height = 0.0f,
    };
    return ctx->measure_text(
        text,
        length,
        &spec,
        ctx->measure_user_data);
}

static EcsUiSolverTextMeasure EcsUiSolverMeasureText(
    EcsUiSolverContext *ctx,
    uint32_t index)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiSolverTextMeasure measured = {0};
    if (ctx->measure_text == NULL) {
        EcsUiSolverSetError(
            ctx,
            "native layout solver text measure callback missing");
        return measured;
    }

    const float scale = EcsUiSolverScale(tree);
    const uint16_t font_size =
        EcsUiSolverU16(EcsUiSolverInheritedTextSize(tree, index) * scale);
    const char *text = node->text.text;
    const int32_t text_length = (int32_t)strlen(text);
    const float space_width =
        EcsUiSolverMeasureTextSlice(ctx, " ", 1, font_size).width;

    int32_t start = 0;
    int32_t end = 0;
    float line_width = 0.0f;
    float measured_width = 0.0f;
    float measured_height = 0.0f;
    float min_width = 0.0f;
    while (end < text_length) {
        const char current = text[end];
        if (current == ' ' || current == '\n') {
            const int32_t word_length = end - start;
            EcsUiSize dimensions = {0};
            if (word_length > 0) {
                dimensions = EcsUiSolverMeasureTextSlice(
                    ctx,
                    &text[start],
                    word_length,
                    font_size);
            }
            min_width = EcsUiSolverMaxFloat(min_width, dimensions.width);
            measured_height =
                EcsUiSolverMaxFloat(measured_height, dimensions.height);
            if (current == ' ') {
                dimensions.width += space_width;
                line_width += dimensions.width;
            } else {
                line_width += dimensions.width;
                measured_width =
                    EcsUiSolverMaxFloat(measured_width, line_width);
                line_width = 0.0f;
            }
            start = end + 1;
        }
        end += 1;
    }

    if (end - start > 0) {
        const EcsUiSize dimensions = EcsUiSolverMeasureTextSlice(
            ctx,
            &text[start],
            end - start,
            font_size);
        line_width += dimensions.width;
        measured_height =
            EcsUiSolverMaxFloat(measured_height, dimensions.height);
        min_width = EcsUiSolverMaxFloat(min_width, dimensions.width);
    }

    measured_width = EcsUiSolverMaxFloat(line_width, measured_width);
    measured.width = measured_width / scale;
    measured.height = measured_height / scale;
    measured.min_width = min_width / scale;
    return measured;
}

static void EcsUiSolverComputeBottomUp(
    EcsUiSolverContext *ctx,
    uint32_t index,
    float parent_opacity)
{
    EcsUiTreeSnapshot *tree = ctx->tree;
    EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiSolverMetrics *metrics = &ctx->metrics[index];
    const float node_opacity =
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

    float width = 0.0f;
    float height = 0.0f;
    float min_width = 0.0f;
    float min_height = 0.0f;
    uint32_t visible_child_count = 0u;

    if (node->kind == ECS_UI_NODE_TEXT) {
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
            if (!child_metrics->visible) {
                continue;
            }
            const float child_width = child_metrics->width;
            const float child_height = child_metrics->height;
            const float child_min_width = child_metrics->min_width;
            const float child_min_height = child_metrics->min_height;
            if (horizontal) {
                if (visible_child_count > 0u) {
                    width += gap;
                    min_width += gap;
                }
                width += child_width;
                min_width += child_min_width;
                height = EcsUiSolverMaxFloat(height, child_height);
                min_height = EcsUiSolverMaxFloat(min_height, child_min_height);
            } else {
                if (visible_child_count > 0u) {
                    height += gap;
                    min_height += gap;
                }
                height += child_height;
                min_height += child_min_height;
                width = EcsUiSolverMaxFloat(width, child_width);
                min_width = EcsUiSolverMaxFloat(min_width, child_min_width);
            }
            visible_child_count += 1u;
        }
        width += padding_left + padding_right;
        min_width += padding_left + padding_right;
        height += padding_top + padding_bottom;
        min_height += padding_top + padding_bottom;
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
    EcsUiTreeSnapshot *tree = ctx->tree;
    uint32_t count = 0u;
    for (uint32_t child = tree->nodes[parent].first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
        if (!child_metrics->visible ||
                !EcsUiSolverAxisResizable(child_metrics, x_axis)) {
            continue;
        }
        if (grow_only &&
                EcsUiSolverAxisSizing(child_metrics, x_axis).kind !=
                    ECS_UI_SOLVER_SIZE_GROW) {
            continue;
        }
        ctx->scratch_indices[count] = child;
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
    while (free_space > epsilon && active_count > 0u) {
        float smallest = 3.4e38f;
        float second_smallest = 3.4e38f;
        float amount = free_space;
        for (uint32_t i = 0u; i < active_count; i += 1u) {
            uint32_t child = ctx->scratch_indices[i];
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            const float size = *EcsUiSolverAxisSize(child_metrics, x_axis);
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
            uint32_t child = ctx->scratch_indices[i];
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            float *size = EcsUiSolverAxisSize(child_metrics, x_axis);
            if (EcsUiSolverFloatEqual(tree, *size, smallest)) {
                *size += amount;
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
    uint32_t iteration_limit =
        active_count > 0u ? (active_count * active_count * 8u) + 8u : 0u;

    /* Stage 3 min dimensions equal content for supported node kinds, so this
     * path usually cannot shrink yet. Keep the loop shaped like Clay so text
     * and scroll stages inherit the same compression/dropout behavior. */
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
            uint32_t child = ctx->scratch_indices[i];
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            const float size = *EcsUiSolverAxisSize(child_metrics, x_axis);
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
            uint32_t child = ctx->scratch_indices[i];
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            float *size = EcsUiSolverAxisSize(child_metrics, x_axis);
            const float min_size = *EcsUiSolverAxisMin(child_metrics, x_axis);
            if (EcsUiSolverFloatEqual(tree, *size, largest)) {
                const float previous = *size;
                *size += amount;
                if (*size <= min_size) {
                    *size = min_size;
                    active_count -= 1u;
                    ctx->scratch_indices[i] = ctx->scratch_indices[active_count];
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
        for (uint32_t child = node->first_child;
             child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            if (!child_metrics->visible) {
                continue;
            }
            inner += *EcsUiSolverAxisSize(child_metrics, x_axis);
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
        } else if (free_space < 0.0f) {
            EcsUiSolverCompressResizable(ctx, index, x_axis, free_space);
            if (ctx->failed) {
                return;
            }
        }
    } else {
        const float max_size = parent_size - parent_padding;
        for (uint32_t child = node->first_child;
             child != ECS_UI_TREE_INVALID_INDEX;
             child = tree->nodes[child].next_sibling) {
            EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
            if (!child_metrics->visible ||
                    !EcsUiSolverAxisResizable(child_metrics, x_axis)) {
                continue;
            }
            float *size = EcsUiSolverAxisSize(child_metrics, x_axis);
            const float min_size = *EcsUiSolverAxisMin(child_metrics, x_axis);
            if (EcsUiSolverAxisSizing(child_metrics, x_axis).kind ==
                    ECS_UI_SOLVER_SIZE_GROW) {
                *size = max_size;
            }
            if (*size > max_size) {
                *size = max_size;
            }
            if (*size < min_size) {
                *size = min_size;
            }
        }
    }

    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
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
        if (!child_metrics->visible) {
            continue;
        }
        if (visible_child_count > 0u) {
            size += gap;
        }
        size += horizontal ? child_metrics->width : child_metrics->height;
        visible_child_count += 1u;
    }
    return size;
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
    const float main_inner_size = horizontal ?
        rect.width - padding_left - padding_right :
        rect.height - padding_top - padding_bottom;
    const float main_offset =
        EcsUiSolverMainAxisOffset(node, horizontal, main_inner_size - main_children_size);
    float cursor_x = content_x + (horizontal ? main_offset : 0.0f);
    float cursor_y = content_y + (!horizontal ? main_offset : 0.0f);
    uint32_t child_count = 0u;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        EcsUiSolverMetrics *child_metrics = &ctx->metrics[child];
        if (!child_metrics->visible) {
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
            rect.width - padding_left - padding_right - child_metrics->width);
        const float child_offset_y = EcsUiSolverAlignmentOffset(
            EcsUiSolverAlignY(node),
            rect.height - padding_top - padding_bottom - child_metrics->height);
        EcsUiSolverRect child_rect = {
            .x = horizontal ? cursor_x : content_x + child_offset_x,
            .y = horizontal ? content_y + child_offset_y : cursor_y,
            .width = child_metrics->width,
            .height = child_metrics->height,
        };
        EcsUiSolverPlaceNode(ctx, child, child_rect);
        if (horizontal) {
            cursor_x += child_metrics->width;
        } else {
            cursor_y += child_metrics->height;
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
    EcsUiSolverContext ctx = {
        .tree = tree,
        .arena = arena,
        .measure_text = options != NULL ? options->measure_text : NULL,
        .measure_user_data =
            options != NULL ? options->measure_user_data : NULL,
        .error_message = options != NULL ? options->error_message : NULL,
        .error_message_size =
            options != NULL ? options->error_message_size : 0u,
    };
    if (!EcsUiSolverValidateSupported(&ctx)) {
        return false;
    }
    size_t layout_bytes = 0u;
    size_t metrics_bytes = 0u;
    size_t scratch_bytes = 0u;
    size_t metrics_offset = 0u;
    size_t scratch_offset = 0u;
    size_t total_bytes = 0u;
    if (!EcsUiSolverMulSize(sizeof(EcsUiSolverLayout), tree->count, &layout_bytes) ||
            !EcsUiSolverMulSize(sizeof(EcsUiSolverMetrics), tree->count, &metrics_bytes) ||
            !EcsUiSolverMulSize(sizeof(uint32_t), tree->count, &scratch_bytes) ||
            !EcsUiSolverAlignSize(layout_bytes, sizeof(void *), &metrics_offset) ||
            metrics_offset > SIZE_MAX - metrics_bytes ||
            !EcsUiSolverAlignSize(
                metrics_offset + metrics_bytes,
                sizeof(void *),
                &scratch_offset) ||
            scratch_offset > SIZE_MAX - scratch_bytes) {
        EcsUiSolverSetError(&ctx, "native layout solver allocation failed");
        return false;
    }
    total_bytes = scratch_offset + scratch_bytes;
    unsigned char *work = (unsigned char *)EcsUiSolverArenaAlloc(
        arena,
        total_bytes,
        sizeof(void *));
    if (work == NULL) {
        EcsUiSolverSetError(&ctx, "native layout solver allocation failed");
        return false;
    }
    ctx.layouts = (EcsUiSolverLayout *)work;
    ctx.metrics = (EcsUiSolverMetrics *)(void *)(work + metrics_offset);
    ctx.scratch_indices = (uint32_t *)(void *)(work + scratch_offset);
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
