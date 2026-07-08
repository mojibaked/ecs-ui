#include "ecs_ui/ecs_ui_clay.h"
#include "ecs_ui_scroll_state.h"
#include "ecs_ui_style.h"

#include <stdio.h>
#include <string.h>

static int16_t g_ecs_ui_clay_z_index_base;
static float g_ecs_ui_clay_scale = 1.0f;
static float g_ecs_ui_clay_root_x;
static float g_ecs_ui_clay_root_y;
static float g_ecs_ui_clay_root_width;
static float g_ecs_ui_clay_root_height;

#define ECS_UI_CLAY_ICON_SIZE 16.0f

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

static void EcsUiClayViewportId(
    const EcsUiTreeSnapshot *tree,
    char *out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (tree == NULL) {
        return;
    }

    (void)snprintf(
        out,
        out_size,
        "EcsUiClayViewport_%llu",
        (unsigned long long)tree->root);
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

static float EcsUiClayScale(void)
{
    return g_ecs_ui_clay_scale > 0.0f ? g_ecs_ui_clay_scale : 1.0f;
}

static float EcsUiClayTreeScale(const EcsUiTreeSnapshot *tree)
{
    return tree != NULL && tree->scale > 0.0f ? tree->scale : 1.0f;
}

static float EcsUiClayLogicalPointerValue(float physical, float scale)
{
    const float normalized_scale = scale > 0.0f ? scale : 1.0f;
    return physical / normalized_scale;
}

static float EcsUiClayScaled(float value)
{
    return value * EcsUiClayScale();
}

static bool EcsUiPlacementIsPointAnchored(const EcsUiPlacement *placement)
{
    return placement != NULL && placement->mode == ECS_UI_PLACEMENT_POINT;
}

static float EcsUiClayResolvePointAnchorAxis(
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

static Clay_Vector2 EcsUiClayPointAnchorOffset(
    const EcsUiPlacement *placement,
    const EcsUiVisual *visual)
{
    const float scale = EcsUiClayScale();
    const float width = placement != NULL ? placement->width : 0.0f;
    const float height = placement != NULL ? placement->height : 0.0f;
    const float point_x =
        (placement != NULL ? placement->point_x : 0.0f) +
        (visual != NULL ? visual->offset_x : 0.0f);
    const float point_y =
        (placement != NULL ? placement->point_y : 0.0f) +
        (visual != NULL ? visual->offset_y : 0.0f);
    const float x = EcsUiClayResolvePointAnchorAxis(
        point_x,
        width,
        g_ecs_ui_clay_root_width);
    const float y = EcsUiClayResolvePointAnchorAxis(
        point_y,
        height,
        g_ecs_ui_clay_root_height);
    return (Clay_Vector2){
        .x = g_ecs_ui_clay_root_x + (x * scale),
        .y = g_ecs_ui_clay_root_y + (y * scale),
    };
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

static uint16_t EcsUiClayScaledU16(float value)
{
    return EcsUiClayU16(EcsUiClayScaled(value));
}

static Clay_SizingAxis EcsUiClayFixed(float value)
{
    return CLAY_SIZING_FIXED(EcsUiClayScaled(value));
}

static EcsUiColorF EcsUiClayStyleFromClay(Clay_Color color)
{
    return (EcsUiColorF){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a,
    };
}

static Clay_Color EcsUiClayStyleColor(EcsUiColorF color)
{
    return (Clay_Color){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a,
    };
}

static float EcsUiClayClamp01(float value)
{
    return EcsUiStyleClamp01(value);
}

static Clay_Color EcsUiClayApplyOpacity(Clay_Color color, float opacity)
{
    return EcsUiClayStyleColor(
        EcsUiStyleApplyOpacity(EcsUiClayStyleFromClay(color), opacity));
}

static Clay_Color EcsUiClayColor(EcsUiColor color)
{
    return EcsUiClayStyleColor(EcsUiStyleColorFrom(color));
}

static float EcsUiClayRoleTextSize(EcsUiTextRole role)
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

static float EcsUiClayTextStyleSize(
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

static float EcsUiClayTextSize(
    EcsUiTextRole role,
    EcsUiTextStyle text_style,
    bool has_text_style)
{
    const float styled_size =
        has_text_style ? EcsUiClayTextStyleSize(role, text_style) : 0.0f;
    return styled_size > 0.0f ? styled_size : EcsUiClayRoleTextSize(role);
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
    return EcsUiClayStyleColor(
        EcsUiStyleTextColor(
            theme,
            role,
            inverse,
            text_style,
            has_text_style,
            disabled));
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
        .fontSize = EcsUiClayU16(
            EcsUiClayScaled(EcsUiClayTextSize(
                role,
                text_style,
                has_text_style))),
        .wrapMode = CLAY_TEXT_WRAP_NONE,
        .textAlignment = EcsUiClayTextAlign(text_layout.align_x),
    });
}

static Clay_Color EcsUiClayButtonColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStyleColor(EcsUiStyleButtonColor(theme, node));
}

static float EcsUiClayCustomHeight(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->custom.preferred_height <= 0.0f) {
        return 96.0f;
    }
    return node->custom.preferred_height;
}

static float EcsUiClayButtonHeight(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->button.preferred_height <= 0.0f) {
        return 46.0f;
    }
    return node->button.preferred_height;
}

static Clay_SizingAxis EcsUiClayButtonWidth(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node != NULL && node->button.preferred_width > 0.0f) {
        return EcsUiClayFixed(node->button.preferred_width);
    }
    return CLAY_SIZING_GROW(0);
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
        return EcsUiClayFixed(value);
    }
    return CLAY_SIZING_GROW(0);
}

static Clay_SizingAxis EcsUiClayApplySizing(
    EcsUiSizing sizing,
    Clay_SizingAxis inferred)
{
    switch (sizing) {
    case ECS_UI_SIZE_GROW:
        return CLAY_SIZING_GROW(0);
    case ECS_UI_SIZE_FIT:
        return CLAY_SIZING_FIT(0);
    case ECS_UI_SIZE_AUTO:
    default:
        return inferred;
    }
}

