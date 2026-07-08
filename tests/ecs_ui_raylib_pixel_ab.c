#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_raylib.h"
#include "../src/ecs_ui_frame_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SKIP 77
#define TEST_RENDER_WIDTH 360
#define TEST_RENDER_HEIGHT 240

typedef struct PixelImage {
    Color *pixels;
    int width;
    int height;
} PixelImage;

typedef struct PixelDiff {
    uint32_t count;
    int first_x;
    int first_y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
} PixelDiff;

typedef struct PixelFixture {
    const char *name;
    int (*build)(ecs_world_t *world, ecs_entity_t root, float scale);
    int16_t base_z;
    float bounds_x;
    float bounds_y;
    uint16_t forced_letter_spacing;
    float culling_width_factor;
    float culling_height_factor;
} PixelFixture;

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static EcsUiSize TestMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    (void)utf8;
    (void)user_data;
    const float font_size =
        spec != NULL && spec->font_size > 0.0f ? spec->font_size : 16.0f;
    const float chars = length > 0 ? (float)length : 0.0f;
    return (EcsUiSize){
        .width = chars * font_size * 0.5f + 3.0f,
        .height = font_size + 4.0f,
    };
}

static void TestFrameHandleError(
    EcsUiFrameErrorKind kind,
    const char *message,
    void *user_data)
{
    (void)user_data;
    (void)fprintf(
        stderr,
        "frame error: kind=%d message=%s\n",
        (int)kind,
        message != NULL ? message : "");
}

static bool TestDisplayAvailable(void)
{
#if defined(_WIN32) || defined(__APPLE__)
    return true;
#else
    const char *display = getenv("DISPLAY");
    const char *wayland = getenv("WAYLAND_DISPLAY");
    return (display != NULL && display[0] != '\0') ||
        (wayland != NULL && wayland[0] != '\0');
#endif
}

static ecs_world_t *TestCreateWorld(void)
{
    ecs_world_t *world = ecs_init();
    if (world != NULL) {
        EcsUiImport(world);
    }
    return world;
}

static Color TestFade(Color color, float opacity)
{
    if (opacity < 0.0f) {
        opacity = 0.0f;
    } else if (opacity > 1.0f) {
        opacity = 1.0f;
    }
    color.a = (unsigned char)roundf((float)color.a * opacity);
    return color;
}

static void TestCustomDraw(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data)
{
    (void)user_data;
    Color color = {190u, 72u, 58u, 255u};
    if (node != NULL && node->kind == ECS_UI_NODE_ICON) {
        color = (Color){222u, 176u, 54u, 255u};
    }
    DrawRectangleRec(context->physical_bounds, TestFade(color, opacity));
}

static void TestIconDraw(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data)
{
    (void)node;
    (void)user_data;
    DrawRectangleRec(
        context->physical_bounds,
        TestFade((Color){238u, 186u, 55u, 255u}, opacity));
    DrawLineEx(
        (Vector2){
            .x = context->physical_bounds.x,
            .y = context->physical_bounds.y,
        },
        (Vector2){
            .x = context->physical_bounds.x + context->physical_bounds.width,
            .y = context->physical_bounds.y + context->physical_bounds.height,
        },
        1.0f,
        TestFade((Color){78u, 58u, 30u, 255u}, opacity));
}

static void TestNineSliceDraw(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data)
{
    (void)node;
    (void)user_data;
    DrawRectangleRec(
        context->physical_bounds,
        TestFade((Color){72u, 145u, 196u, 255u}, opacity));
    DrawRectangleLinesEx(
        context->physical_bounds,
        1.0f,
        TestFade((Color){28u, 82u, 112u, 255u}, opacity));
}

/*
 * Custom, icon, and nine-slice callbacks are intentionally shared between the
 * bridge and direct paint render. These pixel cases validate callback
 * selection plus bounds/opacity plumbing; the callback's own drawing remains
 * the host's responsibility in both paths.
 */

