#include "ecs_ui/ecs_ui_clay.h"

#include <stdio.h>
#include <string.h>

static int16_t g_ecs_ui_clay_z_index_base;

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

static bool EcsUiClayElementIdIsZero(Clay_ElementId id)
{
    return id.id == 0u;
}

static bool EcsUiClayElementIdEquals(Clay_ElementId left, Clay_ElementId right)
{
    return left.id == right.id;
}

static bool EcsUiClayElementIdArrayContains(
    Clay_ElementIdArray ids,
    Clay_ElementId id)
{
    if (EcsUiClayElementIdIsZero(id)) {
        return false;
    }
    for (int32_t i = 0; i < ids.length; i += 1) {
        Clay_ElementId *candidate = &ids.internalArray[i];
        if (candidate != NULL && EcsUiClayElementIdEquals(*candidate, id)) {
            return true;
        }
    }
    return false;
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

static int16_t EcsUiClayZIndex(int16_t relative)
{
    int value = (int)g_ecs_ui_clay_z_index_base + (int)relative;
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return (int16_t)value;
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

static EcsUiTextLayout EcsUiClayDefaultTextLayout(void)
{
    return (EcsUiTextLayout){
        .align_x = ECS_UI_ALIGN_START,
        .align_y = ECS_UI_ALIGN_CENTER,
    };
}

static Clay_TextAlignment EcsUiClayTextAlign(EcsUiAlign align)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return CLAY_TEXT_ALIGN_CENTER;
    case ECS_UI_ALIGN_END:
        return CLAY_TEXT_ALIGN_RIGHT;
    case ECS_UI_ALIGN_START:
    default:
        return CLAY_TEXT_ALIGN_LEFT;
    }
}

static Clay_Color EcsUiClayTextColor(
    const EcsUiTheme *theme,
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
        return EcsUiClayColor(theme->text_inverse);
    }
    if (role == ECS_UI_TEXT_CAPTION) {
        return EcsUiClayColor(theme->text_muted);
    }
    return EcsUiClayColor(theme->text);
}

static Clay_TextElementConfig *EcsUiClayTextConfig(
    const EcsUiTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled,
    EcsUiTextLayout text_layout,
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
        .textAlignment = EcsUiClayTextAlign(text_layout.align_x),
    });
}

static Clay_Color EcsUiClayButtonColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node != NULL && node->button.disabled) {
        return EcsUiClayColor(theme->button_disabled);
    }

    Clay_Color fill = EcsUiClayColor(theme->button);
    switch (node != NULL ? node->button.variant : ECS_UI_BUTTON_DEFAULT) {
    case ECS_UI_BUTTON_PRIMARY:
        fill = EcsUiClayColor(theme->button_primary);
        break;
    case ECS_UI_BUTTON_SUBTLE:
        fill = EcsUiClayColor(theme->button_subtle);
        break;
    case ECS_UI_BUTTON_DANGER:
        fill = EcsUiClayColor(theme->button_danger);
        break;
    case ECS_UI_BUTTON_DEFAULT:
    default:
        fill = EcsUiClayColor(theme->button);
        break;
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

static Clay_LayoutAlignmentX EcsUiClayAlignX(EcsUiAlign align)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return CLAY_ALIGN_X_CENTER;
    case ECS_UI_ALIGN_END:
        return CLAY_ALIGN_X_RIGHT;
    case ECS_UI_ALIGN_START:
    default:
        return CLAY_ALIGN_X_LEFT;
    }
}

static Clay_LayoutAlignmentY EcsUiClayAlignY(EcsUiAlign align)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return CLAY_ALIGN_Y_CENTER;
    case ECS_UI_ALIGN_END:
        return CLAY_ALIGN_Y_BOTTOM;
    case ECS_UI_ALIGN_START:
    default:
        return CLAY_ALIGN_Y_TOP;
    }
}