static Clay_SizingAxis EcsUiClayApplyCustomSizing(
    EcsUiSizing sizing,
    Clay_SizingAxis inferred)
{
    if (sizing == ECS_UI_SIZE_GROW) {
        return CLAY_SIZING_GROW(0);
    }
    return inferred;
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

static bool EcsUiClayHasNineSlice(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleHasNineSlice(node);
}

static Clay_Color EcsUiClayContainerBackground(
    const EcsUiTreeNodeSnapshot *node,
    Clay_Color fallback)
{
    return EcsUiClayStyleColor(
        EcsUiStyleContainerBackground(
            node,
            EcsUiClayStyleFromClay(fallback)));
}

static Clay_Color EcsUiClayTransparent(void)
{
    return (Clay_Color){0};
}

static Clay_Color EcsUiClayNineSliceTint(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStyleColor(EcsUiStyleNineSliceTint(node));
}

static Clay_CustomElementConfig EcsUiClayNineSliceCustom(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayHasNineSlice(node) ?
        (Clay_CustomElementConfig){.customData = (void *)node} :
        (Clay_CustomElementConfig){0};
}

static bool EcsUiClayHasBevel(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleHasBevel(node);
}

static bool EcsUiClayHasDrawableBevel(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleHasDrawableBevel(node);
}

static Clay_CornerRadius EcsUiClayCornerRadius(
    const EcsUiTreeNodeSnapshot *node,
    float fallback)
{
    const float radius = EcsUiClayScaled(
        EcsUiStyleCornerRadius(node, fallback));
    return (Clay_CornerRadius){
        .topLeft = radius,
        .topRight = radius,
        .bottomLeft = radius,
        .bottomRight = radius,
    };
}

static Clay_BorderElementConfig EcsUiClayBorder(
    const EcsUiTreeNodeSnapshot *node,
    float opacity)
{
    if (EcsUiClayHasNineSlice(node) || EcsUiClayHasBevel(node)) {
        return (Clay_BorderElementConfig){0};
    }

    if (node == NULL || !node->has_box_style ||
        node->box_style.border_color.a == 0u) {
        return (Clay_BorderElementConfig){0};
    }

    const EcsUiBoxStyle *style = &node->box_style;
    const uint16_t left = EcsUiClayScaledU16(
        style->border_left_width > 0.0f ?
            style->border_left_width :
            style->border_width);
    const uint16_t top = EcsUiClayScaledU16(
        style->border_top_width > 0.0f ?
            style->border_top_width :
            style->border_width);
    const uint16_t right = EcsUiClayScaledU16(
        style->border_right_width > 0.0f ?
            style->border_right_width :
            style->border_width);
    const uint16_t bottom = EcsUiClayScaledU16(
        style->border_bottom_width > 0.0f ?
            style->border_bottom_width :
            style->border_width);
    if (left == 0u && top == 0u && right == 0u && bottom == 0u) {
        return (Clay_BorderElementConfig){0};
    }

    return (Clay_BorderElementConfig){
        .color = EcsUiClayApplyOpacity(
            EcsUiClayColor(node->box_style.border_color),
            opacity),
        .width = {
            .left = left,
            .right = right,
            .top = top,
            .bottom = bottom,
        },
    };
}

static Clay_ElementDeclaration EcsUiClayContainerDeclaration(
    Clay_LayoutConfig layout,
    const EcsUiTreeNodeSnapshot *node,
    Clay_Color fallback_background,
    float fallback_radius,
    float opacity)
{
    Clay_Color background = EcsUiClayHasNineSlice(node) ?
        EcsUiClayNineSliceTint(node) :
        EcsUiClayContainerBackground(node, fallback_background);
    Clay_CornerRadius radius = background.a != 0.0f ?
        EcsUiClayCornerRadius(node, fallback_radius) :
        (Clay_CornerRadius){0};
    return (Clay_ElementDeclaration){
        .layout = layout,
        .backgroundColor = EcsUiClayApplyOpacity(background, opacity),
        .cornerRadius = radius,
        .custom = EcsUiClayNineSliceCustom(node),
        .border = EcsUiClayBorder(node, opacity),
    };
}

static bool EcsUiClayScrollsHorizontal(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->has_scroll_view &&
        (node->scroll_view.axes & ECS_UI_SCROLL_AXIS_X) != 0u;
}

static bool EcsUiClayScrollsVertical(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->has_scroll_view &&
        (node->scroll_view.axes & ECS_UI_SCROLL_AXIS_Y) != 0u;
}

static Clay_Vector2 EcsUiClayScrollStateOffset(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || !node->has_scroll_state) {
        return (Clay_Vector2){0};
    }
    return (Clay_Vector2){
        .x = EcsUiClayScaled(node->scroll_state.offset_x),
        .y = EcsUiClayScaled(node->scroll_state.offset_y),
    };
}

static void EcsUiClaySyncScrollPosition(
    Clay_ElementId clay_id,
    Clay_Vector2 offset)
{
    Clay_ScrollContainerData data = Clay_GetScrollContainerData(clay_id);
    if (data.found && data.scrollPosition != NULL) {
        data.scrollPosition->x = offset.x;
        data.scrollPosition->y = offset.y;
    }
}

static Clay_ElementDeclaration EcsUiClayScrollContainerDeclaration(
    Clay_LayoutConfig layout,
    const EcsUiTreeNodeSnapshot *node,
    Clay_Color fallback_background,
    float fallback_radius,
    float opacity,
    Clay_Vector2 child_offset)
{
    Clay_ElementDeclaration declaration = EcsUiClayContainerDeclaration(
        layout,
        node,
        fallback_background,
        fallback_radius,
        opacity);
    declaration.clip = (Clay_ClipElementConfig){
        .horizontal = EcsUiClayScrollsHorizontal(node),
        .vertical = EcsUiClayScrollsVertical(node),
        .childOffset = child_offset,
    };
    return declaration;
}

static Clay_Color EcsUiClayPressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStyleColor(EcsUiStylePressableColor(theme, node));
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

static float EcsUiClayStackPaddingSide(float side, float uniform)
{
    return EcsUiClayClampPositive(side > 0.0f ? side : uniform);
}

static float EcsUiClayStackPaddingLeft(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStackPaddingSide(
        node->stack.padding_left,
        node->stack.padding);
}

static float EcsUiClayStackPaddingTop(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStackPaddingSide(
        node->stack.padding_top,
        node->stack.padding);
}

static float EcsUiClayStackPaddingRight(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStackPaddingSide(
        node->stack.padding_right,
        node->stack.padding);
}

static float EcsUiClayStackPaddingBottom(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStackPaddingSide(
        node->stack.padding_bottom,
        node->stack.padding);
}

static float EcsUiClayPreferredHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    float width)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding_left = EcsUiClayStackPaddingLeft(node);
    const float padding_top = EcsUiClayStackPaddingTop(node);
    const float padding_right = EcsUiClayStackPaddingRight(node);
    const float padding_bottom = EcsUiClayStackPaddingBottom(node);
    const float gap = EcsUiClayClampPositive(node->stack.gap);
    if (EcsUiClayNodeIsStack(node) &&
        node->stack.height_sizing == ECS_UI_SIZE_AUTO &&
        node->stack.preferred_height > 0.0f) {
        return EcsUiClayScaled(node->stack.preferred_height);
    }

    switch (node->kind) {
    case ECS_UI_NODE_TEXT:
        return EcsUiClayScaled(
            EcsUiClayTextSize(
                node->text.role,
                node->text_style,
                node->has_text_style) +
            8.0f);
    case ECS_UI_NODE_ICON:
        return EcsUiClayScaled(ECS_UI_CLAY_ICON_SIZE);
    case ECS_UI_NODE_BUTTON:
        return EcsUiClayScaled(EcsUiClayButtonHeight(node));
    case ECS_UI_NODE_PRESSABLE:
        return EcsUiClayScaled(EcsUiClayPressableHeight(node));
    case ECS_UI_NODE_CUSTOM:
        if (node->custom.height_sizing == ECS_UI_SIZE_GROW) {
            return 0.0f;
        }
        return EcsUiClayScaled(EcsUiClayCustomHeight(node));
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
        return EcsUiClayScaled(padding_top + padding_bottom) + height;
    }
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_ZSTACK: {
        float height = EcsUiClayScaled(padding_top + padding_bottom);
        uint32_t child_count = 0u;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            if (child_count > 0u && node->kind != ECS_UI_NODE_ZSTACK) {
                height += EcsUiClayScaled(gap);
            }
            height += EcsUiClayPreferredHeight(
                tree,
                child,
                width - EcsUiClayScaled(padding_left + padding_right));
            child_count += 1u;
            child = tree->nodes[child].next_sibling;
        }
        return height;
    }
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }
}

