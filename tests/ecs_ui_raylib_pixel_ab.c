#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "ecs_ui/ecs_ui_raylib.h"
#include "../src/ecs_ui_frame_internal.h"

#include <raylib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestImage {
    Image image;
    Color *pixels;
} TestImage;

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
        .width = (float)safe_length * font_size * 0.5f + 3.0f,
        .height = font_size + 4.0f,
    };
}

static void CopyString(char *out, size_t out_size, const char *value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    const char *source = value != NULL ? value : "";
    size_t i = 0u;
    for (; i + 1u < out_size && source[i] != '\0'; i += 1u) {
        out[i] = source[i];
    }
    out[i] = '\0';
}

static EcsUiTreeNodeSnapshot TestNode(
    ecs_entity_t entity,
    const char *id,
    EcsUiNodeKind kind)
{
    EcsUiTreeNodeSnapshot node = {
        .entity = entity,
        .kind = kind,
        .parent_index = ECS_UI_TREE_INVALID_INDEX,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .visual = {.opacity = 1.0f},
    };
    CopyString(node.id, sizeof(node.id), id);
    return node;
}

static void TestAppendChild(
    EcsUiTreeSnapshot *tree,
    uint32_t parent_index,
    EcsUiTreeNodeSnapshot child)
{
    if (tree == NULL || parent_index >= tree->count ||
            tree->count >= ECS_UI_TREE_NODE_MAX) {
        return;
    }
    const uint32_t index = tree->count;
    tree->count += 1u;
    child.parent = tree->nodes[parent_index].entity;
    child.parent_index = parent_index;
    child.depth = tree->nodes[parent_index].depth + 1u;
    child.next_sibling = ECS_UI_TREE_INVALID_INDEX;
    tree->nodes[index] = child;
    if (tree->nodes[parent_index].first_child == ECS_UI_TREE_INVALID_INDEX) {
        tree->nodes[parent_index].first_child = index;
        return;
    }
    uint32_t sibling = tree->nodes[parent_index].first_child;
    while (tree->nodes[sibling].next_sibling != ECS_UI_TREE_INVALID_INDEX) {
        sibling = tree->nodes[sibling].next_sibling;
    }
    tree->nodes[sibling].next_sibling = index;
}

static EcsUiTreeSnapshot TestPixelTree(float scale)
{
    EcsUiTreeSnapshot tree = {
        .root = 1u,
        .scale = scale,
        .count = 1u,
    };
    tree.nodes[0] = TestNode(1u, "PixelRoot", ECS_UI_NODE_ROOT);
    tree.nodes[0].stack.axis = ECS_UI_AXIS_VERTICAL;
    tree.nodes[0].stack.padding_left = 8.5f;
    tree.nodes[0].stack.padding_top = 6.5f;
    tree.nodes[0].stack.gap = 4.5f;
    tree.nodes[0].has_box_style = true;
    tree.nodes[0].box_style.background =
        (EcsUiColor){.r = 20, .g = 24, .b = 32, .a = 255};

    EcsUiTreeNodeSnapshot box = TestNode(2u, "PixelBox", ECS_UI_NODE_CUSTOM);
    box.custom.preferred_width = 60.5f;
    box.custom.preferred_height = 24.5f;
    box.has_box_style = true;
    box.box_style.background =
        (EcsUiColor){.r = 160, .g = 80, .b = 40, .a = 255};
    box.box_style.border_color =
        (EcsUiColor){.r = 240, .g = 230, .b = 120, .a = 255};
    box.box_style.border_width = 2.0f;
    TestAppendChild(&tree, 0u, box);

    EcsUiTreeNodeSnapshot text = TestNode(3u, "PixelText", ECS_UI_NODE_TEXT);
    CopyString(text.text.text, sizeof(text.text.text), "native");
    text.text.role = ECS_UI_TEXT_BODY;
    TestAppendChild(&tree, 0u, text);
    return tree;
}

static int RunFrame(EcsUiTreeSnapshot *tree)
{
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 180.0f * tree->scale,
            .height = 120.0f * tree->scale,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    return Require(
        EcsUiFrameRun(tree, &theme, &options, NULL, NULL) != NULL,
        "pixel frame run failed");
}