static Clay_FloatingAttachPointType EcsUiClayAttachPoint(
    EcsUiAlign x,
    EcsUiAlign y)
{
    if (x == ECS_UI_ALIGN_CENTER) {
        switch (y) {
        case ECS_UI_ALIGN_CENTER:
            return CLAY_ATTACH_POINT_CENTER_CENTER;
        case ECS_UI_ALIGN_END:
            return CLAY_ATTACH_POINT_CENTER_BOTTOM;
        case ECS_UI_ALIGN_START:
        default:
            return CLAY_ATTACH_POINT_CENTER_TOP;
        }
    }
    if (x == ECS_UI_ALIGN_END) {
        switch (y) {
        case ECS_UI_ALIGN_CENTER:
            return CLAY_ATTACH_POINT_RIGHT_CENTER;
        case ECS_UI_ALIGN_END:
            return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
        case ECS_UI_ALIGN_START:
        default:
            return CLAY_ATTACH_POINT_RIGHT_TOP;
        }
    }

    switch (y) {
    case ECS_UI_ALIGN_CENTER:
        return CLAY_ATTACH_POINT_LEFT_CENTER;
    case ECS_UI_ALIGN_END:
        return CLAY_ATTACH_POINT_LEFT_BOTTOM;
    case ECS_UI_ALIGN_START:
    default:
        return CLAY_ATTACH_POINT_LEFT_TOP;
    }
}

static Clay_SizingAxis EcsUiClayPlacementSizing(float value)
{
    if (value > 0.0f) {
        return CLAY_SIZING_FIXED(value);
    }
    return CLAY_SIZING_GROW(0);
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

static Clay_Color EcsUiClayContainerBackground(
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

static Clay_ElementDeclaration EcsUiClayContainerDeclaration(
    Clay_LayoutConfig layout,
    const EcsUiTreeNodeSnapshot *node,
    Clay_Color fallback_background,
    float fallback_radius,
    float opacity)
{
    return (Clay_ElementDeclaration){
        .layout = layout,
        .backgroundColor = EcsUiClayApplyOpacity(
            EcsUiClayContainerBackground(node, fallback_background),
            opacity),
        .cornerRadius = EcsUiClayCornerRadius(node, fallback_radius),
    };
}

static Clay_Color EcsUiClayPressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    Clay_Color fill = EcsUiClayColor(theme->button_subtle);
    if (node == NULL || !node->has_box_style) {
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
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame);
static void EcsUiClayEmitNodeContent(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame);

static void EcsUiClayEmitChildren(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
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
            opacity,
            frame);
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
    const EcsUiTheme *theme,
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
            EcsUiClayDefaultTextLayout(),
            opacity));
}

static void EcsUiClayEmitTextContent(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiTheme *theme,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    EcsUiTextLayout text_layout,
    float opacity)
{
    if (node == NULL) {
        return;
    }

    EcsUiTextStyle node_text_style = text_style;
    bool node_has_text_style = has_text_style;
    if (node->has_text_style) {
        node_text_style = node->text_style;
        node_has_text_style = true;
    }

    CLAY_TEXT(
        EcsUiClayString(node->text.text),
        EcsUiClayTextConfig(
            theme,
            node->text.role,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            text_layout,
            opacity));
}

static void EcsUiClayEmitTextFieldCaret(
    const EcsUiTreeNodeSnapshot *value_node,
    const EcsUiTextFieldView *view,
    const EcsUiTheme *theme,
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
    const EcsUiTheme *theme,
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
    Clay_Color selection = EcsUiClayColor(theme->button_primary);
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
    const EcsUiTheme *theme,
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
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
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
                opacity,
                frame);
        }
        child = tree->nodes[child].next_sibling;
    }
}

static bool EcsUiClayNodeCapturesSelf(
    const EcsUiTreeNodeSnapshot *node);

static bool EcsUiClayNodeIsPressableTarget(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->hit_test.mode == ECS_UI_HIT_TEST_NONE ||
        node->hit_test.mode == ECS_UI_HIT_TEST_CHILDREN) {
        return false;
    }

    switch (node->kind) {
    case ECS_UI_NODE_BUTTON:
        return !node->button.disabled;
    case ECS_UI_NODE_PRESSABLE:
        return !node->pressable.disabled;
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        return node->on_click != 0;
    case ECS_UI_NODE_CUSTOM:
        return node->on_click != 0;
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return false;
    }
}