static float EcsUiClayPreferredWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding_left = EcsUiClayStackPaddingLeft(node);
    const float padding_right = EcsUiClayStackPaddingRight(node);
    const float gap = EcsUiClayClampPositive(node->stack.gap);

    if (EcsUiClayNodeIsStack(node) &&
        node->stack.width_sizing == ECS_UI_SIZE_AUTO &&
        node->stack.preferred_width > 0.0f) {
        return EcsUiClayScaled(node->stack.preferred_width);
    }

    switch (node->kind) {
    case ECS_UI_NODE_ICON:
        return EcsUiClayScaled(ECS_UI_CLAY_ICON_SIZE);
    case ECS_UI_NODE_BUTTON:
        return node->button.preferred_width > 0.0f ?
            EcsUiClayScaled(node->button.preferred_width) :
            0.0f;
    case ECS_UI_NODE_CUSTOM:
        if (node->custom.width_sizing == ECS_UI_SIZE_GROW) {
            return 0.0f;
        }
        return node->custom.preferred_width > 0.0f ?
            EcsUiClayScaled(node->custom.preferred_width) :
            0.0f;
    case ECS_UI_NODE_HSTACK: {
        float width = EcsUiClayScaled(padding_left + padding_right);
        uint32_t child_count = 0u;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            if (child_count > 0u) {
                width += EcsUiClayScaled(gap);
            }
            width += EcsUiClayPreferredWidth(tree, child);
            child_count += 1u;
            child = tree->nodes[child].next_sibling;
        }
        return width;
    }
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_ZSTACK: {
        float width = EcsUiClayScaled(padding_left + padding_right);
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            width = EcsUiClayMaxFloat(
                width,
                EcsUiClayScaled(padding_left + padding_right) +
                    EcsUiClayPreferredWidth(tree, child));
            child = tree->nodes[child].next_sibling;
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

static float EcsUiClayStackContentWidth(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding_left = EcsUiClayStackPaddingLeft(node);
    const float padding_right = EcsUiClayStackPaddingRight(node);
    const float gap = EcsUiClayClampPositive(node->stack.gap);
    float width = EcsUiClayScaled(padding_left + padding_right);
    uint32_t child_count = 0u;
    uint32_t child = node->first_child;
    if (node->stack.axis == ECS_UI_AXIS_HORIZONTAL) {
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            if (child_count > 0u) {
                width += EcsUiClayScaled(gap);
            }
            width += EcsUiClayPreferredWidth(tree, child);
            child_count += 1u;
            child = tree->nodes[child].next_sibling;
        }
        return width;
    }

    while (child != ECS_UI_TREE_INVALID_INDEX) {
        width = EcsUiClayMaxFloat(
            width,
            EcsUiClayScaled(padding_left + padding_right) +
                EcsUiClayPreferredWidth(tree, child));
        child = tree->nodes[child].next_sibling;
    }
    return width;
}

static float EcsUiClayStackContentHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding_top = EcsUiClayStackPaddingTop(node);
    const float padding_bottom = EcsUiClayStackPaddingBottom(node);
    const float padding_left = EcsUiClayStackPaddingLeft(node);
    const float padding_right = EcsUiClayStackPaddingRight(node);
    const float gap = EcsUiClayClampPositive(node->stack.gap);
    const float content_width =
        EcsUiClayMaxFloat(node->stack.preferred_width, 0.0f);
    const float available_width = EcsUiClayMaxFloat(
        content_width - padding_left - padding_right,
        0.0f);
    float height = EcsUiClayScaled(padding_top + padding_bottom);
    uint32_t child_count = 0u;
    uint32_t child = node->first_child;
    if (node->stack.axis == ECS_UI_AXIS_HORIZONTAL) {
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            height = EcsUiClayMaxFloat(
                height,
                EcsUiClayScaled(padding_top + padding_bottom) +
                    EcsUiClayPreferredHeight(
                        tree,
                        child,
                        EcsUiClayScaled(content_width)));
            child = tree->nodes[child].next_sibling;
        }
        return height;
    }

    while (child != ECS_UI_TREE_INVALID_INDEX) {
        if (child_count > 0u && node->stack.axis != ECS_UI_AXIS_DEPTH) {
            height += EcsUiClayScaled(gap);
        }
        height += EcsUiClayPreferredHeight(
            tree,
            child,
            EcsUiClayScaled(available_width));
        child_count += 1u;
        child = tree->nodes[child].next_sibling;
    }
    return height;
}

static Clay_Padding EcsUiClayStackPadding(
    const EcsUiTreeNodeSnapshot *node)
{
    return (Clay_Padding){
        .left = EcsUiClayScaledU16(EcsUiClayStackPaddingLeft(node)),
        .right = EcsUiClayScaledU16(EcsUiClayStackPaddingRight(node)),
        .top = EcsUiClayScaledU16(EcsUiClayStackPaddingTop(node)),
        .bottom = EcsUiClayScaledU16(EcsUiClayStackPaddingBottom(node)),
    };
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
        height = EcsUiClayFixed(
            EcsUiClayTextSize(
                node->text.role,
                node->text_style,
                node->has_text_style) +
            8.0f);
        break;
    case ECS_UI_NODE_ICON:
        width = EcsUiClayFixed(ECS_UI_CLAY_ICON_SIZE);
        height = EcsUiClayFixed(ECS_UI_CLAY_ICON_SIZE);
        break;
    case ECS_UI_NODE_BUTTON:
        width = EcsUiClayButtonWidth(node);
        height = EcsUiClayFixed(EcsUiClayButtonHeight(node));
        break;
    case ECS_UI_NODE_PRESSABLE:
        height = EcsUiClayFixed(EcsUiClayPressableHeight(node));
        break;
    case ECS_UI_NODE_CUSTOM:
        height = EcsUiClayFixed(EcsUiClayCustomHeight(node));
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
            width = EcsUiClayFixed(node->stack.preferred_width);
        }
        if (node->stack.preferred_height > 0.0f) {
            height = EcsUiClayFixed(node->stack.preferred_height);
        }
        width = EcsUiClayApplySizing(node->stack.width_sizing, width);
        height = EcsUiClayApplySizing(node->stack.height_sizing, height);
    } else if (node->kind == ECS_UI_NODE_CUSTOM &&
        node->custom.preferred_width > 0.0f) {
        width = EcsUiClayFixed(node->custom.preferred_width);
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        width = EcsUiClayApplyCustomSizing(node->custom.width_sizing, width);
        height = EcsUiClayApplyCustomSizing(node->custom.height_sizing, height);
    }

    return (Clay_LayoutConfig){
        .sizing = {
            .width = width,
            .height = height,
        },
    };
}