static int BuildCompositeTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "scale failed");

    const EcsUiNineSliceStyle nine_slice_style = {
        .image = "pixel.frame",
        .slice_left = 3u,
        .slice_top = 4u,
        .slice_right = 5u,
        .slice_bottom = 6u,
        .scale = 1.0f,
        .tint = {180u, 190u, 200u, 210u},
    };

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t panel = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelPanel",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .padding = 1.25f,
            .gap = 2.5f,
        });
    ecs_entity_t inner = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelInner",
            .preferred_width = 32.5f,
            .preferred_height = 18.25f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PixelText",
            .text = "line one\nline two",
            .role = ECS_UI_TEXT_CAPTION,
        });
    ecs_entity_t field = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PixelTextField",
            .preferred_height = 28.5f,
        });
    ecs_entity_t field_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PixelTextFieldValue",
            .text = "aa bb",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t custom = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PixelCustom",
            .kind = "pixel.custom",
            .preferred_width = 18.5f,
            .preferred_height = 12.5f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "PixelIcon",
            .name = "pixel-icon",
        });
    ecs_entity_t nine_slice = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelNine",
            .preferred_width = 19.25f,
            .preferred_height = 11.75f,
            .nine_slice_style = &nine_slice_style,
        });
    EcsUiEnd(&builder);
    ecs_entity_t bevel = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelBevel",
            .preferred_width = 21.5f,
            .preferred_height = 13.25f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t clip = EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "PixelClip",
                .preferred_width = 54.5f,
                .preferred_height = 28.5f,
                .padding = 1.25f,
            },
            .axes = ECS_UI_SCROLL_AXIS_Y,
        });
    ecs_entity_t clip_child = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelClipChild",
            .preferred_width = 48.5f,
            .preferred_height = 72.5f,
            .gap = 1.25f,
        });
    ecs_entity_t nested_clip = EcsUiBeginHScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "PixelNestedClip",
                .preferred_width = 34.5f,
                .preferred_height = 16.5f,
                .padding = 1.25f,
            },
            .axes = ECS_UI_SCROLL_AXIS_X,
        });
    ecs_entity_t nested_clip_child = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelNestedClipChild",
            .preferred_width = 72.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t z = EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelZOrder",
            .preferred_width = 46.5f,
            .preferred_height = 26.5f,
        });
    ecs_entity_t z_flow = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelZFlow",
            .preferred_width = 28.5f,
            .preferred_height = 18.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t z_float_a = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelZFloatA",
            .preferred_width = 24.5f,
            .preferred_height = 16.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t z_float_b = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PixelZFloatB",
            .preferred_width = 22.5f,
            .preferred_height = 14.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "builder failed");

    ecs_set(world, panel, EcsUiBoxStyle, {
        .background = {22u, 33u, 44u, 235u},
        .radius = 0.25f,
        .border_width = 1.25f,
        .border_left_width = 2.5f,
        .border_bottom_width = 3.5f,
        .border_color = {80u, 90u, 100u, 240u},
    });
    ecs_set(world, panel, EcsUiVisual, {.opacity = 0.88f});
    ecs_set(world, inner, EcsUiBoxStyle, {
        .background = {120u, 130u, 140u, 240u},
        .radius = 5.75f,
        .border_width = 2.25f,
        .border_top_width = 3.25f,
        .border_color = {150u, 160u, 170u, 230u},
    });
    ecs_set(world, text, EcsUiBoxStyle, {
        .border_width = 2.0f,
        .border_color = {135u, 145u, 155u, 230u},
    });
    ecs_set(world, text, EcsUiTextLayout, {
        .align_x = ECS_UI_ALIGN_CENTER,
        .align_y = ECS_UI_ALIGN_CENTER,
    });
    ecs_set(world, field, EcsUiTextFieldView, {
        .value_node = field_value,
        .focused = true,
        .cursor = 5u,
        .selection_anchor = 3u,
        .selection_focus = 5u,
        .caret_width = 2.5f,
    });
    ecs_set(world, field, EcsUiBoxStyle, {
        .background = {55u, 65u, 75u, 230u},
        .border_width = 2.0f,
        .border_color = {145u, 155u, 165u, 230u},
    });
    ecs_set(world, custom, EcsUiBoxStyle, {
        .border_width = 2.0f,
        .border_color = {70u, 80u, 90u, 230u},
    });
    ecs_set(world, custom, EcsUiVisual, {
        .opacity = 1.0f,
        .offset_x = 3.5f,
        .offset_y = 2.25f,
    });
    ecs_set(world, nine_slice, EcsUiBoxStyle, {
        .background = {77u, 88u, 99u, 255u},
    });
    ecs_set(world, bevel, EcsUiBoxStyle, {
        .background = {30u, 40u, 50u, 240u},
        .bevel = ECS_UI_BEVEL_RAISED,
        .bevel_light = {240u, 245u, 250u, 245u},
        .bevel_dark = {20u, 25u, 30u, 235u},
    });
    ecs_set(world, clip, EcsUiBoxStyle, {
        .background = {88u, 98u, 108u, 235u},
        .border_width = 1.5f,
        .border_color = {165u, 175u, 185u, 245u},
    });
    ecs_set(world, clip, EcsUiScrollState, {
        .offset_y = -9.5f,
    });
    ecs_set(world, clip_child, EcsUiBoxStyle, {
        .background = {98u, 108u, 118u, 235u},
    });
    ecs_set(world, nested_clip, EcsUiBoxStyle, {
        .background = {68u, 78u, 88u, 235u},
        .border_width = 1.25f,
        .border_color = {178u, 188u, 198u, 245u},
    });
    ecs_set(world, nested_clip, EcsUiScrollState, {
        .offset_x = -11.5f,
    });
    ecs_set(world, nested_clip_child, EcsUiBoxStyle, {
        .background = {108u, 118u, 128u, 235u},
    });
    ecs_set(world, z, EcsUiBoxStyle, {
        .background = {108u, 118u, 128u, 235u},
    });
    ecs_set(world, z_flow, EcsUiBoxStyle, {
        .background = {118u, 128u, 138u, 245u},
    });
    ecs_set(world, z_float_a, EcsUiBoxStyle, {
        .background = {128u, 138u, 148u, 230u},
    });
    ecs_set(world, z_float_b, EcsUiBoxStyle, {
        .background = {138u, 148u, 158u, 230u},
    });
    ecs_set(world, z_float_a, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .width = 24.5f,
        .height = 16.5f,
    });
    ecs_set(world, z_float_b, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .offset_x = 5.5f,
        .offset_y = 4.5f,
        .width = 22.5f,
        .height = 14.5f,
    });
    return result;
}

