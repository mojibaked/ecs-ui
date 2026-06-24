#include "ecs_ui/ecs_ui_raylib.h"

#include <stdio.h>
#include <string.h>

/*
 * Optional font override. The renderer draws all text with GetFontDefault() —
 * raylib's 10px bitmap font, which only looks crisp at multiples of 10. Callers
 * can supply a higher-resolution font (e.g. a TTF atlas) via EcsUiRaylibSetFont
 * to stay sharp at any size. The renderer never takes ownership of the font.
 */
static Font g_ecs_ui_raylib_font = {0};
static bool g_ecs_ui_raylib_font_set = false;

static Font EcsUiRaylibActiveFont(void)
{
    return g_ecs_ui_raylib_font_set ? g_ecs_ui_raylib_font : GetFontDefault();
}

void EcsUiRaylibSetFont(Font font)
{
    g_ecs_ui_raylib_font = font;
    g_ecs_ui_raylib_font_set = true;
}

void EcsUiRaylibResetFont(void)
{
    g_ecs_ui_raylib_font = (Font){0};
    g_ecs_ui_raylib_font_set = false;
}

static float EcsUiRaylibMaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static float EcsUiRaylibClampPositive(float value)
{
    return value > 0.0f ? value : 0.0f;
}

static float EcsUiRaylibClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static Rectangle EcsUiRaylibOffset(Rectangle bounds, EcsUiVisual visual)
{
    bounds.x += visual.offset_x;
    bounds.y += visual.offset_y;
    return bounds;
}

static Rectangle EcsUiRaylibInset(Rectangle bounds, float amount)
{
    const float inset = EcsUiRaylibClampPositive(amount);
    const float width = bounds.width - (inset * 2.0f);
    const float height = bounds.height - (inset * 2.0f);
    return (Rectangle){
        .x = bounds.x + inset,
        .y = bounds.y + inset,
        .width = EcsUiRaylibClampPositive(width),
        .height = EcsUiRaylibClampPositive(height),
    };
}

static Color EcsUiRaylibApplyOpacity(Color color, float opacity)
{
    color.a = (unsigned char)((float)color.a * EcsUiRaylibClamp01(opacity));
    return color;
}

static Color EcsUiRaylibColor(EcsUiColor color)
{
    return (Color){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a,
    };
}

static Color EcsUiRaylibStyleColorOr(EcsUiColor color, Color fallback)
{
    return color.a != 0u ? EcsUiRaylibColor(color) : fallback;
}

static Color EcsUiRaylibLerpColor(Color from, Color to, float amount)
{
    const float t = EcsUiRaylibClamp01(amount);
    return (Color){
        .r = (unsigned char)((float)from.r + ((float)to.r - (float)from.r) * t),
        .g = (unsigned char)((float)from.g + ((float)to.g - (float)from.g) * t),
        .b = (unsigned char)((float)from.b + ((float)to.b - (float)from.b) * t),
        .a = (unsigned char)((float)from.a + ((float)to.a - (float)from.a) * t),
    };
}

typedef struct EcsUiRaylibPointerCapture {
    bool active;
    ecs_entity_t node;
    ecs_entity_t action;
    char node_id[ECS_UI_ID_MAX];
    Vector2 start;
    double start_time;
} EcsUiRaylibPointerCapture;

static EcsUiRaylibPointerCapture g_ecs_ui_raylib_pointer_capture;

static float EcsUiRaylibDistanceSquared(Vector2 from, Vector2 to)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    return (dx * dx) + (dy * dy);
}

static uint32_t EcsUiRaylibChildCount(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    uint32_t count = 0u;
    uint32_t child = tree->nodes[index].first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        count += 1u;
        child = tree->nodes[child].next_sibling;
    }
    return count;
}

static uint32_t EcsUiRaylibFindNodeIndex(
    const EcsUiTreeSnapshot *tree,
    ecs_entity_t entity)
{
    if (tree == NULL || entity == 0) {
        return ECS_UI_TREE_INVALID_INDEX;
    }

    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (tree->nodes[i].entity == entity) {
            return i;
        }
    }
    return ECS_UI_TREE_INVALID_INDEX;
}

static uint32_t EcsUiRaylibClampTextIndex(uint32_t index, size_t length)
{
    return index <= length ? index : (uint32_t)length;
}

static void EcsUiRaylibCopyTextRange(
    char *out,
    size_t out_size,
    const char *text,
    uint32_t start,
    uint32_t end)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *source = text != NULL ? text : "";
    const size_t length = strlen(source);
    uint32_t range_start = EcsUiRaylibClampTextIndex(start, length);
    uint32_t range_end = EcsUiRaylibClampTextIndex(end, length);
    if (range_start > range_end) {
        uint32_t swap = range_start;
        range_start = range_end;
        range_end = swap;
    }

    size_t out_index = 0u;
    for (uint32_t i = range_start;
         i < range_end && out_index + 1u < out_size;
         i += 1u) {
        out[out_index] = source[i];
        out_index += 1u;
    }
    out[out_index] = '\0';
}

static float EcsUiRaylibTextSize(EcsUiTextRole role)
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

static float EcsUiRaylibPreferredCustomHeight(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->custom.preferred_height <= 0.0f) {
        return 96.0f;
    }
    return node->custom.preferred_height;
}

