#include "ecs_ui/ecs_ui_clay.h"

#include <stdio.h>
#include <string.h>

typedef struct EcsUiClayRect {
    float x;
    float y;
    float width;
    float height;
} EcsUiClayRect;

typedef struct EcsUiClayHit {
    bool found;
    uint32_t index;
    EcsUiClayRect bounds;
} EcsUiClayHit;

typedef struct EcsUiClayPointerCapture {
    bool active;
    ecs_entity_t node;
    ecs_entity_t action;
    char node_id[ECS_UI_ID_MAX];
    float start_x;
    float start_y;
    double start_time;
} EcsUiClayPointerCapture;

static EcsUiClayPointerCapture g_ecs_ui_clay_pointer_capture;

static Clay_String EcsUiClayString(const char *text)
{
    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = text != NULL ? (int32_t)strlen(text) : 0,
        .chars = text != NULL ? text : "",
    };
}

static void EcsUiClayElementId(
    const EcsUiTreeNodeSnapshot *node,
    const char *suffix,
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
        "%s_%llu%s",
        authored_id,
        (unsigned long long)node->entity,
        suffix != NULL ? suffix : "");
}

static float EcsUiClayMaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static float EcsUiClayClampPositive(float value)
{
    return value > 0.0f ? value : 0.0f;
}

static float EcsUiClayClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static bool EcsUiClayHasOffset(EcsUiVisual visual)
{
    return visual.offset_x < -0.01f || visual.offset_x > 0.01f ||
        visual.offset_y < -0.01f || visual.offset_y > 0.01f;
}

static uint16_t EcsUiClayU16(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= 65535.0f) {
        return 65535u;
    }
    return (uint16_t)value;
}

static Clay_Color EcsUiClayApplyOpacity(Clay_Color color, float opacity)
{
    color.a *= EcsUiClayClamp01(opacity);
    return color;
}

static Clay_Color EcsUiClayColor(EcsUiColor color)
{
    return (Clay_Color){
        .r = (float)color.r,
        .g = (float)color.g,
        .b = (float)color.b,
        .a = (float)color.a,
    };
}

static Clay_Color EcsUiClayLerpColor(
    Clay_Color from,
    Clay_Color to,
    float amount)
{
    const float t = EcsUiClayClamp01(amount);
    return (Clay_Color){
        .r = from.r + ((to.r - from.r) * t),
        .g = from.g + ((to.g - from.g) * t),
        .b = from.b + ((to.b - from.b) * t),
        .a = from.a + ((to.a - from.a) * t),
    };
}

static Clay_Color EcsUiClayStyleColorOr(
    EcsUiColor color,
    Clay_Color fallback)
{
    return color.a != 0u ? EcsUiClayColor(color) : fallback;
}

static float EcsUiClayTextSize(EcsUiTextRole role)
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

static Clay_Color EcsUiClayTextColor(
    const EcsUiClayTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled)
{
    if (has_text_style) {
        if (disabled && text_style.disabled_color.a != 0u) {
            return EcsUiClayColor(text_style.disabled_color);
        }
        if (role == ECS_UI_TEXT_CAPTION && text_style.muted_color.a != 0u) {
            return EcsUiClayColor(text_style.muted_color);
        }
        if (text_style.color.a != 0u) {
            return EcsUiClayColor(text_style.color);
        }
    }
    if (inverse) {
        return theme->text_inverse;
    }
    if (role == ECS_UI_TEXT_CAPTION) {
        return theme->text_muted;
    }
    return theme->text;
}

static Clay_TextElementConfig *EcsUiClayTextConfig(
    const EcsUiClayTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled,
    float opacity)
{
    return CLAY_TEXT_CONFIG({
        .textColor = EcsUiClayApplyOpacity(
            EcsUiClayTextColor(
                theme,
                role,
                inverse,
                text_style,
                has_text_style,
                disabled),
            opacity),
        .fontSize = EcsUiClayU16(EcsUiClayTextSize(role)),
        .wrapMode = CLAY_TEXT_WRAP_NONE,
    });
}

static bool EcsUiClayPointerOverNode(const EcsUiTreeNodeSnapshot *node);

static Clay_Color EcsUiClayButtonColor(
    const EcsUiClayTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node != NULL && node->button.disabled) {
        return theme->button_disabled;
    }

    Clay_Color fill = theme->button;
    switch (node != NULL ? node->button.variant : ECS_UI_BUTTON_DEFAULT) {
    case ECS_UI_BUTTON_PRIMARY:
        fill = theme->button_primary;
        break;
    case ECS_UI_BUTTON_SUBTLE:
        fill = theme->button_subtle;
        break;
    case ECS_UI_BUTTON_DANGER:
        fill = theme->button_danger;
        break;
    case ECS_UI_BUTTON_DEFAULT:
    default:
        fill = theme->button;
        break;
    }

    if (node != NULL && EcsUiClayPointerOverNode(node)) {
        fill.a *= 0.86f;
    }
    return EcsUiClayLerpColor(
        fill,
        (Clay_Color){255.0f, 255.0f, 255.0f, fill.a},
        node != NULL ? EcsUiClayClamp01(node->visual.highlight) * 0.42f : 0.0f);
}