static Clay_LayoutConfig EcsUiClayCustomLayout(
    const EcsUiTreeNodeSnapshot *node)
{
    Clay_SizingAxis width =
        (node != NULL && node->custom.preferred_width > 0.0f) ?
            EcsUiClayFixed(node->custom.preferred_width) :
            CLAY_SIZING_GROW(0);
    Clay_SizingAxis height = EcsUiClayFixed(EcsUiClayCustomHeight(node));
    if (node != NULL) {
        width = EcsUiClayApplyCustomSizing(node->custom.width_sizing, width);
        height = EcsUiClayApplyCustomSizing(node->custom.height_sizing, height);
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
                .width = EcsUiClayFixed(caret_width),
                .height = EcsUiClayFixed(
                    EcsUiClayTextSize(
                        value_node->text.role,
                        text_style,
                        has_text_style) +
                    8.0f),
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
    Clay_Color selection = EcsUiClayStyleColor(EcsUiStyleSelectionColor(theme));
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
    bool blocking,
    bool scroll_container,
    bool scroll_subscribed,
    uint32_t scroll_axes)
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
        .payload = node->payload,
        .tree_snapshot = tree,
        .node_index = index,
        .emit_order = frame->target_count,
        .depth = node->depth,
        .scale = EcsUiClayScale(),
        .area = area,
        .pressable = pressable,
        .blocking = blocking,
        .scroll_container = scroll_container,
        .scroll_subscribed = scroll_subscribed,
        .scroll_axes = scroll_axes,
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
    const bool scroll_container = node->has_scroll_view;
    const bool scroll_subscribed = node->scroll_subscribed;
    const uint32_t scroll_axes =
        node->has_scroll_view ? node->scroll_view.axes : ECS_UI_SCROLL_AXIS_NONE;
    if (!area && !pressable && !blocking && !scroll_container &&
        !scroll_subscribed) {
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
        blocking,
        scroll_container,
        scroll_subscribed,
        scroll_axes);
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
        true,
        false,
        false,
        ECS_UI_SCROLL_AXIS_NONE);
}

static Clay_Color EcsUiClayBevelTopLeftColor(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStyleColor(EcsUiStyleBevelTopLeftColor(node));
}

static Clay_Color EcsUiClayBevelBottomRightColor(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiClayStyleColor(EcsUiStyleBevelBottomRightColor(node));
}

static void EcsUiClayEmitBevelEdge(
    const EcsUiTreeNodeSnapshot *node,
    const char *suffix,
    Clay_Color color,
    Clay_SizingAxis width,
    Clay_SizingAxis height,
    Clay_FloatingAttachPointType element_attach,
    Clay_FloatingAttachPointType parent_attach,
    float opacity)
{
    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, suffix, clay_id, sizeof(clay_id));
    CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
        .layout = {
            .sizing = {
                .width = width,
                .height = height,
            },
        },
        .backgroundColor = EcsUiClayApplyOpacity(color, opacity),
        .floating = {
            .zIndex = EcsUiClayZIndex(20),
            .attachPoints = {
                .element = element_attach,
                .parent = parent_attach,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_PARENT,
        },
    }) {}
}