static int BuildSnapTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "snap scale failed");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t panel = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "SnapPanel",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .padding = 2.25f,
            .gap = 1.75f,
        });
    ecs_entity_t box = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "SnapBox",
            .preferred_width = 33.25f,
            .preferred_height = 13.25f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "SnapText",
            .text = "snap",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "snap builder failed");

    ecs_set(world, panel, EcsUiBoxStyle, {
        .background = {62u, 72u, 82u, 255u},
        .border_width = 1.25f,
        .border_color = {182u, 192u, 202u, 255u},
        .radius = 0.5f,
    });
    ecs_set(world, box, EcsUiBoxStyle, {
        .background = {210u, 90u, 75u, 255u},
        .border_width = 1.25f,
        .border_color = {80u, 30u, 20u, 255u},
    });
    ecs_set(world, text, EcsUiTextLayout, {
        .align_x = ECS_UI_ALIGN_END,
        .align_y = ECS_UI_ALIGN_END,
    });
    return result;
}

static int BuildEqualZTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "equal-z scale failed");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t row = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EqualZRow",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .gap = 0.0f,
        });
    ecs_entity_t outer = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EqualZOuter",
            .preferred_width = 70.0f,
            .preferred_height = 60.0f,
        });
    ecs_entity_t inner = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EqualZInner",
            .preferred_width = 45.0f,
            .preferred_height = 32.0f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t cover = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EqualZCover",
            .preferred_width = 100.0f,
            .preferred_height = 70.0f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "equal-z builder failed");

    ecs_set(world, row, EcsUiBoxStyle, {
        .background = {18u, 22u, 26u, 255u},
    });
    ecs_set(world, outer, EcsUiBoxStyle, {
        .background = {215u, 60u, 55u, 255u},
    });
    ecs_set(world, outer, EcsUiVisual, {
        .opacity = 1.0f,
        .offset_x = 58.0f,
        .offset_y = 6.0f,
    });
    ecs_set(world, inner, EcsUiBoxStyle, {
        .background = {45u, 112u, 230u, 255u},
    });
    ecs_set(world, inner, EcsUiVisual, {
        .opacity = 1.0f,
        .offset_x = 8.0f,
        .offset_y = 8.0f,
    });
    ecs_set(world, cover, EcsUiBoxStyle, {
        .background = {38u, 48u, 58u, 255u},
    });
    return result;
}