static float EcsUiClayCustomHeight(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->custom.preferred_height <= 0.0f) {
        return 96.0f;
    }
    return node->custom.preferred_height;
}

static bool EcsUiClayNodeIsStack(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL &&
        (node->kind == ECS_UI_NODE_ROOT ||
            node->kind == ECS_UI_NODE_VSTACK ||
            node->kind == ECS_UI_NODE_HSTACK ||
            node->kind == ECS_UI_NODE_ZSTACK);
}

static float EcsUiClayPreferredWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    if (tree == NULL || index >= tree->count) {
        return 0.0f;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (EcsUiClayNodeIsStack(node) && node->stack.preferred_width > 0.0f) {
        return node->stack.preferred_width;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM &&
        node->custom.preferred_width > 0.0f) {
        return node->custom.preferred_width;
    }
    return 0.0f;
}

static float EcsUiClayPressableHeight(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->pressable.preferred_height <= 0.0f) {
        return 46.0f;
    }
    return node->pressable.preferred_height;
}

static float EcsUiClayBoxPadding(
    const EcsUiTreeNodeSnapshot *node,
    float fallback)
{
    if (node != NULL && node->has_box_style && node->box_style.padding > 0.0f) {
        return node->box_style.padding;
    }
    return fallback;
}

static Clay_Color EcsUiClayStackColor(
    const EcsUiTreeNodeSnapshot *node,
    Clay_Color fallback)
{
    if (node == NULL || !node->has_box_style) {
        return fallback;
    }
    return EcsUiClayStyleColorOr(node->box_style.background, fallback);
}

static Clay_CornerRadius EcsUiClayCornerRadius(
    const EcsUiTreeNodeSnapshot *node,
    float fallback)
{
    float radius = fallback;
    if (node != NULL && node->has_box_style && node->box_style.radius > 0.0f) {
        radius = node->box_style.radius < 1.0f ?
            node->box_style.radius * 50.0f :
            node->box_style.radius;
    }
    return (Clay_CornerRadius){
        .topLeft = radius,
        .topRight = radius,
        .bottomLeft = radius,
        .bottomRight = radius,
    };
}

static Clay_Color EcsUiClayPressableColor(
    const EcsUiClayTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    Clay_Color fill = theme->button_subtle;
    if (node == NULL || !node->has_box_style) {
        if (node != NULL && EcsUiClayPointerOverNode(node)) {
            fill.a *= 0.86f;
        }
        return EcsUiClayLerpColor(
            fill,
            (Clay_Color){255.0f, 255.0f, 255.0f, fill.a},
            node != NULL ? EcsUiClayClamp01(node->visual.highlight) * 0.42f : 0.0f);
    }

    fill = EcsUiClayColor(node->box_style.background);
    if (node->pressable.disabled) {
        fill = EcsUiClayStyleColorOr(
            node->box_style.disabled_background,
            fill);
    } else if (EcsUiClayPointerOverNode(node)) {
        fill = EcsUiClayStyleColorOr(
            node->box_style.hover_background,
            fill);
    }
    Clay_Color highlight = EcsUiClayStyleColorOr(
        node->box_style.highlight_background,
        (Clay_Color){255.0f, 255.0f, 255.0f, fill.a});
    return EcsUiClayLerpColor(
        fill,
        highlight,
        EcsUiClayClamp01(node->visual.highlight));
}

static uint32_t EcsUiClayChildCount(
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

static float EcsUiClayPreferredHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    float width)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding = EcsUiClayClampPositive(node->stack.padding);
    const float gap = EcsUiClayClampPositive(node->stack.gap);
    if (EcsUiClayNodeIsStack(node) && node->stack.preferred_height > 0.0f) {
        return node->stack.preferred_height;
    }

    switch (node->kind) {
    case ECS_UI_NODE_TEXT:
        return EcsUiClayTextSize(node->text.role) + 8.0f;
    case ECS_UI_NODE_ICON:
        return 24.0f;
    case ECS_UI_NODE_BUTTON:
        return 46.0f;
    case ECS_UI_NODE_PRESSABLE:
        return EcsUiClayPressableHeight(node);
    case ECS_UI_NODE_CUSTOM:
        return EcsUiClayCustomHeight(node);
    case ECS_UI_NODE_HSTACK: {
        float height = 0.0f;
        uint32_t child_count = EcsUiClayChildCount(tree, index);
        const float child_width =
            child_count > 0u ? width / (float)child_count : width;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            height = EcsUiClayMaxFloat(
                height,
                EcsUiClayPreferredHeight(tree, child, child_width));
            child = tree->nodes[child].next_sibling;
        }
        return padding * 2.0f + EcsUiClayMaxFloat(height, 44.0f);
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
            height += EcsUiClayPreferredHeight(
                tree,
                child,
                width - (padding * 2.0f));
            child_count += 1u;
            child = tree->nodes[child].next_sibling;
        }
        return EcsUiClayMaxFloat(height, 44.0f);
    }
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }
}