static bool EcsUiClayNodeIsDisabledTarget(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return node->button.disabled;
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return node->pressable.disabled;
    }
    return false;
}

static bool EcsUiClayNodeIsBlockingTarget(
    const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->hit_test.mode == ECS_UI_HIT_TEST_CAPTURE;
}

static void EcsUiClayRegisterTarget(
    EcsUiClayInteractionFrame *frame,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Clay_ElementId clay_id,
    Clay_ElementId wrapper_id,
    bool area,
    bool pressable,
    bool blocking)
{
    if (frame == NULL || tree == NULL || index >= tree->count ||
        EcsUiClayElementIdIsZero(clay_id)) {
        return;
    }
    if (frame->target_count >= ECS_UI_CLAY_INTERACTION_TARGET_MAX) {
        frame->truncated = true;
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    EcsUiClayInteractionTarget *target =
        &frame->targets[frame->target_count];
    frame->target_count += 1u;
    *target = (EcsUiClayInteractionTarget){
        .clay_id = clay_id,
        .wrapper_id = wrapper_id,
        .tree = tree->root,
        .node = node->entity,
        .action = node->on_click,
        .node_index = index,
        .emit_order = frame->target_count,
        .depth = node->depth,
        .area = area,
        .pressable = pressable,
        .blocking = blocking,
        .disabled = EcsUiClayNodeIsDisabledTarget(node),
    };
    (void)snprintf(target->node_id, sizeof(target->node_id), "%s", node->id);
}

static void EcsUiClayRegisterNodeTarget(
    EcsUiClayInteractionFrame *frame,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Clay_ElementId clay_id)
{
    if (tree == NULL || index >= tree->count) {
        return;
    }
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const bool area = index == 0u;
    const bool pressable = EcsUiClayNodeIsPressableTarget(node);
    const bool blocking = EcsUiClayNodeIsBlockingTarget(node);
    if (!area && !pressable && !blocking) {
        return;
    }
    EcsUiClayRegisterTarget(
        frame,
        tree,
        index,
        clay_id,
        (Clay_ElementId){0},
        area,
        pressable,
        blocking);
}

static void EcsUiClayRegisterWrapperBlocker(
    EcsUiClayInteractionFrame *frame,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Clay_ElementId wrapper_id)
{
    if (tree == NULL || index >= tree->count) {
        return;
    }
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (!EcsUiClayNodeCapturesSelf(node)) {
        return;
    }
    EcsUiClayRegisterTarget(
        frame,
        tree,
        index,
        wrapper_id,
        wrapper_id,
        false,
        false,
        true);
}

static void EcsUiClayEmitStack(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    Clay_Color background,
    float radius,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
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
    layout.childAlignment = (Clay_ChildAlignment){
        .x = EcsUiClayAlignX(node->stack.align_x),
        .y = EcsUiClayAlignY(node->stack.align_y),
    };

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    EcsUiClayRegisterNodeTarget(
        frame,
        tree,
        index,
        Clay_GetElementId(EcsUiClayString(clay_id)));
    CLAY(
        CLAY_SID(EcsUiClayString(clay_id)),
        EcsUiClayContainerDeclaration(
            layout,
            node,
            background,
            radius,
            opacity)) {
        EcsUiClayEmitChildren(
            tree,
            theme,
            index,
            inverse_text,
            text_style,
            has_text_style,
            text_disabled,
            opacity,
            frame);
    }
}

static void EcsUiClayEmitZStack(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Clay_LayoutConfig layout = EcsUiClayFlowLayout(tree, index);
    layout.padding = CLAY_PADDING_ALL(EcsUiClayU16(node->stack.padding));

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    EcsUiClayRegisterNodeTarget(
        frame,
        tree,
        index,
        Clay_GetElementId(EcsUiClayString(clay_id)));
    CLAY(
        CLAY_SID(EcsUiClayString(clay_id)),
        EcsUiClayContainerDeclaration(
            layout,
            node,
            EcsUiClayColor(theme->surface),
            theme->radius,
            opacity)) {
        uint32_t child = node->first_child;
        int16_t z_index = 1;
        bool first = true;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
            const bool placed = child_node->has_placement;
            if (first && !placed) {
                EcsUiClayEmitNode(
                    tree,
                    theme,
                    child,
                    inverse_text,
                    text_style,
                    has_text_style,
                    text_disabled,
                    opacity,
                    frame);
                first = false;
            } else {
                const float child_opacity =
                    opacity * EcsUiClayClamp01(child_node->visual.opacity);
                if (child_opacity <= 0.01f) {
                    child = child_node->next_sibling;
                    continue;
                }
                const EcsUiPlacement *placement = &child_node->placement;
                const float offset_x = child_node->visual.offset_x +
                    (placed ? placement->offset_x : 0.0f);
                const float offset_y = child_node->visual.offset_y +
                    (placed ? placement->offset_y : 0.0f);
                const Clay_FloatingAttachPoints attach_points = placed ?
                    (Clay_FloatingAttachPoints){
                        .element = EcsUiClayAttachPoint(
                            placement->child_x,
                            placement->child_y),
                        .parent = EcsUiClayAttachPoint(
                            placement->parent_x,
                            placement->parent_y),
                    } :
                    (Clay_FloatingAttachPoints){
                        .element = CLAY_ATTACH_POINT_LEFT_TOP,
                        .parent = CLAY_ATTACH_POINT_LEFT_TOP,
                    };
                const bool placed_text =
                    placed && child_node->kind == ECS_UI_NODE_TEXT;
                const EcsUiTextLayout child_text_layout =
                    child_node->has_text_layout ?
                        child_node->text_layout :
                        EcsUiClayDefaultTextLayout();
                char floating_id[ECS_UI_ID_MAX * 2u] = {0};
                EcsUiClayElementId(
                    child_node,
                    placed_text ? NULL : "Floating",
                    floating_id,
                    sizeof(floating_id));
                EcsUiClayRegisterWrapperBlocker(
                    frame,
                    tree,
                    child,
                    Clay_GetElementId(EcsUiClayString(floating_id)));
                CLAY(CLAY_SID(EcsUiClayString(floating_id)), {
                    .layout = {
                        .sizing = {
                            .width = placed ?
                                EcsUiClayPlacementSizing(placement->width) :
                                CLAY_SIZING_GROW(0),
                            .height = placed ?
                                EcsUiClayPlacementSizing(placement->height) :
                                CLAY_SIZING_GROW(0),
                        },
                        .childAlignment = {
                            .x = placed_text ?
                                EcsUiClayAlignX(child_text_layout.align_x) :
                                CLAY_ALIGN_X_LEFT,
                            .y = placed_text ?
                                EcsUiClayAlignY(child_text_layout.align_y) :
                                CLAY_ALIGN_Y_TOP,
                        },
                    },
                    .floating = {
                        .offset = {
                            .x = offset_x,
                            .y = offset_y,
                        },
                        .zIndex = EcsUiClayZIndex(z_index),
                        .attachPoints = attach_points,
                        .pointerCaptureMode = EcsUiClayNodeCapturesSelf(
                            child_node) ?
                                CLAY_POINTER_CAPTURE_MODE_CAPTURE :
                                CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                }) {
                    if (placed_text) {
                        EcsUiClayEmitTextContent(
                            child_node,
                            theme,
                            inverse_text,
                            text_style,
                            has_text_style,
                            text_disabled,
                            child_text_layout,
                            child_opacity);
                    } else {
                        EcsUiClayEmitNodeContent(
                            tree,
                            theme,
                            child,
                            inverse_text,
                            text_style,
                            has_text_style,
                            text_disabled,
                            child_opacity,
                            frame);
                    }
                }
                z_index += 1;
                first = false;
            }
            child = tree->nodes[child].next_sibling;
        }
    }
}

static void EcsUiClayEmitOffsetNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
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
                opacity,
                frame);
        }
    }
}