static TestImage RenderTree(EcsUiTreeSnapshot *tree, int width, int height)
{
    TestImage out = {0};
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (target.id == 0u) {
        return out;
    }

    BeginTextureMode(target);
    ClearBackground(BLACK);
    Font font = GetFontDefault();
    EcsUiRaylibRenderPaintList(
        EcsUiFramePaintList(),
        tree,
        &font,
        &(EcsUiRaylibRenderContext){
            .physical_root_bounds = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)width,
                .height = (float)height,
            },
            .scale = tree->scale,
        },
        &(EcsUiRaylibDrawOptions){0});
    EndTextureMode();

    out.image = LoadImageFromTexture(target.texture);
    out.pixels = LoadImageColors(out.image);
    UnloadRenderTexture(target);
    return out;
}

static void UnloadTestImage(TestImage *image)
{
    if (image == NULL) {
        return;
    }
    if (image->pixels != NULL) {
        UnloadImageColors(image->pixels);
    }
    if (image->image.data != NULL) {
        UnloadImage(image->image);
    }
    *image = (TestImage){0};
}

static uint32_t CountPixelDiffs(const TestImage *a, const TestImage *b)
{
    if (a == NULL || b == NULL || a->pixels == NULL || b->pixels == NULL ||
            a->image.width != b->image.width ||
            a->image.height != b->image.height) {
        return UINT32_MAX;
    }
    uint32_t count = 0u;
    const uint32_t total =
        (uint32_t)(a->image.width * a->image.height);
    for (uint32_t i = 0u; i < total; i += 1u) {
        if (memcmp(&a->pixels[i], &b->pixels[i], sizeof(Color)) != 0) {
            count += 1u;
        }
    }
    return count;
}

static int TestStableNativePixels(float scale)
{
    const int width = (int)(180.0f * scale);
    const int height = (int)(120.0f * scale);
    EcsUiTreeSnapshot first = TestPixelTree(scale);
    EcsUiTreeSnapshot second = TestPixelTree(scale);

    int result = RunFrame(&first);
    TestImage a = RenderTree(&first, width, height);
    result |= Require(a.pixels != NULL, "first pixel render failed");
    result |= RunFrame(&second);
    TestImage b = RenderTree(&second, width, height);
    result |= Require(b.pixels != NULL, "second pixel render failed");
    const uint32_t diffs = CountPixelDiffs(&a, &b);
    result |= Require(diffs == 0u, "native pixel render should be stable");
    UnloadTestImage(&a);
    UnloadTestImage(&b);
    return result;
}

static int TestDivergenceHookPixels(void)
{
    EcsUiTreeSnapshot normal = TestPixelTree(1.0f);
    EcsUiTreeSnapshot divergent = TestPixelTree(1.0f);

    int result = RunFrame(&normal);
    TestImage a = RenderTree(&normal, 180, 120);
    result |= Require(a.pixels != NULL, "normal pixel render failed");
    EcsUiFrameInternalSetNativeDivergenceForTest(true, false);
    result |= RunFrame(&divergent);
    EcsUiFrameInternalSetNativeDivergenceForTest(false, false);
    TestImage b = RenderTree(&divergent, 180, 120);
    result |= Require(b.pixels != NULL, "divergent pixel render failed");
    const uint32_t diffs = CountPixelDiffs(&a, &b);
    result |= Require(
        diffs != 0u && diffs != UINT32_MAX,
        "native divergence hook should change rendered pixels");
    UnloadTestImage(&a);
    UnloadTestImage(&b);
    return result;
}

int main(void)
{
    if (getenv("DISPLAY") == NULL && getenv("WAYLAND_DISPLAY") == NULL) {
        (void)fprintf(stderr, "skipping pixel test: no display environment\n");
        return 77;
    }

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "ecs-ui native pixel harness");
    if (!IsWindowReady()) {
        (void)fprintf(stderr, "raylib window init failed with display present\n");
        return 1;
    }

    int result = 0;
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 360.0f,
                .surface_height = 240.0f,
                .measure_text = TestMeasureText,
            }),
        "failed to initialize pixel frame backend");
    if (result == 0) {
        result |= TestStableNativePixels(1.0f);
        result |= TestStableNativePixels(2.0f);
        result |= TestDivergenceHookPixels();
    }
    EcsUiFrameBackendShutdown();
    CloseWindow();
    return result;
}
