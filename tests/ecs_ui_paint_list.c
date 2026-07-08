#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "../src/ecs_ui_frame_internal.h"
#include "../src/ecs_ui_paint_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

typedef struct TestFrameErrors {
    uint32_t count;
    EcsUiFrameErrorKind last_kind;
    char last_message[256];
} TestFrameErrors;

static void CopyString(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0u) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, dst_size, "%s", src);
}

static void TestFrameHandleError(
    EcsUiFrameErrorKind kind,
    const char *message,
    void *user_data)
{
    TestFrameErrors *errors = user_data;
    if (errors == NULL) {
        return;
    }
    errors->count += 1u;
    errors->last_kind = kind;
    CopyString(errors->last_message, sizeof(errors->last_message), message);
}

static int RequireNear(
    float actual,
    float expected,
    float epsilon,
    const char *message)
{
    float delta = actual - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    if (delta > epsilon) {
        (void)fprintf(
            stderr,
            "%s: actual=%f expected=%f\n",
            message,
            actual,
            expected);
        return 1;
    }
    return 0;
}

static int RequireColor(
    EcsUiColorF actual,
    EcsUiColorF expected,
    const char *message)
{
    int result = 0;
    result |= RequireNear(actual.r, expected.r, 0.001f, message);
    result |= RequireNear(actual.g, expected.g, 0.001f, message);
    result |= RequireNear(actual.b, expected.b, 0.001f, message);
    result |= RequireNear(actual.a, expected.a, 0.001f, message);
    return result;
}

static EcsUiColorF TestColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (EcsUiColorF){
        .r = (float)r,
        .g = (float)g,
        .b = (float)b,
        .a = (float)a,
    };
}

static EcsUiPaintCornerRadius TestRadius(float radius)
{
    return (EcsUiPaintCornerRadius){
        .top_left = radius,
        .top_right = radius,
        .bottom_left = radius,
        .bottom_right = radius,
    };
}

static EcsUiPaintBorder TestNoBorder(void)
{
    return (EcsUiPaintBorder){0};
}

static EcsUiPaintBorder TestBorder(
    EcsUiColorF color,
    float left,
    float top,
    float right,
    float bottom)
{
    return (EcsUiPaintBorder){
        .color = color,
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
        .has_border = true,
    };
}

static int RequireRadius(
    EcsUiPaintCornerRadius actual,
    EcsUiPaintCornerRadius expected,
    const char *message)
{
    int result = 0;
    result |= RequireNear(actual.top_left, expected.top_left, 0.001f, message);
    result |= RequireNear(actual.top_right, expected.top_right, 0.001f, message);
    result |= RequireNear(actual.bottom_left, expected.bottom_left, 0.001f, message);
    result |= RequireNear(actual.bottom_right, expected.bottom_right, 0.001f, message);
    return result;
}

static int RequireBorder(
    EcsUiPaintBorder actual,
    EcsUiPaintBorder expected,
    const char *message)
{
    int result = 0;
    result |= Require(
        actual.has_border == expected.has_border,
        "paint box border presence mismatch");
    if (!expected.has_border) {
        return result;
    }
    result |= RequireColor(actual.color, expected.color, message);
    result |= RequireNear(actual.left, expected.left, 0.001f, message);
    result |= RequireNear(actual.top, expected.top, 0.001f, message);
    result |= RequireNear(actual.right, expected.right, 0.001f, message);
    result |= RequireNear(actual.bottom, expected.bottom, 0.001f, message);
    return result;
}

static EcsUiSize TestMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    (void)utf8;
    (void)user_data;
    const float font_size = spec != NULL ? spec->font_size : 0.0f;
    const float chars = length > 0 ? (float)length : 0.0f;
    return (EcsUiSize){
        .width = chars * font_size * 0.5f + 3.0f,
        .height = font_size + 4.0f,
    };
}

static ecs_world_t *CreateWorld(void)
{
    ecs_world_t *world = ecs_init();
    if (world != NULL) {
        EcsUiImport(world);
    }
    return world;
}

static const EcsUiTreeNodeSnapshot *FindNode(
    const EcsUiTreeSnapshot *tree,
    const char *id)
{
    return EcsUiTreeSnapshotFindNodeById(tree, id);
}

static int BuildPaintTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set paint scale");

    const EcsUiNineSliceStyle nine_slice_style = {
        .image = "paint.frame",
        .slice_left = 3u,
        .slice_top = 4u,
        .slice_right = 5u,
        .slice_bottom = 6u,
        .scale = 1.0f,
        .tint = {200u, 210u, 220u, 180u},
    };
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t fit = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintFit",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .gap = 2.5f,
            .padding = 1.25f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PaintCustom",
            .kind = "paint.custom",
            .preferred_width = 20.5f,
            .preferred_height = 12.25f,
        });
    ecs_entity_t single_text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintSingleText",
            .text = "one two",
            .role = ECS_UI_TEXT_BODY,
        });
    ecs_entity_t multiline_text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintMultiText",
            .text = "aa\n\nbbb ",
            .role = ECS_UI_TEXT_CAPTION,
        });
    ecs_entity_t pressable = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintPress",
            .preferred_height = 18.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "PaintIcon",
            .name = "paint.icon",
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "PaintButton",
            .variant = ECS_UI_BUTTON_PRIMARY,
            .preferred_width = 19.5f,
            .preferred_height = 13.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintNoFill",
            .preferred_width = 8.5f,
            .preferred_height = 6.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t transparent_group = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintTransparentGroup",
            .preferred_width = 13.25f,
            .preferred_height = 7.25f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PaintTransparentChild",
            .kind = "paint.transparent",
            .preferred_width = 9.5f,
            .preferred_height = 6.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PaintAfter",
            .kind = "paint.after",
            .preferred_width = 11.5f,
            .preferred_height = 9.5f,
        });
    ecs_entity_t nine_slice = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintNine",
            .preferred_width = 17.25f,
            .preferred_height = 10.75f,
            .nine_slice_style = &nine_slice_style,
        });
    EcsUiEnd(&builder);
    ecs_entity_t bevel = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintBevel",
            .preferred_width = 15.25f,
            .preferred_height = 11.5f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "paint builder failed");

    ecs_set(world, fit, EcsUiBoxStyle, {
        .background = {10u, 20u, 30u, 255u},
        .radius = 0.5f,
        .border_width = 1.5f,
        .border_left_width = 2.25f,
        .border_bottom_width = 3.5f,
        .border_color = {100u, 110u, 120u, 230u},
    });
    ecs_set(world, fit, EcsUiVisual, {
        .opacity = 0.75f,
    });
    ecs_set(world, single_text, EcsUiTextLayout, {
        .align_x = ECS_UI_ALIGN_END,
        .align_y = ECS_UI_ALIGN_START,
    });
    ecs_set(world, multiline_text, EcsUiTextLayout, {
        .align_x = ECS_UI_ALIGN_CENTER,
        .align_y = ECS_UI_ALIGN_END,
    });
    ecs_set(world, pressable, EcsUiBoxStyle, {
        .background = {40u, 50u, 60u, 200u},
        .radius = 6.25f,
        .border_width = 2.0f,
        .border_top_width = 2.5f,
        .border_color = {130u, 140u, 150u, 240u},
    });
    ecs_set(world, transparent_group, EcsUiBoxStyle, {
        .background = {70u, 80u, 90u, 255u},
    });
    ecs_set(world, transparent_group, EcsUiVisual, {
        .opacity = 0.005f,
    });
    ecs_set(world, nine_slice, EcsUiBoxStyle, {
        .background = {77u, 88u, 99u, 255u},
    });
    ecs_set(world, bevel, EcsUiBoxStyle, {
        .background = {90u, 100u, 110u, 255u},
        .bevel = ECS_UI_BEVEL_RAISED,
        .bevel_light = {240u, 245u, 250u, 230u},
        .bevel_dark = {20u, 25u, 30u, 210u},
    });
    return result;
}