static Clay_LayoutConfig EcsUiClayFlowLayout(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Clay_SizingAxis width = CLAY_SIZING_GROW(0);
    Clay_SizingAxis height = CLAY_SIZING_GROW(0);

    switch (node->kind) {
    case ECS_UI_NODE_TEXT:
        height = CLAY_SIZING_FIXED(EcsUiClayTextSize(node->text.role) + 8.0f);
        break;
    case ECS_UI_NODE_ICON:
        height = CLAY_SIZING_FIXED(24.0f);
        break;
    case ECS_UI_NODE_BUTTON:
        height = CLAY_SIZING_FIXED(46.0f);
        break;
    case ECS_UI_NODE_PRESSABLE:
        height = CLAY_SIZING_FIXED(EcsUiClayPressableHeight(node));
        break;
    case ECS_UI_NODE_CUSTOM:
        height = CLAY_SIZING_FIXED(EcsUiClayCustomHeight(node));
        break;
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        if (node->parent_index != ECS_UI_TREE_INVALID_INDEX &&
            node->parent_index < tree->count &&
            tree->nodes[node->parent_index].kind != ECS_UI_NODE_ZSTACK) {
            height = CLAY_SIZING_FIXED(
                EcsUiClayPreferredHeight(tree, index, 0.0f));
        }
        break;
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_NONE:
    default:
        break;
    }
    if (EcsUiClayNodeIsStack(node)) {
        if (node->stack.preferred_width > 0.0f) {
            width = CLAY_SIZING_FIXED(node->stack.preferred_width);
        }
        if (node->stack.preferred_height > 0.0f) {
            height = CLAY_SIZING_FIXED(node->stack.preferred_height);
        }
    } else if (node->kind == ECS_UI_NODE_CUSTOM &&
        node->custom.preferred_width > 0.0f) {
        width = CLAY_SIZING_FIXED(node->custom.preferred_width);
    }

    return (Clay_LayoutConfig){
        .sizing = {
            .width = width,
            .height = height,
        },
    };
}

static void EcsUiClayEmitNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity);
static void EcsUiClayEmitNodeContent(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity);

static void EcsUiClayEmitChildren(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    uint32_t child = tree->nodes[index].first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        EcsUiClayEmitNode(
            tree,
            theme,
            child,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity);
        child = tree->nodes[child].next_sibling;
    }
}

static uint32_t EcsUiClayFindNodeIndex(
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

static uint32_t EcsUiClayClampTextIndex(uint32_t index, size_t length)
{
    return index <= length ? index : (uint32_t)length;
}

static Clay_String EcsUiClayStringRange(
    const char *text,
    uint32_t start,
    uint32_t end)
{
    const char *source = text != NULL ? text : "";
    const size_t length = strlen(source);
    uint32_t range_start = EcsUiClayClampTextIndex(start, length);
    uint32_t range_end = EcsUiClayClampTextIndex(end, length);
    if (range_start > range_end) {
        uint32_t swap = range_start;
        range_start = range_end;
        range_end = swap;
    }

    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = (int32_t)(range_end - range_start),
        .chars = &source[range_start],
    };
}

static void EcsUiClayEmitInlineTextRange(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiClayTheme *theme,
    uint32_t start,
    uint32_t end,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    if (node == NULL || start == end) {
        return;
    }

    CLAY_TEXT(
        EcsUiClayStringRange(node->text.text, start, end),
        EcsUiClayTextConfig(
            theme,
            node->text.role,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity));
}

static void EcsUiClayEmitTextFieldCaret(
    const EcsUiTreeNodeSnapshot *value_node,
    const EcsUiTextFieldView *view,
    const EcsUiClayTheme *theme,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    if (value_node == NULL || view == NULL || theme == NULL) {
        return;
    }

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(value_node, "_Caret", clay_id, sizeof(clay_id));
    const float caret_width =
        view->caret_width > 0.0f ?
            view->caret_width :
            2.0f;
    CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(caret_width),
                .height = CLAY_SIZING_FIXED(
                    EcsUiClayTextSize(value_node->text.role) + 8.0f),
            },
        },
        .backgroundColor = EcsUiClayApplyOpacity(
            EcsUiClayTextColor(
                theme,
                value_node->text.role,
                inverse_text,
                text_style,
                has_text_style,
                text_disabled),
            opacity),
    }) {}
}

static void EcsUiClayEmitSelectedTextRange(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiClayTheme *theme,
    uint32_t start,
    uint32_t end,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    if (node == NULL || start == end) {
        return;
    }

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, "_Selection", clay_id, sizeof(clay_id));
    Clay_Color selection = theme->button_primary;
    selection.a *= 0.35f;
    CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
        .layout = {
            .childAlignment = {
                .y = CLAY_ALIGN_Y_CENTER,
            },
        },
        .backgroundColor = EcsUiClayApplyOpacity(selection, opacity),
    }) {
        EcsUiClayEmitInlineTextRange(
            node,
            theme,
            start,
            end,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity);
    }
}