static bool EcsUiRaylibNodeIsStack(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL &&
        (node->kind == ECS_UI_NODE_ROOT ||
            node->kind == ECS_UI_NODE_VSTACK ||
            node->kind == ECS_UI_NODE_HSTACK ||
            node->kind == ECS_UI_NODE_ZSTACK);
}

static float EcsUiRaylibPreferredWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    /* Explicit GROW/FIT sizing is currently honored by the Clay adapter only. */
    if (tree == NULL || index >= tree->count) {
        return 0.0f;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiRaylibNodeIsStack(node) &&
        node->stack.preferred_width > 0.0f) {
        return node->stack.preferred_width;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM &&
        node->custom.preferred_width > 0.0f) {
        return node->custom.preferred_width;
    }
    return 0.0f;
}

static float EcsUiRaylibPressableHeight(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->pressable.preferred_height <= 0.0f) {
        return 46.0f;
    }
    return node->pressable.preferred_height;
}

static float EcsUiRaylibPreferredHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    float width)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding = EcsUiRaylibClampPositive(node->stack.padding);
    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    if (EcsUiRaylibNodeIsStack(node) &&
        node->stack.preferred_height > 0.0f) {
        return node->stack.preferred_height;
    }

    switch (node->kind) {
    case ECS_UI_NODE_TEXT:
        return EcsUiRaylibTextSize(node->text.role) + 8.0f;
    case ECS_UI_NODE_ICON:
        return 24.0f;
    case ECS_UI_NODE_BUTTON:
        return 46.0f;
    case ECS_UI_NODE_PRESSABLE:
        return EcsUiRaylibPressableHeight(node);
    case ECS_UI_NODE_CUSTOM:
        return EcsUiRaylibPreferredCustomHeight(node);
    case ECS_UI_NODE_HSTACK: {
        float height = 0.0f;
        uint32_t child_count = EcsUiRaylibChildCount(tree, index);
        const float child_width =
            child_count > 0u ? width / (float)child_count : width;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            height = EcsUiRaylibMaxFloat(
                height,
                EcsUiRaylibPreferredHeight(tree, child, child_width));
            child = tree->nodes[child].next_sibling;
        }
        return padding * 2.0f + EcsUiRaylibMaxFloat(height, 44.0f);
    }
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_ZSTACK: {
        float height = padding * 2.0f;
        uint32_t child_count = 0u;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            if (child_count > 0u && node->kind != ECS_UI_NODE_ZSTACK) {
                height += gap;
            }
            height += EcsUiRaylibPreferredHeight(
                tree,
                child,
                width - (padding * 2.0f));
            child_count += 1u;
            child = tree->nodes[child].next_sibling;
        }
        return EcsUiRaylibMaxFloat(height, 44.0f);
    }
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }
}

static Color EcsUiRaylibButtonColor(
    const EcsUiTheme *theme,
    EcsUiButton button)
{
    if (button.disabled) {
        return EcsUiRaylibColor(theme->button_disabled);
    }

    switch (button.variant) {
    case ECS_UI_BUTTON_PRIMARY:
        return EcsUiRaylibColor(theme->button_primary);
    case ECS_UI_BUTTON_SUBTLE:
        return EcsUiRaylibColor(theme->button_subtle);
    case ECS_UI_BUTTON_DANGER:
        return EcsUiRaylibColor(theme->button_danger);
    case ECS_UI_BUTTON_DEFAULT:
    default:
        return EcsUiRaylibColor(theme->button);
    }
}

static Color EcsUiRaylibTextColor(
    const EcsUiTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled)
{
    if (has_text_style) {
        if (disabled && text_style.disabled_color.a != 0u) {
            return EcsUiRaylibColor(text_style.disabled_color);
        }
        if (role == ECS_UI_TEXT_CAPTION && text_style.muted_color.a != 0u) {
            return EcsUiRaylibColor(text_style.muted_color);
        }
        if (text_style.color.a != 0u) {
            return EcsUiRaylibColor(text_style.color);
        }
    }
    if (inverse) {
        return EcsUiRaylibColor(theme->text_inverse);
    }
    if (role == ECS_UI_TEXT_CAPTION) {
        return EcsUiRaylibColor(theme->text_muted);
    }
    return EcsUiRaylibColor(theme->text);
}

static float EcsUiRaylibBoxRadius(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiTheme *theme)
{
    if (node != NULL && node->has_box_style && node->box_style.radius > 0.0f) {
        return node->box_style.radius;
    }
    return theme != NULL ? theme->radius : 0.0f;
}

static float EcsUiRaylibBoxPadding(
    const EcsUiTreeNodeSnapshot *node,
    float fallback)
{
    if (node != NULL && node->has_box_style && node->box_style.padding > 0.0f) {
        return node->box_style.padding;
    }
    return fallback;
}

static Color EcsUiRaylibStackColor(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || !node->has_box_style ||
        node->box_style.background.a == 0u) {
        return (Color){0};
    }
    return EcsUiRaylibColor(node->box_style.background);
}