static int BuildTextFieldPaintTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set text paint scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t unfocused = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldUnfocused",
            .preferred_height = 30.5f,
        });
    ecs_entity_t unfocused_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldUnfocusedValue",
            .text = "abc def",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t cursor_start = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldCursorStart",
            .preferred_height = 30.5f,
        });
    ecs_entity_t cursor_start_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldCursorStartValue",
            .text = "xy",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t cursor_end = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldCursorEnd",
            .preferred_height = 30.5f,
        });
    ecs_entity_t cursor_end_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldCursorEndValue",
            .text = "xy",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t selection_start = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldSelectionStart",
            .preferred_height = 30.5f,
        });
    ecs_entity_t selection_start_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldSelectionStartValue",
            .text = "abcde",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t selection_end = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldSelectionEnd",
            .preferred_height = 30.5f,
        });
    ecs_entity_t selection_end_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldSelectionEndValue",
            .text = "abcde",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t styled = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldStyled",
            .preferred_height = 32.5f,
        });
    ecs_entity_t styled_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldStyledValue",
            .text = "hi",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t fractional_padding = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldFractionalPadding",
            .preferred_height = 30.5f,
        });
    ecs_entity_t fractional_padding_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldFractionalPaddingValue",
            .text = "p",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "text paint builder failed");

    ecs_set(world, unfocused, EcsUiTextFieldView, {
        .value_node = unfocused_value,
        .cursor = 5u,
    });
    ecs_set(world, cursor_start, EcsUiTextFieldView, {
        .value_node = cursor_start_value,
        .focused = true,
        .cursor = 0u,
    });
    ecs_set(world, cursor_end, EcsUiTextFieldView, {
        .value_node = cursor_end_value,
        .focused = true,
        .cursor = 2u,
        .caret_width = 2.5f,
    });
    ecs_set(world, selection_start, EcsUiTextFieldView, {
        .value_node = selection_start_value,
        .focused = true,
        .cursor = 1u,
        .selection_anchor = 1u,
        .selection_focus = 4u,
    });
    ecs_set(world, selection_end, EcsUiTextFieldView, {
        .value_node = selection_end_value,
        .focused = true,
        .cursor = 4u,
        .selection_anchor = 1u,
        .selection_focus = 4u,
        .caret_width = 2.5f,
    });
    ecs_set(world, styled, EcsUiTextFieldView, {
        .value_node = styled_value,
        .focused = false,
        .disabled = true,
    });
    ecs_set(world, styled_value, EcsUiTextStyle, {
        .color = {10u, 20u, 30u, 255u},
        .disabled_color = {120u, 20u, 10u, 240u},
        .body_size = 20.5f,
    });
    ecs_set(world, fractional_padding, EcsUiTextFieldView, {
        .value_node = fractional_padding_value,
        .focused = false,
    });
    ecs_set(world, fractional_padding, EcsUiBoxStyle, {
        .background = {40u, 50u, 60u, 200u},
        .padding = 12.25f,
    });
    return result;
}

static int BuildPlacedTextPaintTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set placed text scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintPlacedZ",
            .preferred_width = 80.0f,
            .preferred_height = 40.0f,
        });
    ecs_entity_t text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintPlacedText",
            .text = "zz",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "placed text builder failed");

    ecs_set(world, text, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .width = 60.0f,
        .height = 30.0f,
    });
    ecs_set(world, text, EcsUiTextLayout, {
        .align_x = ECS_UI_ALIGN_CENTER,
        .align_y = ECS_UI_ALIGN_END,
    });
    return result;
}

static void SetPaintBox(
    ecs_world_t *world,
    ecs_entity_t entity,
    EcsUiColor color)
{
    ecs_set(world, entity, EcsUiBoxStyle, {
        .background = color,
    });
}

static int BuildOrderPaintTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set order scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t before = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderBefore",
            .preferred_width = 18.5f,
            .preferred_height = 14.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t offset = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderOffset",
            .preferred_width = 18.5f,
            .preferred_height = 14.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t after = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderAfter",
            .preferred_width = 18.5f,
            .preferred_height = 14.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t zstack = EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderZ",
            .preferred_width = 42.5f,
            .preferred_height = 26.5f,
        });
    ecs_entity_t z_flow = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderZFlow",
            .preferred_width = 16.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t z_float_a = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderZFloatA",
            .preferred_width = 24.5f,
            .preferred_height = 18.5f,
        });
    ecs_entity_t nested_z = EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderNestedZ",
            .preferred_width = 20.5f,
            .preferred_height = 14.5f,
        });
    ecs_entity_t nested_flow = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderNestedFlow",
            .preferred_width = 10.5f,
            .preferred_height = 8.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t nested_float = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderNestedFloat",
            .preferred_width = 10.5f,
            .preferred_height = 8.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t outer_later = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderOuterLater",
            .preferred_width = 11.5f,
            .preferred_height = 9.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t z_float_b = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderZFloatB",
            .preferred_width = 14.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t bevel = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderBevel",
            .preferred_width = 20.5f,
            .preferred_height = 16.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t later_base = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintOrderLaterBase",
            .preferred_width = 19.5f,
            .preferred_height = 15.5f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "order paint builder failed");

    SetPaintBox(world, before, (EcsUiColor){20u, 30u, 40u, 255u});
    SetPaintBox(world, offset, (EcsUiColor){30u, 40u, 50u, 255u});
    SetPaintBox(world, after, (EcsUiColor){40u, 50u, 60u, 255u});
    SetPaintBox(world, zstack, (EcsUiColor){50u, 60u, 70u, 255u});
    SetPaintBox(world, z_flow, (EcsUiColor){60u, 70u, 80u, 255u});
    SetPaintBox(world, z_float_a, (EcsUiColor){70u, 80u, 90u, 255u});
    SetPaintBox(world, nested_z, (EcsUiColor){80u, 90u, 100u, 255u});
    SetPaintBox(world, nested_flow, (EcsUiColor){90u, 100u, 110u, 255u});
    SetPaintBox(world, nested_float, (EcsUiColor){100u, 110u, 120u, 255u});
    SetPaintBox(world, outer_later, (EcsUiColor){105u, 115u, 125u, 255u});
    SetPaintBox(world, z_float_b, (EcsUiColor){110u, 120u, 130u, 255u});
    ecs_set(world, bevel, EcsUiBoxStyle, {
        .background = {120u, 130u, 140u, 255u},
        .bevel = ECS_UI_BEVEL_RAISED,
        .bevel_light = {240u, 245u, 250u, 230u},
        .bevel_dark = {20u, 25u, 30u, 210u},
    });
    SetPaintBox(world, later_base, (EcsUiColor){130u, 140u, 150u, 255u});
    ecs_set(world, offset, EcsUiVisual, {
        .opacity = 1.0f,
        .offset_x = 3.5f,
        .offset_y = 1.5f,
    });
    ecs_set(world, z_float_a, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .width = 24.5f,
        .height = 18.5f,
    });
    ecs_set(world, nested_float, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .width = 10.5f,
        .height = 8.5f,
    });
    ecs_set(world, z_float_b, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_END,
        .parent_y = ECS_UI_ALIGN_END,
        .child_x = ECS_UI_ALIGN_END,
        .child_y = ECS_UI_ALIGN_END,
        .width = 14.5f,
        .height = 10.5f,
    });
    return result;
}

static int BuildClipPaintTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set clip scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t outer = EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "PaintClipOuter",
                .preferred_width = 82.5f,
                .preferred_height = 44.5f,
                .padding = 2.5f,
            },
            .axes = ECS_UI_SCROLL_AXIS_Y,
        });
    ecs_entity_t child = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintClipChild",
            .preferred_width = 70.5f,
            .preferred_height = 64.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t inner = EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "PaintClipInner",
                .preferred_width = 52.5f,
                .preferred_height = 24.5f,
                .padding = 1.5f,
            },
            .axes = ECS_UI_SCROLL_AXIS_Y,
        });
    ecs_entity_t inner_child = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintClipInnerChild",
            .preferred_width = 44.5f,
            .preferred_height = 38.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t zstack = EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintClipZ",
            .preferred_width = 56.5f,
            .preferred_height = 28.5f,
        });
    ecs_entity_t z_flow = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintClipZFlow",
            .preferred_width = 32.5f,
            .preferred_height = 20.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t z_float = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintClipZFloat",
            .preferred_width = 20.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "clip paint builder failed");

    SetPaintBox(world, outer, (EcsUiColor){30u, 40u, 50u, 255u});
    SetPaintBox(world, child, (EcsUiColor){40u, 50u, 60u, 255u});
    SetPaintBox(world, inner, (EcsUiColor){50u, 60u, 70u, 255u});
    SetPaintBox(world, inner_child, (EcsUiColor){60u, 70u, 80u, 255u});
    SetPaintBox(world, zstack, (EcsUiColor){70u, 80u, 90u, 255u});
    SetPaintBox(world, z_flow, (EcsUiColor){80u, 90u, 100u, 255u});
    SetPaintBox(world, z_float, (EcsUiColor){90u, 100u, 110u, 255u});
    ecs_set(world, z_float, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_END,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_END,
        .child_y = ECS_UI_ALIGN_START,
        .width = 20.5f,
        .height = 12.5f,
    });
    return result;
}

