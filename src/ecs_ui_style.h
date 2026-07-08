#ifndef ECS_UI_STYLE_H
#define ECS_UI_STYLE_H

#include "ecs_ui/ecs_ui_paint.h"

float EcsUiStyleClamp01(float value);
EcsUiColorF EcsUiStyleColorFrom(EcsUiColor color);
EcsUiColorF EcsUiStyleApplyOpacity(
    EcsUiColorF color,
    float opacity);
EcsUiColorF EcsUiStyleLerpColor(
    EcsUiColorF from,
    EcsUiColorF to,
    float amount);
EcsUiColorF EcsUiStyleColorOr(
    EcsUiColor color,
    EcsUiColorF fallback);

EcsUiColorF EcsUiStyleTextColor(
    const EcsUiTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled);
EcsUiColorF EcsUiStyleButtonColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node);
EcsUiColorF EcsUiStyleContainerBackground(
    const EcsUiTreeNodeSnapshot *node,
    EcsUiColorF fallback);
EcsUiColorF EcsUiStylePressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node);

bool EcsUiStyleHasNineSlice(const EcsUiTreeNodeSnapshot *node);
EcsUiColorF EcsUiStyleNineSliceTint(
    const EcsUiTreeNodeSnapshot *node);
bool EcsUiStyleHasBevel(const EcsUiTreeNodeSnapshot *node);
bool EcsUiStyleHasDrawableBevel(const EcsUiTreeNodeSnapshot *node);
float EcsUiStyleCornerRadius(
    const EcsUiTreeNodeSnapshot *node,
    float fallback);
EcsUiPaintBorder EcsUiStyleBorder(
    const EcsUiTreeNodeSnapshot *node);
EcsUiColorF EcsUiStyleBevelTopLeftColor(
    const EcsUiTreeNodeSnapshot *node);
EcsUiColorF EcsUiStyleBevelBottomRightColor(
    const EcsUiTreeNodeSnapshot *node);

EcsUiColorF EcsUiStyleSelectionColor(const EcsUiTheme *theme);
EcsUiColorF EcsUiStyleIconColor(void);

#endif
