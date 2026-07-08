#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_raylib.h"
#include "../src/ecs_ui_frame_internal.h"

#include <math.h>
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
    bool saw_custom_node;
    bool saw_icon_node;
    bool saw_nine_slice_node;
    bool saw_custom_context;
    bool saw_icon_context;
    bool saw_nine_slice_context;
} TestDrawCallbacks;

typedef struct TestFrameErrors {
    uint32_t count;
    EcsUiFrameErrorKind last_kind;
} TestFrameErrors;

typedef enum TestDrawCallKind {
    TEST_DRAW_CALL_RECTANGLE,
    TEST_DRAW_CALL_ROUNDED_RECTANGLE,
    TEST_DRAW_CALL_ROUNDED_LINES,
    TEST_DRAW_CALL_TEXT,
    TEST_DRAW_CALL_TEXTURE,
    TEST_DRAW_CALL_SCISSOR_START,
    TEST_DRAW_CALL_SCISSOR_END,
} TestDrawCallKind;

typedef struct TestDrawCall {
    TestDrawCallKind kind;
    Rectangle rect;
    Color color;
    float font_size;
    char text[64];
} TestDrawCall;

#define TEST_DRAW_LOG_MAX 256u

static TestRaylibStubCounts g_raylib_counts;
static TestDrawCall g_draw_log[TEST_DRAW_LOG_MAX];
static uint32_t g_draw_log_count;

static void TestLogDraw(
    TestDrawCallKind kind,
    Rectangle rect,
    Color color,
    const char *text,
    float font_size)
{
    if (g_draw_log_count >= TEST_DRAW_LOG_MAX) {
        return;
    }
    TestDrawCall *call = &g_draw_log[g_draw_log_count];
    *call = (TestDrawCall){
        .kind = kind,
        .rect = rect,
        .color = color,
        .font_size = font_size,
    };
    if (text != NULL) {
        (void)snprintf(call->text, sizeof(call->text), "%s", text);
    }
    g_draw_log_count += 1u;
}

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
    (void)spacing;
    g_raylib_counts.text += 1u;
    TestLogDraw(
        TEST_DRAW_CALL_TEXT,
        (Rectangle){.x = position.x, .y = position.y},
        tint,
        text,
        fontSize);
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
    g_raylib_counts.textures += 1u;
    TestLogDraw(TEST_DRAW_CALL_TEXTURE, dest, tint, NULL, 0.0f);
}

void BeginScissorMode(int x, int y, int width, int height)
{
    g_raylib_counts.scissor_start += 1u;
    TestLogDraw(
        TEST_DRAW_CALL_SCISSOR_START,
        (Rectangle){
            .x = (float)x,
            .y = (float)y,
            .width = (float)width,
            .height = (float)height,
        },
        (Color){0},
        NULL,
        0.0f);
}

void EndScissorMode(void)
{
    g_raylib_counts.scissor_end += 1u;
    TestLogDraw(
        TEST_DRAW_CALL_SCISSOR_END,
        (Rectangle){0},
        (Color){0},
        NULL,
        0.0f);
}

void DrawRectangleRec(Rectangle rec, Color color)
{
    g_raylib_counts.rectangles += 1u;
    TestLogDraw(TEST_DRAW_CALL_RECTANGLE, rec, color, NULL, 0.0f);
}

void DrawRectangleRounded(
    Rectangle rec,
    float roundness,
    int segments,
    Color color)
{
    (void)roundness;
    (void)segments;
    g_raylib_counts.rounded_rectangles += 1u;
    TestLogDraw(TEST_DRAW_CALL_ROUNDED_RECTANGLE, rec, color, NULL, 0.0f);
}

