#include "ecs_ui_style.h"

#include <stdint.h>
#include <string.h>

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

EcsUiColorF EcsUiStyleColorFrom(EcsUiColor color)
{
    return (EcsUiColorF){
        .r = (float)color.r,
        .g = (float)color.g,
        .b = (float)color.b,
        .a = (float)color.a,
    };
}

EcsUiColorF EcsUiStyleApplyOpacity(
    EcsUiColorF color,
    float opacity)
{
    color.a *= EcsUiStyleClamp01(opacity);
    return color;
}

EcsUiColorF EcsUiStyleLerpColor(
    EcsUiColorF from,
    EcsUiColorF to,
    float amount)
{
    const float t = EcsUiStyleClamp01(amount);
    return (EcsUiColorF){
        .r = from.r + ((to.r - from.r) * t),
        .g = from.g + ((to.g - from.g) * t),
        .b = from.b + ((to.b - from.b) * t),
        .a = from.a + ((to.a - from.a) * t),
    };
}

EcsUiColorF EcsUiStyleColorOr(
    EcsUiColor color,
    EcsUiColorF fallback)
{
    return color.a != 0u ? EcsUiStyleColorFrom(color) : fallback;
}

EcsUiColorF EcsUiStyleTextColor(
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

EcsUiColorF EcsUiStyleButtonColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node != NULL && node->button.disabled) {
        return EcsUiStyleColorFrom(theme->button_disabled);
    }

    EcsUiColorF fill = EcsUiStyleColorFrom(theme->button);
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
        (EcsUiColorF){255.0f, 255.0f, 255.0f, fill.a},
        node != NULL ? EcsUiStyleClamp01(node->visual.highlight) * 0.42f : 0.0f);
}

bool EcsUiStyleHasNineSlice(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->has_nine_slice_style &&
        node->nine_slice_style.image[0] != '\0';
}

EcsUiColorF EcsUiStyleContainerBackground(
    const EcsUiTreeNodeSnapshot *node,
    EcsUiColorF fallback)
{
    if (EcsUiStyleHasNineSlice(node)) {
        return (EcsUiColorF){0};
    }
    if (node == NULL || !node->has_box_style) {
        return fallback;
    }
    EcsUiColorF fill = EcsUiStyleColorOr(
        node->box_style.background,
        fallback);
    if (node->hover_within) {
        fill = EcsUiStyleColorOr(
            node->box_style.hover_background,
            fill);
    }
    return fill;
}