static void EcsUiRaylibDrawStackBackground(
    Rectangle bounds,
    const EcsUiTreeNodeSnapshot *node,
    float opacity)
{
    Color fill = EcsUiRaylibStackColor(node);
    if (fill.a == 0u) {
        return;
    }

    const float radius =
        node != NULL && node->has_box_style ? node->box_style.radius : 0.0f;
    DrawRectangleRounded(
        bounds,
        radius,
        8,
        EcsUiRaylibApplyOpacity(fill, opacity));
}

static void EcsUiRaylibDrawBoxBorder(
    Rectangle bounds,
    const EcsUiTreeNodeSnapshot *node,
    float opacity)
{
    if (node == NULL || !node->has_box_style ||
        node->box_style.border_width <= 0.0f ||
        node->box_style.border_color.a == 0u ||
        bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }

    float width = EcsUiRaylibClampPositive(node->box_style.border_width);
    if (width <= 0.0f) {
        return;
    }
    if (width > bounds.width) {
        width = bounds.width;
    }
    if (width > bounds.height) {
        width = bounds.height;
    }

    Color color = EcsUiRaylibApplyOpacity(
        EcsUiRaylibColor(node->box_style.border_color),
        opacity);
    DrawRectangleRec(
        (Rectangle){bounds.x, bounds.y, width, bounds.height},
        color);
    DrawRectangleRec(
        (Rectangle){
            bounds.x + bounds.width - width,
            bounds.y,
            width,
            bounds.height,
        },
        color);
    DrawRectangleRec(
        (Rectangle){bounds.x, bounds.y, bounds.width, width},
        color);
    DrawRectangleRec(
        (Rectangle){
            bounds.x,
            bounds.y + bounds.height - width,
            bounds.width,
            width,
        },
        color);
}

static Color EcsUiRaylibPressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node,
    bool hovered)
{
    Color fill = EcsUiRaylibColor(theme->button_subtle);
    if (node != NULL && node->has_box_style) {
        fill = EcsUiRaylibColor(node->box_style.background);
        if (node->pressable.disabled) {
            fill = EcsUiRaylibStyleColorOr(
                node->box_style.disabled_background,
                fill);
        } else if (hovered) {
            fill = EcsUiRaylibStyleColorOr(
                node->box_style.hover_background,
                fill);
        }
        Color highlight = EcsUiRaylibStyleColorOr(
            node->box_style.highlight_background,
            (Color){255, 255, 255, fill.a});
        return EcsUiRaylibLerpColor(
            fill,
            highlight,
            EcsUiRaylibClamp01(node->visual.highlight));
    }

    if (hovered) {
        fill = ColorAlpha(fill, 0.86f);
    }
    return EcsUiRaylibLerpColor(
        fill,
        (Color){255, 255, 255, fill.a},
        EcsUiRaylibClamp01(node->visual.highlight) * 0.42f);
}

static float EcsUiRaylibAlignedPosition(
    float start,
    float available,
    EcsUiAlign align)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return start + (available * 0.5f);
    case ECS_UI_ALIGN_END:
        return start + available;
    case ECS_UI_ALIGN_START:
    default:
        return start;
    }
}

static EcsUiTextLayout EcsUiRaylibDefaultTextLayout(void)
{
    return (EcsUiTextLayout){
        .align_x = ECS_UI_ALIGN_START,
        .align_y = ECS_UI_ALIGN_CENTER,
    };
}

static void EcsUiRaylibDrawTextLine(
    const char *text,
    Rectangle bounds,
    float font_size,
    EcsUiTextLayout layout,
    Color color)
{
    const char *value = text != NULL ? text : "";
    Vector2 text_size = MeasureTextEx(EcsUiRaylibActiveFont(), value, font_size, 1.0f);
    const float extra_x = bounds.width - text_size.x;
    const float extra_y = bounds.height - text_size.y;
    Vector2 position = {
        .x = EcsUiRaylibAlignedPosition(
            bounds.x,
            extra_x > 0.0f ? extra_x : 0.0f,
            layout.align_x),
        .y = EcsUiRaylibAlignedPosition(
            bounds.y,
            extra_y > 0.0f ? extra_y : 0.0f,
            layout.align_y),
    };
    DrawTextEx(EcsUiRaylibActiveFont(), value, position, font_size, 1.0f, color);
}

static Vector2 EcsUiRaylibNaturalNodeSize(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle fallback)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    switch (node->kind) {
    case ECS_UI_NODE_TEXT: {
        const float font_size = EcsUiRaylibTextSize(node->text.role);
        Vector2 text_size = MeasureTextEx(
            EcsUiRaylibActiveFont(),
            node->text.text,
            font_size,
            1.0f);
        return (Vector2){
            .x = text_size.x,
            .y = font_size + 8.0f,
        };
    }
    case ECS_UI_NODE_ICON: {
        Vector2 text_size = MeasureTextEx(
            EcsUiRaylibActiveFont(),
            node->icon.name,
            18.0f,
            1.0f);
        return (Vector2){
            .x = text_size.x,
            .y = 24.0f,
        };
    }
    case ECS_UI_NODE_CUSTOM:
        return (Vector2){
            .x = node->custom.preferred_width > 0.0f ?
                node->custom.preferred_width :
                fallback.width,
            .y = EcsUiRaylibPreferredCustomHeight(node),
        };
    case ECS_UI_NODE_BUTTON:
        return (Vector2){
            .x = fallback.width,
            .y = 46.0f,
        };
    case ECS_UI_NODE_PRESSABLE:
        return (Vector2){
            .x = fallback.width,
            .y = EcsUiRaylibPressableHeight(node),
        };
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        return (Vector2){
            .x = EcsUiRaylibPreferredWidth(tree, index) > 0.0f ?
                EcsUiRaylibPreferredWidth(tree, index) :
                fallback.width,
            .y = EcsUiRaylibPreferredHeight(tree, index, fallback.width),
        };
    case ECS_UI_NODE_NONE:
    default:
        return (Vector2){
            .x = fallback.width,
            .y = fallback.height,
        };
    }
}