static int RequirePaintItemCommon(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    uint16_t expected_role,
    uint16_t expected_part,
    uint16_t expected_primitive,
    float expected_opacity,
    const EcsUiTreeNodeSnapshot **out_node)
{
    int result = 0;
    result |= Require(paint != NULL, "paint list missing");
    result |= Require(tree != NULL, "paint tree missing");
    if (paint == NULL || tree == NULL || index >= paint->count) {
        return result | Require(false, "paint item index missing");
    }

    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    char message[256] = {0};
    (void)snprintf(
        message,
        sizeof(message),
        "paint node missing: %s",
        node_id);
    result |= Require(node != NULL, message);
    if (node == NULL) {
        return result;
    }
    if (out_node != NULL) {
        *out_node = node;
    }

    const EcsUiPaintItem *item = &paint->items[index];
    (void)snprintf(
        message,
        sizeof(message),
        "paint source mismatch for %s",
        node_id);
    result |= Require(item->key.source == node->entity, message);
    result |= Require(
        item->key.role == expected_role,
        "paint role mismatch");
    result |= Require(item->key.part == expected_part, "paint part mismatch");
    result |= Require(
        item->key.generation == paint->generation,
        "paint key generation should match list");
    result |= Require(
        item->primitive == expected_primitive,
        "paint primitive mismatch");
    result |= Require(item->order == index, "paint order should match index");
    result |= Require(!item->clip.enabled, "paint clip should be disabled");
    if (expected_role != ECS_UI_PAINT_ROLE_BEVEL_EDGE) {
        result |= RequireNear(
            item->rect.x,
            node->layout_x,
            0.001f,
            "paint rect x should copy snapshot layout");
        result |= RequireNear(
            item->rect.y,
            node->layout_y,
            0.001f,
            "paint rect y should copy snapshot layout");
        result |= RequireNear(
            item->rect.width,
            node->layout_width,
            0.001f,
            "paint rect width should copy snapshot layout");
        result |= RequireNear(
            item->rect.height,
            node->layout_height,
            0.001f,
            "paint rect height should copy snapshot layout");
    }
    result |= RequireNear(
        item->opacity,
        expected_opacity,
        0.001f,
        "paint opacity mismatch");
    return result;
}

static int RequirePaintBoxItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    EcsUiColorF expected_fill,
    EcsUiPaintCornerRadius expected_radius,
    EcsUiPaintBorder expected_border,
    float expected_opacity)
{
    const EcsUiTreeNodeSnapshot *node = NULL;
    int result = RequirePaintItemCommon(
        paint,
        tree,
        index,
        node_id,
        ECS_UI_PAINT_ROLE_BOX,
        0u,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        expected_opacity,
        &node);
    if (paint == NULL || index >= paint->count) {
        return result;
    }
    const EcsUiPaintItem *item = &paint->items[index];
    result |= RequireColor(
        item->payload.box.fill,
        expected_fill,
        "paint box fill mismatch");
    result |= RequireRadius(
        item->payload.box.radius,
        expected_radius,
        "paint box radius mismatch");
    result |= RequireBorder(
        item->payload.box.border,
        expected_border,
        "paint box border mismatch");
    return result;
}

static int RequirePaintCustomItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    uint16_t expected_role,
    EcsUiColorF expected_color,
    float expected_opacity)
{
    const EcsUiTreeNodeSnapshot *node = NULL;
    int result = RequirePaintItemCommon(
        paint,
        tree,
        index,
        node_id,
        expected_role,
        0u,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        expected_opacity,
        &node);
    if (paint == NULL || index >= paint->count || node == NULL) {
        return result;
    }
    const EcsUiPaintItem *item = &paint->items[index];
    result |= Require(
        item->payload.custom.source == node->entity,
        "paint custom source mismatch");
    result |= RequireColor(
        item->payload.custom.color,
        expected_color,
        "paint custom color mismatch");
    return result;
}

static float TestMeasuredWordWidth(uint32_t length, uint16_t font_size)
{
    return (float)length * (float)font_size * 0.5f + 3.0f;
}

static float TestMeasuredRangeWidth(
    const char *text,
    uint32_t start,
    uint32_t end,
    uint16_t font_size,
    float scale)
{
    float width = 0.0f;
    uint32_t word_start = start;
    for (uint32_t i = start; i < end; i += 1u) {
        if (text[i] == ' ') {
            if (i > word_start) {
                width += TestMeasuredWordWidth(i - word_start, font_size);
            }
            width += TestMeasuredWordWidth(1u, font_size);
            word_start = i + 1u;
        }
    }
    if (end > word_start) {
        width += TestMeasuredWordWidth(end - word_start, font_size);
    }
    return width / scale;
}

static int RequirePaintTextRunItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    uint16_t expected_part,
    EcsUiPaintRect expected_rect,
    uint32_t expected_start,
    uint32_t expected_end,
    uint16_t expected_font_size,
    EcsUiColorF expected_color,
    float expected_opacity)
{
    int result = 0;
    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    result |= Require(node != NULL, "paint text node missing");
    if (paint == NULL || tree == NULL || node == NULL || index >= paint->count) {
        return result | Require(false, "paint text item index missing");
    }
    const EcsUiPaintItem *item = &paint->items[index];
    result |= Require(item->key.source == node->entity, "paint text source mismatch");
    result |= Require(
        item->key.role == ECS_UI_PAINT_ROLE_TEXT_RUN,
        "paint text role mismatch");
    result |= Require(item->key.part == expected_part, "paint text part mismatch");
    result |= Require(
        item->primitive == ECS_UI_PAINT_PRIMITIVE_TEXT_RUN,
        "paint text primitive mismatch");
    result |= Require(item->order == index, "paint text order mismatch");
    result |= RequireNear(item->rect.x, expected_rect.x, 0.001f, "paint text x mismatch");
    result |= RequireNear(item->rect.y, expected_rect.y, 0.001f, "paint text y mismatch");
    result |= RequireNear(
        item->rect.width,
        expected_rect.width,
        0.001f,
        "paint text width mismatch");
    result |= RequireNear(
        item->rect.height,
        expected_rect.height,
        0.001f,
        "paint text height mismatch");
    result |= RequireNear(
        item->opacity,
        expected_opacity,
        0.001f,
        "paint text opacity mismatch");
    result |= Require(
        item->payload.text_run.text == node->text.text,
        "paint text should alias snapshot text");
    result |= Require(
        item->payload.text_run.byte_start == expected_start &&
            item->payload.text_run.byte_end == expected_end,
        "paint text byte range mismatch");
    result |= Require(
        item->payload.text_run.font_size == expected_font_size,
        "paint text font size mismatch");
    result |= RequireColor(
        item->payload.text_run.color,
        expected_color,
        "paint text color mismatch");
    return result;
}

static int RequirePaintRoleBoxItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    uint16_t expected_role,
    uint16_t expected_part,
    EcsUiPaintRect expected_rect,
    EcsUiColorF expected_fill,
    float expected_opacity)
{
    int result = 0;
    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    result |= Require(node != NULL, "paint role-box node missing");
    if (paint == NULL || tree == NULL || node == NULL || index >= paint->count) {
        return result | Require(false, "paint role-box item index missing");
    }
    const EcsUiPaintItem *item = &paint->items[index];
    result |= Require(item->key.source == node->entity, "paint role-box source mismatch");
    result |= Require(item->key.role == expected_role, "paint role-box role mismatch");
    result |= Require(item->key.part == expected_part, "paint role-box part mismatch");
    result |= Require(
        item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX,
        "paint role-box primitive mismatch");
    result |= Require(item->order == index, "paint role-box order mismatch");
    result |= RequireNear(item->rect.x, expected_rect.x, 0.001f, "role-box x mismatch");
    result |= RequireNear(item->rect.y, expected_rect.y, 0.001f, "role-box y mismatch");
    result |= RequireNear(
        item->rect.width,
        expected_rect.width,
        0.001f,
        "role-box width mismatch");
    result |= RequireNear(
        item->rect.height,
        expected_rect.height,
        0.001f,
        "role-box height mismatch");
    result |= RequireNear(
        item->opacity,
        expected_opacity,
        0.001f,
        "role-box opacity mismatch");
    result |= RequireColor(
        item->payload.box.fill,
        expected_fill,
        "role-box fill mismatch");
    return result;
}

static EcsUiPaintRect TestBevelRect(
    const EcsUiTreeNodeSnapshot *node,
    uint16_t part)
{
    if (node == NULL) {
        return (EcsUiPaintRect){0};
    }
    switch (part) {
    case ECS_UI_PAINT_BEVEL_EDGE_TOP:
        return (EcsUiPaintRect){
            .x = node->layout_x,
            .y = node->layout_y,
            .width = node->layout_width,
            .height = 1.0f,
        };
    case ECS_UI_PAINT_BEVEL_EDGE_LEFT:
        return (EcsUiPaintRect){
            .x = node->layout_x,
            .y = node->layout_y,
            .width = 1.0f,
            .height = node->layout_height,
        };
    case ECS_UI_PAINT_BEVEL_EDGE_BOTTOM:
        return (EcsUiPaintRect){
            .x = node->layout_x,
            .y = node->layout_y + node->layout_height - 1.0f,
            .width = node->layout_width,
            .height = 1.0f,
        };
    case ECS_UI_PAINT_BEVEL_EDGE_RIGHT:
    default:
        return (EcsUiPaintRect){
            .x = node->layout_x + node->layout_width - 1.0f,
            .y = node->layout_y,
            .width = 1.0f,
            .height = node->layout_height,
        };
    }
}