static void EcsUiClayEmitBevel(
    const EcsUiTreeNodeSnapshot *node,
    float opacity)
{
    if (!EcsUiClayHasDrawableBevel(node)) {
        return;
    }

    const Clay_Color top_left = EcsUiClayBevelTopLeftColor(node);
    const Clay_Color bottom_right = EcsUiClayBevelBottomRightColor(node);
    EcsUiClayEmitBevelEdge(
        node,
        "BevelTop",
        top_left,
        CLAY_SIZING_GROW(0),
        EcsUiClayFixed(1.0f),
        CLAY_ATTACH_POINT_LEFT_TOP,
        CLAY_ATTACH_POINT_LEFT_TOP,
        opacity);
    EcsUiClayEmitBevelEdge(
        node,
        "BevelLeft",
        top_left,
        EcsUiClayFixed(1.0f),
        CLAY_SIZING_GROW(0),
        CLAY_ATTACH_POINT_LEFT_TOP,
        CLAY_ATTACH_POINT_LEFT_TOP,
        opacity);
    EcsUiClayEmitBevelEdge(
        node,
        "BevelBottom",
        bottom_right,
        CLAY_SIZING_GROW(0),
        EcsUiClayFixed(1.0f),
        CLAY_ATTACH_POINT_LEFT_BOTTOM,
        CLAY_ATTACH_POINT_LEFT_BOTTOM,
        opacity);
    EcsUiClayEmitBevelEdge(
        node,
        "BevelRight",
        bottom_right,
        EcsUiClayFixed(1.0f),
        CLAY_SIZING_GROW(0),
        CLAY_ATTACH_POINT_RIGHT_TOP,
        CLAY_ATTACH_POINT_RIGHT_TOP,
        opacity);
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
    layout.padding = EcsUiClayStackPadding(node);
    layout.layoutDirection = direction;
    layout.childGap = EcsUiClayScaledU16(node->stack.gap);
    layout.childAlignment = (Clay_ChildAlignment){
        .x = EcsUiClayAlignX(node->stack.align_x),
        .y = EcsUiClayAlignY(node->stack.align_y),
    };

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    const Clay_ElementId element_id = Clay_GetElementId(EcsUiClayString(clay_id));
    EcsUiClayRegisterNodeTarget(
        frame,
        tree,
        index,
        element_id);
    if (node->has_scroll_view) {
        const Clay_Vector2 child_offset = EcsUiClayScrollStateOffset(node);
        CLAY(
            CLAY_SID(EcsUiClayString(clay_id)),
            EcsUiClayScrollContainerDeclaration(
                layout,
                node,
                background,
                radius,
                opacity,
                child_offset)) {
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
            EcsUiClayEmitBevel(node, opacity);
        }
        EcsUiClaySyncScrollPosition(element_id, child_offset);
    } else {
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
            EcsUiClayEmitBevel(node, opacity);
        }
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
    layout.padding = EcsUiClayStackPadding(node);

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
            EcsUiClayTransparent(),
            0.0f,
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
                const bool point_anchored =
                    placed && EcsUiPlacementIsPointAnchored(placement);
                const Clay_Vector2 offset = point_anchored ?
                    EcsUiClayPointAnchorOffset(
                        placement,
                        &child_node->visual) :
                    (Clay_Vector2){
                        .x = EcsUiClayScaled(
                            child_node->visual.offset_x +
                            (placed ? placement->offset_x : 0.0f)),
                        .y = EcsUiClayScaled(
                            child_node->visual.offset_y +
                            (placed ? placement->offset_y : 0.0f)),
                    };
                const Clay_FloatingAttachPoints attach_points =
                    placed && !point_anchored ?
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
                        .offset = offset,
                        .zIndex = EcsUiClayZIndex(z_index),
                        .attachPoints = attach_points,
                        .pointerCaptureMode = EcsUiClayNodeCapturesSelf(
                            child_node) ?
                                CLAY_POINTER_CAPTURE_MODE_CAPTURE :
                                CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                        .attachTo = point_anchored ?
                            CLAY_ATTACH_TO_ROOT :
                            CLAY_ATTACH_TO_PARENT,
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
        EcsUiClayEmitBevel(node, opacity);
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
                    .x = EcsUiClayScaled(node->visual.offset_x),
                    .y = EcsUiClayScaled(node->visual.offset_y),
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
            EcsUiClayTransparent(),
            0.0f,
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
            EcsUiClayTransparent(),
            0.0f,
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
                    .width = EcsUiClayButtonWidth(node),
                    .height = EcsUiClayFixed(EcsUiClayButtonHeight(node)),
                },
                .padding = {
                    .left = EcsUiClayScaledU16(14.0f),
                    .right = EcsUiClayScaledU16(14.0f),
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = EcsUiClayScaledU16(8.0f),
                .childAlignment = {
                    .x = CLAY_ALIGN_X_CENTER,
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayHasNineSlice(node) ?
                    EcsUiClayNineSliceTint(node) :
                    EcsUiClayButtonColor(theme, node),
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, theme->radius),
            .custom = EcsUiClayNineSliceCustom(node),
            .border = EcsUiClayBorder(node, opacity),
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
            EcsUiClayEmitBevel(node, opacity);
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
                    .height = EcsUiClayFixed(EcsUiClayPressableHeight(node)),
                },
                .padding = {
                    .left = EcsUiClayScaledU16(
                        EcsUiClayBoxPadding(node, 12.0f)),
                    .right = EcsUiClayScaledU16(
                        EcsUiClayBoxPadding(node, 12.0f)),
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = node->has_text_field_view ?
                    0u :
                    EcsUiClayScaledU16(8.0f),
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayHasNineSlice(node) ?
                    EcsUiClayNineSliceTint(node) :
                    EcsUiClayPressableColor(theme, node),
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, theme->radius),
            .custom = EcsUiClayNineSliceCustom(node),
            .border = EcsUiClayBorder(node, opacity),
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
            EcsUiClayEmitBevel(node, opacity);
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
                    .height = EcsUiClayFixed(
                        EcsUiClayTextSize(
                            node->text.role,
                            node_text_style,
                            node_has_text_style) +
                        8.0f),
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
        EcsUiClayRegisterNodeTarget(
            frame,
            tree,
            index,
            Clay_GetElementId(EcsUiClayString(clay_id)));
        CLAY(CLAY_SID(EcsUiClayString(clay_id)), {
            .layout = {
                .sizing = {
                    .width = EcsUiClayFixed(ECS_UI_CLAY_ICON_SIZE),
                    .height = EcsUiClayFixed(ECS_UI_CLAY_ICON_SIZE),
                },
            },
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayStyleColor(EcsUiStyleIconColor()),
                opacity),
            .custom = {
                .customData = (void *)node,
            },
        }) {
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
            .layout = EcsUiClayCustomLayout(node),
            .backgroundColor = EcsUiClayApplyOpacity(
                EcsUiClayHasNineSlice(node) ?
                    EcsUiClayNineSliceTint(node) :
                    EcsUiClayColor(theme->surface_subtle),
                opacity),
            .cornerRadius = EcsUiClayCornerRadius(node, theme->radius),
            .border = EcsUiClayBorder(node, opacity),
            .custom = {
                .customData = (void *)node,
            },
        }) {
            EcsUiClayEmitBevel(node, opacity);
        }
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
    const float previous_scale = g_ecs_ui_clay_scale;
    const float previous_root_x = g_ecs_ui_clay_root_x;
    const float previous_root_y = g_ecs_ui_clay_root_y;
    const float previous_root_width = g_ecs_ui_clay_root_width;
    const float previous_root_height = g_ecs_ui_clay_root_height;
    g_ecs_ui_clay_scale = tree->scale > 0.0f ? tree->scale : 1.0f;
    g_ecs_ui_clay_root_x = options != NULL ? options->bounds.x : 0.0f;
    g_ecs_ui_clay_root_y = options != NULL ? options->bounds.y : 0.0f;
    g_ecs_ui_clay_root_width = options != NULL ?
        options->bounds.width / EcsUiClayScale() :
        tree->nodes[0].stack.preferred_width;
    g_ecs_ui_clay_root_height = options != NULL ?
        options->bounds.height / EcsUiClayScale() :
        tree->nodes[0].stack.preferred_height;
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
        g_ecs_ui_clay_scale = previous_scale;
        g_ecs_ui_clay_root_x = previous_root_x;
        g_ecs_ui_clay_root_y = previous_root_y;
        g_ecs_ui_clay_root_width = previous_root_width;
        g_ecs_ui_clay_root_height = previous_root_height;
        return;
    }

    char viewport_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayViewportId(tree, viewport_id, sizeof(viewport_id));
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
    g_ecs_ui_clay_scale = previous_scale;
    g_ecs_ui_clay_root_x = previous_root_x;
    g_ecs_ui_clay_root_y = previous_root_y;
    g_ecs_ui_clay_root_width = previous_root_width;
    g_ecs_ui_clay_root_height = previous_root_height;
}

static void EcsUiClayClearSnapshotLayout(EcsUiTreeSnapshot *tree)
{
    if (tree == NULL) {
        return;
    }

    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        EcsUiTreeNodeSnapshot *node = &tree->nodes[i];
        node->layout_x = 0.0f;
        node->layout_y = 0.0f;
        node->layout_width = 0.0f;
        node->layout_height = 0.0f;
        node->has_layout = false;
    }
}

static Clay_Vector2 EcsUiClaySnapshotLayoutOrigin(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayLayoutOptions *options)
{
    if (tree == NULL || options == NULL) {
        return (Clay_Vector2){0.0f, 0.0f};
    }

    Clay_Vector2 origin = {
        .x = options->bounds.x,
        .y = options->bounds.y,
    };
    char viewport_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayViewportId(tree, viewport_id, sizeof(viewport_id));
    Clay_ElementData viewport_data =
        Clay_GetElementData(Clay_GetElementId(EcsUiClayString(viewport_id)));
    if (viewport_data.found) {
        origin.x = viewport_data.boundingBox.x;
        origin.y = viewport_data.boundingBox.y;
    }
    return origin;
}