static Rectangle EcsUiRaylibPlacedChildBounds(
    const EcsUiTreeSnapshot *tree,
    uint32_t child,
    Rectangle bounds)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[child];
    if (!node->has_placement) {
        return bounds;
    }

    Vector2 natural = EcsUiRaylibNaturalNodeSize(tree, child, bounds);
    const EcsUiPlacement *placement = &node->placement;
    const float width =
        placement->width > 0.0f ? placement->width : natural.x;
    const float height =
        placement->height > 0.0f ? placement->height : natural.y;
    const float parent_x = EcsUiRaylibAlignedPosition(
        bounds.x,
        bounds.width,
        placement->parent_x);
    const float parent_y = EcsUiRaylibAlignedPosition(
        bounds.y,
        bounds.height,
        placement->parent_y);
    const float child_x =
        EcsUiRaylibAlignedPosition(0.0f, width, placement->child_x);
    const float child_y =
        EcsUiRaylibAlignedPosition(0.0f, height, placement->child_y);

    return (Rectangle){
        .x = parent_x - child_x + placement->offset_x,
        .y = parent_y - child_y + placement->offset_y,
        .width = EcsUiRaylibClampPositive(width),
        .height = EcsUiRaylibClampPositive(height),
    };
}

static void EcsUiRaylibDrawTextFieldView(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *field_node,
    Rectangle bounds,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    if (tree == NULL || theme == NULL || field_node == NULL ||
        !field_node->has_text_field_view) {
        return;
    }

    const uint32_t value_index = EcsUiRaylibFindNodeIndex(
        tree,
        field_node->text_field_view.value_node);
    if (value_index == ECS_UI_TREE_INVALID_INDEX ||
        tree->nodes[value_index].kind != ECS_UI_NODE_TEXT) {
        return;
    }

    const EcsUiTreeNodeSnapshot *value_node = &tree->nodes[value_index];
    EcsUiTextStyle value_text_style = text_style;
    bool value_has_text_style = has_text_style;
    if (value_node->has_text_style) {
        value_text_style = value_node->text_style;
        value_has_text_style = true;
    }

    const float font_size = EcsUiRaylibTextSize(value_node->text.role);
    const Color text_color = EcsUiRaylibApplyOpacity(
        EcsUiRaylibTextColor(
            theme,
            value_node->text.role,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || field_node->text_field_view.disabled),
        opacity);
    const char *text = value_node->text.text;
    const size_t length = strlen(text);
    const uint32_t cursor = EcsUiRaylibClampTextIndex(
        field_node->text_field_view.cursor,
        length);
    char prefix[ECS_UI_TEXT_MAX] = {0};
    EcsUiRaylibCopyTextRange(
        prefix,
        sizeof(prefix),
        text,
        0u,
        cursor);
    const Vector2 prefix_size =
        MeasureTextEx(EcsUiRaylibActiveFont(), prefix, font_size, 1.0f);
    const Vector2 all_size =
        MeasureTextEx(EcsUiRaylibActiveFont(), text, font_size, 1.0f);
    const Vector2 position = {
        .x = bounds.x,
        .y = bounds.y + ((bounds.height - all_size.y) * 0.5f),
    };

    uint32_t selection_start = EcsUiRaylibClampTextIndex(
        field_node->text_field_view.selection_anchor,
        length);
    uint32_t selection_end = EcsUiRaylibClampTextIndex(
        field_node->text_field_view.selection_focus,
        length);
    if (selection_start > selection_end) {
        uint32_t swap = selection_start;
        selection_start = selection_end;
        selection_end = swap;
    }
    if (field_node->text_field_view.focused &&
        selection_start < selection_end) {
        char before_selection[ECS_UI_TEXT_MAX] = {0};
        char selected[ECS_UI_TEXT_MAX] = {0};
        EcsUiRaylibCopyTextRange(
            before_selection,
            sizeof(before_selection),
            text,
            0u,
            selection_start);
        EcsUiRaylibCopyTextRange(
            selected,
            sizeof(selected),
            text,
            selection_start,
            selection_end);
        const Vector2 before_size =
            MeasureTextEx(EcsUiRaylibActiveFont(), before_selection, font_size, 1.0f);
        const Vector2 selected_size =
            MeasureTextEx(EcsUiRaylibActiveFont(), selected, font_size, 1.0f);
        DrawRectangleRec(
            (Rectangle){
                .x = position.x + before_size.x,
                .y = position.y,
                .width = selected_size.x,
                .height = all_size.y,
            },
            EcsUiRaylibApplyOpacity(
                ColorAlpha(EcsUiRaylibColor(theme->button_primary), 0.35f),
                opacity));
    }

    DrawTextEx(EcsUiRaylibActiveFont(), text, position, font_size, 1.0f, text_color);
    if (field_node->text_field_view.focused) {
        const float caret_width =
            field_node->text_field_view.caret_width > 0.0f ?
                field_node->text_field_view.caret_width :
                2.0f;
        DrawRectangleRec(
            (Rectangle){
                .x = position.x + prefix_size.x,
                .y = position.y,
                .width = caret_width,
                .height = all_size.y,
            },
            text_color);
    }
}

static void EcsUiRaylibDrawNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity);

static void EcsUiRaylibDrawChildrenVertical(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    float y = bounds.y;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_height =
            EcsUiRaylibPreferredHeight(tree, child, bounds.width);
        const float remaining = bounds.y + bounds.height - y;
        if (remaining <= 0.0f) {
            return;
        }
        Rectangle child_bounds = {
            .x = bounds.x,
            .y = y,
            .width = bounds.width,
            .height = preferred_height < remaining ? preferred_height : remaining,
        };
        EcsUiRaylibDrawNode(
            tree,
            theme,
            options,
            child,
            child_bounds,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity);
        y += child_bounds.height + gap;
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiRaylibDrawChildrenHorizontal(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const uint32_t child_count = EcsUiRaylibChildCount(tree, index);
    if (child_count == 0u) {
        return;
    }

    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    const float total_gap = gap * (float)(child_count - 1u);
    float fixed_width = 0.0f;
    uint32_t flexible_count = 0u;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_width = EcsUiRaylibPreferredWidth(tree, child);
        if (preferred_width > 0.0f) {
            fixed_width += preferred_width;
        } else {
            flexible_count += 1u;
        }
        child = tree->nodes[child].next_sibling;
    }
    const float flexible_width = flexible_count > 0u ?
        EcsUiRaylibClampPositive(bounds.width - total_gap - fixed_width) /
            (float)flexible_count :
        0.0f;
    float x = bounds.x;

    child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_width = EcsUiRaylibPreferredWidth(tree, child);
        const float child_width =
            preferred_width > 0.0f ? preferred_width : flexible_width;
        Rectangle child_bounds = {
            .x = x,
            .y = bounds.y,
            .width = child_width,
            .height = bounds.height,
        };
        EcsUiRaylibDrawNode(
            tree,
            theme,
            options,
            child,
            child_bounds,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity);
        x += child_width + gap;
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiRaylibDrawNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float node_opacity =
        opacity * EcsUiRaylibClamp01(node->visual.opacity);
    if (node_opacity <= 0.01f) {
        return;
    }

    Rectangle node_bounds = EcsUiRaylibOffset(bounds, node->visual);
    EcsUiTextStyle node_text_style = text_style;
    bool node_has_text_style = has_text_style;
    if (node->has_text_style) {
        node_text_style = node->text_style;
        node_has_text_style = true;
    }

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
        DrawRectangleRec(
            node_bounds,
            EcsUiRaylibApplyOpacity(
                EcsUiRaylibColor(theme->root_background),
                node_opacity));
        EcsUiRaylibDrawChildrenVertical(
            tree,
            theme,
            options,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            node_opacity);
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    case ECS_UI_NODE_VSTACK:
        EcsUiRaylibDrawStackBackground(node_bounds, node, node_opacity);
        EcsUiRaylibDrawChildrenVertical(
            tree,
            theme,
            options,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            node_opacity);
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    case ECS_UI_NODE_HSTACK:
        EcsUiRaylibDrawStackBackground(node_bounds, node, node_opacity);
        EcsUiRaylibDrawChildrenHorizontal(
            tree,
            theme,
            options,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            node_opacity);
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    case ECS_UI_NODE_ZSTACK: {
        EcsUiRaylibDrawStackBackground(node_bounds, node, node_opacity);
        Rectangle inner = EcsUiRaylibInset(node_bounds, node->stack.padding);
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            Rectangle child_bounds =
                EcsUiRaylibPlacedChildBounds(tree, child, inner);
            EcsUiRaylibDrawNode(
                tree,
                theme,
                options,
                child,
                child_bounds,
                inverse_text,
                node_text_style,
                node_has_text_style,
                text_disabled,
                node_opacity);
            child = tree->nodes[child].next_sibling;
        }
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    }
    case ECS_UI_NODE_BUTTON: {
        const bool hovered =
            !node->button.disabled &&
            CheckCollisionPointRec(GetMousePosition(), node_bounds);
        Color fill = EcsUiRaylibButtonColor(theme, node->button);
        if (hovered) {
            fill = ColorAlpha(fill, 0.86f);
        }
        fill = EcsUiRaylibLerpColor(
            fill,
            (Color){255, 255, 255, fill.a},
            EcsUiRaylibClamp01(node->visual.highlight) * 0.42f);
        DrawRectangleRounded(
            node_bounds,
            theme->radius,
            8,
            EcsUiRaylibApplyOpacity(fill, node_opacity));
        Rectangle inner = EcsUiRaylibInset(node_bounds, 12.0f);
        EcsUiRaylibDrawChildrenHorizontal(
            tree,
            theme,
            options,
            index,
            inner,
            node->button.variant == ECS_UI_BUTTON_PRIMARY,
            node_text_style,
            node_has_text_style,
            text_disabled || node->button.disabled,
            node_opacity);
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    }
    case ECS_UI_NODE_PRESSABLE: {
        const bool hovered =
            !node->pressable.disabled &&
            CheckCollisionPointRec(GetMousePosition(), node_bounds);
        Color fill = EcsUiRaylibPressableColor(theme, node, hovered);
        DrawRectangleRounded(
            node_bounds,
            EcsUiRaylibBoxRadius(node, theme),
            8,
            EcsUiRaylibApplyOpacity(fill, node_opacity));
        Rectangle inner =
            EcsUiRaylibInset(node_bounds, EcsUiRaylibBoxPadding(node, 12.0f));
        if (node->has_text_field_view) {
            EcsUiRaylibDrawTextFieldView(
                tree,
                theme,
                node,
                inner,
                false,
                node_text_style,
                node_has_text_style,
                text_disabled || node->pressable.disabled,
                node_opacity);
        } else {
            EcsUiRaylibDrawChildrenHorizontal(
                tree,
                theme,
                options,
                index,
                inner,
                false,
                node_text_style,
                node_has_text_style,
                text_disabled || node->pressable.disabled,
                node_opacity);
        }
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    }
    case ECS_UI_NODE_TEXT:
        EcsUiRaylibDrawTextLine(
            node->text.text,
            node_bounds,
            EcsUiRaylibTextSize(node->text.role),
            node->has_text_layout ?
                node->text_layout :
                EcsUiRaylibDefaultTextLayout(),
            EcsUiRaylibApplyOpacity(
                EcsUiRaylibTextColor(
                    theme,
                    node->text.role,
                    inverse_text,
                    node_text_style,
                    node_has_text_style,
                    text_disabled),
                node_opacity));
        break;
    case ECS_UI_NODE_ICON:
        EcsUiRaylibDrawTextLine(
            node->icon.name,
            node_bounds,
            18.0f,
            EcsUiRaylibDefaultTextLayout(),
            EcsUiRaylibApplyOpacity(
                EcsUiRaylibTextColor(
                    theme,
                    ECS_UI_TEXT_LABEL,
                    inverse_text,
                    node_text_style,
                    node_has_text_style,
                    text_disabled),
                node_opacity));
        break;
    case ECS_UI_NODE_CUSTOM:
        if (options != NULL && options->custom_draw != NULL) {
            options->custom_draw(
                node,
                node_bounds,
                node_opacity,
                options->user_data);
        } else {
            DrawRectangleRounded(
                node_bounds,
                theme->radius,
                8,
                EcsUiRaylibApplyOpacity(
                    EcsUiRaylibColor(theme->surface_subtle),
                    node_opacity));
            DrawRectangleRoundedLines(
                node_bounds,
                theme->radius,
                8,
                EcsUiRaylibApplyOpacity(
                    EcsUiRaylibColor(theme->text_muted),
                    node_opacity));
            EcsUiRaylibDrawTextLine(
                node->custom.kind,
                EcsUiRaylibInset(node_bounds, 12.0f),
                EcsUiRaylibTextSize(ECS_UI_TEXT_CAPTION),
                EcsUiRaylibDefaultTextLayout(),
                EcsUiRaylibApplyOpacity(
                    EcsUiRaylibColor(theme->text_muted),
                    node_opacity));
        }
        EcsUiRaylibDrawBoxBorder(node_bounds, node, node_opacity);
        break;
    case ECS_UI_NODE_NONE:
    default:
        break;
    }
}