static int RequirePaintBevelItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    uint16_t part,
    EcsUiColorF expected_color,
    float expected_opacity)
{
    const EcsUiTreeNodeSnapshot *node = NULL;
    int result = RequirePaintItemCommon(
        paint,
        tree,
        index,
        node_id,
        ECS_UI_PAINT_ROLE_BEVEL_EDGE,
        part,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        expected_opacity,
        &node);
    if (paint == NULL || index >= paint->count || node == NULL) {
        return result;
    }
    const EcsUiPaintItem *item = &paint->items[index];
    const EcsUiPaintRect expected_rect = TestBevelRect(node, part);
    result |= RequireNear(item->rect.x, expected_rect.x, 0.001f, "bevel rect x mismatch");
    result |= RequireNear(item->rect.y, expected_rect.y, 0.001f, "bevel rect y mismatch");
    result |= RequireNear(
        item->rect.width,
        expected_rect.width,
        0.001f,
        "bevel rect width mismatch");
    result |= RequireNear(
        item->rect.height,
        expected_rect.height,
        0.001f,
        "bevel rect height mismatch");
    result |= RequireColor(
        item->payload.bevel_edge.color,
        expected_color,
        "paint bevel color mismatch");
    return result;
}

static int RequireNoPaintSource(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const char *node_id)
{
    int result = 0;
    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    char message[256] = {0};
    (void)snprintf(
        message,
        sizeof(message),
        "paint node missing for absent check: %s",
        node_id);
    result |= Require(node != NULL, message);
    if (paint == NULL || node == NULL) {
        return result;
    }
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        if (paint->items[i].key.source == node->entity) {
            (void)snprintf(
                message,
                sizeof(message),
                "paint item should be absent for %s",
                node_id);
            result |= Require(false, message);
        }
    }
    return result;
}

static uint32_t FindPaintItemIndex(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const char *node_id,
    uint16_t role,
    uint16_t part)
{
    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    if (paint == NULL || node == NULL) {
        return UINT32_MAX;
    }
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        const EcsUiPaintItem *item = &paint->items[i];
        if (item->key.source == node->entity &&
                item->key.role == role &&
                item->key.part == part) {
            return i;
        }
    }
    return UINT32_MAX;
}

static int RequirePaintIndexFound(uint32_t index, const char *message)
{
    return Require(index != UINT32_MAX, message);
}

static int RequirePaintIndexBefore(
    uint32_t before,
    uint32_t after,
    const char *message)
{
    return Require(
        before != UINT32_MAX && after != UINT32_MAX && before < after,
        message);
}

static int16_t TestClampZ(int16_t base, int relative)
{
    const int value = (int)base + relative;
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static int RequirePaintItemZ(
    const EcsUiPaintList *paint,
    uint32_t index,
    int16_t expected_z,
    const char *message)
{
    if (paint == NULL || index >= paint->count) {
        return Require(false, message);
    }
    return Require(paint->items[index].z_index == expected_z, message);
}

static int RequirePaintItemClip(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *scope_id,
    bool enabled,
    const char *message)
{
    if (paint == NULL || tree == NULL || index >= paint->count) {
        return Require(false, message);
    }
    const EcsUiPaintItem *item = &paint->items[index];
    if (!enabled) {
        return Require(!item->clip.enabled, message);
    }
    const EcsUiTreeNodeSnapshot *scope = FindNode(tree, scope_id);
    int result = Require(scope != NULL, "paint clip scope node missing");
    if (scope == NULL) {
        return result;
    }
    result |= Require(item->clip.enabled, message);
    result |= Require(
        item->clip.scope == (uint32_t)scope->entity,
        "paint clip scope id mismatch");
    result |= RequireNear(
        item->clip.rect.x,
        scope->layout_x,
        0.001f,
        "paint clip x mismatch");
    result |= RequireNear(
        item->clip.rect.y,
        scope->layout_y,
        0.001f,
        "paint clip y mismatch");
    result |= RequireNear(
        item->clip.rect.width,
        scope->layout_width,
        0.001f,
        "paint clip width mismatch");
    result |= RequireNear(
        item->clip.rect.height,
        scope->layout_height,
        0.001f,
        "paint clip height mismatch");
    return result;
}

static int RequirePaintClipScopeItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    uint16_t part,
    int16_t expected_z)
{
    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    int result = Require(node != NULL, "clip scope node missing");
    if (paint == NULL || node == NULL || index >= paint->count) {
        return result | Require(false, "clip scope item missing");
    }
    const EcsUiPaintItem *item = &paint->items[index];
    result |= Require(
        item->key.source == node->entity,
        "clip scope source mismatch");
    result |= Require(
        item->key.role == ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        "clip scope role mismatch");
    result |= Require(item->key.part == part, "clip scope part mismatch");
    result |= Require(
        item->primitive == ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE,
        "clip scope primitive mismatch");
    result |= Require(item->z_index == expected_z, "clip scope z mismatch");
    result |= RequireNear(item->rect.x, node->layout_x, 0.001f, "clip scope x mismatch");
    result |= RequireNear(item->rect.y, node->layout_y, 0.001f, "clip scope y mismatch");
    result |= RequireNear(
        item->rect.width,
        node->layout_width,
        0.001f,
        "clip scope width mismatch");
    result |= RequireNear(
        item->rect.height,
        node->layout_height,
        0.001f,
        "clip scope height mismatch");
    return result;
}