EcsUiColorF EcsUiStylePressableColor(
    const EcsUiTheme *theme,
    const EcsUiTreeNodeSnapshot *node)
{
    EcsUiColorF fill = EcsUiStyleColorFrom(theme->button_subtle);
    if (node == NULL || !node->has_box_style) {
        if (node != NULL && node->hover_within) {
            fill = EcsUiStyleApplyOpacity(fill, 0.86f);
        }
        return EcsUiStyleLerpColor(
            fill,
            (EcsUiColorF){255.0f, 255.0f, 255.0f, fill.a},
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
    EcsUiColorF highlight = EcsUiStyleColorOr(
        node->box_style.highlight_background,
        (EcsUiColorF){255.0f, 255.0f, 255.0f, fill.a});
    return EcsUiStyleLerpColor(
        fill,
        highlight,
        EcsUiStyleClamp01(node->visual.highlight));
}

EcsUiColorF EcsUiStyleNineSliceTint(
    const EcsUiTreeNodeSnapshot *node)
{
    if (!EcsUiStyleHasNineSlice(node) || node->nine_slice_style.tint.a == 0u) {
        return (EcsUiColorF){255.0f, 255.0f, 255.0f, 255.0f};
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

static float EcsUiStyleNonNegative(float value)
{
    return value > 0.0f ? value : 0.0f;
}

EcsUiPaintBorder EcsUiStyleBorder(
    const EcsUiTreeNodeSnapshot *node)
{
    if (EcsUiStyleHasNineSlice(node) || EcsUiStyleHasBevel(node)) {
        return (EcsUiPaintBorder){0};
    }
    if (node == NULL || !node->has_box_style ||
            node->box_style.border_color.a == 0u) {
        return (EcsUiPaintBorder){0};
    }

    const EcsUiBoxStyle *style = &node->box_style;
    EcsUiPaintBorder border = {
        .color = EcsUiStyleColorFrom(style->border_color),
        .left = EcsUiStyleNonNegative(
            style->border_left_width > 0.0f ?
                style->border_left_width :
                style->border_width),
        .top = EcsUiStyleNonNegative(
            style->border_top_width > 0.0f ?
                style->border_top_width :
                style->border_width),
        .right = EcsUiStyleNonNegative(
            style->border_right_width > 0.0f ?
                style->border_right_width :
                style->border_width),
        .bottom = EcsUiStyleNonNegative(
            style->border_bottom_width > 0.0f ?
                style->border_bottom_width :
                style->border_width),
    };
    border.has_border =
        border.left > 0.0f ||
        border.top > 0.0f ||
        border.right > 0.0f ||
        border.bottom > 0.0f;
    if (!border.has_border) {
        return (EcsUiPaintBorder){0};
    }
    return border;
}

EcsUiColorF EcsUiStyleBevelTopLeftColor(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleColorFrom(
        node->box_style.bevel == ECS_UI_BEVEL_SUNKEN ?
            node->box_style.bevel_dark :
            node->box_style.bevel_light);
}

EcsUiColorF EcsUiStyleBevelBottomRightColor(
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiStyleColorFrom(
        node->box_style.bevel == ECS_UI_BEVEL_SUNKEN ?
            node->box_style.bevel_light :
            node->box_style.bevel_dark);
}

EcsUiColorF EcsUiStyleSelectionColor(const EcsUiTheme *theme)
{
    EcsUiColorF selection = EcsUiStyleColorFrom(theme->button_primary);
    selection.a *= 0.35f;
    return selection;
}

EcsUiColorF EcsUiStyleIconColor(void)
{
    return (EcsUiColorF){0.0f, 0.0f, 0.0f, 255.0f};
}

uint16_t EcsUiStyleScaledU16(float value, float scale)
{
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    const float scaled = value * scale;
    if (scaled <= 0.0f) {
        return 0u;
    }
    if (scaled >= 65535.0f) {
        return UINT16_MAX;
    }
    return (uint16_t)scaled;
}

float EcsUiStyleScaledU16Logical(float value, float scale)
{
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    return (float)EcsUiStyleScaledU16(value, scale) / scale;
}

float EcsUiStyleRoleTextSize(EcsUiTextRole role)
{
    switch (role) {
    case ECS_UI_TEXT_TITLE:
        return 28.0f;
    case ECS_UI_TEXT_LABEL:
    case ECS_UI_TEXT_BUTTON:
    case ECS_UI_TEXT_BODY:
        return 18.0f;
    case ECS_UI_TEXT_CAPTION:
        return 13.0f;
    default:
        return 18.0f;
    }
}

float EcsUiStyleTextStyleSize(
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

float EcsUiStyleTextSize(
    EcsUiTextRole role,
    EcsUiTextStyle text_style,
    bool has_text_style)
{
    const float styled_size =
        has_text_style ? EcsUiStyleTextStyleSize(role, text_style) : 0.0f;
    return styled_size > 0.0f ? styled_size : EcsUiStyleRoleTextSize(role);
}

bool EcsUiStyleInheritedTextStyle(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    EcsUiTextStyle *out_style,
    bool *out_has_style)
{
    if (out_style != NULL) {
        *out_style = (EcsUiTextStyle){0};
    }
    if (out_has_style != NULL) {
        *out_has_style = false;
    }
    if (tree == NULL || index >= tree->count) {
        return false;
    }

    uint32_t current = index;
    while (current != ECS_UI_TREE_INVALID_INDEX && current < tree->count) {
        const EcsUiTreeNodeSnapshot *candidate = &tree->nodes[current];
        if (candidate->has_text_style) {
            if (out_style != NULL) {
                *out_style = candidate->text_style;
            }
            if (out_has_style != NULL) {
                *out_has_style = true;
            }
            return true;
        }
        current = candidate->parent_index;
    }
    return false;
}

float EcsUiStyleInheritedTextSize(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    if (tree == NULL || index >= tree->count) {
        return EcsUiStyleRoleTextSize(ECS_UI_TEXT_BODY);
    }

    EcsUiTextStyle text_style = {0};
    bool has_text_style = false;
    (void)EcsUiStyleInheritedTextStyle(
        tree,
        index,
        &text_style,
        &has_text_style);
    return EcsUiStyleTextSize(
        tree->nodes[index].text.role,
        text_style,
        has_text_style);
}

static uint32_t EcsUiStyleClampTextIndex(uint32_t index, size_t length)
{
    return index <= length ? index : (uint32_t)length;
}

static EcsUiSize EcsUiStyleMeasureTextSlice(
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
    const char *text,
    int32_t length,
    uint16_t font_size)
{
    if (measure_text == NULL) {
        return (EcsUiSize){0};
    }

    const EcsUiTextMeasureSpec spec = {
        .font_id = 0u,
        .font_size = (float)font_size,
        .letter_spacing = 0.0f,
        .line_height = 0.0f,
    };
    return measure_text(text, length, &spec, measure_user_data);
}

static float EcsUiStyleMaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static void EcsUiStyleAddTextLine(
    EcsUiStyleTextRangeMeasure *measured,
    uint32_t start,
    uint32_t end,
    float width,
    float height,
    float scale,
    uint32_t line_capacity)
{
    if (measured == NULL) {
        return;
    }
    if (line_capacity == 0u ||
            line_capacity > ECS_UI_STYLE_TEXT_LINE_MAX) {
        line_capacity = ECS_UI_STYLE_TEXT_LINE_MAX;
    }
    if (measured->line_count >= line_capacity) {
        measured->truncated = true;
        return;
    }
    measured->lines[measured->line_count] = (EcsUiStyleTextLineMeasure){
        .byte_start = start,
        .byte_end = end,
        .width = width / scale,
        .height = height / scale,
    };
    measured->line_count += 1u;
}

EcsUiStyleTextRangeMeasure EcsUiStyleMeasureTextRangeWithCapacity(
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
    const char *text_or_null,
    uint32_t start,
    uint32_t end,
    uint16_t font_size,
    float scale,
    uint32_t line_capacity)
{
    EcsUiStyleTextRangeMeasure measured = {0};
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    if (line_capacity == 0u ||
            line_capacity > ECS_UI_STYLE_TEXT_LINE_MAX) {
        line_capacity = ECS_UI_STYLE_TEXT_LINE_MAX;
    }
    if (measure_text == NULL) {
        return measured;
    }

    const char *text = text_or_null != NULL ? text_or_null : "";
    const size_t source_length = strlen(text);
    uint32_t range_start = EcsUiStyleClampTextIndex(start, source_length);
    uint32_t range_end = EcsUiStyleClampTextIndex(end, source_length);
    if (range_start > range_end) {
        uint32_t swap = range_start;
        range_start = range_end;
        range_end = swap;
    }

    const char *range_text = &text[range_start];
    const int32_t text_length = (int32_t)(range_end - range_start);
    const float space_width =
        EcsUiStyleMeasureTextSlice(
            measure_text,
            measure_user_data,
            " ",
            1,
            font_size)
            .width;

    int32_t word_start = 0;
    int32_t word_end = 0;
    int32_t line_start = 0;
    float line_width = 0.0f;
    float measured_width = 0.0f;
    float measured_height = 0.0f;
    float min_width = 0.0f;
    while (word_end < text_length) {
        const char current = range_text[word_end];
        if (current == ' ' || current == '\n') {
            const int32_t word_length = word_end - word_start;
            EcsUiSize dimensions = {0};
            if (word_length > 0) {
                dimensions = EcsUiStyleMeasureTextSlice(
                    measure_text,
                    measure_user_data,
                    &range_text[word_start],
                    word_length,
                    font_size);
            }
            min_width = EcsUiStyleMaxFloat(min_width, dimensions.width);
            measured_height =
                EcsUiStyleMaxFloat(measured_height, dimensions.height);
            if (current == ' ') {
                dimensions.width += space_width;
                line_width += dimensions.width;
            } else {
                line_width += dimensions.width;
                measured_width =
                    EcsUiStyleMaxFloat(measured_width, line_width);
                const bool final_space =
                    word_end > line_start && range_text[word_end - 1] == ' ';
                const int32_t line_length =
                    word_end - line_start - (final_space ? 1 : 0);
                EcsUiStyleAddTextLine(
                    &measured,
                    range_start + (uint32_t)line_start,
                    range_start + (uint32_t)(line_start + line_length),
                    line_width - (final_space ? space_width : 0.0f),
                    measured_height,
                    scale,
                    line_capacity);
                line_width = 0.0f;
                line_start = word_end + 1;
            }
            word_start = word_end + 1;
        }
        word_end += 1;
    }

    if (word_end - word_start > 0) {
        const EcsUiSize dimensions = EcsUiStyleMeasureTextSlice(
            measure_text,
            measure_user_data,
            &range_text[word_start],
            word_end - word_start,
            font_size);
        line_width += dimensions.width;
        measured_height =
            EcsUiStyleMaxFloat(measured_height, dimensions.height);
        min_width = EcsUiStyleMaxFloat(min_width, dimensions.width);
    }

    measured_width = EcsUiStyleMaxFloat(line_width, measured_width);
    if (text_length > 0 && line_start < text_length) {
        EcsUiStyleAddTextLine(
            &measured,
            range_start + (uint32_t)line_start,
            range_end,
            line_width,
            measured_height,
            scale,
            line_capacity);
    }

    measured.width = measured_width / scale;
    measured.height = measured_height / scale;
    measured.min_width = min_width / scale;
    return measured;
}

EcsUiStyleTextRangeMeasure EcsUiStyleMeasureTextRange(
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
    const char *text_or_null,
    uint32_t start,
    uint32_t end,
    uint16_t font_size,
    float scale)
{
    return EcsUiStyleMeasureTextRangeWithCapacity(
        measure_text,
        measure_user_data,
        text_or_null,
        start,
        end,
        font_size,
        scale,
        ECS_UI_STYLE_TEXT_LINE_MAX);
}