void DrawRectangleRoundedLines(
    Rectangle rec,
    float roundness,
    int segments,
    Color color)
{
    (void)roundness;
    (void)segments;
    g_raylib_counts.rounded_lines += 1u;
    TestLogDraw(TEST_DRAW_CALL_ROUNDED_LINES, rec, color, NULL, 0.0f);
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

static int RequireNear(float actual, float expected, float epsilon, const char *message)
{
    if (fabsf(actual - expected) > epsilon) {
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

static int RequireColor(Color actual, Color expected, const char *message)
{
    return Require(
        actual.r == expected.r &&
            actual.g == expected.g &&
            actual.b == expected.b &&
            actual.a == expected.a,
        message);
}

static int RequireRect(Rectangle actual, Rectangle expected, const char *message)
{
    int result = 0;
    result |= RequireNear(actual.x, expected.x, 0.001f, message);
    result |= RequireNear(actual.y, expected.y, 0.001f, message);
    result |= RequireNear(actual.width, expected.width, 0.001f, message);
    result |= RequireNear(actual.height, expected.height, 0.001f, message);
    return result;
}

static Color TestColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (Color){.r = r, .g = g, .b = b, .a = a};
}

static EcsUiColorF TestColorF(float r, float g, float b, float a)
{
    return (EcsUiColorF){.r = r, .g = g, .b = b, .a = a};
}

static EcsUiPaintRect TestPaintRect(float x, float y, float width, float height)
{
    return (EcsUiPaintRect){
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };
}

static Rectangle TestSnappedRect(EcsUiPaintRect rect, float scale)
{
    return (Rectangle){
        .x = roundf(rect.x * scale),
        .y = roundf(rect.y * scale),
        .width = roundf(rect.width * scale),
        .height = roundf(rect.height * scale),
    };
}

static bool TestRectIntersects(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width &&
        a.x + a.width > b.x &&
        a.y < b.y + b.height &&
        a.y + a.height > b.y;
}

static int32_t TestFindFirstDraw(TestDrawCallKind kind)
{
    for (uint32_t i = 0u; i < g_draw_log_count; i += 1u) {
        if (g_draw_log[i].kind == kind) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t TestFindColorDraw(Color color, uint32_t start)
{
    for (uint32_t i = start; i < g_draw_log_count; i += 1u) {
        if (g_draw_log[i].color.r == color.r &&
                g_draw_log[i].color.g == color.g &&
                g_draw_log[i].color.b == color.b &&
                g_draw_log[i].color.a == color.a) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int RequireDrawPrefixEqual(
    const TestDrawCall *expected,
    uint32_t expected_count,
    uint32_t actual_count)
{
    int result = 0;
    result |= Require(
        actual_count <= expected_count,
        "culled paint log should not grow");
    for (uint32_t i = 0u; i < actual_count && i < expected_count; i += 1u) {
        result |= Require(
            g_draw_log[i].kind == expected[i].kind,
            "culled paint call kind mismatch");
        result |= RequireRect(
            g_draw_log[i].rect,
            expected[i].rect,
            "culled paint call rect mismatch");
        result |= RequireColor(
            g_draw_log[i].color,
            expected[i].color,
            "culled paint call color mismatch");
    }
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
    if (callbacks == NULL) {
        return;
    }
    callbacks->custom_count += 1u;
    if (node == NULL) {
        return;
    }
    if (strcmp(node->id, "RendererCustom") == 0) {
        callbacks->saw_custom_node = true;
    }
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
    if (callbacks == NULL) {
        return;
    }
    callbacks->icon_count += 1u;
    if (node == NULL) {
        return;
    }
    if (strcmp(node->id, "RendererIcon") == 0) {
        callbacks->saw_icon_node = true;
    }
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
    if (callbacks == NULL) {
        return;
    }
    callbacks->nine_slice_count += 1u;
    if (node == NULL) {
        return;
    }
    if (strcmp(node->id, "RendererNineSlice") == 0) {
        callbacks->saw_nine_slice_node = true;
    }
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

static void TestSetPaintNode(
    EcsUiTreeSnapshot *tree,
    uint32_t index,
    ecs_entity_t entity,
    const char *id,
    EcsUiNodeKind kind)
{
    EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    *node = (EcsUiTreeNodeSnapshot){
        .entity = entity,
        .kind = kind,
        .has_layout = true,
    };
    (void)snprintf(node->id, sizeof(node->id), "%s", id);
}

static EcsUiPaintItem *TestPushPaintItem(
    EcsUiPaintList *paint,
    ecs_entity_t source,
    uint16_t role,
    uint16_t primitive,
    EcsUiPaintRect rect)
{
    EcsUiPaintItem *item = &paint->items[paint->count];
    *item = (EcsUiPaintItem){
        .key = {
            .source = source,
            .role = role,
            .part = 0u,
            .generation = paint->generation,
        },
        .primitive = primitive,
        .rect = rect,
        .opacity = 1.0f,
        .order = paint->count,
    };
    paint->count += 1u;
    return item;
}

static void BuildDirectPaintFixture(
    EcsUiTreeSnapshot *tree,
    EcsUiPaintList *paint,
    float scale)
{
    *tree = (EcsUiTreeSnapshot){
        .root = 100u,
        .scale = scale,
        .generation = 77u,
        .count = 6u,
    };
    TestSetPaintNode(tree, 0u, 100u, "RendererBox", ECS_UI_NODE_VSTACK);
    TestSetPaintNode(tree, 1u, 101u, "RendererCustom", ECS_UI_NODE_CUSTOM);
    TestSetPaintNode(tree, 2u, 102u, "RendererIcon", ECS_UI_NODE_ICON);
    TestSetPaintNode(tree, 3u, 103u, "RendererNineSlice", ECS_UI_NODE_HSTACK);
    TestSetPaintNode(tree, 4u, 104u, "RendererText", ECS_UI_NODE_TEXT);
    TestSetPaintNode(tree, 5u, 105u, "RendererBorderParent", ECS_UI_NODE_VSTACK);
    (void)snprintf(
        tree->nodes[1].custom.kind,
        sizeof(tree->nodes[1].custom.kind),
        "renderer.custom");
    tree->nodes[3].has_nine_slice_style = true;

    EcsUiPaintListReset(paint, tree->root, tree->generation);
    EcsUiPaintItem *item = TestPushPaintItem(
        paint,
        100u,
        ECS_UI_PAINT_ROLE_BOX,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(1.25f, 2.25f, 10.25f, 6.25f));
    item->payload.box.fill = TestColorF(10.0f, 20.0f, 30.0f, 255.0f);

    item = TestPushPaintItem(
        paint,
        100u,
        ECS_UI_PAINT_ROLE_BOX,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(15.25f, 2.25f, 10.25f, 6.25f));
    item->payload.box.fill = TestColorF(40.0f, 50.0f, 60.0f, 255.0f);
    item->payload.box.radius = (EcsUiPaintCornerRadius){
        .top_left = 2.0f,
        .top_right = 2.0f,
        .bottom_left = 2.0f,
        .bottom_right = 2.0f,
    };

    static const char text[] = "paint";
    item = TestPushPaintItem(
        paint,
        104u,
        ECS_UI_PAINT_ROLE_TEXT_RUN,
        ECS_UI_PAINT_PRIMITIVE_TEXT_RUN,
        TestPaintRect(3.25f, 4.25f, 18.25f, 8.25f));
    item->payload.text_run = (EcsUiPaintTextRun){
        .text = text,
        .byte_start = 0u,
        .byte_end = 5u,
        .font_size = (uint16_t)(13.0f * scale),
        .color = TestColorF(90.0f, 100.0f, 110.0f, 255.0f),
    };

    item = TestPushPaintItem(
        paint,
        104u,
        ECS_UI_PAINT_ROLE_SELECTION,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(4.25f, 14.25f, 9.25f, 3.25f));
    item->payload.box.fill = TestColorF(70.0f, 80.0f, 90.0f, 200.0f);

    item = TestPushPaintItem(
        paint,
        104u,
        ECS_UI_PAINT_ROLE_CARET,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(14.25f, 14.25f, 1.25f, 9.25f));
    item->payload.box.fill = TestColorF(95.0f, 105.0f, 115.0f, 255.0f);

    item = TestPushPaintItem(
        paint,
        101u,
        ECS_UI_PAINT_ROLE_CUSTOM,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        TestPaintRect(28.25f, 2.25f, 7.25f, 5.25f));
    item->payload.custom.source = 101u;
    item->payload.custom.color = TestColorF(255.0f, 255.0f, 255.0f, 255.0f);

    item = TestPushPaintItem(
        paint,
        102u,
        ECS_UI_PAINT_ROLE_ICON,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        TestPaintRect(36.25f, 2.25f, 7.25f, 5.25f));
    item->payload.custom.source = 102u;
    item->payload.custom.color = TestColorF(255.0f, 255.0f, 255.0f, 255.0f);

    item = TestPushPaintItem(
        paint,
        103u,
        ECS_UI_PAINT_ROLE_NINE_SLICE,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        TestPaintRect(44.25f, 2.25f, 7.25f, 5.25f));
    item->payload.custom.source = 103u;
    item->payload.custom.color = TestColorF(200.0f, 210.0f, 220.0f, 180.0f);

    item = TestPushPaintItem(
        paint,
        105u,
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE,
        TestPaintRect(7.25f, 8.25f, 30.25f, 20.25f));
    item->key.part = ECS_UI_PAINT_CLIP_SCOPE_START;

    item = TestPushPaintItem(
        paint,
        105u,
        ECS_UI_PAINT_ROLE_BOX,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(7.40f, 8.40f, 18.25f, 8.25f));
    item->payload.box.fill = TestColorF(120.0f, 130.0f, 140.0f, 255.0f);

    item = TestPushPaintItem(
        paint,
        105u,
        ECS_UI_PAINT_ROLE_BORDER,
        ECS_UI_PAINT_PRIMITIVE_BORDER,
        TestPaintRect(7.25f, 8.25f, 30.25f, 20.25f));
    item->payload.border = (EcsUiPaintBorder){
        .color = TestColorF(1.0f, 2.0f, 3.0f, 255.0f),
        .left = 1.25f,
        .top = 1.25f,
        .right = 1.25f,
        .bottom = 1.25f,
        .has_border = true,
    };

    item = TestPushPaintItem(
        paint,
        105u,
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE,
        TestPaintRect(7.25f, 8.25f, 30.25f, 20.25f));
    item->key.part = ECS_UI_PAINT_CLIP_SCOPE_END;

    item = TestPushPaintItem(
        paint,
        100u,
        ECS_UI_PAINT_ROLE_BEVEL_EDGE,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(2.25f, 30.25f, 30.25f, 1.25f));
    item->payload.bevel_edge.color =
        TestColorF(150.0f, 160.0f, 170.0f, 255.0f);

    item = TestPushPaintItem(
        paint,
        100u,
        ECS_UI_PAINT_ROLE_BOX,
        ECS_UI_PAINT_PRIMITIVE_BOX,
        TestPaintRect(250.25f, 250.25f, 10.25f, 10.25f));
    item->payload.box.fill = TestColorF(220.0f, 10.0f, 10.0f, 255.0f);
}

static int RequireDirectPaintLog(float scale, const TestDrawCallbacks *callbacks)
{
    int result = 0;
    result |= Require(
        callbacks != NULL &&
            callbacks->custom_count == 1u &&
            callbacks->icon_count == 1u &&
            callbacks->nine_slice_count == 1u &&
            callbacks->saw_custom_node &&
            callbacks->saw_icon_node &&
            callbacks->saw_nine_slice_node,
        "paint renderer callbacks should receive snapshot nodes");
    result |= Require(g_raylib_counts.text == 1u, "paint renderer text missing");
    result |= Require(
        g_raylib_counts.scissor_start == 1u &&
            g_raylib_counts.scissor_end == 1u,
        "paint renderer scissor markers should balance");
    result |= Require(
        g_raylib_counts.rounded_rectangles == 1u,
        "paint renderer rounded box branch missing");

    result |= Require(
        g_draw_log_count > 10u,
        "paint renderer draw log should be populated");
    result |= Require(
        g_draw_log[0].kind == TEST_DRAW_CALL_RECTANGLE,
        "first paint draw should be the plain box");
    result |= RequireRect(
        g_draw_log[0].rect,
        TestSnappedRect(TestPaintRect(1.25f, 2.25f, 10.25f, 6.25f), scale),
        "paint renderer scale-then-snap rectangle mismatch");
    result |= RequireColor(
        g_draw_log[0].color,
        TestColor(10u, 20u, 30u, 255u),
        "paint renderer box color mismatch");

    const int32_t text_index = TestFindFirstDraw(TEST_DRAW_CALL_TEXT);
    result |= Require(text_index >= 0, "paint renderer text draw missing");
    if (text_index >= 0) {
        result |= RequireRect(
            g_draw_log[(uint32_t)text_index].rect,
            (Rectangle){
                .x = roundf(3.25f * scale),
                .y = roundf(4.25f * scale),
            },
            "paint renderer text origin should use snapped physical rect");
        result |= RequireNear(
            g_draw_log[(uint32_t)text_index].font_size,
            13.0f * scale,
            0.001f,
            "paint renderer text font size mismatch");
        result |= Require(
            strcmp(g_draw_log[(uint32_t)text_index].text, "paint") == 0,
            "paint renderer text slice mismatch");
    }

    const int32_t scissor_index =
        TestFindFirstDraw(TEST_DRAW_CALL_SCISSOR_START);
    result |= Require(scissor_index >= 0, "paint renderer scissor missing");
    if (scissor_index >= 0) {
        result |= RequireRect(
            g_draw_log[(uint32_t)scissor_index].rect,
            TestSnappedRect(TestPaintRect(7.25f, 8.25f, 30.25f, 20.25f), scale),
            "paint renderer scissor rect mismatch");
    }

    const int32_t child_index =
        TestFindColorDraw(TestColor(120u, 130u, 140u, 255u), 0u);
    const int32_t border_index =
        TestFindColorDraw(TestColor(1u, 2u, 3u, 255u), 0u);
    const Rectangle child_bounds =
        TestSnappedRect(TestPaintRect(7.40f, 8.40f, 18.25f, 8.25f), scale);
    const Rectangle border_bounds =
        TestSnappedRect(TestPaintRect(7.25f, 8.25f, 30.25f, 20.25f), scale);
    const float border_px = (float)(uint16_t)(1.25f * scale);
    result |= Require(
        border_px > 0.0f &&
            (TestRectIntersects(
                    child_bounds,
                    (Rectangle){
                        .x = border_bounds.x,
                        .y = border_bounds.y,
                        .width = border_px,
                        .height = border_bounds.height,
                    }) ||
                TestRectIntersects(
                    child_bounds,
                    (Rectangle){
                        .x = border_bounds.x,
                        .y = border_bounds.y,
                        .width = border_bounds.width,
                        .height = border_px,
                    })),
        "paint renderer overlap case should intersect snapped border band");
    result |= Require(
        child_index >= 0 && border_index >= 0 && child_index < border_index,
        "paint renderer border should draw after overlapping child");
    return result;
}

static int RunDirectPaintRendererCase(float scale)
{
    static EcsUiTreeSnapshot tree;
    static EcsUiPaintList paint;
    BuildDirectPaintFixture(&tree, &paint, scale);

    const EcsUiRaylibRenderContext context = {
        .physical_root_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 320.0f * scale,
            .height = 220.0f * scale,
        },
        .scale = scale,
    };
    TestDrawCallbacks callbacks = {0};
    g_raylib_counts = (TestRaylibStubCounts){0};
    g_draw_log_count = 0u;
    EcsUiRaylibRenderPaintList(
        &paint,
        &tree,
        NULL,
        &context,
        &(EcsUiRaylibDrawOptions){
            .custom_draw = TestCustomDraw,
            .icon_draw = TestIconDraw,
            .nine_slice_draw = TestNineSliceDraw,
            .user_data = &callbacks,
        });

    int result = RequireDirectPaintLog(scale, &callbacks);
    TestDrawCall uncull_log[TEST_DRAW_LOG_MAX];
    const uint32_t uncull_count = g_draw_log_count;
    (void)memcpy(uncull_log, g_draw_log, sizeof(uncull_log));

    callbacks = (TestDrawCallbacks){0};
    g_raylib_counts = (TestRaylibStubCounts){0};
    g_draw_log_count = 0u;
    EcsUiRaylibRenderPaintList(
        &paint,
        &tree,
        NULL,
        &context,
        &(EcsUiRaylibDrawOptions){
            .custom_draw = TestCustomDraw,
            .icon_draw = TestIconDraw,
            .nine_slice_draw = TestNineSliceDraw,
            .user_data = &callbacks,
            .culling_enabled = true,
            .culling_bounds = {
                .x = 0.0f,
                .y = 0.0f,
                .width = 220.0f * scale,
                .height = 180.0f * scale,
            },
        });
    result |= RequireDirectPaintLog(scale, &callbacks);
    result |= Require(
        g_draw_log_count + 1u == uncull_count,
        "paint culling should drop only the offscreen primitive");
    result |= RequireDrawPrefixEqual(uncull_log, uncull_count, g_draw_log_count);
    return result;
}

static int RunCustomFallbackPaintCase(float scale)
{
    static EcsUiTreeSnapshot tree;
    static EcsUiPaintList paint;
    tree = (EcsUiTreeSnapshot){
        .root = 200u,
        .scale = scale,
        .generation = 88u,
        .count = 1u,
    };
    TestSetPaintNode(&tree, 0u, 201u, "RendererCustom", ECS_UI_NODE_CUSTOM);
    (void)snprintf(
        tree.nodes[0].custom.kind,
        sizeof(tree.nodes[0].custom.kind),
        "renderer.fallback");
    EcsUiPaintListReset(&paint, tree.root, tree.generation);

    EcsUiPaintItem *item = TestPushPaintItem(
        &paint,
        201u,
        ECS_UI_PAINT_ROLE_CUSTOM,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        TestPaintRect(2.25f, 3.25f, 20.25f, 10.25f));
    item->payload.custom.source = 201u;
    item->payload.custom.color = TestColorF(44.0f, 55.0f, 66.0f, 128.0f);

    item = TestPushPaintItem(
        &paint,
        999u,
        ECS_UI_PAINT_ROLE_ICON,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        TestPaintRect(28.25f, 3.25f, 12.25f, 8.25f));
    item->payload.custom.source = 999u;
    item->payload.custom.color = TestColorF(70.0f, 80.0f, 90.0f, 200.0f);

    item = TestPushPaintItem(
        &paint,
        998u,
        ECS_UI_PAINT_ROLE_NINE_SLICE,
        ECS_UI_PAINT_PRIMITIVE_CUSTOM,
        TestPaintRect(42.25f, 3.25f, 12.25f, 8.25f));
    item->payload.custom.source = 998u;
    item->payload.custom.color = TestColorF(90.0f, 100.0f, 110.0f, 210.0f);

    const EcsUiRaylibRenderContext context = {
        .physical_root_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 320.0f * scale,
            .height = 220.0f * scale,
        },
        .scale = scale,
    };
    TestDrawCallbacks callbacks = {0};
    g_raylib_counts = (TestRaylibStubCounts){0};
    g_draw_log_count = 0u;
    EcsUiRaylibRenderPaintList(
        &paint,
        &tree,
        NULL,
        &context,
        &(EcsUiRaylibDrawOptions){
            .icon_draw = TestIconDraw,
            .nine_slice_draw = TestNineSliceDraw,
            .user_data = &callbacks,
        });

    int result = 0;
    result |= Require(
        callbacks.custom_count == 0u &&
            callbacks.icon_count == 0u &&
            callbacks.nine_slice_count == 0u,
        "missing-source custom roles should not invoke icon/nine callbacks");
    result |= Require(
        g_raylib_counts.rounded_rectangles == 3u &&
            g_raylib_counts.rounded_lines == 3u,
        "paint custom fallback should draw rounded fill and outline");
    result |= Require(
        g_raylib_counts.text == 1u,
        "paint custom fallback should label resolved custom nodes only");
    result |= Require(
        g_draw_log_count >= 7u &&
            g_draw_log[0].kind == TEST_DRAW_CALL_ROUNDED_RECTANGLE &&
            g_draw_log[1].kind == TEST_DRAW_CALL_ROUNDED_LINES &&
            g_draw_log[2].kind == TEST_DRAW_CALL_TEXT,
        "paint custom fallback draw order mismatch");
    result |= RequireRect(
        g_draw_log[0].rect,
        TestSnappedRect(TestPaintRect(2.25f, 3.25f, 20.25f, 10.25f), scale),
        "paint custom fallback bounds mismatch");
    result |= RequireColor(
        g_draw_log[0].color,
        TestColor(44u, 55u, 66u, 128u),
        "paint custom fallback fill mismatch");
    result |= Require(
        strcmp(g_draw_log[2].text, "renderer.fallback") == 0,
        "paint custom fallback label mismatch");
    result |= RequireNear(
        g_draw_log[2].rect.x,
        g_draw_log[0].rect.x + 10.0f,
        0.001f,
        "paint custom fallback label x mismatch");
    result |= RequireNear(
        g_draw_log[2].rect.y,
        g_draw_log[0].rect.y + 10.0f,
        0.001f,
        "paint custom fallback label y mismatch");
    result |= RequireColor(
        g_draw_log[2].color,
        TestColor(255u, 255u, 255u, 128u),
        "paint custom fallback label color mismatch");
    return result;
}

static int RunOffscreenClipCullingCase(float scale)
{
    static EcsUiTreeSnapshot tree;
    static EcsUiPaintList paint;
    tree = (EcsUiTreeSnapshot){
        .root = 300u,
        .scale = scale,
        .generation = 99u,
    };
    EcsUiPaintListReset(&paint, tree.root, tree.generation);
    EcsUiPaintItem *item = TestPushPaintItem(
        &paint,
        0u,
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE,
        TestPaintRect(240.25f, 240.25f, 20.25f, 20.25f));
    item->key.part = ECS_UI_PAINT_CLIP_SCOPE_START;
    item = TestPushPaintItem(
        &paint,
        0u,
        ECS_UI_PAINT_ROLE_CLIP_SCOPE,
        ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE,
        TestPaintRect(240.25f, 240.25f, 20.25f, 20.25f));
    item->key.part = ECS_UI_PAINT_CLIP_SCOPE_END;

    g_raylib_counts = (TestRaylibStubCounts){0};
    g_draw_log_count = 0u;
    EcsUiRaylibRenderPaintList(
        &paint,
        &tree,
        NULL,
        &(EcsUiRaylibRenderContext){
            .physical_root_bounds = {
                .width = 320.0f * scale,
                .height = 220.0f * scale,
            },
            .scale = scale,
        },
        &(EcsUiRaylibDrawOptions){
            .culling_enabled = true,
            .culling_bounds = {
                .x = 0.0f,
                .y = 0.0f,
                .width = 40.0f * scale,
                .height = 40.0f * scale,
            },
        });
    return Require(
        g_raylib_counts.scissor_start == 1u &&
            g_raylib_counts.scissor_end == 1u,
        "offscreen clip scope should remain balanced under culling");
}

static int RunPaintGenerationMismatchCase(void)
{
    static EcsUiTreeSnapshot tree;
    static EcsUiPaintList paint;
    BuildDirectPaintFixture(&tree, &paint, 1.0f);
    tree.generation = paint.generation + 1u;

    TestDrawCallbacks callbacks = {0};
    g_raylib_counts = (TestRaylibStubCounts){0};
    g_draw_log_count = 0u;
    EcsUiRaylibRenderPaintList(
        &paint,
        &tree,
        NULL,
        &(EcsUiRaylibRenderContext){
            .physical_root_bounds = {
                .width = 320.0f,
                .height = 220.0f,
            },
            .scale = 1.0f,
        },
        &(EcsUiRaylibDrawOptions){
            .custom_draw = TestCustomDraw,
            .icon_draw = TestIconDraw,
            .nine_slice_draw = TestNineSliceDraw,
            .user_data = &callbacks,
        });
    return Require(
        g_draw_log_count == 0u &&
            callbacks.custom_count == 0u &&
            callbacks.icon_count == 0u &&
            callbacks.nine_slice_count == 0u,
        "generation-mismatched paint should not render");
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
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
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
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    if (errors.count != 0u) {
        (void)fprintf(
            stderr,
            "last renderer frame error kind=%d\n",
            (int)errors.last_kind);
    }

    result |= RunDirectPaintRendererCase(1.0f);
    result |= RunDirectPaintRendererCase(2.0f);
    result |= RunCustomFallbackPaintCase(1.0f);
    result |= RunCustomFallbackPaintCase(2.0f);
    result |= RunOffscreenClipCullingCase(1.0f);
    result |= RunOffscreenClipCullingCase(2.0f);
    result |= RunPaintGenerationMismatchCase();

    ecs_fini(world);
    EcsUiRaylibReleaseDrawListRenderer();
    EcsUiFrameBackendShutdown();
    return result;
}
