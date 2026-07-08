#include "ecs_ui/ecs_ui_frame.h"
#include "../src/ecs_ui_frame_internal.h"

#include <stdint.h>
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
        .width = (float)safe_length * font_size * 0.5f,
        .height = font_size + 4.0f,
    };
}

static EcsUiFrameLayoutOptions TestLayout(void)
{
    return (EcsUiFrameLayoutOptions){
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 160.0f,
            .height = 100.0f,
        },
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

static EcsUiTreeSnapshot TestRootTree(const char *id)
{
    EcsUiTreeSnapshot tree = {
        .root = 1,
        .scale = 1.0f,
        .count = 1u,
    };
    tree.nodes[0] = TestNode(1, id, ECS_UI_NODE_ROOT);
    tree.nodes[0].stack.preferred_width = 120.0f;
    tree.nodes[0].stack.preferred_height = 80.0f;
    return tree;
}

static void TestAppendChild(EcsUiTreeSnapshot *tree, EcsUiTreeNodeSnapshot child)
{
    if (tree == NULL || tree->count >= ECS_UI_TREE_NODE_MAX) {
        return;
    }
    const uint32_t index = tree->count;
    tree->count += 1u;
    child.parent = tree->nodes[0].entity;
    child.parent_index = 0u;
    child.next_sibling = ECS_UI_TREE_INVALID_INDEX;
    tree->nodes[index] = child;
    if (tree->nodes[0].first_child == ECS_UI_TREE_INVALID_INDEX) {
        tree->nodes[0].first_child = index;
        return;
    }
    uint32_t last = tree->nodes[0].first_child;
    while (tree->nodes[last].next_sibling != ECS_UI_TREE_INVALID_INDEX) {
        last = tree->nodes[last].next_sibling;
    }
    tree->nodes[last].next_sibling = index;
}

static int ExpectFrameError(
    EcsUiTreeSnapshot *tree,
    EcsUiFrameErrorKind kind,
    const char *message,
    TestFrameErrors *errors)
{
    int result = 0;
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = TestLayout();
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(tree, &theme, &options, NULL, NULL) == NULL,
        message);
    result |= Require(
        errors->count == 1u && errors->last_kind == kind,
        "frame should report expected error kind");
    return result;
}

static int TestBackendErrors(TestFrameErrors *errors)
{
    int result = 0;
    *errors = (TestFrameErrors){0};
    result |= Require(
        !EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 160.0f,
                .surface_height = 100.0f,
                .error = TestFrameHandleError,
                .error_user_data = errors,
            }),
        "backend init without measure callback should fail");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_MEASURE_TEXT_MISSING,
        "backend init should report missing measure callback");

    EcsUiFrameInternalSetBackendDescForTest(
        &(EcsUiFrameBackendDesc){
            .surface_width = 160.0f,
            .surface_height = 100.0f,
            .measure_text = TestMeasureText,
            .error = TestFrameHandleError,
            .error_user_data = errors,
        });
    EcsUiTreeSnapshot tree = TestRootTree("NotInitialized");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = TestLayout();
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) == NULL,
        "uninitialized frame run should fail");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_NOT_INITIALIZED,
        "uninitialized frame run should report not initialized");
    EcsUiFrameInternalSetBackendDescForTest(NULL);
    return result;
}