static void EcsUiClayEmitTextFieldValue(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    const EcsUiTreeNodeSnapshot *field_node,
    uint32_t value_index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    if (tree == NULL || field_node == NULL || value_index >= tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *value_node = &tree->nodes[value_index];
    EcsUiTextStyle value_text_style = text_style;
    bool value_has_text_style = has_text_style;
    if (value_node->has_text_style) {
        value_text_style = value_node->text_style;
        value_has_text_style = true;
    }

    const char *text = value_node->text.text;
    const size_t length = strlen(text);
    const uint32_t text_end = (uint32_t)length;
    const EcsUiTextFieldView *view = &field_node->text_field_view;
    const uint32_t cursor = EcsUiClayClampTextIndex(view->cursor, length);
    uint32_t selection_start =
        EcsUiClayClampTextIndex(view->selection_anchor, length);
    uint32_t selection_end =
        EcsUiClayClampTextIndex(view->selection_focus, length);
    if (selection_start > selection_end) {
        uint32_t swap = selection_start;
        selection_start = selection_end;
        selection_end = swap;
    }
    const bool has_selection =
        view->focused && selection_start < selection_end;

    if (!view->focused) {
        EcsUiClayEmitInlineTextRange(
            value_node,
            theme,
            0u,
            text_end,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || view->disabled,
            opacity);
        return;
    }

    if (!has_selection) {
        EcsUiClayEmitInlineTextRange(
            value_node,
            theme,
            0u,
            cursor,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || view->disabled,
            opacity);
        EcsUiClayEmitTextFieldCaret(
            value_node,
            view,
            theme,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || view->disabled,
            opacity);
        EcsUiClayEmitInlineTextRange(
            value_node,
            theme,
            cursor,
            text_end,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || view->disabled,
            opacity);
        return;
    }

    EcsUiClayEmitInlineTextRange(
        value_node,
        theme,
        0u,
        selection_start,
        inverse_text,
        value_text_style,
        value_has_text_style,
        text_disabled || view->disabled,
        opacity);
    if (cursor == selection_start) {
        EcsUiClayEmitTextFieldCaret(
            value_node,
            view,
            theme,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || view->disabled,
            opacity);
    }
    EcsUiClayEmitSelectedTextRange(
        value_node,
        theme,
        selection_start,
        selection_end,
        inverse_text,
        value_text_style,
        value_has_text_style,
        text_disabled || view->disabled,
        opacity);
    if (cursor != selection_start) {
        EcsUiClayEmitTextFieldCaret(
            value_node,
            view,
            theme,
            inverse_text,
            value_text_style,
            value_has_text_style,
            text_disabled || view->disabled,
            opacity);
    }
    EcsUiClayEmitInlineTextRange(
        value_node,
        theme,
        selection_end,
        text_end,
        inverse_text,
        value_text_style,
        value_has_text_style,
        text_disabled || view->disabled,
        opacity);
}

static void EcsUiClayEmitTextFieldChildren(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const uint32_t value_index = EcsUiClayFindNodeIndex(
        tree,
        node->text_field_view.value_node);
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        if (child == value_index &&
            tree->nodes[child].kind == ECS_UI_NODE_TEXT) {
            EcsUiClayEmitTextFieldValue(
                tree,
                theme,
                node,
                child,
                inverse_text,
                text_style,
                has_text_style,
                text_disabled,
                opacity);
        } else {
            EcsUiClayEmitNode(
                tree,
                theme,
                child,
                inverse_text,
                text_style,
                has_text_style,
                text_disabled,
                opacity);
        }
        child = tree->nodes[child].next_sibling;
    }
}

static bool EcsUiClayNodeCapturesSelf(
    const EcsUiTreeNodeSnapshot *node);

static void EcsUiClayEmitStack(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    Clay_Color background,
    float radius,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Clay_LayoutDirection direction = CLAY_TOP_TO_BOTTOM;
    if (node->stack.axis == ECS_UI_AXIS_HORIZONTAL) {
        direction = CLAY_LEFT_TO_RIGHT;
    }
    Clay_LayoutConfig layout = EcsUiClayFlowLayout(tree, index);
    layout.padding = CLAY_PADDING_ALL(EcsUiClayU16(node->stack.padding));
    layout.layoutDirection = direction;
    layout.childGap = EcsUiClayU16(node->stack.gap);

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
        .layout = layout,
        .backgroundColor = EcsUiClayApplyOpacity(
            EcsUiClayStackColor(node, background),
            opacity),
        .cornerRadius = EcsUiClayCornerRadius(node, radius),
    }) {
        EcsUiClayEmitChildren(
            tree,
            theme,
            index,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity);
    }
}