typedef struct EcsUiRaylibHit {
    bool found;
    uint32_t index;
    Rectangle bounds;
} EcsUiRaylibHit;

static bool EcsUiRaylibHitNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit);

static bool EcsUiRaylibHitSet(
    EcsUiRaylibHit *hit,
    uint32_t index,
    Rectangle bounds)
{
    if (hit == NULL) {
        return false;
    }
    hit->found = true;
    hit->index = index;
    hit->bounds = bounds;
    return true;
}

static bool EcsUiRaylibHitTestDisabled(
    const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->hit_test.mode == ECS_UI_HIT_TEST_NONE;
}

static bool EcsUiRaylibHitTestCapturesSelf(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (node->hit_test.mode == ECS_UI_HIT_TEST_CAPTURE) {
        return true;
    }
    if (node->hit_test.mode == ECS_UI_HIT_TEST_CHILDREN ||
        node->hit_test.mode == ECS_UI_HIT_TEST_NONE) {
        return false;
    }

    switch (node->kind) {
    case ECS_UI_NODE_BUTTON:
        return !node->button.disabled;
    case ECS_UI_NODE_PRESSABLE:
        return !node->pressable.disabled;
    case ECS_UI_NODE_CUSTOM:
        return node->on_click != 0;
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return false;
    }
}