static int RequirePaintList(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme)
{
    int result = 0;
    result |= Require(paint != NULL, "paint list missing");
    result |= Require(tree != NULL, "paint source tree missing");
    result |= Require(theme != NULL, "paint theme missing");
    if (paint == NULL || tree == NULL || theme == NULL) {
        return result;
    }

    result |= Require(!paint->truncated, "paint list should not truncate");
    result |= Require(paint->tree == tree->root, "paint tree id mismatch");
    result |= Require(
        paint->generation == tree->generation,
        "paint generation should match snapshot");
    result |= Require(
        paint->count == 16u,
        "paint list should contain stage 7.5 paint items");
    result |= RequirePaintBoxItem(
        paint,
        tree,
        0u,
        "PaintRoot",
        TestColor(
            theme->root_background.r,
            theme->root_background.g,
            theme->root_background.b,
            theme->root_background.a),
        TestRadius(0.0f),
        TestNoBorder(),
        1.0f);
    result |= RequirePaintBoxItem(
        paint,
        tree,
        1u,
        "PaintFit",
        TestColor(10u, 20u, 30u, 255u),
        TestRadius(25.0f),
        TestBorder(TestColor(100u, 110u, 120u, 230u), 2.25f, 1.5f, 1.5f, 3.5f),
        0.75f);
    result |= RequirePaintCustomItem(
        paint,
        tree,
        2u,
        "PaintCustom",
        ECS_UI_PAINT_ROLE_CUSTOM,
        TestColor(
            theme->surface_subtle.r,
            theme->surface_subtle.g,
            theme->surface_subtle.b,
            theme->surface_subtle.a),
        0.75f);
    const float scale = tree->scale > 0.0f ? tree->scale : 1.0f;
    const EcsUiTreeNodeSnapshot *single = FindNode(tree, "PaintSingleText");
    const EcsUiTreeNodeSnapshot *multi = FindNode(tree, "PaintMultiText");
    const uint16_t body_font = (uint16_t)(18.0f * scale);
    const uint16_t caption_font = (uint16_t)(13.0f * scale);
    if (single != NULL) {
        const float single_width = (
            TestMeasuredWordWidth(3u, body_font) +
            TestMeasuredWordWidth(1u, body_font) +
            TestMeasuredWordWidth(3u, body_font)) / scale;
        const float single_height = ((float)body_font + 4.0f) / scale;
        result |= RequirePaintTextRunItem(
            paint,
            tree,
            3u,
            "PaintSingleText",
            0u,
            (EcsUiPaintRect){
                .x = single->layout_x,
                .y = single->layout_y,
                .width = single_width,
                .height = single_height,
            },
            0u,
            7u,
            body_font,
            TestColor(
                theme->text.r,
                theme->text.g,
                theme->text.b,
                theme->text.a),
            0.75f);
    }
    if (multi != NULL) {
        const float line0_width =
            TestMeasuredWordWidth(2u, caption_font) / scale;
        const float line2_width = (
            TestMeasuredWordWidth(3u, caption_font) +
            TestMeasuredWordWidth(1u, caption_font)) / scale;
        const float element_width = line2_width;
        const float line_height = ((float)caption_font + 4.0f) / scale;
        const float element_height = line_height * 3.0f;
        const float element_y =
            multi->layout_y + multi->layout_height - element_height;
        result |= RequirePaintTextRunItem(
            paint,
            tree,
            4u,
            "PaintMultiText",
            0u,
            (EcsUiPaintRect){
                .x = multi->layout_x + (element_width - line0_width) * 0.5f,
                .y = element_y,
                .width = line0_width,
                .height = line_height,
            },
            0u,
            2u,
            caption_font,
            TestColor(
                theme->text_muted.r,
                theme->text_muted.g,
                theme->text_muted.b,
                theme->text_muted.a),
            0.75f);
        result |= RequirePaintTextRunItem(
            paint,
            tree,
            5u,
            "PaintMultiText",
            2u,
            (EcsUiPaintRect){
                .x = multi->layout_x,
                .y = element_y + line_height * 2.0f,
                .width = line2_width,
                .height = line_height,
            },
            4u,
            8u,
            caption_font,
            TestColor(
                theme->text_muted.r,
                theme->text_muted.g,
                theme->text_muted.b,
                theme->text_muted.a),
            0.75f);
    }
    result |= RequirePaintBoxItem(
        paint,
        tree,
        6u,
        "PaintPress",
        TestColor(40u, 50u, 60u, 200u),
        TestRadius(6.25f),
        TestBorder(TestColor(130u, 140u, 150u, 240u), 2.0f, 2.5f, 2.0f, 2.0f),
        0.75f);
    result |= RequirePaintCustomItem(
        paint,
        tree,
        7u,
        "PaintIcon",
        ECS_UI_PAINT_ROLE_ICON,
        TestColor(0u, 0u, 0u, 255u),
        0.75f);
    result |= RequirePaintBoxItem(
        paint,
        tree,
        8u,
        "PaintButton",
        TestColor(
            theme->button_primary.r,
            theme->button_primary.g,
            theme->button_primary.b,
            theme->button_primary.a),
        TestRadius(theme->radius),
        TestNoBorder(),
        1.0f);
    result |= RequirePaintCustomItem(
        paint,
        tree,
        9u,
        "PaintAfter",
        ECS_UI_PAINT_ROLE_CUSTOM,
        TestColor(
            theme->surface_subtle.r,
            theme->surface_subtle.g,
            theme->surface_subtle.b,
            theme->surface_subtle.a),
        1.0f);
    result |= RequirePaintCustomItem(
        paint,
        tree,
        10u,
        "PaintNine",
        ECS_UI_PAINT_ROLE_NINE_SLICE,
        TestColor(200u, 210u, 220u, 180u),
        1.0f);
    result |= RequirePaintBoxItem(
        paint,
        tree,
        11u,
        "PaintBevel",
        TestColor(90u, 100u, 110u, 255u),
        TestRadius(0.0f),
        TestNoBorder(),
        1.0f);
    result |= RequirePaintBevelItem(
        paint,
        tree,
        12u,
        "PaintBevel",
        ECS_UI_PAINT_BEVEL_EDGE_TOP,
        TestColor(240u, 245u, 250u, 230u),
        1.0f);
    result |= RequirePaintBevelItem(
        paint,
        tree,
        13u,
        "PaintBevel",
        ECS_UI_PAINT_BEVEL_EDGE_LEFT,
        TestColor(240u, 245u, 250u, 230u),
        1.0f);
    result |= RequirePaintBevelItem(
        paint,
        tree,
        14u,
        "PaintBevel",
        ECS_UI_PAINT_BEVEL_EDGE_BOTTOM,
        TestColor(20u, 25u, 30u, 210u),
        1.0f);
    result |= RequirePaintBevelItem(
        paint,
        tree,
        15u,
        "PaintBevel",
        ECS_UI_PAINT_BEVEL_EDGE_RIGHT,
        TestColor(20u, 25u, 30u, 210u),
        1.0f);
    result |= RequireNoPaintSource(paint, tree, "PaintNoFill");
    result |= RequireNoPaintSource(paint, tree, "PaintTransparentGroup");
    result |= RequireNoPaintSource(paint, tree, "PaintTransparentChild");
    return result;
}