static void EcsUiClayEmitNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
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
            node_opacity,
            frame);
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
        node_opacity,
        frame);
}

static void EcsUiClayEmitNodeContent(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    bool inverse_text,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool text_disabled,
    float opacity,
    EcsUiClayInteractionFrame *frame)
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
            EcsUiClayColor(theme->root_background),
            0.0f,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity,
            frame);
        break;
    case ECS_UI_NODE_VSTACK:
        EcsUiClayEmitStack(
            tree,
            theme,
            index,
            EcsUiClayColor(theme->surface),
            theme->radius,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity,
            frame);
        break;
    case ECS_UI_NODE_HSTACK:
        EcsUiClayEmitStack(
            tree,
            theme,
            index,
            EcsUiClayColor(theme->surface_subtle),
            theme->radius,
            inverse_text,
            node_text_style,
            node_has_text_style,
            text_disabled,
            opacity,
            frame);
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
            opacity,
            frame);
        break;
    case ECS_UI_NODE_BUTTON: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        EcsUiClayRegisterNodeTarget(
            frame,
            tree,
            index,
            Clay_GetElementId(EcsUiClayString(clay_id)));
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
            .cornerRadius = EcsUiClayCornerRadius(node, theme->radius),
        }) {
            EcsUiClayEmitChildren(
                tree,
                theme,
                index,
                node->button.variant == ECS_UI_BUTTON_PRIMARY,
                node_text_style,
                node_has_text_style,
                text_disabled || node->button.disabled,
                opacity,
                frame);
        }
        break;
    }
    case ECS_UI_NODE_PRESSABLE: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        EcsUiClayRegisterNodeTarget(
            frame,
            tree,
            index,
            Clay_GetElementId(EcsUiClayString(clay_id)));
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
            .cornerRadius = EcsUiClayCornerRadius(node, theme->radius),
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
                    opacity,
                    frame);
            } else {
                EcsUiClayEmitChildren(
                    tree,
                    theme,
                    index,
                    false,
                    node_text_style,
                    node_has_text_style,
                    text_disabled || node->pressable.disabled,
                    opacity,
                    frame);
            }
        }
        break;
    }
    case ECS_UI_NODE_TEXT: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        EcsUiTextLayout text_layout = node->has_text_layout ?
            node->text_layout :
            EcsUiClayDefaultTextLayout();
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(
                        EcsUiClayTextSize(node->text.role) + 8.0f),
                },
                .childAlignment = {
                    .y = EcsUiClayAlignY(text_layout.align_y),
                },
            },
        }) {
            EcsUiClayEmitTextContent(
                node,
                theme,
                inverse_text,
                node_text_style,
                node_has_text_style,
                text_disabled,
                text_layout,
                opacity);
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
                    EcsUiClayDefaultTextLayout(),
                    opacity));
        }
        break;
    }
    case ECS_UI_NODE_CUSTOM: {
        char clay_id[ECS_UI_ID_MAX * 2u] = {0};
        EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
        EcsUiClayRegisterNodeTarget(
            frame,
            tree,
            index,
            Clay_GetElementId(EcsUiClayString(clay_id)));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(EcsUiClayCustomHeight(node)),
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayColor(theme->surface_subtle),
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, theme->radius),
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