static bool EcsUiRaylibHitSelf(
    const EcsUiTreeNodeSnapshot *node,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    if (!EcsUiRaylibHitTestCapturesSelf(node) ||
        !CheckCollisionPointRec(point, bounds)) {
        return false;
    }
    return EcsUiRaylibHitSet(hit, index, bounds);
}

static bool EcsUiRaylibHitChildrenVertical(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    float y = bounds.y;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_height =
            EcsUiRaylibPreferredHeight(tree, child, bounds.width);
        const float remaining = bounds.y + bounds.height - y;
        if (remaining <= 0.0f) {
            return false;
        }
        Rectangle child_bounds = {
            .x = bounds.x,
            .y = y,
            .width = bounds.width,
            .height = preferred_height < remaining ? preferred_height : remaining,
        };
        if (EcsUiRaylibHitNode(tree, child, child_bounds, point, hit)) {
            return true;
        }
        y += child_bounds.height + gap;
        child = tree->nodes[child].next_sibling;
    }
    return false;
}

static bool EcsUiRaylibHitChildrenHorizontal(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const uint32_t child_count = EcsUiRaylibChildCount(tree, index);
    if (child_count == 0u) {
        return false;
    }

    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    const float total_gap = gap * (float)(child_count - 1u);
    float fixed_width = 0.0f;
    uint32_t flexible_count = 0u;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_width = EcsUiRaylibPreferredWidth(tree, child);
        if (preferred_width > 0.0f) {
            fixed_width += preferred_width;
        } else {
            flexible_count += 1u;
        }
        child = tree->nodes[child].next_sibling;
    }
    const float flexible_width = flexible_count > 0u ?
        EcsUiRaylibClampPositive(bounds.width - total_gap - fixed_width) /
            (float)flexible_count :
        0.0f;
    float x = bounds.x;

    child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_width = EcsUiRaylibPreferredWidth(tree, child);
        const float child_width =
            preferred_width > 0.0f ? preferred_width : flexible_width;
        Rectangle child_bounds = {
            .x = x,
            .y = bounds.y,
            .width = child_width,
            .height = bounds.height,
        };
        if (EcsUiRaylibHitNode(tree, child, child_bounds, point, hit)) {
            return true;
        }
        x += child_width + gap;
        child = tree->nodes[child].next_sibling;
    }
    return false;
}

static bool EcsUiRaylibHitChildrenZStack(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    uint32_t children[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t child_count = 0u;
    uint32_t child = tree->nodes[index].first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX &&
           child_count < ECS_UI_TREE_NODE_MAX) {
        children[child_count] = child;
        child_count += 1u;
        child = tree->nodes[child].next_sibling;
    }

    for (uint32_t i = child_count; i > 0u; i -= 1u) {
        const uint32_t child_index = children[i - 1u];
        Rectangle child_bounds =
            EcsUiRaylibPlacedChildBounds(tree, child_index, bounds);
        if (EcsUiRaylibHitNode(
                tree,
                child_index,
                child_bounds,
                point,
                hit)) {
            return true;
        }
    }
    return false;
}

static bool EcsUiRaylibHitNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Rectangle node_bounds = EcsUiRaylibOffset(bounds, node->visual);
    if (EcsUiRaylibClamp01(node->visual.opacity) <= 0.01f ||
        EcsUiRaylibHitTestDisabled(node)) {
        return false;
    }

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
        if (EcsUiRaylibHitChildrenVertical(
            tree,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            point,
            hit)) {
            return true;
        }
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    case ECS_UI_NODE_HSTACK:
        if (EcsUiRaylibHitChildrenHorizontal(
            tree,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            point,
            hit)) {
            return true;
        }
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    case ECS_UI_NODE_ZSTACK: {
        Rectangle inner = EcsUiRaylibInset(node_bounds, node->stack.padding);
        if (EcsUiRaylibHitChildrenZStack(tree, index, inner, point, hit)) {
            return true;
        }
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    }
    case ECS_UI_NODE_BUTTON:
        if (EcsUiRaylibHitChildrenHorizontal(
            tree,
            index,
            EcsUiRaylibInset(
                node_bounds,
                EcsUiRaylibBoxPadding(node, 12.0f)),
            point,
            hit)) {
            return true;
        }
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    case ECS_UI_NODE_PRESSABLE:
        if (EcsUiRaylibHitChildrenHorizontal(
            tree,
            index,
            EcsUiRaylibInset(node_bounds, 12.0f),
            point,
            hit)) {
            return true;
        }
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    case ECS_UI_NODE_CUSTOM:
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return EcsUiRaylibHitSelf(node, index, node_bounds, point, hit);
    }
    return false;
}