static int RunPaintCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create paint world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintRoot");
    result |= Require(root != 0, "paint root missing");
    result |= BuildPaintTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "paint tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f * scale,
            .height = 160.0f * scale,
        },
    };

    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "paint frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    const uint32_t first_generation = paint != NULL ? paint->generation : 0u;
    result |= RequirePaintList(paint, &tree, &theme);
    result |= Require(
        EcsUiFrameApply(world, NULL),
        "paint artifact apply failed");
    const EcsUiFrameArtifacts *artifacts =
        ecs_singleton_get(world, EcsUiFrameArtifacts);
    result |= Require(artifacts != NULL, "paint artifacts singleton missing");
    if (artifacts != NULL) {
        result |= Require(
            artifacts->paint == paint,
            "paint artifacts should point at backend list");
        result |= Require(
            artifacts->generation == first_generation,
            "paint artifacts generation mismatch");
    }

    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "second paint frame run failed");
    paint = EcsUiFrameInternalPaintList();
    result |= Require(
        paint != NULL && paint->generation == first_generation + 1u,
        "paint generation should increment per frame run");
    result |= Require(
        tree.generation == first_generation + 1u,
        "snapshot generation should increment per frame run");
    result |= RequirePaintList(paint, &tree, &theme);
    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "paint case emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int RunOrderPaintCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    int16_t base_z,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create order paint world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintOrderRoot");
    result |= Require(root != 0, "order paint root missing");
    result |= BuildOrderPaintTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "order tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 320.0f * scale,
            .height = 220.0f * scale,
        },
        .z_index = base_z,
    };
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "order paint frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    result |= Require(paint != NULL, "order paint list missing");

    const uint32_t root_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderRoot",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t before_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderBefore",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t offset_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderOffset",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t after_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderAfter",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t z_flow_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderZFlow",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t z_float_a_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderZFloatA",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t nested_flow_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderNestedFlow",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t nested_float_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderNestedFloat",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t outer_later_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderOuterLater",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t z_float_b_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderZFloatB",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t bevel_box_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderBevel",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t bevel_top_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderBevel",
        ECS_UI_PAINT_ROLE_BEVEL_EDGE,
        ECS_UI_PAINT_BEVEL_EDGE_TOP);
    const uint32_t later_base_i = FindPaintItemIndex(
        paint,
        &tree,
        "PaintOrderLaterBase",
        ECS_UI_PAINT_ROLE_BOX,
        0u);

    result |= RequirePaintIndexFound(root_i, "order root item missing");
    result |= RequirePaintIndexFound(before_i, "order before item missing");
    result |= RequirePaintIndexFound(offset_i, "order offset item missing");
    result |= RequirePaintIndexFound(after_i, "order after item missing");
    result |= RequirePaintIndexFound(z_flow_i, "order z flow item missing");
    result |= RequirePaintIndexFound(z_float_a_i, "order z float A item missing");
    result |= RequirePaintIndexFound(nested_flow_i, "order nested flow item missing");
    result |= RequirePaintIndexFound(nested_float_i, "order nested float item missing");
    result |= RequirePaintIndexFound(outer_later_i, "order outer later item missing");
    result |= RequirePaintIndexFound(z_float_b_i, "order z float B item missing");
    result |= RequirePaintIndexFound(bevel_box_i, "order bevel box item missing");
    result |= RequirePaintIndexFound(bevel_top_i, "order bevel item missing");
    result |= RequirePaintIndexFound(later_base_i, "order later base item missing");

    if (base_z == 0) {
        result |= RequirePaintIndexBefore(
            before_i,
            offset_i,
            "base z=0 visual root should draw after base subtree");
        result |= RequirePaintIndexBefore(
            after_i,
            offset_i,
            "base z=0 visual root should not interleave at dfs position");
    } else {
        result |= RequirePaintIndexBefore(
            offset_i,
            root_i,
            "base z>0 visual root should sort below base root");
        result |= RequirePaintIndexBefore(
            offset_i,
            before_i,
            "base z>0 visual root should sort below base content");
    }
    result |= RequirePaintIndexBefore(
        z_flow_i,
        z_float_a_i,
        "zstack floater should draw above flow child");
    result |= RequirePaintIndexBefore(
        z_float_a_i,
        nested_flow_i,
        "outer floating root should stay contiguous before nested flow");
    result |= RequirePaintIndexBefore(
        nested_flow_i,
        outer_later_i,
        "outer floating root should include later sibling before nested floater");
    result |= RequirePaintIndexBefore(
        outer_later_i,
        nested_float_i,
        "nested equal-z floating root should draw after outer root subtree");
    result |= RequirePaintIndexBefore(
        nested_float_i,
        z_float_b_i,
        "base+1 nested floater should draw before base+2 sibling");
    result |= RequirePaintIndexBefore(
        bevel_box_i,
        later_base_i,
        "later base sibling should draw after beveled box fill");
    result |= RequirePaintIndexBefore(
        later_base_i,
        bevel_top_i,
        "bevel root should sort after later base sibling");
    result |= RequirePaintIndexBefore(
        z_float_b_i,
        bevel_top_i,
        "bevel root should sort above zstack floaters");

    result |= RequirePaintItemZ(
        paint,
        root_i,
        TestClampZ(base_z, 0),
        "root z mismatch");
    result |= RequirePaintItemZ(
        paint,
        z_flow_i,
        TestClampZ(base_z, 0),
        "z flow z mismatch");
    result |= RequirePaintItemZ(paint, offset_i, 0, "visual offset z mismatch");
    result |= RequirePaintItemZ(
        paint,
        z_float_a_i,
        TestClampZ(base_z, 1),
        "z float A z mismatch");
    result |= RequirePaintItemZ(
        paint,
        nested_flow_i,
        TestClampZ(base_z, 1),
        "nested flow should stay in outer floating root");
    result |= RequirePaintItemZ(
        paint,
        outer_later_i,
        TestClampZ(base_z, 1),
        "outer later should stay in outer floating root");
    result |= RequirePaintItemZ(
        paint,
        nested_float_i,
        TestClampZ(base_z, 1),
        "nested floater z mismatch");
    result |= RequirePaintItemZ(
        paint,
        z_float_b_i,
        TestClampZ(base_z, 2),
        "z float B z mismatch");
    result |= RequirePaintItemZ(
        paint,
        bevel_box_i,
        TestClampZ(base_z, 0),
        "bevel box z mismatch");
    result |= RequirePaintItemZ(
        paint,
        later_base_i,
        TestClampZ(base_z, 0),
        "later base z mismatch");
    result |= RequirePaintItemZ(
        paint,
        bevel_top_i,
        TestClampZ(base_z, 20),
        "bevel z mismatch");
    result |= RequirePaintItemClip(
        paint,
        &tree,
        z_float_a_i,
        NULL,
        false,
        "order floater should not be clipped");
    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "order paint emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int RunClipPaintCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create clip paint world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintClipRoot");
    result |= Require(root != 0, "clip paint root missing");
    result |= BuildClipPaintTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "clip tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 220.0f * scale,
            .height = 160.0f * scale,
        },
        .z_index = 23,
    };
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "clip paint frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    result |= Require(paint != NULL, "clip paint list missing");

    const uint32_t outer_start = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipOuter",
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_CLIP_SCOPE_START);
    const uint32_t outer_end = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipOuter",
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_CLIP_SCOPE_END);
    const uint32_t outer_box = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipOuter",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t child_box = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipChild",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t inner_start = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipInner",
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_CLIP_SCOPE_START);
    const uint32_t inner_end = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipInner",
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_CLIP_SCOPE_END);
    const uint32_t inner_box = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipInner",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t inner_child = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipInnerChild",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t z_flow = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipZFlow",
        ECS_UI_PAINT_ROLE_BOX,
        0u);
    const uint32_t z_float = FindPaintItemIndex(
        paint,
        &tree,
        "PaintClipZFloat",
        ECS_UI_PAINT_ROLE_BOX,
        0u);

    result |= RequirePaintClipScopeItem(
        paint,
        &tree,
        outer_start,
        "PaintClipOuter",
        ECS_UI_PAINT_CLIP_SCOPE_START,
        23);
    result |= RequirePaintClipScopeItem(
        paint,
        &tree,
        outer_end,
        "PaintClipOuter",
        ECS_UI_PAINT_CLIP_SCOPE_END,
        23);
    result |= RequirePaintClipScopeItem(
        paint,
        &tree,
        inner_start,
        "PaintClipInner",
        ECS_UI_PAINT_CLIP_SCOPE_START,
        23);
    result |= RequirePaintClipScopeItem(
        paint,
        &tree,
        inner_end,
        "PaintClipInner",
        ECS_UI_PAINT_CLIP_SCOPE_END,
        23);

    result |= RequirePaintIndexBefore(
        outer_start,
        outer_box,
        "outer clip start should precede outer box");
    result |= RequirePaintIndexBefore(
        outer_box,
        child_box,
        "outer box should precede clipped child");
    result |= RequirePaintIndexBefore(
        inner_start,
        inner_box,
        "inner clip start should precede inner box");
    result |= RequirePaintIndexBefore(
        inner_child,
        inner_end,
        "inner clip end should follow inner subtree");
    result |= RequirePaintIndexBefore(
        z_flow,
        outer_end,
        "outer clip end should follow non-floating zstack flow");
    result |= RequirePaintIndexBefore(
        outer_end,
        z_float,
        "floating descendant should sort outside clip scope");

    result |= RequirePaintItemClip(
        paint,
        &tree,
        outer_box,
        "PaintClipOuter",
        true,
        "outer box should carry outer clip");
    result |= RequirePaintItemClip(
        paint,
        &tree,
        child_box,
        "PaintClipOuter",
        true,
        "outer child should carry outer clip");
    result |= RequirePaintItemClip(
        paint,
        &tree,
        inner_box,
        "PaintClipInner",
        true,
        "inner box should carry inner clip");
    result |= RequirePaintItemClip(
        paint,
        &tree,
        inner_child,
        "PaintClipInner",
        true,
        "inner child should carry inner clip");
    result |= RequirePaintItemClip(
        paint,
        &tree,
        z_flow,
        "PaintClipOuter",
        true,
        "zstack flow child should carry outer clip");
    result |= RequirePaintItemClip(
        paint,
        &tree,
        z_float,
        NULL,
        false,
        "zstack floating child should not carry outer clip");
    result |= RequirePaintItemZ(
        paint,
        z_float,
        24,
        "zstack floating child under clip should sort at base+1");
    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "clip paint emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static EcsUiPaintRect TestFieldSegmentRect(
    const EcsUiTreeNodeSnapshot *field,
    float x,
    float width,
    float height)
{
    return (EcsUiPaintRect){
        .x = x,
        .y = field != NULL ?
            field->layout_y + (field->layout_height - height) * 0.5f :
            0.0f,
        .width = width,
        .height = height,
    };
}