static int TestNativeSolverErrors(TestFrameErrors *errors)
{
    int result = 0;
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    EcsUiFrameBackendSetSurfaceSize(160.0f, 100.0f);

    EcsUiTreeSnapshot duplicate = TestRootTree("Duplicate");
    TestAppendChild(
        &duplicate,
        TestNode(duplicate.nodes[0].entity, "Duplicate", ECS_UI_NODE_VSTACK));
    result |= ExpectFrameError(
        &duplicate,
        ECS_UI_FRAME_ERROR_DUPLICATE_ID,
        "native duplicate id tree should fail",
        errors);

    EcsUiTreeSnapshot floating = TestRootTree("FloatingParent");
    TestAppendChild(
        &floating,
        TestNode(2, "FloatingChild", ECS_UI_NODE_VSTACK));
    floating.nodes[1].has_placement = true;
    floating.nodes[1].placement.mode = ECS_UI_PLACEMENT_PARENT;
    floating.nodes[1].parent = 0;
    floating.nodes[1].parent_index = ECS_UI_TREE_INVALID_INDEX;
    result |= ExpectFrameError(
        &floating,
        ECS_UI_FRAME_ERROR_FLOATING_PARENT_NOT_FOUND,
        "native missing placement parent tree should fail",
        errors);

    EcsUiTreeSnapshot text_capacity = TestRootTree("TextCapacity");
    EcsUiTreeNodeSnapshot text =
        TestNode(3, "TextCapacityText", ECS_UI_NODE_TEXT);
    CopyString(text.text.text, sizeof(text.text.text), "aa\nbb");
    text.text.role = ECS_UI_TEXT_BODY;
    TestAppendChild(&text_capacity, text);
    EcsUiFrameInternalSetTextMeasureLineCapacity(1u);
    result |= ExpectFrameError(
        &text_capacity,
        ECS_UI_FRAME_ERROR_TEXT_MEASURE_CAPACITY,
        "native text measure capacity tree should fail",
        errors);
    EcsUiFrameInternalSetTextMeasureLineCapacity(0u);

    EcsUiTreeSnapshot allocation = TestRootTree("Allocation");
    allocation.count = (UINT32_MAX / 5u) + 1u;
    result |= ExpectFrameError(
        &allocation,
        ECS_UI_FRAME_ERROR_ALLOCATION_FAILED,
        "native allocation guard tree should fail",
        errors);

    EcsUiTreeSnapshot internal = {0};
    result |= ExpectFrameError(
        &internal,
        ECS_UI_FRAME_ERROR_INTERNAL,
        "native empty tree should fail internally",
        errors);
    return result;
}

static int TestRuntimeFrameErrors(TestFrameErrors *errors)
{
    int result = 0;
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = TestLayout();

    *errors = (TestFrameErrors){0};
    result |= Require(
        !EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 160.0f,
                .surface_height = 100.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = errors,
            }),
        "second backend init should fail");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_ALREADY_INITIALIZED,
        "second backend init should report already initialized");

    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(NULL, &theme, NULL, NULL, NULL) == NULL,
        "null tree frame run should fail");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_INVALID_ARGUMENT,
        "null tree should report invalid argument");

    EcsUiTreeSnapshot tree = TestRootTree("StaleRoot");
    EcsUiInteractionState state = {0};
    EcsUiFrameInteractionStateInit(&state);
    EcsUiInteractionFrame frame = {.state = &state};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, &frame) != NULL,
        "stale setup frame run failed");
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "headless stale-invalidating frame run failed");
    EcsUiEventList events = {0};
    *errors = (TestFrameErrors){0};
    EcsUiFrameCollectEvents(&frame, (EcsUiPointerState){0}, &events);
    result |= Require(
        events.count == 0u,
        "stale collect should not emit events");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_STALE_INTERACTION_FRAME,
        "stale collect should report stale interaction frame");

    EcsUiTreeSnapshot paint_capacity = TestRootTree("PaintCapacity");
    EcsUiTreeNodeSnapshot child =
        TestNode(4, "PaintCapacityChild", ECS_UI_NODE_VSTACK);
    child.has_box_style = true;
    child.box_style.background = (EcsUiColor){.r = 80, .g = 90, .b = 100, .a = 255};
    child.stack.preferred_width = 32.0f;
    child.stack.preferred_height = 24.0f;
    TestAppendChild(&paint_capacity, child);
    EcsUiFrameInternalSetPaintItemCapacity(1u);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&paint_capacity, &theme, &options, NULL, NULL) != NULL,
        "paint capacity overflow should not abort frame run");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY,
        "paint capacity overflow should report element capacity");
    EcsUiFrameInternalSetPaintItemCapacity(0u);

    return result | TestNativeSolverErrors(errors);
}

int main(void)
{
    int result = 0;
    TestFrameErrors errors = {0};

    result |= TestBackendErrors(&errors);
    errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 160.0f,
                .surface_height = 100.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = &errors,
            }),
        "failed to initialize frame backend");
    result |= TestRuntimeFrameErrors(&errors);
    EcsUiFrameBackendShutdown();
    return result;
}