void EcsUiClayEmitTree(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    EcsUiClayInteractionFrame *frame)
{
    EcsUiClayEmitTreeEx(tree, theme, NULL, frame);
}

void EcsUiClayEmitTreeEx(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiClayLayoutOptions *options,
    EcsUiClayInteractionFrame *frame)
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
            1.0f,
            frame);
        return;
    }

    char viewport_id[ECS_UI_ID_MAX * 2u] = {0};
    (void)snprintf(
        viewport_id,
        sizeof(viewport_id),
        "EcsUiClayViewport_%llu",
        (unsigned long long)tree->root);
    CLAY(CLAY_SID(EcsUiClayString(viewport_id)), {
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
            .zIndex = options->z_index,
            .attachPoints = {
                .element = options->attach_points.element,
                .parent = options->attach_points.parent,
            },
            .pointerCaptureMode = options->capture_pointer ?
                CLAY_POINTER_CAPTURE_MODE_CAPTURE :
                CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
    }) {
        const int16_t previous_z_index_base = g_ecs_ui_clay_z_index_base;
        g_ecs_ui_clay_z_index_base = options->z_index;
        EcsUiClayEmitNode(
            tree,
            theme,
            0u,
            false,
            (EcsUiTextStyle){0},
            false,
            false,
            1.0f,
            frame);
        g_ecs_ui_clay_z_index_base = previous_z_index_base;
    }
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