static int BuildCullTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "cull scale failed");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stack = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CullStack",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .gap = 0.0f,
        });
    ecs_entity_t visible = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CullVisible",
            .preferred_width = 90.0f,
            .preferred_height = 36.0f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t spacer = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CullSpacer",
            .preferred_width = 10.0f,
            .preferred_height = 82.0f,
        });
    (void)spacer;
    EcsUiEnd(&builder);
    ecs_entity_t offscreen = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CullOffscreen",
            .preferred_width = 90.0f,
            .preferred_height = 36.0f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "cull builder failed");

    ecs_set(world, stack, EcsUiBoxStyle, {
        .background = {24u, 30u, 36u, 255u},
    });
    ecs_set(world, visible, EcsUiBoxStyle, {
        .background = {64u, 142u, 84u, 255u},
    });
    ecs_set(world, offscreen, EcsUiBoxStyle, {
        .background = {228u, 60u, 48u, 255u},
    });
    return result;
}

static PixelImage TestRenderToImage(
    int width,
    int height,
    void (*render)(void *ctx),
    void *ctx)
{
    PixelImage out = {0};
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (target.id == 0u || target.texture.id == 0u) {
        return out;
    }

    BeginTextureMode(target);
    ClearBackground((Color){9u, 11u, 13u, 255u});
    render(ctx);
    EndTextureMode();

    Image image = LoadImageFromTexture(target.texture);
    Color *pixels = LoadImageColors(image);
    if (pixels != NULL) {
        const size_t count = (size_t)width * (size_t)height;
        out.pixels = malloc(count * sizeof(Color));
        if (out.pixels != NULL) {
            memcpy(out.pixels, pixels, count * sizeof(Color));
            out.width = width;
            out.height = height;
        }
        UnloadImageColors(pixels);
    }
    UnloadImage(image);
    UnloadRenderTexture(target);
    return out;
}

static void TestFreeImage(PixelImage *image)
{
    if (image != NULL) {
        free(image->pixels);
        *image = (PixelImage){0};
    }
}

static PixelDiff TestComparePixels(
    const PixelImage *lhs,
    const PixelImage *rhs)
{
    PixelDiff diff = {
        .first_x = -1,
        .first_y = -1,
        .min_x = lhs != NULL ? lhs->width : 0,
        .min_y = lhs != NULL ? lhs->height : 0,
        .max_x = -1,
        .max_y = -1,
    };
    if (lhs == NULL || rhs == NULL || lhs->pixels == NULL ||
            rhs->pixels == NULL || lhs->width != rhs->width ||
            lhs->height != rhs->height) {
        diff.count = UINT32_MAX;
        diff.first_x = 0;
        diff.first_y = 0;
        diff.min_x = 0;
        diff.min_y = 0;
        diff.max_x = lhs != NULL ? lhs->width : 0;
        diff.max_y = lhs != NULL ? lhs->height : 0;
        return diff;
    }

    for (int y = 0; y < lhs->height; y += 1) {
        for (int x = 0; x < lhs->width; x += 1) {
            const size_t index = (size_t)y * (size_t)lhs->width + (size_t)x;
            const Color a = lhs->pixels[index];
            const Color b = rhs->pixels[index];
            if (a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a) {
                continue;
            }
            if (diff.count == 0u) {
                diff.first_x = x;
                diff.first_y = y;
            }
            diff.count += 1u;
            if (x < diff.min_x) {
                diff.min_x = x;
            }
            if (y < diff.min_y) {
                diff.min_y = y;
            }
            if (x > diff.max_x) {
                diff.max_x = x;
            }
            if (y > diff.max_y) {
                diff.max_y = y;
            }
        }
    }
    return diff;
}