static bool EcsUiClayEnrichSnapshotNodeLayout(
    EcsUiTreeNodeSnapshot *node,
    float scale,
    Clay_Vector2 origin)
{
    if (node == NULL || node->kind == ECS_UI_NODE_NONE) {
        return false;
    }

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    EcsUiClayElementId(node, NULL, clay_id, sizeof(clay_id));
    Clay_ElementData data =
        Clay_GetElementData(Clay_GetElementId(EcsUiClayString(clay_id)));
    if (!data.found) {
        return false;
    }

    node->layout_x = (data.boundingBox.x - origin.x) / scale;
    node->layout_y = (data.boundingBox.y - origin.y) / scale;
    node->layout_width = data.boundingBox.width / scale;
    node->layout_height = data.boundingBox.height / scale;
    node->has_layout = true;
    return true;
}

uint32_t EcsUiClayEnrichSnapshotLayout(
    EcsUiTreeSnapshot *tree,
    const EcsUiClayLayoutOptions *options)
{
    if (tree == NULL) {
        return 0u;
    }

    EcsUiClayClearSnapshotLayout(tree);
    const float scale = EcsUiClayTreeScale(tree);
    const Clay_Vector2 origin =
        EcsUiClaySnapshotLayoutOrigin(tree, options);
    uint32_t enriched_count = 0u;
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (EcsUiClayEnrichSnapshotNodeLayout(
                &tree->nodes[i],
                scale,
                origin)) {
            enriched_count += 1u;
        }
    }
    return enriched_count;
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

static bool EcsUiClayTargetPressEligible(
    const EcsUiClayInteractionTarget *target)
{
    return target != NULL &&
        target->inside &&
        !target->disabled &&
        (target->pressable || target->blocking);
}

static bool EcsUiClayTargetWheelEligible(
    const EcsUiClayInteractionTarget *target)
{
    return target != NULL &&
        target->inside &&
        !target->disabled &&
        (target->blocking ||
         target->scroll_container ||
         target->scroll_subscribed);
}

static EcsUiClayInteractionTarget *EcsUiClayResolvePressTarget(
    EcsUiClayInteractionFrame *frame)
{
    if (frame == NULL) {
        return NULL;
    }

    EcsUiClayInteractionTarget *resolved = NULL;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (!EcsUiClayTargetPressEligible(target)) {
            continue;
        }
        if (EcsUiClayTargetHasHigherPriority(target, resolved)) {
            resolved = target;
        }
    }
    return resolved;
}

