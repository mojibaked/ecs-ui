#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_frame.h"
#include "../src/ecs_ui_frame_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct TestFrameErrors {
    uint32_t count;
    EcsUiFrameErrorKind last_kind;
    char last_message[256];
} TestFrameErrors;

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static float AbsFloat(float value)
{
    return value < 0.0f ? -value : value;
}

static int RequireNear(
    float actual,
    float expected,
    float epsilon,
    const char *message)
{
    if (AbsFloat(actual - expected) <= epsilon) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: actual %.3f expected %.3f\n",
        message,
        actual,
        expected);
    return 1;
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

static EcsUiTreeSnapshot TestTree(float scale)
{
    EcsUiTreeSnapshot tree = {
        .root = 1u,
        .scale = scale,
        .count = 1u,
    };
    tree.nodes[0] = TestNode(1u, "Root", ECS_UI_NODE_ROOT);
    tree.nodes[0].stack.axis = ECS_UI_AXIS_VERTICAL;
    tree.nodes[0].stack.align_x = ECS_UI_ALIGN_START;
    tree.nodes[0].stack.align_y = ECS_UI_ALIGN_START;
    return tree;
}

static EcsUiTreeNodeSnapshot TestCustom(
    ecs_entity_t entity,
    const char *id,
    float width,
    float height)
{
    EcsUiTreeNodeSnapshot node = TestNode(entity, id, ECS_UI_NODE_CUSTOM);
    node.custom.preferred_width = width;
    node.custom.preferred_height = height;
    return node;
}

static EcsUiFrameLayoutOptions TestLayout(float scale, float width, float height)
{
    return (EcsUiFrameLayoutOptions){
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = width * scale,
            .height = height * scale,
        },
    };
}

static int RunFrame(
    EcsUiTreeSnapshot *tree,
    float logical_width,
    float logical_height)
{
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options =
        TestLayout(tree->scale, logical_width, logical_height);
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    return Require(
        EcsUiFrameRun(tree, &theme, &options, NULL, NULL) != NULL,
        "native frame run failed");
}

static int RequireRect(
    const EcsUiTreeNodeSnapshot *node,
    float x,
    float y,
    float width,
    float height,
    const char *label)
{
    int result = 0;
    char message[160] = {0};
    result |= Require(node != NULL && node->has_layout, label);
    if (node == NULL || !node->has_layout) {
        return result;
    }
    (void)snprintf(message, sizeof(message), "%s x", label);
    result |= RequireNear(node->layout_x, x, 0.001f, message);
    (void)snprintf(message, sizeof(message), "%s y", label);
    result |= RequireNear(node->layout_y, y, 0.001f, message);
    (void)snprintf(message, sizeof(message), "%s width", label);
    result |= RequireNear(node->layout_width, width, 0.001f, message);
    (void)snprintf(message, sizeof(message), "%s height", label);
    result |= RequireNear(node->layout_height, height, 0.001f, message);
    return result;
}

static int TestFixedVerticalStack(float scale)
{
    EcsUiTreeSnapshot tree = TestTree(scale);
    tree.nodes[0].stack.padding_left = 10.0f;
    tree.nodes[0].stack.padding_top = 5.0f;
    tree.nodes[0].stack.gap = 3.0f;
    TestAppendChild(&tree, 0u, TestCustom(2u, "FixedA", 40.0f, 20.0f));
    TestAppendChild(&tree, 0u, TestCustom(3u, "FixedB", 60.0f, 30.0f));

    int result = RunFrame(&tree, 200.0f, 120.0f);
    result |= RequireRect(&tree.nodes[0], 0.0f, 0.0f, 200.0f, 120.0f, "root");
    result |= RequireRect(&tree.nodes[1], 10.0f, 5.0f, 40.0f, 20.0f, "fixed A");
    result |= RequireRect(&tree.nodes[2], 10.0f, 28.0f, 60.0f, 30.0f, "fixed B");
    return result;
}