static void TestPrintDiff(
    const char *fixture,
    const char *comparison,
    PixelDiff diff,
    const PixelImage *lhs,
    const PixelImage *rhs)
{
    Color lhs_color = {0};
    Color rhs_color = {0};
    if (lhs != NULL && rhs != NULL && lhs->pixels != NULL &&
            rhs->pixels != NULL && diff.first_x >= 0 && diff.first_y >= 0 &&
            diff.first_x < lhs->width && diff.first_y < lhs->height) {
        const size_t index =
            (size_t)diff.first_y * (size_t)lhs->width + (size_t)diff.first_x;
        lhs_color = lhs->pixels[index];
        rhs_color = rhs->pixels[index];
    }
    (void)fprintf(
        stderr,
        "PIXEL MISMATCH fixture=%s comparison=%s first=(%d,%d) "
        "count=%u bbox=(%d,%d)-(%d,%d) lhs=(%u,%u,%u,%u) "
        "rhs=(%u,%u,%u,%u)\n",
        fixture,
        comparison,
        diff.first_x,
        diff.first_y,
        diff.count,
        diff.min_x,
        diff.min_y,
        diff.max_x,
        diff.max_y,
        lhs_color.r,
        lhs_color.g,
        lhs_color.b,
        lhs_color.a,
        rhs_color.r,
        rhs_color.g,
        rhs_color.b,
        rhs_color.a);
}

static bool TestRectsOverlap(
    float ax,
    float ay,
    float aw,
    float ah,
    float bx,
    float by,
    float bw,
    float bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void TestPrintOverlappingCommands(
    const EcsUiDrawList *draw_list,
    const EcsUiPaintList *paint,
    const PixelDiff *diff)
{
    if (draw_list == NULL || paint == NULL || diff == NULL) {
        return;
    }
    const Clay_RenderCommandArray *commands =
        EcsUiFrameInternalClayCommands();
    if (commands != NULL) {
        for (int32_t i = 0; i < commands->length; i += 1) {
            Clay_RenderCommand *command =
                Clay_RenderCommandArray_Get((Clay_RenderCommandArray *)commands, i);
            if (command == NULL) {
                continue;
            }
            Clay_BoundingBox box = command->boundingBox;
            if (!TestRectsOverlap(
                    box.x,
                    box.y,
                    box.width,
                    box.height,
                    (float)diff->min_x,
                    (float)diff->min_y,
                    (float)(diff->max_x - diff->min_x + 1),
                    (float)(diff->max_y - diff->min_y + 1))) {
                continue;
            }
            (void)fprintf(
                stderr,
                "  bridge command[%d] type=%d z=%d box=(%.2f,%.2f %.2fx%.2f)\n",
                i,
                (int)command->commandType,
                (int)command->zIndex,
                box.x,
                box.y,
                box.width,
                box.height);
        }
        for (int32_t i = 0; i < commands->length; i += 1) {
            Clay_RenderCommand *command =
                Clay_RenderCommandArray_Get((Clay_RenderCommandArray *)commands, i);
            if (command != NULL &&
                    command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
                Clay_BoundingBox box = command->boundingBox;
                (void)fprintf(
                    stderr,
                    "  bridge text[%d] box=(%.2f,%.2f %.2fx%.2f) size=%u "
                    "len=%d\n",
                    i,
                    box.x,
                    box.y,
                    box.width,
                    box.height,
                    command->renderData.text.fontSize,
                    command->renderData.text.stringContents.length);
            }
        }
    }
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        const EcsUiPaintItem *item = &paint->items[i];
        const EcsUiPaintRect rect = item->rect;
        if (!TestRectsOverlap(
                rect.x,
                rect.y,
                rect.width,
                rect.height,
                (float)diff->min_x,
                (float)diff->min_y,
                (float)(diff->max_x - diff->min_x + 1),
                (float)(diff->max_y - diff->min_y + 1))) {
            continue;
        }
        (void)fprintf(
            stderr,
            "  paint item[%u] role=%u prim=%u z=%d rect=(%.2f,%.2f %.2fx%.2f) "
            "clip=%u enabled=%d\n",
            i,
            item->key.role,
            item->primitive,
            (int)item->z_index,
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            item->clip.scope,
            item->clip.enabled ? 1 : 0);
    }
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        const EcsUiPaintItem *item = &paint->items[i];
        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_TEXT_RUN) {
            (void)fprintf(
                stderr,
                "  paint text[%u] rect=(%.2f,%.2f %.2fx%.2f) size=%u "
                "range=%u..%u\n",
                i,
                item->rect.x,
                item->rect.y,
                item->rect.width,
                item->rect.height,
                item->payload.text_run.font_size,
                item->payload.text_run.byte_start,
                item->payload.text_run.byte_end);
        }
    }
}

