#include "ecs_ui_style.h"

float EcsUiStyleClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

EcsUiStyleColor EcsUiStyleColorFrom(EcsUiColor color)
{
    return (EcsUiStyleColor){
        .r = (float)color.r,
        .g = (float)color.g,
        .b = (float)color.b,
        .a = (float)color.a,
    };
}

EcsUiStyleColor EcsUiStyleApplyOpacity(
    EcsUiStyleColor color,
    float opacity)
{
    color.a *= EcsUiStyleClamp01(opacity);
    return color;
}

EcsUiStyleColor EcsUiStyleLerpColor(
    EcsUiStyleColor from,
    EcsUiStyleColor to,
    float amount)
{
    const float t = EcsUiStyleClamp01(amount);
    return (EcsUiStyleColor){
        .r = from.r + ((to.r - from.r) * t),
        .g = from.g + ((to.g - from.g) * t),
        .b = from.b + ((to.b - from.b) * t),
        .a = from.a + ((to.a - from.a) * t),
    };
}

EcsUiStyleColor EcsUiStyleColorOr(
    EcsUiColor color,
    EcsUiStyleColor fallback)
{
    return color.a != 0u ? EcsUiStyleColorFrom(color) : fallback;
}

EcsUiStyleColor EcsUiStyleTextColor(
    const EcsUiTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled)
{
    if (has_text_style) {
        if (disabled && text_style.disabled_color.a != 0u) {
            return EcsUiStyleColorFrom(text_style.disabled_color);
        }
        if (role == ECS_UI_TEXT_CAPTION && text_style.muted_color.a != 0u) {
            return EcsUiStyleColorFrom(text_style.muted_color);
        }
        if (text_style.color.a != 0u) {
            return EcsUiStyleColorFrom(text_style.color);
        }
    }
    if (inverse) {
        return EcsUiStyleColorFrom(theme->text_inverse);
    }
    if (role == ECS_UI_TEXT_CAPTION) {
        return EcsUiStyleColorFrom(theme->text_muted);
    }
    return EcsUiStyleColorFrom(theme->text);
}

EcsUiStyleColor EcsUiStyleButtonColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node != NULL && node->button.disabled) {
        return EcsUiStyleColorFrom(theme->button_disabled);
    }

    EcsUiStyleColor fill = EcsUiStyleColorFrom(theme->button);
    switch (node != NULL ? node->button.variant : ECS_UI_BUTTON_DEFAULT) {
    case ECS_UI_BUTTON_PRIMARY:
        fill = EcsUiStyleColorFrom(theme->button_primary);
        break;
    case ECS_UI_BUTTON_SUBTLE:
        fill = EcsUiStyleColorFrom(theme->button_subtle);
        break;
    case ECS_UI_BUTTON_DANGER:
        fill = EcsUiStyleColorFrom(theme->button_danger);
        break;
    case ECS_UI_BUTTON_DEFAULT:
    default:
        fill = EcsUiStyleColorFrom(theme->button);
        break;
    }

    return EcsUiStyleLerpColor(
        fill,
        (EcsUiStyleColor){255.0f, 255.0f, 255.0f, fill.a},
        node != NULL ? EcsUiStyleClamp01(node->visual.highlight) * 0.42f : 0.0f);
}

bool EcsUiStyleHasNineSlice(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->has_nine_slice_style &&
        node->nine_slice_style.image[0] != '\0';
}

EcsUiStyleColor EcsUiStyleContainerBackground(
    const EcsUiTreeNodeSnapshot *node,
    EcsUiStyleColor fallback)
{
    if (EcsUiStyleHasNineSlice(node)) {
        return (EcsUiStyleColor){0};
    }
    if (node == NULL || !node->has_box_style) {
        return fallback;
    }
    EcsUiStyleColor fill = EcsUiStyleColorOr(
        node->box_style.background,
        fallback);
    if (node->hover_within) {
        fill = EcsUiStyleColorOr(
            node->box_style.hover_background,
            fill);
    }
    return fill;
}

EcsUiStyleColor EcsUiStylePressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    EcsUiStyleColor fill = EcsUiStyleColorFrom(theme->button_subtle);
    if (node == NULL || !node->has_box_style) {
        if (node != NULL && node->hover_within) {
            fill = EcsUiStyleApplyOpacity(fill, 0.86f);
        }
        return EcsUiStyleLerpColor(
            fill,
            (EcsUiStyleColor){255.0f, 255.0f, 255.0f, fill.a},
            node != NULL ? EcsUiStyleClamp01(node->visual.highlight) * 0.42f : 0.0f);
    }

    fill = EcsUiStyleColorFrom(node->box_style.background);
    if (node->pressable.disabled) {
        fill = EcsUiStyleColorOr(
            node->box_style.disabled_background,
            fill);
    } else if (node->hover_within) {
        fill = EcsUiStyleColorOr(
            node->box_style.hover_background,
            fill);
    }
    EcsUiStyleColor highlight = EcsUiStyleColorOr(
        node->box_style.highlight_background,
        (EcsUiStyleColor){255.0f, 255.0f, 255.0f, fill.a});
    return EcsUiStyleLerpColor(
        fill,
        highlight,
        EcsUiStyleClamp01(node->visual.highlight));
}

EcsUiStyleColor EcsUiStyleNineSliceTint(
    const EcsUiTreeNodeSnapshot *node)
{
    if (!EcsUiStyleHasNineSlice(node) || node->nine_slice_style.tint.a == 0u) {
        return (EcsUiStyleColor){255.0f, 255.0f, 255.0f, 255.0f};
    }
    return EcsUiStyleColorFrom(node->nine_slice_style.tint);
}

bool EcsUiStyleHasBevel(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && !EcsUiStyleHasNineSlice(node) &&
        node->has_box_style && node->box_style.bevel != ECS_UI_BEVEL_NONE;
}

bool EcsUiStyleHasDrawableBevel(const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleHasBevel(node) &&
        node->box_style.bevel_light.a != 0u &&
        node->box_style.bevel_dark.a != 0u;
}

float EcsUiStyleCornerRadius(
    const EcsUiTreeNodeSnapshot *node,
    float fallback)
{
    if (EcsUiStyleHasNineSlice(node) || EcsUiStyleHasBevel(node)) {
        return 0.0f;
    }

    float radius = fallback;
    if (node != NULL && node->has_box_style && node->box_style.radius > 0.0f) {
        radius = node->box_style.radius < 1.0f ?
            node->box_style.radius * 50.0f :
            node->box_style.radius;
    }
    return radius;
}

EcsUiStyleColor EcsUiStyleBevelTopLeftColor(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleColorFrom(
        node->box_style.bevel == ECS_UI_BEVEL_SUNKEN ?
            node->box_style.bevel_dark :
            node->box_style.bevel_light);
}

EcsUiStyleColor EcsUiStyleBevelBottomRightColor(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleColorFrom(
        node->box_style.bevel == ECS_UI_BEVEL_SUNKEN ?
            node->box_style.bevel_light :
            node->box_style.bevel_dark);
}

EcsUiStyleColor EcsUiStyleSelectionColor(const EcsUiTheme *theme)
{
    EcsUiStyleColor selection = EcsUiStyleColorFrom(theme->button_primary);
    selection.a *= 0.35f;
    return selection;
}

EcsUiStyleColor EcsUiStyleIconColor(void)
{
    return (EcsUiStyleColor){0.0f, 0.0f, 0.0f, 255.0f};
}
