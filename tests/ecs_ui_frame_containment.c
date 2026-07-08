#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_frame.h"

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

static EcsUiTreeSnapshot TestPressableTree(float scale)
{
    EcsUiTreeSnapshot tree = {
        .root = 1u,
        .scale = scale,
        .count = 1u,
    };
    tree.nodes[0] = TestNode(1u, "Root", ECS_UI_NODE_ROOT);
    tree.nodes[0].stack.axis = ECS_UI_AXIS_VERTICAL;
    tree.nodes[0].stack.padding = 4.0f;

    EcsUiTreeNodeSnapshot pressable =
        TestNode(2u, "Pressable", ECS_UI_NODE_PRESSABLE);
    pressable.on_click = 99u;
    pressable.payload = 1234u;
    pressable.pressable.preferred_height = 30.0f;
    pressable.has_box_style = true;
    pressable.box_style.background =
        (EcsUiColor){.r = 40, .g = 80, .b = 120, .a = 255};
    TestAppendChild(&tree, 0u, pressable);
    return tree;
}

static EcsUiFrameLayoutOptions TestLayout(float scale)
{
    return (EcsUiFrameLayoutOptions){
        .physical_bounds = {
            .x = 10.0f,
            .y = 14.0f,
            .width = 160.0f * scale,
            .height = 100.0f * scale,
        },
    };
}

static int TestNativeFrameInteraction(float scale)
{
    EcsUiTreeSnapshot tree = TestPressableTree(scale);
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = TestLayout(scale);
    EcsUiInteractionState state = {0};
    EcsUiFrameInteractionStateInit(&state);
    EcsUiInteractionFrame frame = {.state = &state};

    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.x + options.physical_bounds.width + 10.0f,
        options.physical_bounds.y + options.physical_bounds.height + 10.0f);
    const EcsUiPaintList *paint =
        EcsUiFrameRun(&tree, &theme, &options, NULL, &frame);
    int result = Require(paint != NULL, "native frame should produce paint");
    result |= Require(
        EcsUiFramePaintList() == paint,
        "paint accessor should return current native frame paint");
    result |= Require(
        frame.target_count > 0u,
        "native frame should build interaction targets");

    EcsUiEventList events = {0};
    const EcsUiPointerState pointer = {
        .x = options.physical_bounds.x + 12.0f * scale,
        .y = options.physical_bounds.y + 12.0f * scale,
        .down = true,
        .pressed = true,
    };
    EcsUiFrameCollectEvents(&frame, pointer, &events);
    result |= Require(events.count > 0u, "press should emit events");
    const EcsUiEvent *pressed = NULL;
    for (uint32_t i = 0u; i < events.count; i += 1u) {
        if (events.events[i].type == ECS_UI_EVENT_PRESSED) {
            pressed = &events.events[i];
            break;
        }
    }
    result |= Require(pressed != NULL, "press should emit PRESSED");
    if (pressed != NULL) {
        result |= Require(
            pressed->node == 2u &&
                pressed->action == 99u &&
                pressed->payload == 1234u,
            "press event should carry pressable identity");
    }
    result |= Require(
        frame.resolved_node == 2u &&
            strcmp(frame.resolved_node_id, "Pressable") == 0,
        "native frame should resolve pointer target");
    return result;
}

int main(void)
{
    int result = 0;
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 320.0f,
                .surface_height = 240.0f,
                .measure_text = TestMeasureText,
            }),
        "failed to initialize frame backend");
    if (result != 0) {
        return result;
    }

    result |= TestNativeFrameInteraction(1.0f);
    result |= TestNativeFrameInteraction(2.0f);

    EcsUiFrameBackendShutdown();
    return result;
}