static int TestFractionalPaddingGap(float scale)
{
    EcsUiTreeSnapshot tree = TestTree(scale);
    tree.nodes[0].stack.padding_left = 2.5f;
    tree.nodes[0].stack.padding_top = 2.5f;
    tree.nodes[0].stack.gap = 3.5f;
    TestAppendChild(&tree, 0u, TestCustom(4u, "FracA", 10.0f, 10.0f));
    TestAppendChild(&tree, 0u, TestCustom(5u, "FracB", 10.0f, 10.0f));

    const float snapped_padding = scale == 1.0f ? 2.0f : 2.5f;
    const float snapped_gap = scale == 1.0f ? 3.0f : 3.5f;
    int result = RunFrame(&tree, 80.0f, 60.0f);
    result |= RequireRect(
        &tree.nodes[1],
        snapped_padding,
        snapped_padding,
        10.0f,
        10.0f,
        "fractional A");
    result |= RequireRect(
        &tree.nodes[2],
        snapped_padding,
        snapped_padding + 10.0f + snapped_gap,
        10.0f,
        10.0f,
        "fractional B");
    return result;
}

static int TestScrollContentReport(float scale)
{
    EcsUiTreeSnapshot tree = TestTree(scale);
    EcsUiTreeNodeSnapshot scroll = TestNode(6u, "Scroll", ECS_UI_NODE_VSTACK);
    scroll.stack.axis = ECS_UI_AXIS_VERTICAL;
    scroll.stack.preferred_width = 50.0f;
    scroll.stack.preferred_height = 25.0f;
    scroll.has_scroll_view = true;
    scroll.scroll_view.axes = ECS_UI_SCROLL_AXIS_Y;
    TestAppendChild(&tree, 0u, scroll);
    TestAppendChild(&tree, 1u, TestCustom(7u, "ScrollChild", 40.0f, 90.0f));

    EcsUiSolverScrollContent contents[ECS_UI_TREE_NODE_MAX] = {0};
    EcsUiFrameInternalSetNativeScrollContentOutput(contents, tree.count);
    int result = RunFrame(&tree, 120.0f, 80.0f);
    EcsUiFrameInternalSetNativeScrollContentOutput(NULL, 0u);
    result |= RequireRect(&tree.nodes[1], 0.0f, 0.0f, 50.0f, 25.0f, "scroll");
    result |= Require(
        contents[1].valid,
        "scroll content report should be valid");
    result |= RequireNear(contents[1].width, 40.0f, 0.001f, "scroll content width");
    result |= RequireNear(contents[1].height, 90.0f, 0.001f, "scroll content height");
    return result;
}

static int TestDivergenceHook(void)
{
    EcsUiTreeSnapshot tree = TestTree(1.0f);
    TestAppendChild(&tree, 0u, TestCustom(8u, "DivergeChild", 20.0f, 10.0f));

    EcsUiFrameInternalSetNativeDivergenceForTest(true, false);
    int result = RunFrame(&tree, 80.0f, 50.0f);
    EcsUiFrameInternalSetNativeDivergenceForTest(false, false);
    result |= RequireNear(
        tree.nodes[0].layout_x,
        7.0f,
        0.001f,
        "forced root divergence should perturb layout");
    return result;
}

int main(void)
{
    int result = 0;
    TestFrameErrors errors = {0};
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 640.0f,
                .surface_height = 480.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = &errors,
            }),
        "failed to initialize native solver golden backend");
    if (result != 0) {
        return result;
    }

    result |= TestFixedVerticalStack(1.0f);
    result |= TestFixedVerticalStack(2.0f);
    result |= TestFractionalPaddingGap(1.0f);
    result |= TestFractionalPaddingGap(2.0f);
    result |= TestScrollContentReport(1.0f);
    result |= TestScrollContentReport(2.0f);
    result |= TestDivergenceHook();

    EcsUiFrameBackendShutdown();
    return result;
}