static void TestForceLetterSpacing(
    const EcsUiDrawList *draw_list,
    const EcsUiPaintList *paint,
    uint16_t letter_spacing)
{
    (void)draw_list;
    if (letter_spacing == 0u) {
        return;
    }
    const Clay_RenderCommandArray *commands =
        EcsUiFrameInternalClayCommands();
    if (commands != NULL) {
        Clay_RenderCommandArray *mutable_commands =
            (Clay_RenderCommandArray *)commands;
        for (int32_t i = 0; i < mutable_commands->length; i += 1) {
            Clay_RenderCommand *command =
                Clay_RenderCommandArray_Get(mutable_commands, i);
            if (command != NULL &&
                    command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
                command->renderData.text.letterSpacing = letter_spacing;
            }
        }
    }
    EcsUiPaintList *mutable_paint = (EcsUiPaintList *)paint;
    if (mutable_paint != NULL) {
        for (uint32_t i = 0u; i < mutable_paint->count; i += 1u) {
            EcsUiPaintItem *item = &mutable_paint->items[i];
            if (item->primitive == ECS_UI_PAINT_PRIMITIVE_TEXT_RUN) {
                item->payload.text_run.letter_spacing = letter_spacing;
            }
        }
    }
}

typedef struct DrawListRenderCtx {
    const EcsUiDrawList *draw_list;
    Font *fonts;
    const EcsUiRaylibRenderContext *context;
    const EcsUiRaylibDrawOptions *options;
} DrawListRenderCtx;

static void TestRenderDrawList(void *ctx)
{
    const DrawListRenderCtx *render = ctx;
    EcsUiRaylibRenderDrawList(
        render->draw_list,
        render->fonts,
        render->context,
        render->options);
}

typedef struct PaintRenderCtx {
    const EcsUiPaintList *paint;
    const EcsUiTreeSnapshot *tree;
    Font *fonts;
    const EcsUiRaylibRenderContext *context;
    const EcsUiRaylibDrawOptions *options;
} PaintRenderCtx;

static void TestRenderPaint(void *ctx)
{
    const PaintRenderCtx *render = ctx;
    EcsUiRaylibRenderPaintList(
        render->paint,
        render->tree,
        render->fonts,
        render->context,
        render->options);
}

