#ifndef ECS_UI_STYLE_H
#define ECS_UI_STYLE_H

#include "ecs_ui/ecs_ui.h"

typedef struct EcsUiStyleColor {
    float r;
    float g;
    float b;
    float a;
} EcsUiStyleColor;

float EcsUiStyleClamp01(float value);
EcsUiStyleColor EcsUiStyleColorFrom(EcsUiColor color);
EcsUiStyleColor EcsUiStyleApplyOpacity(
    EcsUiStyleColor color,
    float opacity);
EcsUiStyleColor EcsUiStyleLerpColor(
    EcsUiStyleColor from,
    EcsUiStyleColor to,
    float amount);
EcsUiStyleColor EcsUiStyleColorOr(
    EcsUiColor color,
    EcsUiStyleColor fallback);

EcsUiStyleColor EcsUiStyleTextColor(
    const EcsUiTheme *theme,
    EcsUiTextRole role,
    bool inverse,
    EcsUiTextStyle text_style,
    bool has_text_style,
    bool disabled);
EcsUiStyleColor EcsUiStyleButtonColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node);
EcsUiStyleColor EcsUiStyleContainerBackground(
    const EcsUiTreeNodeSnapshot *node,
    EcsUiStyleColor fallback);
EcsUiStyleColor EcsUiStylePressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node);

bool EcsUiStyleHasNineSlice(const EcsUiTreeNodeSnapshot *node);
EcsUiStyleColor EcsUiStyleNineSliceTint(
    const EcsUiTreeNodeSnapshot *node);
bool EcsUiStyleHasBevel(const EcsUiTreeNodeSnapshot *node);
bool EcsUiStyleHasDrawableBevel(const EcsUiTreeNodeSnapshot *node);
float EcsUiStyleCornerRadius(
    const EcsUiTreeNodeSnapshot *node,
    float fallback);
EcsUiStyleColor EcsUiStyleBevelTopLeftColor(
    const EcsUiTreeNodeSnapshot *node);
EcsUiStyleColor EcsUiStyleBevelBottomRightColor(
    const EcsUiTreeNodeSnapshot *node);

EcsUiStyleColor EcsUiStyleSelectionColor(const EcsUiTheme *theme);
EcsUiStyleColor EcsUiStyleIconColor(void);

#endif