static void EcsUiClayEmitZStack(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Clay_LayoutConfig layout = EcsUiClayFlowLayout(tree, index);
    layout.padding = CLAY_PADDING_ALL(EcsUiClayU16(node->stack.padding));

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
        .layout = layout,
        .backgroundColor = EcsUiClayApplyOpacity(theme->surface, opacity),
        .cornerRadius = EcsUiClayCornerRadius(node, 8.0f),
    }) {
        uint32_t child = node->first_child;
        int16_t z_index = 1;
        bool first = true;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            if (first) {
                EcsUiClayEmitNode(
                    tree,
                    theme,
                    child,
                    inverse_text,
                    text_style,
                    has_text_style,
                    text_disabled,
                    opacity);
                first = false;
            } else {
                const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
                const float child_opacity =
                    opacity * EcsUiClayClamp01(child_node->visual.opacity);
                if (child_opacity <= 0.01f) {
                    child = child_node->next_sibling;
                    continue;
                }
                char floating_id[ECS_UI_ID_MAX * 2u] = {0};
                EcsUiClayElementId(
                    child_node,
                    "Floating",
                    floating_id,
                    sizeof(floating_id));
                CLAY(CLAY_SID(EcsUiClayString(floating_id)), {
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_GROW(0),
                            .height = CLAY_SIZING_GROW(0),
                        },
                    },
                    .floating = {
                        .offset = {
                            .x = child_node->visual.offset_x,
                            .y = child_node->visual.offset_y,
                        },
                        .zIndex = z_index,
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_TOP,
                            .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                        },
                        .pointerCaptureMode = EcsUiClayNodeCapturesSelf(
                            child_node) ?
                                CLAY_POINTER_CAPTURE_MODE_CAPTURE :
                                CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                }) {
                    EcsUiClayEmitNodeContent(
                        tree,
                        theme,
                        child,
                        inverse_text,
                        text_style,
                        has_text_style,
                        text_disabled,
                        child_opacity);
                }
                z_index += 1;
            }
            child = tree->nodes[child].next_sibling;
        }
    }
}

static void EcsUiClayEmitOffsetNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    char layout_id[ECS_UI_ID_MAX * 2u] = {0};
    char visual_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, "Layout", layout_id, sizeof(layout_id));
    EcsUiClayElementId(node, "Visual", visual_id, sizeof(visual_id));

    CLAY(CLAY_SID(EcsUiClayString(layout_id)), {
        .layout = EcsUiClayFlowLayout(tree, index),
    }) {
        CLAY(CLAY_SID(EcsUiClayString(visual_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_GROW(0),
                },
            },
            .floating = {
                .offset = {
                    .x = node->visual.offset_x,
                    .y = node->visual.offset_y,
                },
                .attachPoints = {
                    .element = CLAY_ATTACH_POINT_LEFT_TOP,
                    .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                },
                .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                .attachTo = CLAY_ATTACH_TO_PARENT,
            },
        }) {
            EcsUiClayEmitNodeContent(
                tree,
                theme,
                index,
                inverse_text,
                text_style,
                has_text_style,
                text_disabled,
                opacity);
        }
    }
}

static void EcsUiClayEmitNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float node_opacity =
        opacity * EcsUiClayClamp01(node->visual.opacity);
    if (node_opacity <= 0.01f) {
        return;
    }
    if (node->kind != ECS_UI_NODE_ROOT && EcsUiClayHasOffset(node->visual)) {
        EcsUiClayEmitOffsetNode(
            tree,
            theme,
            index,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            node_opacity);
        return;
    }
    EcsUiClayEmitNodeContent(
        tree,
        theme,
        index,
        inverse_text,
        text_style,
        has_text_style,
        text_disabled,
        node_opacity);
}

static void EcsUiClayEmitNodeContent(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiTextStyle node_text_style = text_style;
    bool node_has_text_style = has_text_style;
    if (node->has_text_style) {
        node_text_style = node->text_style;
        node_has_text_style = true;
    }

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
        EcsUiClayEmitStack(
            tree,
            theme,
            index,
            theme->root_background,
            0.0f,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity);
        break;
    case ECS_UI_NODE_VSTACK:
        EcsUiClayEmitStack(
            tree,
            theme,
            index,
            theme->surface,
            8.0f,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity);
        break;
    case ECS_UI_NODE_HSTACK:
        EcsUiClayEmitStack(
            tree,
            theme,
            index,
            theme->surface_subtle,
            8.0f,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity);
        break;
    case ECS_UI_NODE_ZSTACK:
        EcsUiClayEmitZStack(
            tree,
            theme,
            index,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity);
        break;
    case ECS_UI_NODE_BUTTON: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(46.0f),
                },
                .padding = {
                    .left = 14u,
                    .right = 14u,
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 8u,
                .childAlignment = {
                    .x = CLAY_ALIGN_X_CENTER,
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayButtonColor(theme, node),
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, 8.0f),
        }) {
            EcsUiClayEmitChildren(
                tree,
                theme,
                index,
                node->button.variant == ECS_UI_BUTTON_PRIMARY,
                node_text_style,
                node_has_text_style,
                text_disabled || node->button.disabled,
                opacity);
        }
        break;
    }
    case ECS_UI_NODE_PRESSABLE: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(
                        EcsUiClayPressableHeight(node)),
                },
                .padding = {
                    .left = EcsUiClayU16(EcsUiClayBoxPadding(node, 12.0f)),
                    .right = EcsUiClayU16(EcsUiClayBoxPadding(node, 12.0f)),
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = node->has_text_field_view ? 0u : 8u,
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayPressableColor(theme, node),
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, 8.0f),
        }) {
            if (node->has_text_field_view) {
                EcsUiClayEmitTextFieldChildren(
                    tree,
                    theme,
                    index,
                    false,
                    node_text_style,
                    node_has_text_style,
                    text_disabled || node->pressable.disabled,
                    opacity);
            } else {
                EcsUiClayEmitChildren(
                    tree,
                    theme,
                    index,
                    false,
                    node_text_style,
                    node_has_text_style,
                    text_disabled || node->pressable.disabled,
                    opacity);
            }
        }
        break;
    }
    case ECS_UI_NODE_TEXT: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(
                        EcsUiClayTextSize(node->text.role) + 8.0f),
                },
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
        }) {
            CLAY_TEXT(
                EcsUiClayString(node->text.text),
                EcsUiClayTextConfig(
                    theme,
                    node->text.role,
                    inverse_text,
                    node_text_style,
                    node_has_text_style,
                    text_disabled,
                    opacity));
        }
        break;
    }
    case ECS_UI_NODE_ICON: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .height = CLAY_SIZING_FIXED(24.0f),
                },
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
        }) {
            CLAY_TEXT(
                EcsUiClayString(node->icon.name),
                EcsUiClayTextConfig(
                    theme,
                    ECS_UI_TEXT_LABEL,
                    inverse_text,
                    node_text_style,
                    node_has_text_style,
                    text_disabled,
                    opacity));
        }
        break;
    }
    case ECS_UI_NODE_CUSTOM: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(EcsUiClayCustomHeight(node)),
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                theme->surface_subtle,
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, 8.0f),
            .custom = {
                .customData = (void *)node,
            },
        }) {}
        break;
    }
    case ECS_UI_NODE_NONE:
    default:
        break;
    }
}