static int RunPixelCase(
    const PixelFixture *fixture,
    float scale,
    bool culling_enabled)
{
    int result = 0;
    ecs_world_t *world = TestCreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, fixture->name);
    result |= Require(root != 0, "root missing");
    result |= fixture->build(world, root, scale);

    const int width = (int)ceilf((float)TEST_RENDER_WIDTH * scale);
    const int height = (int)ceilf((float)TEST_RENDER_HEIGHT * scale);
    const float culling_width_factor =
        fixture->culling_width_factor > 0.0f ?
            fixture->culling_width_factor :
            1.0f;
    const float culling_height_factor =
        fixture->culling_height_factor > 0.0f ?
            fixture->culling_height_factor :
            1.0f;
    const float culling_width = culling_enabled ?
        floorf((float)width * culling_width_factor) :
        (float)width;
    const float culling_height = culling_enabled ?
        floorf((float)height * culling_height_factor) :
        (float)height;
    const bool shrink_layout_width =
        culling_enabled && culling_width_factor < 0.999f;
    const bool shrink_layout_height =
        culling_enabled && culling_height_factor < 0.999f;
    const float layout_right =
        shrink_layout_width ? culling_width : (float)width;
    const float layout_bottom =
        shrink_layout_height ? culling_height : (float)height;
    const EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = fixture->bounds_x * scale,
            .y = fixture->bounds_y * scale,
            .width = layout_right - fixture->bounds_x * scale,
            .height = layout_bottom - fixture->bounds_y * scale,
        },
        .z_index = fixture->base_z,
    };
    const EcsUiRaylibRenderContext render_context = {
        .physical_root_bounds = {
            .x = options.physical_bounds.x,
            .y = options.physical_bounds.y,
            .width = options.physical_bounds.width,
            .height = options.physical_bounds.height,
        },
        .scale = scale,
    };
    EcsUiRaylibDrawOptions draw_options = {
        .custom_draw = TestCustomDraw,
        .icon_draw = TestIconDraw,
        .nine_slice_draw = TestNineSliceDraw,
        .culling_enabled = culling_enabled,
        .culling_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = culling_width,
            .height = culling_height,
        },
    };
    EcsUiTheme theme = EcsUiThemeDefault();

    EcsUiTreeSnapshot clay_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &clay_tree),
        "clay tree read failed");
    EcsUiFrameBackendSetSurfaceSize(culling_width, culling_height);
    EcsUiFrameBackendSetCullingEnabled(culling_enabled);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    const EcsUiDrawList *draw_list =
        EcsUiFrameRun(&clay_tree, &theme, &options, NULL, NULL);
    result |= Require(draw_list != NULL, "clay frame run failed");
    const EcsUiPaintList *clay_paint = EcsUiFrameInternalPaintList();
    result |= Require(clay_paint != NULL, "clay paint missing");
    TestForceLetterSpacing(
        draw_list,
        clay_paint,
        fixture->forced_letter_spacing);

    DrawListRenderCtx draw_ctx = {
        .draw_list = draw_list,
        .fonts = NULL,
        .context = &render_context,
        .options = &draw_options,
    };
    PaintRenderCtx clay_paint_ctx = {
        .paint = clay_paint,
        .tree = &clay_tree,
        .fonts = NULL,
        .context = &render_context,
        .options = &draw_options,
    };
    PixelImage bridge =
        TestRenderToImage(width, height, TestRenderDrawList, &draw_ctx);
    PixelImage paint =
        TestRenderToImage(width, height, TestRenderPaint, &clay_paint_ctx);

    const char *culling = culling_enabled ? "cull-on" : "cull-off";
    char case_name[128];
    (void)snprintf(
        case_name,
        sizeof(case_name),
        "%s scale=%.0f %s",
        fixture->name,
        scale,
        culling);
    PixelDiff ab = TestComparePixels(&bridge, &paint);
    if (ab.count != 0u) {
        TestPrintDiff(
            case_name,
            "clay-draw-list vs paint-direct",
            ab,
            &bridge,
            &paint);
        if (getenv("ECS_UI_PIXEL_AB_DEBUG") != NULL) {
            TestPrintOverlappingCommands(draw_list, clay_paint, &ab);
        }
        result |= 1;
    }

    EcsUiTreeSnapshot native_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &native_tree),
        "native tree read failed");
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    (void)EcsUiFrameRun(&native_tree, &theme, &options, NULL, NULL);
    const EcsUiPaintList *native_paint = EcsUiFrameInternalPaintList();
    if (native_paint != NULL) {
        TestForceLetterSpacing(
            NULL,
            native_paint,
            fixture->forced_letter_spacing);
        PaintRenderCtx native_paint_ctx = {
            .paint = native_paint,
            .tree = &native_tree,
            .fonts = NULL,
            .context = &render_context,
            .options = &draw_options,
        };
        PixelImage native =
            TestRenderToImage(width, height, TestRenderPaint, &native_paint_ctx);
        PixelDiff ac = TestComparePixels(&bridge, &native);
        if (ac.count != 0u) {
            (void)fprintf(
                stdout,
                "INFO native-layout delta fixture=%s first=(%d,%d) "
                "count=%u bbox=(%d,%d)-(%d,%d)\n",
                case_name,
                ac.first_x,
                ac.first_y,
                ac.count,
                ac.min_x,
                ac.min_y,
                ac.max_x,
                ac.max_y);
        }
        TestFreeImage(&native);
    } else {
        (void)fprintf(
            stdout,
            "INFO native-layout render unavailable fixture=%s\n",
            case_name);
    }

    TestFreeImage(&bridge);
    TestFreeImage(&paint);
    ecs_fini(world);
    return result;
}