static float EcsUiClayDistanceSquared(
    EcsUiClayPointerState pointer,
    float start_x,
    float start_y)
{
    const float dx = pointer.x - start_x;
    const float dy = pointer.y - start_y;
    return (dx * dx) + (dy * dy);
}

static bool EcsUiClayTargetIsInside(
    const EcsUiClayInteractionTarget *target,
    Clay_ElementIdArray pointer_over_ids)
{
    if (target == NULL) {
        return false;
    }
    if (EcsUiClayElementIdArrayContains(pointer_over_ids, target->clay_id)) {
        return true;
    }
    return EcsUiClayElementIdArrayContains(
        pointer_over_ids,
        target->wrapper_id);
}

static bool EcsUiClayTargetHasHigherPriority(
    const EcsUiClayInteractionTarget *candidate,
    const EcsUiClayInteractionTarget *current)
{
    if (candidate == NULL) {
        return false;
    }
    if (current == NULL) {
        return true;
    }
    if (candidate->emit_order != current->emit_order) {
        return candidate->emit_order > current->emit_order;
    }
    return candidate->depth > current->depth;
}

static EcsUiClayInteractionTarget *EcsUiClayResolveTarget(
    EcsUiClayInteractionFrame *frame)
{
    if (frame == NULL) {
        return NULL;
    }

    EcsUiClayInteractionTarget *resolved = NULL;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (!target->inside || target->disabled ||
            (!target->pressable && !target->blocking)) {
            continue;
        }
        if (EcsUiClayTargetHasHigherPriority(target, resolved)) {
            resolved = target;
        }
    }
    return resolved;
}

static EcsUiClayInteractionTarget *EcsUiClayFindCaptureTarget(
    EcsUiClayInteractionFrame *frame,
    const EcsUiClayPointerCapture *capture)
{
    if (frame == NULL || capture == NULL || !capture->active) {
        return NULL;
    }
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (target->tree == capture->tree && target->node == capture->node &&
            target->pressable && !target->disabled) {
            return target;
        }
    }
    return NULL;
}

static bool EcsUiClayCaptureCovered(
    const EcsUiClayInteractionTarget *captured_target,
    const EcsUiClayInteractionTarget *resolved)
{
    if (captured_target == NULL || resolved == NULL ||
        (resolved->tree == captured_target->tree &&
         resolved->node == captured_target->node)) {
        return false;
    }
    return resolved->blocking &&
        EcsUiClayTargetHasHigherPriority(resolved, captured_target);
}