static void EcsUiRaylibPushPointerEvent(
    EcsUiEventList *events,
    const EcsUiTreeNodeSnapshot *node,
    EcsUiEventType type,
    Vector2 point)
{
    if (events == NULL || node == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .node = node->entity,
        .action = node->on_click,
        .x = point.x,
        .y = point.y,
        .start_x = point.x,
        .start_y = point.y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", node->id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibStartPointerCapture(
    const EcsUiTreeNodeSnapshot *node,
    Vector2 point)
{
    if (node == NULL) {
        return;
    }

    g_ecs_ui_raylib_pointer_capture = (EcsUiRaylibPointerCapture){
        .active = true,
        .node = node->entity,
        .action = node->on_click,
        .start = point,
        .start_time = GetTime(),
    };
    (void)snprintf(
        g_ecs_ui_raylib_pointer_capture.node_id,
        sizeof(g_ecs_ui_raylib_pointer_capture.node_id),
        "%s",
        node->id);
}

static void EcsUiRaylibPushCapturedPointerEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    Vector2 point)
{
    const EcsUiRaylibPointerCapture *capture =
        &g_ecs_ui_raylib_pointer_capture;
    if (events == NULL || !capture->active) {
        return;
    }

    const float elapsed =
        (float)EcsUiRaylibMaxFloat((float)(GetTime() - capture->start_time), 0.001f);
    const float delta_x = point.x - capture->start.x;
    const float delta_y = point.y - capture->start.y;
    EcsUiEvent event = {
        .type = type,
        .node = capture->node,
        .action = capture->action,
        .x = point.x,
        .y = point.y,
        .start_x = capture->start.x,
        .start_y = capture->start.y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .elapsed = elapsed,
        .velocity_x = delta_x / elapsed,
        .velocity_y = delta_y / elapsed,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", capture->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibPushKeyboardEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    uint32_t codepoint)
{
    if (events == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .codepoint = codepoint,
    };
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibPushKeyboardTextEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    const char *text)
{
    if (events == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
    };
    const char *value = text != NULL ? text : "";
    (void)snprintf(event.text, sizeof(event.text), "%s", value);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibCollectKeyboardEvents(EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }

    const bool shortcut_down =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    for (int key = GetCharPressed(); key > 0; key = GetCharPressed()) {
        if (!shortcut_down) {
            EcsUiRaylibPushKeyboardEvent(
                events,
                ECS_UI_EVENT_TEXT_INPUT,
                (uint32_t)key);
        }
    }
    if (shortcut_down && IsKeyPressed(KEY_C)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_COPY, 0u);
    }
    if (shortcut_down && IsKeyPressed(KEY_X)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_CUT, 0u);
    }
    if (shortcut_down && IsKeyPressed(KEY_V)) {
        EcsUiRaylibPushKeyboardTextEvent(
            events,
            ECS_UI_EVENT_TEXT_PASTE,
            GetClipboardText());
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_DELETE, 0u);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_SUBMIT, 0u);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_CANCEL, 0u);
    }
    const bool shift_down =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_TAB)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_FOCUS_PREVIOUS :
                ECS_UI_EVENT_TEXT_FOCUS_NEXT,
            0u);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_LEFT :
                ECS_UI_EVENT_TEXT_CURSOR_LEFT,
            0u);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_RIGHT :
                ECS_UI_EVENT_TEXT_CURSOR_RIGHT,
            0u);
    }
    if (IsKeyPressed(KEY_HOME)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_START :
                ECS_UI_EVENT_TEXT_CURSOR_START,
            0u);
    }
    if (IsKeyPressed(KEY_END)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_END :
                ECS_UI_EVENT_TEXT_CURSOR_END,
            0u);
    }
}

static void EcsUiRaylibCollectPointerEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events)
{
    if (tree == NULL || tree->count == 0u || events == NULL) {
        return;
    }

    const Vector2 point = GetMousePosition();
    if (g_ecs_ui_raylib_pointer_capture.active) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            EcsUiRaylibPushCapturedPointerEvent(
                events,
                ECS_UI_EVENT_DRAGGED,
                point);
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            const bool did_drag =
                EcsUiRaylibDistanceSquared(
                    g_ecs_ui_raylib_pointer_capture.start,
                    point) > 36.0f;
            EcsUiRaylibPushCapturedPointerEvent(
                events,
                ECS_UI_EVENT_DRAG_ENDED,
                point);
            if (!did_drag) {
                EcsUiRaylibPushCapturedPointerEvent(
                    events,
                    ECS_UI_EVENT_CLICKED,
                    point);
            }
            g_ecs_ui_raylib_pointer_capture =
                (EcsUiRaylibPointerCapture){0};
        }
        return;
    }

    EcsUiRaylibHit hit = {0};
    EcsUiRaylibHitNode(tree, 0u, bounds, point, &hit);
    if (!hit.found || hit.index >= tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[hit.index];
    EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_HOVERED, point);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_PRESSED, point);
        EcsUiRaylibStartPointerCapture(node, point);
        EcsUiRaylibPushCapturedPointerEvent(
            events,
            ECS_UI_EVENT_DRAG_STARTED,
            point);
    }
}

void EcsUiRaylibDrawTree(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme)
{
    EcsUiRaylibDrawTreeEx(tree, bounds, theme, NULL);
}

void EcsUiRaylibDrawTreeEx(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options)
{
    if (tree == NULL || tree->count == 0u || theme == NULL) {
        return;
    }

    EcsUiRaylibDrawNode(
        tree,
        theme,
        options,
        0u,
        bounds,
        false,
        (EcsUiTextStyle){0},
        false,
        false,
        1.0f);
}

void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events)
{
    EcsUiEventListClear(events);
    if (events == NULL) {
        return;
    }

    EcsUiRaylibCollectPointerEvents(tree, bounds, events);
    EcsUiRaylibCollectKeyboardEvents(events);
}