static EcsUiClayInteractionTarget *EcsUiClayResolveWheelTarget(
    EcsUiClayInteractionFrame *frame)
{
    if (frame == NULL) {
        return NULL;
    }

    EcsUiClayInteractionTarget *resolved = NULL;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (!EcsUiClayTargetWheelEligible(target)) {
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

static void EcsUiClayPushPointerEventWithAction(
    EcsUiEventList *events,
    const EcsUiClayInteractionTarget *target,
    EcsUiEventType type,
    EcsUiClayPointerState pointer,
    ecs_entity_t action,
    EcsUiPointerButton button)
{
    if (events == NULL || target == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .tree = target->tree,
        .node = target->node,
        .action = action,
        .payload = target->payload,
        .x = EcsUiClayLogicalPointerValue(pointer.x, target->scale),
        .y = EcsUiClayLogicalPointerValue(pointer.y, target->scale),
        .start_x = EcsUiClayLogicalPointerValue(pointer.x, target->scale),
        .start_y = EcsUiClayLogicalPointerValue(pointer.y, target->scale),
        .button = button,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", target->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiClayPushPointerEvent(
    EcsUiEventList *events,
    const EcsUiClayInteractionTarget *target,
    EcsUiEventType type,
    EcsUiClayPointerState pointer)
{
    EcsUiClayPushPointerEventWithAction(
        events,
        target,
        type,
        pointer,
        target != NULL ? target->action : 0,
        ECS_UI_POINTER_BUTTON_PRIMARY);
}

static bool EcsUiClayPointerHasScroll(EcsUiClayPointerState pointer)
{
    return pointer.scroll_x != 0.0f || pointer.scroll_y != 0.0f;
}

static float EcsUiClayClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool EcsUiClayScrollAxisCanConsume(
    float delta,
    bool enabled,
    float content_size,
    float container_size)
{
    return delta != 0.0f &&
        enabled &&
        content_size > container_size;
}

static bool EcsUiClayApplyScrollAxis(
    float delta,
    float content_size,
    float container_size,
    float *value)
{
    if (value == NULL || delta == 0.0f ||
        content_size <= container_size) {
        return false;
    }

    const float old_value = *value;
    const float max_scroll =
        EcsUiClayMaxFloat(content_size - container_size, 0.0f);
    const float next_value = EcsUiClayClampFloat(
        old_value + (delta * 10.0f),
        -max_scroll,
        0.0f);
    if (next_value == old_value) {
        return false;
    }

    *value = next_value;
    return true;
}

typedef struct EcsUiClayScrollContainerWheelResult {
    bool consumed_x;
    bool consumed_y;
    bool mutated;
} EcsUiClayScrollContainerWheelResult;

static void EcsUiClayScrollContainerFallbackContentSize(
    const EcsUiClayInteractionTarget *target,
    float *out_width,
    float *out_height)
{
    if (out_width != NULL) {
        *out_width = 0.0f;
    }
    if (out_height != NULL) {
        *out_height = 0.0f;
    }
    if (target == NULL || target->tree_snapshot == NULL ||
        target->node_index >= target->tree_snapshot->count) {
        return;
    }
    /* This duplicates a small subset of Clay sizing quirks only for the
     * first-frame path where Clay has not retained content dimensions yet. */
    if (out_width != NULL) {
        *out_width = EcsUiClayStackContentWidth(
            target->tree_snapshot,
            target->node_index);
    }
    if (out_height != NULL) {
        *out_height = EcsUiClayStackContentHeight(
            target->tree_snapshot,
            target->node_index);
    }
}

static Clay_Vector2 EcsUiClayTargetScrollStateOffset(
    const EcsUiClayInteractionTarget *target)
{
    if (target == NULL || target->tree_snapshot == NULL ||
            target->node_index >= target->tree_snapshot->count) {
        return (Clay_Vector2){0};
    }
    const EcsUiTreeNodeSnapshot *node =
        &target->tree_snapshot->nodes[target->node_index];
    if (!node->has_scroll_state) {
        return (Clay_Vector2){0};
    }
    const float scale = target->scale > 0.0f ? target->scale : 1.0f;
    return (Clay_Vector2){
        .x = node->scroll_state.offset_x * scale,
        .y = node->scroll_state.offset_y * scale,
    };
}

static void EcsUiClayPushPendingScrollUpdate(
    EcsUiClayInteractionFrame *frame,
    const EcsUiClayInteractionTarget *target,
    float offset_x,
    float offset_y,
    float content_width,
    float content_height,
    float container_width,
    float container_height)
{
    if (frame == NULL || target == NULL) {
        return;
    }
    const float scale = target->scale > 0.0f ? target->scale : 1.0f;
    EcsUiScrollUpdate update = {
        .tree = target->tree,
        .node = target->node,
        .node_index = target->node_index,
        .axes = target->scroll_axes,
        .offset_x = offset_x / scale,
        .offset_y = offset_y / scale,
        .content_w = content_width / scale,
        .content_h = content_height / scale,
        .viewport_w = container_width / scale,
        .viewport_h = container_height / scale,
    };
    for (uint32_t i = 0u; i < frame->pending_scroll_count; i += 1u) {
        if (frame->pending_scrolls[i].node == update.node) {
            frame->pending_scrolls[i] = update;
            return;
        }
    }
    if (frame->pending_scroll_count >= ECS_UI_SCROLL_UPDATE_MAX) {
        frame->truncated = true;
        return;
    }
    frame->pending_scrolls[frame->pending_scroll_count] = update;
    frame->pending_scroll_count += 1u;
}

static EcsUiClayScrollContainerWheelResult
EcsUiClayApplyScrollContainerWheel(
    EcsUiClayInteractionFrame *frame,
    const EcsUiClayInteractionTarget *target,
    EcsUiClayPointerState pointer)
{
    EcsUiClayScrollContainerWheelResult result = {0};
    if (frame == NULL || target == NULL || !target->scroll_container ||
        !EcsUiClayPointerHasScroll(pointer)) {
        return result;
    }

    Clay_ScrollContainerData data =
        Clay_GetScrollContainerData(target->clay_id);
    if (!data.found) {
        return result;
    }
    Clay_ElementData element_data = Clay_GetElementData(target->clay_id);
    const float container_width =
        data.scrollContainerDimensions.width > 0.0f ?
            data.scrollContainerDimensions.width :
            (element_data.found ? element_data.boundingBox.width : 0.0f);
    const float container_height =
        data.scrollContainerDimensions.height > 0.0f ?
            data.scrollContainerDimensions.height :
            (element_data.found ? element_data.boundingBox.height : 0.0f);
    float fallback_content_width = 0.0f;
    float fallback_content_height = 0.0f;
    if (data.contentDimensions.width <= 0.0f ||
        data.contentDimensions.height <= 0.0f) {
        EcsUiClayScrollContainerFallbackContentSize(
            target,
            &fallback_content_width,
            &fallback_content_height);
    }
    const float content_width = data.contentDimensions.width > 0.0f ?
        data.contentDimensions.width :
        fallback_content_width;
    const float content_height = data.contentDimensions.height > 0.0f ?
        data.contentDimensions.height :
        fallback_content_height;

    const bool can_x = EcsUiClayScrollAxisCanConsume(
        pointer.scroll_x,
        data.config.horizontal &&
            (target->scroll_axes & ECS_UI_SCROLL_AXIS_X) != 0u,
        content_width,
        container_width);
    const bool can_y = EcsUiClayScrollAxisCanConsume(
        pointer.scroll_y,
        data.config.vertical &&
            (target->scroll_axes & ECS_UI_SCROLL_AXIS_Y) != 0u,
        content_height,
        container_height);
    bool changed = false;
    Clay_Vector2 next_offset = EcsUiClayTargetScrollStateOffset(target);
    if (can_x) {
        result.consumed_x = true;
        changed |= EcsUiClayApplyScrollAxis(
            pointer.scroll_x,
            content_width,
            container_width,
            &next_offset.x);
    }
    if (can_y) {
        result.consumed_y = true;
        changed |= EcsUiClayApplyScrollAxis(
            pointer.scroll_y,
            content_height,
            container_height,
            &next_offset.y);
    }
    if (changed) {
        EcsUiClayPushPendingScrollUpdate(
            frame,
            target,
            next_offset.x,
            next_offset.y,
            content_width,
            content_height,
            container_width,
            container_height);
        frame->scroll_consumed = true;
    }
    result.mutated = changed;
    return result;
}

static void EcsUiClayPushScrollEvent(
    EcsUiEventList *events,
    const EcsUiClayInteractionTarget *target,
    EcsUiClayPointerState pointer,
    float scroll_x,
    float scroll_y)
{
    if (events == NULL || target == NULL ||
        (scroll_x == 0.0f && scroll_y == 0.0f)) {
        return;
    }

    EcsUiEvent event = {
        .type = ECS_UI_EVENT_SCROLLED,
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .payload = target->payload,
        .x = EcsUiClayLogicalPointerValue(pointer.x, target->scale),
        .y = EcsUiClayLogicalPointerValue(pointer.y, target->scale),
        .start_x = EcsUiClayLogicalPointerValue(pointer.x, target->scale),
        .start_y = EcsUiClayLogicalPointerValue(pointer.y, target->scale),
        .scroll_x = scroll_x,
        .scroll_y = scroll_y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", target->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiClayRouteWheel(
    EcsUiClayInteractionFrame *frame,
    EcsUiClayPointerState pointer,
    EcsUiEventList *events)
{
    if (frame == NULL || events == NULL ||
        !EcsUiClayPointerHasScroll(pointer)) {
        return;
    }

    EcsUiClayInteractionTarget *target =
        EcsUiClayResolveWheelTarget(frame);
    if (target == NULL) {
        return;
    }
    float subscribed_scroll_x = pointer.scroll_x;
    float subscribed_scroll_y = pointer.scroll_y;
    if (target->scroll_container) {
        EcsUiClayScrollContainerWheelResult result =
            EcsUiClayApplyScrollContainerWheel(frame, target, pointer);
        if (result.consumed_x) {
            subscribed_scroll_x = 0.0f;
        }
        if (result.consumed_y) {
            subscribed_scroll_y = 0.0f;
        }
        if (target->scroll_subscribed) {
            /* The subscriber is the same node, not a target behind the
             * container, so it receives axes the container cannot consume. */
            EcsUiClayPushScrollEvent(
                events,
                target,
                pointer,
                subscribed_scroll_x,
                subscribed_scroll_y);
        }
        return;
    }
    if (target->scroll_subscribed) {
        EcsUiClayPushScrollEvent(
            events,
            target,
            pointer,
            subscribed_scroll_x,
            subscribed_scroll_y);
        return;
    }
    if (target->blocking) {
        return;
    }
}

static void EcsUiClayStartPointerCapture(
    EcsUiClayInteractionState *state,
    const EcsUiClayInteractionTarget *target,
    EcsUiClayPointerState pointer,
    EcsUiPointerButton button)
{
    if (state == NULL || target == NULL) {
        return;
    }

    state->capture = (EcsUiClayPointerCapture){
        .active = true,
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .payload = target->payload,
        .scale = target->scale,
        .start_x = EcsUiClayLogicalPointerValue(pointer.x, target->scale),
        .start_y = EcsUiClayLogicalPointerValue(pointer.y, target->scale),
        .physical_start_x = pointer.x,
        .physical_start_y = pointer.y,
        .start_time = pointer.time,
        .button = button,
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
    const float x = EcsUiClayLogicalPointerValue(pointer.x, capture->scale);
    const float y = EcsUiClayLogicalPointerValue(pointer.y, capture->scale);
    const float delta_x = x - capture->start_x;
    const float delta_y = y - capture->start_y;
    EcsUiEvent event = {
        .type = type,
        .tree = capture->tree,
        .node = capture->node,
        .action = capture->action,
        .payload = capture->payload,
        .x = x,
        .y = y,
        .start_x = capture->start_x,
        .start_y = capture->start_y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .elapsed = elapsed,
        .velocity_x = delta_x / elapsed,
        .velocity_y = delta_y / elapsed,
        .button = capture->button,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", capture->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static bool EcsUiClayPointerDownForButton(
    EcsUiClayPointerState pointer,
    EcsUiPointerButton button)
{
    if (button == ECS_UI_POINTER_BUTTON_SECONDARY) {
        return pointer.secondary_down;
    }
    if (button == ECS_UI_POINTER_BUTTON_MIDDLE) {
        return pointer.middle_down;
    }
    return pointer.down;
}

static bool EcsUiClayPointerReleasedForButton(
    EcsUiClayPointerState pointer,
    EcsUiPointerButton button)
{
    if (button == ECS_UI_POINTER_BUTTON_SECONDARY) {
        return pointer.secondary_released;
    }
    if (button == ECS_UI_POINTER_BUTTON_MIDDLE) {
        return pointer.middle_released;
    }
    return pointer.released;
}

static void EcsUiClayMarkPointerInsideTargets(
    EcsUiClayInteractionFrame *frame)
{
    if (frame == NULL) {
        return;
    }
    frame->inside_target_count = 0u;
    frame->pressable_target_count = 0u;
    Clay_ElementIdArray pointer_over_ids = Clay_GetPointerOverIds();
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        if (frame->targets[i].pressable && !frame->targets[i].disabled) {
            frame->pressable_target_count += 1u;
        }
        frame->targets[i].inside =
            EcsUiClayTargetIsInside(&frame->targets[i], pointer_over_ids);
        if (frame->targets[i].inside) {
            frame->inside_target_count += 1u;
        }
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
    EcsUiClayInteractionTarget *resolved =
        EcsUiClayResolvePressTarget(frame);
    frame->resolved_tree = resolved != NULL ? resolved->tree : 0;
    frame->resolved_node = resolved != NULL ? resolved->node : 0;
    frame->resolved_action = resolved != NULL ? resolved->action : 0;
    frame->resolved_payload = resolved != NULL ? resolved->payload : 0u;
    frame->resolved_pressable = resolved != NULL && resolved->pressable;
    if (resolved != NULL) {
        (void)snprintf(
            frame->resolved_node_id,
            sizeof(frame->resolved_node_id),
            "%s",
            resolved->node_id);
    }

    EcsUiClayRouteWheel(frame, pointer, events);

    EcsUiClayPointerCapture *capture = &frame->state->capture;
    if (capture->active) {
        EcsUiClayInteractionTarget *captured_target =
            EcsUiClayFindCaptureTarget(frame, capture);
        if (captured_target == NULL) {
            frame->capture_missing_target = true;
            frame->capture_missing_node = capture->node;
            frame->capture_missing_action = capture->action;
            frame->capture_missing_payload = capture->payload;
            (void)snprintf(
                frame->capture_missing_node_id,
                sizeof(frame->capture_missing_node_id),
                "%s",
                capture->node_id);
            *capture = (EcsUiClayPointerCapture){0};
        } else {
            if (EcsUiClayCaptureCovered(captured_target, resolved)) {
                EcsUiClayPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_DRAG_ENDED,
                    pointer);
                *capture = (EcsUiClayPointerCapture){0};
                return;
            }

            const bool button_down =
                EcsUiClayPointerDownForButton(pointer, capture->button);
            const bool button_released =
                EcsUiClayPointerReleasedForButton(pointer, capture->button);
            if (button_down) {
                EcsUiClayPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_DRAGGED,
                    pointer);
                return;
            }
            if (button_released || !button_down) {
                if (!button_released) {
                    frame->capture_missed_release = true;
                    frame->capture_missed_release_node = capture->node;
                    frame->capture_missed_release_action = capture->action;
                    frame->capture_missed_release_payload =
                        capture->payload;
                    (void)snprintf(
                        frame->capture_missed_release_node_id,
                        sizeof(frame->capture_missed_release_node_id),
                        "%s",
                        capture->node_id);
                }
                const bool did_drag =
                    EcsUiClayDistanceSquared(
                        pointer,
                        capture->physical_start_x,
                        capture->physical_start_y) > 36.0f;
                const bool click_eligible = captured_target->inside;
                EcsUiClayPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_DRAG_ENDED,
                    pointer);
                if (!did_drag && click_eligible &&
                    capture->button != ECS_UI_POINTER_BUTTON_MIDDLE) {
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
    }

    if (resolved == NULL || !resolved->pressable) {
        return;
    }

    EcsUiClayPushPointerEvent(events, resolved, ECS_UI_EVENT_HOVERED, pointer);
    if (pointer.middle_pressed) {
        EcsUiClayStartPointerCapture(
            frame->state,
            resolved,
            pointer,
            ECS_UI_POINTER_BUTTON_MIDDLE);
        EcsUiClayPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    } else if (pointer.secondary_pressed) {
        EcsUiClayPushPointerEventWithAction(
            events,
            resolved,
            ECS_UI_EVENT_SECONDARY_PRESSED,
            pointer,
            resolved->action,
            ECS_UI_POINTER_BUTTON_SECONDARY);
        EcsUiClayStartPointerCapture(
            frame->state,
            resolved,
            pointer,
            ECS_UI_POINTER_BUTTON_SECONDARY);
        EcsUiClayPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    } else if (pointer.pressed) {
        EcsUiClayPushPointerEvent(events, resolved, ECS_UI_EVENT_PRESSED, pointer);
        EcsUiClayStartPointerCapture(
            frame->state,
            resolved,
            pointer,
            ECS_UI_POINTER_BUTTON_PRIMARY);
        EcsUiClayPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    }
}

bool EcsUiClayApplyInteractionFrame(
    ecs_world_t *world,
    const EcsUiClayInteractionFrame *frame)
{
    bool ok = EcsUiApplyHoverState(
        world,
        frame != NULL ? frame->resolved_node : 0);
    if (frame != NULL) {
        for (uint32_t i = 0u; i < frame->pending_scroll_count; i += 1u) {
            ok = EcsUiScrollStateApplyUpdate(
                world,
                &frame->pending_scrolls[i]) && ok;
        }
    }
    return ok;
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