static void EcsUiClayPushPointerEvent(
    EcsUiEventList *events,
    const EcsUiClayInteractionTarget *target,
    EcsUiEventType type,
    EcsUiClayPointerState pointer)
{
    if (events == NULL || target == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .x = pointer.x,
        .y = pointer.y,
        .start_x = pointer.x,
        .start_y = pointer.y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", target->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiClayStartPointerCapture(
    EcsUiClayInteractionState *state,
    const EcsUiClayInteractionTarget *target,
    EcsUiClayPointerState pointer)
{
    if (state == NULL || target == NULL) {
        return;
    }

    state->capture = (EcsUiClayPointerCapture){
        .active = true,
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .start_x = pointer.x,
        .start_y = pointer.y,
        .start_time = pointer.time,
    };
    (void)snprintf(
        state->capture.node_id,
        sizeof(state->capture.node_id),
        "%s",
        target->node_id);
}

static void EcsUiClayPushCapturedPointerEvent(
    EcsUiEventList *events,
    const EcsUiClayPointerCapture *capture,
    EcsUiEventType type,
    EcsUiClayPointerState pointer)
{
    if (events == NULL || capture == NULL || !capture->active) {
        return;
    }

    float elapsed = (float)(pointer.time - capture->start_time);
    elapsed = EcsUiClayMaxFloat(elapsed, 0.001f);
    const float delta_x = pointer.x - capture->start_x;
    const float delta_y = pointer.y - capture->start_y;
    EcsUiEvent event = {
        .type = type,
        .tree = capture->tree,
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

static void EcsUiClayMarkPointerInsideTargets(
    EcsUiClayInteractionFrame *frame)
{
    if (frame == NULL) {
        return;
    }
    Clay_ElementIdArray pointer_over_ids = Clay_GetPointerOverIds();
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        frame->targets[i].inside =
            EcsUiClayTargetIsInside(&frame->targets[i], pointer_over_ids);
    }
}

void EcsUiClayInteractionStateInit(EcsUiClayInteractionState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void EcsUiClayInteractionFrameBegin(
    EcsUiClayInteractionFrame *frame,
    EcsUiClayInteractionState *state)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    frame->state = state;
}

void EcsUiClayCollectFrameEvents(
    EcsUiClayInteractionFrame *frame,
    EcsUiClayPointerState pointer,
    EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }
    EcsUiEventListClear(events);
    if (frame == NULL || frame->state == NULL) {
        return;
    }

    EcsUiClayMarkPointerInsideTargets(frame);
    EcsUiClayInteractionTarget *resolved = EcsUiClayResolveTarget(frame);
    frame->resolved_tree = resolved != NULL ? resolved->tree : 0;
    frame->resolved_node = resolved != NULL ? resolved->node : 0;

    EcsUiClayPointerCapture *capture = &frame->state->capture;
    if (capture->active) {
        EcsUiClayInteractionTarget *captured_target =
            EcsUiClayFindCaptureTarget(frame, capture);
        if (captured_target == NULL) {
            *capture = (EcsUiClayPointerCapture){0};
            return;
        }

        if (EcsUiClayCaptureCovered(captured_target, resolved)) {
            EcsUiClayPushCapturedPointerEvent(
                events,
                capture,
                ECS_UI_EVENT_DRAG_ENDED,
                pointer);
            *capture = (EcsUiClayPointerCapture){0};
            return;
        }

        if (pointer.down) {
            EcsUiClayPushCapturedPointerEvent(
                events,
                capture,
                ECS_UI_EVENT_DRAGGED,
                pointer);
        }
        if (pointer.released) {
            const bool did_drag =
                EcsUiClayDistanceSquared(
                    pointer,
                    capture->start_x,
                    capture->start_y) > 36.0f;
            const bool click_eligible = captured_target->inside;
            EcsUiClayPushCapturedPointerEvent(
                events,
                capture,
                ECS_UI_EVENT_DRAG_ENDED,
                pointer);
            if (!did_drag && click_eligible) {
                EcsUiClayPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_CLICKED,
                    pointer);
            }
            *capture = (EcsUiClayPointerCapture){0};
        }
        return;
    }

    if (resolved == NULL || !resolved->pressable) {
        return;
    }

    EcsUiClayPushPointerEvent(events, resolved, ECS_UI_EVENT_HOVERED, pointer);
    if (pointer.pressed) {
        EcsUiClayPushPointerEvent(events, resolved, ECS_UI_EVENT_PRESSED, pointer);
        EcsUiClayStartPointerCapture(frame->state, resolved, pointer);
        EcsUiClayPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    }
}

bool EcsUiClayInteractionFrameTreePointerInside(
    const EcsUiClayInteractionFrame *frame,
    ecs_entity_t tree)
{
    if (frame == NULL || tree == 0) {
        return false;
    }
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        const EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (target->tree == tree && target->area && target->inside) {
            return true;
        }
    }
    return false;
}