EcsUiClayTheme EcsUiClayThemeDefault(void)
{
    return (EcsUiClayTheme){
        .root_background = {16.0f, 20.0f, 25.0f, 255.0f},
        .surface = {24.0f, 32.0f, 37.0f, 255.0f},
        .surface_subtle = {18.0f, 27.0f, 31.0f, 255.0f},
        .button = {38.0f, 72.0f, 76.0f, 255.0f},
        .button_primary = {49.0f, 211.0f, 186.0f, 255.0f},
        .button_subtle = {88.0f, 111.0f, 116.0f, 255.0f},
        .button_danger = {255.0f, 125.0f, 95.0f, 255.0f},
        .button_disabled = {70.0f, 78.0f, 82.0f, 255.0f},
        .text = {243.0f, 247.0f, 247.0f, 255.0f},
        .text_muted = {142.0f, 161.0f, 164.0f, 255.0f},
        .text_inverse = {16.0f, 20.0f, 25.0f, 255.0f},
    };
}

void EcsUiClayEmitTree(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme)
{
    EcsUiClayEmitTreeEx(tree, theme, NULL);
}

void EcsUiClayEmitTreeEx(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    const EcsUiClayLayoutOptions *options)
{
    if (tree == NULL || tree->count == 0u || theme == NULL) {
        return;
    }
    if (options == NULL) {
        EcsUiClayEmitNode(
            tree,
            theme,
            0u,
            false,
            (EcsUiTextStyle){0},
            false,
            false,
            1.0f);
        return;
    }

    CLAY(CLAY_ID("EcsUiClayViewport"), {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(
                    EcsUiClayClampPositive(options->bounds.width)),
                .height = CLAY_SIZING_FIXED(
                    EcsUiClayClampPositive(options->bounds.height)),
            },
        },
        .floating = {
            .offset = {
                .x = options->bounds.x,
                .y = options->bounds.y,
            },
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
    }) {
        EcsUiClayEmitNode(
            tree,
            theme,
            0u,
            false,
            (EcsUiTextStyle){0},
            false,
            false,
            1.0f);
    }
}

static bool EcsUiClayPointerOverNode(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    return Clay_PointerOver(Clay_GetElementId(EcsUiClayString(clay_id)));
}

static bool EcsUiClayNodeCapturesSelf(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->hit_test.mode == ECS_UI_HIT_TEST_NONE ||
        node->hit_test.mode == ECS_UI_HIT_TEST_CHILDREN) {
        return false;
    }
    if (node->hit_test.mode == ECS_UI_HIT_TEST_CAPTURE) {
        return true;
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

static EcsUiClayRect EcsUiClayRectFromBounds(Clay_BoundingBox bounds)
{
    return (EcsUiClayRect){
        .x = bounds.x,
        .y = bounds.y,
        .width = bounds.width,
        .height = bounds.height,
    };
}

static EcsUiClayRect EcsUiClayLayoutBounds(
    const EcsUiClayLayoutOptions *options)
{
    if (options != NULL) {
        return EcsUiClayRectFromBounds(options->bounds);
    }

    Clay_ElementData root = Clay_GetElementData(
        Clay_GetElementId(CLAY_STRING("Clay__RootContainer")));
    if (root.found) {
        return EcsUiClayRectFromBounds(root.boundingBox);
    }
    return (EcsUiClayRect){0};
}

static EcsUiClayRect EcsUiClayOffset(
    EcsUiClayRect bounds,
    EcsUiVisual visual)
{
    bounds.x += visual.offset_x;
    bounds.y += visual.offset_y;
    return bounds;
}

static EcsUiClayRect EcsUiClayInset(EcsUiClayRect bounds, float amount)
{
    const float inset = EcsUiClayClampPositive(amount);
    const float width = bounds.width - (inset * 2.0f);
    const float height = bounds.height - (inset * 2.0f);
    return (EcsUiClayRect){
        .x = bounds.x + inset,
        .y = bounds.y + inset,
        .width = EcsUiClayClampPositive(width),
        .height = EcsUiClayClampPositive(height),
    };
}

static bool EcsUiClayPointInRect(
    EcsUiClayPointerState pointer,
    EcsUiClayRect bounds)
{
    return pointer.x >= bounds.x && pointer.y >= bounds.y &&
        pointer.x <= bounds.x + bounds.width &&
        pointer.y <= bounds.y + bounds.height;
}

static bool EcsUiClayHitTestDisabled(
    const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->hit_test.mode == ECS_UI_HIT_TEST_NONE;
}

static bool EcsUiClayHitSet(
    EcsUiClayHit *hit,
    uint32_t index,
    EcsUiClayRect bounds)
{
    if (hit == NULL) {
        return false;
    }
    hit->found = true;
    hit->index = index;
    hit->bounds = bounds;
    return true;
}

static bool EcsUiClayHitSelf(
    const EcsUiTreeNodeSnapshot *node,
    uint32_t index,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiClayHit *hit)
{
    if (!EcsUiClayNodeCapturesSelf(node) ||
        !EcsUiClayPointInRect(pointer, bounds)) {
        return false;
    }
    return EcsUiClayHitSet(hit, index, bounds);
}

static bool EcsUiClayHitNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiClayHit *hit);