static int RunTextFieldPaintCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create text-field paint world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintTextFieldRoot");
    result |= Require(root != 0, "text-field paint root missing");
    result |= BuildTextFieldPaintTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "text-field tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 360.0f * scale,
            .height = 260.0f * scale,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "text-field paint frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    result |= Require(paint != NULL, "text-field paint list missing");
    result |= Require(
        paint != NULL && paint->count == 25u,
        "text-field paint item count mismatch");

    const uint16_t body_font = (uint16_t)(18.0f * scale);
    const float body_height = ((float)body_font + 4.0f) / scale;
    const EcsUiColorF body_color = TestColor(
        theme.text.r,
        theme.text.g,
        theme.text.b,
        theme.text.a);
    const EcsUiColorF selection_color = TestColor(
        theme.button_primary.r,
        theme.button_primary.g,
        theme.button_primary.b,
        theme.button_primary.a);
    const EcsUiColorF selection_fill = {
        .r = selection_color.r,
        .g = selection_color.g,
        .b = selection_color.b,
        .a = selection_color.a * 0.35f,
    };

    const EcsUiTreeNodeSnapshot *field = FindNode(&tree, "PaintFieldUnfocused");
    float x = field != NULL ? field->layout_x + 12.0f : 0.0f;
    const float unfocused_width =
        TestMeasuredRangeWidth("abc def", 0u, 7u, body_font, scale);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        2u,
        "PaintFieldUnfocusedValue",
        0u,
        TestFieldSegmentRect(field, x, unfocused_width, body_height),
        0u,
        7u,
        body_font,
        body_color,
        1.0f);

    field = FindNode(&tree, "PaintFieldCursorStart");
    x = field != NULL ? field->layout_x + 12.0f : 0.0f;
    result |= RequirePaintRoleBoxItem(
        paint,
        &tree,
        4u,
        "PaintFieldCursorStartValue",
        ECS_UI_PAINT_ROLE_CARET,
        0u,
        TestFieldSegmentRect(field, x, 2.0f, 26.0f),
        body_color,
        1.0f);
    x += 2.0f;
    const float xy_width = TestMeasuredRangeWidth("xy", 0u, 2u, body_font, scale);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        5u,
        "PaintFieldCursorStartValue",
        (uint16_t)(1u << 8u),
        TestFieldSegmentRect(field, x, xy_width, body_height),
        0u,
        2u,
        body_font,
        body_color,
        1.0f);

    field = FindNode(&tree, "PaintFieldCursorEnd");
    x = field != NULL ? field->layout_x + 12.0f : 0.0f;
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        7u,
        "PaintFieldCursorEndValue",
        0u,
        TestFieldSegmentRect(field, x, xy_width, body_height),
        0u,
        2u,
        body_font,
        body_color,
        1.0f);
    x += xy_width;
    result |= RequirePaintRoleBoxItem(
        paint,
        &tree,
        8u,
        "PaintFieldCursorEndValue",
        ECS_UI_PAINT_ROLE_CARET,
        (uint16_t)(1u << 8u),
        TestFieldSegmentRect(field, x, 2.5f, 26.0f),
        body_color,
        1.0f);

    field = FindNode(&tree, "PaintFieldSelectionStart");
    x = field != NULL ? field->layout_x + 12.0f : 0.0f;
    const float a_width = TestMeasuredRangeWidth("abcde", 0u, 1u, body_font, scale);
    const float bcd_width = TestMeasuredRangeWidth("abcde", 1u, 4u, body_font, scale);
    const float e_width = TestMeasuredRangeWidth("abcde", 4u, 5u, body_font, scale);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        10u,
        "PaintFieldSelectionStartValue",
        0u,
        TestFieldSegmentRect(field, x, a_width, body_height),
        0u,
        1u,
        body_font,
        body_color,
        1.0f);
    x += a_width;
    result |= RequirePaintRoleBoxItem(
        paint,
        &tree,
        11u,
        "PaintFieldSelectionStartValue",
        ECS_UI_PAINT_ROLE_CARET,
        (uint16_t)(1u << 8u),
        TestFieldSegmentRect(field, x, 2.0f, 26.0f),
        body_color,
        1.0f);
    x += 2.0f;
    result |= RequirePaintRoleBoxItem(
        paint,
        &tree,
        12u,
        "PaintFieldSelectionStartValue",
        ECS_UI_PAINT_ROLE_SELECTION,
        (uint16_t)(2u << 8u),
        TestFieldSegmentRect(field, x, bcd_width, body_height),
        selection_fill,
        1.0f);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        13u,
        "PaintFieldSelectionStartValue",
        (uint16_t)(2u << 8u),
        TestFieldSegmentRect(field, x, bcd_width, body_height),
        1u,
        4u,
        body_font,
        body_color,
        1.0f);
    x += bcd_width;
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        14u,
        "PaintFieldSelectionStartValue",
        (uint16_t)(3u << 8u),
        TestFieldSegmentRect(field, x, e_width, body_height),
        4u,
        5u,
        body_font,
        body_color,
        1.0f);

    field = FindNode(&tree, "PaintFieldSelectionEnd");
    x = field != NULL ? field->layout_x + 12.0f : 0.0f;
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        16u,
        "PaintFieldSelectionEndValue",
        0u,
        TestFieldSegmentRect(field, x, a_width, body_height),
        0u,
        1u,
        body_font,
        body_color,
        1.0f);
    x += a_width;
    result |= RequirePaintRoleBoxItem(
        paint,
        &tree,
        17u,
        "PaintFieldSelectionEndValue",
        ECS_UI_PAINT_ROLE_SELECTION,
        (uint16_t)(1u << 8u),
        TestFieldSegmentRect(field, x, bcd_width, body_height),
        selection_fill,
        1.0f);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        18u,
        "PaintFieldSelectionEndValue",
        (uint16_t)(1u << 8u),
        TestFieldSegmentRect(field, x, bcd_width, body_height),
        1u,
        4u,
        body_font,
        body_color,
        1.0f);
    x += bcd_width;
    result |= RequirePaintRoleBoxItem(
        paint,
        &tree,
        19u,
        "PaintFieldSelectionEndValue",
        ECS_UI_PAINT_ROLE_CARET,
        (uint16_t)(2u << 8u),
        TestFieldSegmentRect(field, x, 2.5f, 26.0f),
        body_color,
        1.0f);
    x += 2.5f;
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        20u,
        "PaintFieldSelectionEndValue",
        (uint16_t)(3u << 8u),
        TestFieldSegmentRect(field, x, e_width, body_height),
        4u,
        5u,
        body_font,
        body_color,
        1.0f);

    field = FindNode(&tree, "PaintFieldStyled");
    x = field != NULL ? field->layout_x + 12.0f : 0.0f;
    const uint16_t styled_font = (uint16_t)(20.5f * scale);
    const float styled_height = ((float)styled_font + 4.0f) / scale;
    const float styled_width =
        TestMeasuredRangeWidth("hi", 0u, 2u, styled_font, scale);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        22u,
        "PaintFieldStyledValue",
        0u,
        TestFieldSegmentRect(field, x, styled_width, styled_height),
        0u,
        2u,
        styled_font,
        TestColor(120u, 20u, 10u, 240u),
        1.0f);

    field = FindNode(&tree, "PaintFieldFractionalPadding");
    const float truncated_padding =
        (float)((uint16_t)(12.25f * scale)) / scale;
    x = field != NULL ? field->layout_x + truncated_padding : 0.0f;
    const float padding_width =
        TestMeasuredRangeWidth("p", 0u, 1u, body_font, scale);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        24u,
        "PaintFieldFractionalPaddingValue",
        0u,
        TestFieldSegmentRect(field, x, padding_width, body_height),
        0u,
        1u,
        body_font,
        body_color,
        1.0f);

    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "text-field paint emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int RunTextFieldOrphanValueCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create orphan text-field world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintTextFieldOrphanRoot");
    result |= Require(root != 0, "orphan text-field root missing");
    result |= Require(
        EcsUiSetScale(world, root, scale),
        "failed to set orphan text-field scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t field = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintFieldOrphanRef",
            .preferred_height = 30.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t orphan = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintFieldOrphanValue",
            .text = "orphan",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "orphan text-field builder failed");
    ecs_set(world, field, EcsUiTextFieldView, {
        .value_node = orphan,
        .focused = false,
    });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "orphan tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 220.0f * scale,
            .height = 120.0f * scale,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "orphan text-field frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    result |= Require(paint != NULL, "orphan paint list missing");
    result |= Require(
        paint != NULL && paint->count == 3u,
        "orphan text-field should not synthesize a value run");

    const EcsUiTreeNodeSnapshot *text =
        FindNode(&tree, "PaintFieldOrphanValue");
    const uint16_t body_font = (uint16_t)(18.0f * scale);
    const float body_height = ((float)body_font + 4.0f) / scale;
    const float width =
        TestMeasuredRangeWidth("orphan", 0u, 6u, body_font, scale);
    result |= RequirePaintTextRunItem(
        paint,
        &tree,
        2u,
        "PaintFieldOrphanValue",
        0u,
        (EcsUiPaintRect){
            .x = text != NULL ? text->layout_x : 0.0f,
            .y = text != NULL ?
                text->layout_y + (text->layout_height - body_height) * 0.5f :
                0.0f,
            .width = width,
            .height = body_height,
        },
        0u,
        6u,
        body_font,
        TestColor(
            theme.text.r,
            theme.text.g,
            theme.text.b,
            theme.text.a),
        1.0f);
    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "orphan text-field emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int RunPlacedTextPaintCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create placed text paint world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintPlacedTextRoot");
    result |= Require(root != 0, "placed text root missing");
    result |= BuildPlacedTextPaintTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "placed text tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 180.0f * scale,
            .height = 100.0f * scale,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "placed text frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    result |= Require(paint != NULL, "placed text paint missing");
    result |= Require(
        paint != NULL && paint->count == 2u,
        "placed text paint item count mismatch");

    const EcsUiTreeNodeSnapshot *text = FindNode(&tree, "PaintPlacedText");
    const uint16_t font = (uint16_t)(18.0f * scale);
    const float width = TestMeasuredRangeWidth("zz", 0u, 2u, font, scale);
    const float height = ((float)font + 4.0f) / scale;
    if (text != NULL) {
        result |= RequirePaintTextRunItem(
            paint,
            &tree,
            1u,
            "PaintPlacedText",
            0u,
            (EcsUiPaintRect){
                .x = text->layout_x + (text->layout_width - width) * 0.5f,
                .y = text->layout_y + text->layout_height - height,
                .width = width,
                .height = height,
            },
            0u,
            2u,
            font,
            TestColor(
                theme.text.r,
                theme.text.g,
                theme.text.b,
                theme.text.a),
            1.0f);
    }
    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "placed text emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int TestSourceTruncationPropagates(void)
{
    EcsUiTreeSnapshot tree = {
        .root = 101u,
        .scale = 1.0f,
        .generation = 77u,
        .count = 1u,
        .truncated = true,
    };
    tree.nodes[0] = (EcsUiTreeNodeSnapshot){
        .entity = 101u,
        .parent = 0u,
        .kind = ECS_UI_NODE_ROOT,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .visual = {.opacity = 1.0f},
        .layout_width = 10.0f,
        .layout_height = 10.0f,
        .has_layout = true,
    };

    static EcsUiPaintList paint;
    memset(&paint, 0, sizeof(paint));
    EcsUiTheme theme = EcsUiThemeDefault();
    int result = 0;
    result |= Require(
        EcsUiPaintListBuild(&paint, &tree, &theme, TestMeasureText, NULL, 0),
        "source-truncated paint build should fit paint capacity");
    result |= Require(
        paint.truncated,
        "source tree truncation should propagate to paint list");
    result |= Require(paint.count == 1u, "source-truncated paint count mismatch");
    return result;
}

