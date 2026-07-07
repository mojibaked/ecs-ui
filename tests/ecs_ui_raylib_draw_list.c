#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestRaylibStubCounts {
    uint32_t rectangles;
    uint32_t rounded_rectangles;
    uint32_t rounded_lines;
    uint32_t text;
    uint32_t textures;
    uint32_t scissor_start;
    uint32_t scissor_end;
} TestRaylibStubCounts;

typedef struct TestDrawCallbacks {
    uint32_t custom_count;
    uint32_t icon_count;
    uint32_t nine_slice_count;
    bool saw_custom_context;
    bool saw_icon_context;
    bool saw_nine_slice_context;
} TestDrawCallbacks;

typedef struct TestFrameErrors {
    uint32_t count;
    EcsUiFrameErrorKind last_kind;
} TestFrameErrors;

static TestRaylibStubCounts g_raylib_counts;

Font GetFontDefault(void)
{
    Font font = {0};
    font.baseSize = 10;
    font.glyphCount = 1;
    font.glyphs = (GlyphInfo *)1;
    return font;
}

Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing)
{
    (void)font;
    const size_t length = text != NULL ? strlen(text) : 0u;
    return (Vector2){
        .x = (float)length * fontSize * 0.5f,
        .y = fontSize + spacing,
    };
}

void DrawTextEx(
    Font font,
    const char *text,
    Vector2 position,
    float fontSize,
    float spacing,
    Color tint)
{
    (void)font;
    (void)text;
    (void)position;
    (void)fontSize;
    (void)spacing;
    (void)tint;
    g_raylib_counts.text += 1u;
}

void DrawTexturePro(
    Texture2D texture,
    Rectangle source,
    Rectangle dest,
    Vector2 origin,
    float rotation,
    Color tint)
{
    (void)texture;
    (void)source;
    (void)dest;
    (void)origin;
    (void)rotation;
    (void)tint;
    g_raylib_counts.textures += 1u;
}

void BeginScissorMode(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    g_raylib_counts.scissor_start += 1u;
}

void EndScissorMode(void)
{
    g_raylib_counts.scissor_end += 1u;
}

void DrawRectangleRec(Rectangle rec, Color color)
{
    (void)rec;
    (void)color;
    g_raylib_counts.rectangles += 1u;
}

void DrawRectangleRounded(
    Rectangle rec,
    float roundness,
    int segments,
    Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
    g_raylib_counts.rounded_rectangles += 1u;
}

void DrawRectangleRoundedLines(
    Rectangle rec,
    float roundness,
    int segments,
    Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
    g_raylib_counts.rounded_lines += 1u;
}

Color Fade(Color color, float alpha)
{
    Color out = color;
    if (alpha < 0.0f) {
        alpha = 0.0f;
    }
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    out.a = (unsigned char)((float)out.a * alpha);
    return out;
}

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

    const int32_t safe_length = length > 0 ? length : 0;
    const float font_size =
        spec != NULL && spec->font_size > 0.0f ? spec->font_size : 16.0f;
    return (EcsUiSize){
        .width = (float)safe_length * font_size * 0.5f,
        .height = font_size + 4.0f,
    };
}

static void TestFrameHandleError(
    EcsUiFrameErrorKind kind,
    const char *message,
    void *user_data)
{
    (void)message;

    TestFrameErrors *errors = user_data;
    if (errors == NULL) {
        return;
    }
    errors->count += 1u;
    errors->last_kind = kind;
}

static bool TestContextLooksValid(
    const EcsUiRaylibRenderContext *context)
{
    return context != NULL &&
        context->physical_bounds.width > 0.0f &&
        context->physical_bounds.height > 0.0f &&
        context->physical_root_bounds.width == 320.0f &&
        context->physical_root_bounds.height == 220.0f &&
        context->scale == 1.0f;
}

static void TestCustomDraw(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data)
{
    (void)opacity;

    TestDrawCallbacks *callbacks = user_data;
    if (callbacks == NULL || node == NULL) {
        return;
    }
    callbacks->custom_count += 1u;
    if (strcmp(node->id, "RendererCustom") == 0 &&
        TestContextLooksValid(context)) {
        callbacks->saw_custom_context = true;
    }
}

static void TestIconDraw(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data)
{
    (void)opacity;

    TestDrawCallbacks *callbacks = user_data;
    if (callbacks == NULL || node == NULL) {
        return;
    }
    callbacks->icon_count += 1u;
    if (strcmp(node->id, "RendererIcon") == 0 &&
        TestContextLooksValid(context)) {
        callbacks->saw_icon_context = true;
    }
}

static void TestNineSliceDraw(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data)
{
    (void)opacity;

    TestDrawCallbacks *callbacks = user_data;
    if (callbacks == NULL || node == NULL) {
        return;
    }
    callbacks->nine_slice_count += 1u;
    if (strcmp(node->id, "RendererNineSlice") == 0 &&
        TestContextLooksValid(context)) {
        callbacks->saw_nine_slice_context = true;
    }
}

