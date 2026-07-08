#ifndef ECS_UI_STYLE_H
#define ECS_UI_STYLE_H

#include "ecs_ui/ecs_ui_paint.h"

#define ECS_UI_STYLE_TEXT_LINE_MAX ECS_UI_TEXT_MAX

typedef struct EcsUiStyleTextLineMeasure {
    uint32_t byte_start;
    uint32_t byte_end;
    float width;
    float height;
} EcsUiStyleTextLineMeasure;

typedef struct EcsUiStyleTextRangeMeasure {
    float width;
    float height;
    float min_width;
    uint32_t line_count;
    bool truncated;
    EcsUiStyleTextLineMeasure lines[ECS_UI_STYLE_TEXT_LINE_MAX];
} EcsUiStyleTextRangeMeasure;

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

uint16_t EcsUiStyleScaledU16(float value, float scale);
float EcsUiStyleScaledU16Logical(float value, float scale);

float EcsUiStyleRoleTextSize(EcsUiTextRole role);
float EcsUiStyleTextStyleSize(
    EcsUiTextRole role,
    EcsUiTextStyle text_style);
float EcsUiStyleTextSize(
    EcsUiTextRole role,
    EcsUiTextStyle text_style,
    bool has_text_style);
bool EcsUiStyleInheritedTextStyle(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiTextStyle *out_style,
    bool *out_has_style);
float EcsUiStyleInheritedTextSize(
    const EcsUiTreeSnapshot *tree,
    uint32_t index);
EcsUiStyleTextRangeMeasure EcsUiStyleMeasureTextRange(
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
    const char *text,
    uint32_t start,
    uint32_t end,
    uint16_t font_size,
    float scale);

#endif