static int TestOpacityCullDirect(void)
{
    EcsUiTreeSnapshot tree = {
        .root = 202u,
        .scale = 1.0f,
        .generation = 88u,
        .count = 1u,
    };
    tree.nodes[0] = (EcsUiTreeNodeSnapshot){
        .entity = 202u,
        .parent = 0u,
        .kind = ECS_UI_NODE_VSTACK,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .box_style = {
            .background = {10u, 20u, 30u, 255u},
        },
        .visual = {.opacity = 0.005f},
        .layout_width = 10.0f,
        .layout_height = 10.0f,
        .has_box_style = true,
        .has_layout = true,
    };

    static EcsUiPaintList paint;
    memset(&paint, 0, sizeof(paint));
    EcsUiTheme theme = EcsUiThemeDefault();
    int result = 0;
    result |= Require(
        EcsUiPaintListBuild(&paint, &tree, &theme, TestMeasureText, NULL, 0),
        "direct opacity-cull paint build should fit paint capacity");
    result |= Require(
        paint.count == 0u,
        "direct opacity-cull paint build should skip low-opacity item");
    result |= Require(
        !paint.truncated,
        "direct opacity-cull paint build should not truncate");
    return result;
}

static int TestGenerationHeldOnSolverFailure(TestFrameErrors *errors)
{
    int result = 0;
    if (errors == NULL) {
        return Require(false, "missing paint error recorder");
    }

    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create generation world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintRoot");
    result |= Require(root != 0, "paint failure root missing");
    result |= BuildPaintTree(world, root, 1.0f);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "paint failure tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f,
            .height = 160.0f,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(240.0f, 160.0f);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "last-good native paint run failed");
    result |= RequirePaintList(EcsUiFrameInternalPaintList(), &tree, &theme);
    result |= Require(EcsUiFrameApply(world, NULL), "last-good apply failed");

    const EcsUiPaintList *last_good = EcsUiFrameInternalPaintList();
    const uint32_t last_generation =
        last_good != NULL ? last_good->generation : 0u;
    const EcsUiFrameArtifacts *artifacts =
        ecs_singleton_get(world, EcsUiFrameArtifacts);
    result |= Require(
        artifacts != NULL && artifacts->paint == last_good,
        "last-good artifact missing before failure");

    EcsUiTreeSnapshot failed_tree = tree;
    failed_tree.count = 0u;
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&failed_tree, &theme, &options, NULL, NULL) == NULL,
        "native solver failure should abort frame run");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_INTERNAL,
        "native solver failure should report an internal frame error");
    result |= Require(
        failed_tree.generation == last_generation,
        "failed run should keep snapshot generation at last good value");
    result |= Require(
        EcsUiFrameInternalPaintList() == last_good,
        "failed run should keep last-good paint list active");
    result |= Require(
        EcsUiFrameApply(world, NULL),
        "failed-run artifact apply should keep last-good handle");
    artifacts = ecs_singleton_get(world, EcsUiFrameArtifacts);
    result |= Require(
        artifacts != NULL && artifacts->paint == last_good,
        "artifact should still point at last-good paint list after failure");
    result |= Require(
        artifacts != NULL && artifacts->generation == last_generation &&
            last_good->generation == last_generation,
        "artifact generation should stay at last good value after failure");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int TestPaintCapacityFailure(TestFrameErrors *errors)
{
    int result = 0;
    if (errors == NULL) {
        return Require(false, "missing paint capacity error recorder");
    }

    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create paint capacity world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintRoot");
    result |= Require(root != 0, "paint capacity root missing");
    result |= BuildPaintTree(world, root, 1.0f);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "paint capacity tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f,
            .height = 160.0f,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(240.0f, 160.0f);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    EcsUiFrameInternalSetPaintItemCapacity(0u);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "paint capacity last-good run failed");
    const EcsUiPaintList *last_good = EcsUiFrameInternalPaintList();
    const uint32_t last_generation =
        last_good != NULL ? last_good->generation : 0u;
    result |= RequirePaintList(last_good, &tree, &theme);

    static EcsUiPaintList local;
    memset(&local, 0, sizeof(local));
    result |= Require(
        !EcsUiPaintListBuildWithCapacity(
            &local,
            &tree,
            &theme,
            TestMeasureText,
            NULL,
            0,
            2u),
        "direct paint build should fail a tiny item capacity");
    result |= Require(
        local.truncated,
        "direct paint build should mark list truncated on item overflow");

    EcsUiFrameInternalSetPaintItemCapacity(2u);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "paint capacity overflow should keep frame draw list alive");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY,
        "paint capacity overflow should report element capacity error");
    result |= Require(
        EcsUiFrameInternalPaintList() == last_good,
        "paint capacity overflow should keep last-good paint list active");
    result |= Require(
        tree.generation == last_generation,
        "paint capacity overflow should restore last-good snapshot generation");
    result |= Require(
        last_good != NULL && !last_good->truncated,
        "active last-good paint list should not expose failed overflow scratch");

    EcsUiFrameInternalSetPaintItemCapacity(0u);
    ecs_fini(world);
    return result;
}

static int TestTextFieldScopeGuard(TestFrameErrors *errors)
{
    int result = 0;
    if (errors == NULL) {
        return Require(false, "missing text-field guard error recorder");
    }

    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create text-field guard world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintTextFieldGuardRoot");
    result |= Require(root != 0, "text-field guard root missing");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t field = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintTextFieldGuard",
            .preferred_height = 30.5f,
        });
    ecs_entity_t value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PaintTextFieldGuardValue",
            .text = "guard",
            .role = ECS_UI_TEXT_BODY,
        });
    ecs_entity_t placed_icon = EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "PaintTextFieldGuardIcon",
            .name = "guard.icon",
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "text-field guard builder failed");
    ecs_set(world, placed_icon, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .width = 16.0f,
        .height = 16.0f,
    });
    ecs_set(world, field, EcsUiTextFieldView, {
        .value_node = value,
        .focused = true,
        .cursor = 2u,
    });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "guard tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 180.0f,
            .height = 80.0f,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(180.0f, 80.0f);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "text-field guard should not abort live draw list");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY,
        "text-field guard should report element capacity error kind");
    result |= Require(
        strstr(errors->last_message, "text-field scope") != NULL,
        "text-field guard should report clear scope message");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

int main(void)
{
    int result = 0;
    TestFrameErrors errors = {0};
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 320.0f,
                .surface_height = 240.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = &errors,
            }),
        "failed to initialize paint frame backend");
    EcsUiFrameBackendSetCullingEnabled(false);

    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY, 1.0f, &errors);
    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY, 2.0f, &errors);
    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE, 1.0f, &errors);
    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE, 2.0f, &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        0,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        2.0f,
        37,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        32760,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        32767,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        0,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        2.0f,
        37,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        32760,
        &errors);
    result |= RunOrderPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        32767,
        &errors);
    result |= RunClipPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        &errors);
    result |= RunClipPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        2.0f,
        &errors);
    result |= RunClipPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        &errors);
    result |= RunClipPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        2.0f,
        &errors);
    result |= RunTextFieldPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        &errors);
    result |= RunTextFieldPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        2.0f,
        &errors);
    result |= RunTextFieldPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        &errors);
    result |= RunTextFieldPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        2.0f,
        &errors);
    result |= RunTextFieldOrphanValueCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        &errors);
    result |= RunTextFieldOrphanValueCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        2.0f,
        &errors);
    result |= RunTextFieldOrphanValueCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        &errors);
    result |= RunTextFieldOrphanValueCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        2.0f,
        &errors);
    result |= RunPlacedTextPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        1.0f,
        &errors);
    result |= RunPlacedTextPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        2.0f,
        &errors);
    result |= RunPlacedTextPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        1.0f,
        &errors);
    result |= RunPlacedTextPaintCase(
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        2.0f,
        &errors);
    result |= TestSourceTruncationPropagates();
    result |= TestOpacityCullDirect();
    result |= TestGenerationHeldOnSolverFailure(&errors);
    result |= TestPaintCapacityFailure(&errors);
    result |= TestTextFieldScopeGuard(&errors);

    EcsUiFrameBackendShutdown();
    return result;
}