static ecs_world_t *CreateWorld(void)
{
    ecs_world_t *world = ecs_init();
    if (world != NULL) {
        EcsUiImport(world);
    }
    return world;
}

static int BuildRendererTree(ecs_world_t *world, ecs_entity_t root)
{
    const EcsUiNineSliceStyle nine_slice_style = {
        .image = "renderer.frame",
        .slice_left = 3u,
        .slice_top = 3u,
        .slice_right = 3u,
        .slice_bottom = 3u,
        .scale = 1.0f,
        .tint = {200u, 210u, 220u, 255u},
    };
    const EcsUiBoxStyle box_style = {
        .background = {45u, 55u, 65u, 255u},
        .border_color = {10u, 20u, 30u, 255u},
        .border_width = 1.0f,
    };

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "RendererRootStack",
            .gap = 4.0f,
            .padding = 4.0f,
            .preferred_width = 180.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RendererCustom",
            .kind = "renderer.custom",
            .preferred_width = 60.0f,
            .preferred_height = 24.0f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "RendererIcon",
            .name = "renderer-icon",
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "RendererNineSlice",
            .preferred_width = 96.0f,
            .preferred_height = 32.0f,
            .style = &box_style,
            .nine_slice_style = &nine_slice_style,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "RendererScroll",
                .preferred_width = 96.0f,
                .preferred_height = 36.0f,
            },
        });
    for (int i = 0; i < 4; i += 1) {
        char id[ECS_UI_ID_MAX] = {0};
        (void)snprintf(id, sizeof(id), "RendererScrollRow%d", i);
        (void)EcsUiAddCustom(
            &builder,
            (EcsUiCustomDesc){
                .id = id,
                .kind = "renderer.row",
                .preferred_width = 72.0f,
                .preferred_height = 18.0f,
            });
    }
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "renderer tree builder failed");
}

int main(void)
{
    int result = 0;
    TestFrameErrors errors = {0};
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 320.0f,
                .surface_height = 220.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = &errors,
            }),
        "failed to initialize renderer frame backend");
    EcsUiFrameBackendSetCullingEnabled(false);

    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        EcsUiFrameBackendShutdown();
        return Require(false, "failed to create renderer world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "RendererRoot");
    result |= BuildRendererTree(world, root);
    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read renderer tree");

    EcsUiTheme theme = EcsUiThemeDefault();
    const EcsUiFrameLayoutOptions layout_options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 320.0f,
            .height = 220.0f,
        },
    };
    const EcsUiDrawList *draw_list = EcsUiFrameRun(
        &tree,
        &theme,
        &layout_options,
        NULL,
        NULL);
    result |= Require(draw_list != NULL, "renderer draw list missing");

    TestDrawCallbacks callbacks = {0};
    g_raylib_counts = (TestRaylibStubCounts){0};
    EcsUiRaylibRenderDrawList(
        draw_list,
        NULL,
        &(EcsUiRaylibRenderContext){
            .physical_root_bounds = {
                .x = 0.0f,
                .y = 0.0f,
                .width = 320.0f,
                .height = 220.0f,
            },
            .scale = 1.0f,
        },
        &(EcsUiRaylibDrawOptions){
            .custom_draw = TestCustomDraw,
            .user_data = &callbacks,
            .icon_draw = TestIconDraw,
            .nine_slice_draw = TestNineSliceDraw,
        });

    result |= Require(
        callbacks.custom_count > 0u && callbacks.saw_custom_context,
        "custom draw-list branch was not dispatched");
    result |= Require(
        callbacks.icon_count > 0u && callbacks.saw_icon_context,
        "icon draw-list branch was not dispatched");
    result |= Require(
        callbacks.nine_slice_count > 0u && callbacks.saw_nine_slice_context,
        "nine-slice draw-list branch was not dispatched");
    result |= Require(
        g_raylib_counts.scissor_start > 0u &&
            g_raylib_counts.scissor_start == g_raylib_counts.scissor_end,
        "scissor draw-list branch was not balanced");
    result |= Require(
        errors.count == 0u,
        "renderer frame backend emitted errors");
    EcsUiSize null_spec_size =
        EcsUiRaylibMeasureText("abc", 3, NULL, NULL);
    result |= Require(
        null_spec_size.height == 11.0f,
        "raylib measure NULL spec should default spacing to 1");
    if (errors.count != 0u) {
        (void)fprintf(
            stderr,
            "last renderer frame error kind=%d\n",
            (int)errors.last_kind);
    }

    ecs_fini(world);
    EcsUiRaylibReleaseDrawListRenderer();
    EcsUiFrameBackendShutdown();
    return result;
}