int main(void)
{
    if (!TestDisplayAvailable()) {
        (void)printf("SKIP: ecs_ui_raylib_pixel_ab requires DISPLAY/Xvfb\n");
        return TEST_SKIP;
    }

    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "ecs-ui pixel ab");
    if (!IsWindowReady()) {
        (void)fprintf(
            stderr,
            "raylib window not available despite declared display\n");
        return 1;
    }

    int result = 0;
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = TEST_RENDER_WIDTH,
                .surface_height = TEST_RENDER_HEIGHT,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
            }),
        "failed to initialize frame backend");

    const PixelFixture fixtures[] = {
        {
            .name = "pixel-composite",
            .build = BuildCompositeTree,
            .base_z = 0,
            .bounds_x = 5.0f,
            .bounds_y = 7.0f,
        },
        {
            .name = "pixel-root-z",
            .build = BuildCompositeTree,
            .base_z = 37,
            .bounds_x = 3.0f,
            .bounds_y = 4.0f,
        },
        {
            .name = "pixel-snap",
            .build = BuildSnapTree,
            .base_z = 23,
            .bounds_x = 6.25f,
            .bounds_y = 4.75f,
        },
        {
            .name = "pixel-letter-spacing",
            .build = BuildSnapTree,
            .base_z = 23,
            .bounds_x = 6.25f,
            .bounds_y = 4.75f,
            .forced_letter_spacing = 3u,
        },
        {
            .name = "pixel-equal-z",
            .build = BuildEqualZTree,
            .base_z = 0,
            .bounds_x = 4.0f,
            .bounds_y = 4.0f,
        },
        {
            .name = "pixel-cull",
            .build = BuildCullTree,
            .base_z = 0,
            .bounds_x = 4.0f,
            .bounds_y = 4.0f,
            .culling_width_factor = 1.0f,
            .culling_height_factor = 0.38f,
        },
    };
    for (uint32_t i = 0u; i < sizeof(fixtures) / sizeof(fixtures[0]); i += 1u) {
        for (uint32_t scale_index = 0u; scale_index < 2u; scale_index += 1u) {
            const float scale = scale_index == 0u ? 1.0f : 2.0f;
            result |= RunPixelCase(&fixtures[i], scale, false);
            result |= RunPixelCase(&fixtures[i], scale, true);
        }
    }

    EcsUiFrameBackendShutdown();
    CloseWindow();
    return result;
}