static bool EcsUiClayHitChildrenVertical(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiClayHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float gap = EcsUiClayClampPositive(node->stack.gap);
    float y = bounds.y;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_height =
            EcsUiClayPreferredHeight(tree, child, bounds.width);
        const float remaining = bounds.y + bounds.height - y;
        if (remaining <= 0.0f) {
            return false;
        }
        EcsUiClayRect child_bounds = {
            .x = bounds.x,
            .y = y,
            .width = bounds.width,
            .height = preferred_height < remaining ? preferred_height : remaining,
        };
        if (EcsUiClayHitNode(tree, child, child_bounds, pointer, hit)) {
            return true;
        }
        y += child_bounds.height + gap;
        child = tree->nodes[child].next_sibling;
    }
    return false;
}

static bool EcsUiClayHitChildrenHorizontal(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiClayHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const uint32_t child_count = EcsUiClayChildCount(tree, index);
    if (child_count == 0u) {
        return false;
    }

    const float gap = EcsUiClayClampPositive(node->stack.gap);
    const float total_gap = gap * (float)(child_count - 1u);
    float fixed_width = 0.0f;
    uint32_t flexible_count = 0u;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_width = EcsUiClayPreferredWidth(tree, child);
        if (preferred_width > 0.0f) {
            fixed_width += preferred_width;
        } else {
            flexible_count += 1u;
        }
        child = tree->nodes[child].next_sibling;
    }
    const float flexible_width = flexible_count > 0u ?
        EcsUiClayClampPositive(bounds.width - total_gap - fixed_width) /
            (float)flexible_count :
        0.0f;
    float x = bounds.x;

    child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_width = EcsUiClayPreferredWidth(tree, child);
        const float child_width =
            preferred_width > 0.0f ? preferred_width : flexible_width;
        EcsUiClayRect child_bounds = {
            .x = x,
            .y = bounds.y,
            .width = child_width,
            .height = bounds.height,
        };
        if (EcsUiClayHitNode(tree, child, child_bounds, pointer, hit)) {
            return true;
        }
        x += child_width + gap;
        child = tree->nodes[child].next_sibling;
    }
    return false;
}

static bool EcsUiClayHitChildrenZStack(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiClayHit *hit)
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
        if (EcsUiClayHitNode(tree, children[i - 1u], bounds, pointer, hit)) {
            return true;
        }
    }
    return false;
}

static bool EcsUiClayHitNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiClayHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiClayRect node_bounds = EcsUiClayOffset(bounds, node->visual);
    if (EcsUiClayClamp01(node->visual.opacity) <= 0.01f ||
        EcsUiClayHitTestDisabled(node)) {
        return false;
    }

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
        if (EcsUiClayHitChildrenVertical(
                tree,
                index,
                EcsUiClayInset(node_bounds, node->stack.padding),
                pointer,
                hit)) {
            return true;
        }
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    case ECS_UI_NODE_HSTACK:
        if (EcsUiClayHitChildrenHorizontal(
                tree,
                index,
                EcsUiClayInset(node_bounds, node->stack.padding),
                pointer,
                hit)) {
            return true;
        }
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    case ECS_UI_NODE_ZSTACK: {
        EcsUiClayRect inner = EcsUiClayInset(node_bounds, node->stack.padding);
        if (EcsUiClayHitChildrenZStack(tree, index, inner, pointer, hit)) {
            return true;
        }
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    }
    case ECS_UI_NODE_BUTTON:
        if (EcsUiClayHitChildrenHorizontal(
                tree,
                index,
                EcsUiClayInset(
                    node_bounds,
                    EcsUiClayBoxPadding(node, 12.0f)),
                pointer,
                hit)) {
            return true;
        }
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    case ECS_UI_NODE_PRESSABLE:
        if (EcsUiClayHitChildrenHorizontal(
                tree,
                index,
                EcsUiClayInset(node_bounds, 12.0f),
                pointer,
                hit)) {
            return true;
        }
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    case ECS_UI_NODE_CUSTOM:
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return EcsUiClayHitSelf(node, index, node_bounds, pointer, hit);
    }
    return false;
}

static void EcsUiClayPushPointerEvent(
    EcsUiEventList *events,
    const EcsUiTreeNodeSnapshot *node,
    EcsUiEventType type,
    EcsUiClayPointerState pointer)
{
    if (events == NULL || node == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .node = node->entity,
        .action = node->on_click,
        .x = pointer.x,
        .y = pointer.y,
        .start_x = pointer.x,
        .start_y = pointer.y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", node->id);
    (void)EcsUiEventListPush(events, &event);
}

static float EcsUiClayDistanceSquared(
    EcsUiClayPointerState pointer,
    float start_x,
    float start_y)
{
    const float dx = pointer.x - start_x;
    const float dy = pointer.y - start_y;
    return (dx * dx) + (dy * dy);
}

static void EcsUiClayStartPointerCapture(
    const EcsUiTreeNodeSnapshot *node,
    EcsUiClayPointerState pointer)
{
    if (node == NULL) {
        return;
    }

    g_ecs_ui_clay_pointer_capture = (EcsUiClayPointerCapture){
        .active = true,
        .node = node->entity,
        .action = node->on_click,
        .start_x = pointer.x,
        .start_y = pointer.y,
        .start_time = pointer.time,
    };
    (void)snprintf(
        g_ecs_ui_clay_pointer_capture.node_id,
        sizeof(g_ecs_ui_clay_pointer_capture.node_id),
        "%s",
        node->id);
}

static void EcsUiClayPushCapturedPointerEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    EcsUiClayPointerState pointer)
{
    const EcsUiClayPointerCapture *capture =
        &g_ecs_ui_clay_pointer_capture;
    if (events == NULL || !capture->active) {
        return;
    }

    float elapsed = (float)(pointer.time - capture->start_time);
    elapsed = EcsUiClayMaxFloat(elapsed, 0.001f);
    const float delta_x = pointer.x - capture->start_x;
    const float delta_y = pointer.y - capture->start_y;
    EcsUiEvent event = {
        .type = type,
        .node = capture->node,
        .action = capture->action,
        .x = pointer.x,
        .y = pointer.y,
        .start_x = capture->start_x,
        .start_y = capture->start_y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .elapsed = elapsed,
        .velocity_x = delta_x / elapsed,
        .velocity_y = delta_y / elapsed,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", capture->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiClayCollectPointerEvents(
    const EcsUiTreeSnapshot *tree,
    EcsUiClayRect bounds,
    EcsUiClayPointerState pointer,
    EcsUiEventList *events)
{
    if (tree == NULL || tree->count == 0u || events == NULL) {
        return;
    }

    if (g_ecs_ui_clay_pointer_capture.active) {
        if (pointer.down) {
            EcsUiClayPushCapturedPointerEvent(
                events,
                ECS_UI_EVENT_DRAGGED,
                pointer);
        }
        if (pointer.released) {
            const bool did_drag =
                EcsUiClayDistanceSquared(
                    pointer,
                    g_ecs_ui_clay_pointer_capture.start_x,
                    g_ecs_ui_clay_pointer_capture.start_y) > 36.0f;
            EcsUiClayPushCapturedPointerEvent(
                events,
                ECS_UI_EVENT_DRAG_ENDED,
                pointer);
            if (!did_drag) {
                EcsUiClayPushCapturedPointerEvent(
                    events,
                    ECS_UI_EVENT_CLICKED,
                    pointer);
            }
            g_ecs_ui_clay_pointer_capture =
                (EcsUiClayPointerCapture){0};
        }
        return;
    }

    EcsUiClayHit hit = {0};
    EcsUiClayHitNode(tree, 0u, bounds, pointer, &hit);
    if (!hit.found || hit.index >= tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[hit.index];
    EcsUiClayPushPointerEvent(events, node, ECS_UI_EVENT_HOVERED, pointer);
    if (pointer.pressed) {
        EcsUiClayPushPointerEvent(events, node, ECS_UI_EVENT_PRESSED, pointer);
        EcsUiClayStartPointerCapture(node, pointer);
        EcsUiClayPushCapturedPointerEvent(
            events,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    }
}

void EcsUiClayCollectEvents(
    const EcsUiTreeSnapshot *tree,
    EcsUiClayPointerState pointer,
    EcsUiEventList *events)
{
    EcsUiClayCollectEventsEx(tree, pointer, NULL, events);
}

void EcsUiClayCollectEventsEx(
    const EcsUiTreeSnapshot *tree,
    EcsUiClayPointerState pointer,
    const EcsUiClayLayoutOptions *options,
    EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }
    EcsUiEventListClear(events);
    if (tree == NULL || tree->count == 0u) {
        return;
    }

    Clay_SetPointerState(
        (Clay_Vector2){
            .x = pointer.x,
            .y = pointer.y,
        },
        pointer.down);

    EcsUiClayCollectPointerEvents(
        tree,
        EcsUiClayLayoutBounds(options),
        pointer,
        events);
}
