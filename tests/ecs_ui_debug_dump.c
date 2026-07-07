#include "ecs_ui/ecs_ui.h"

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

static EcsUiTreeSnapshot SyntheticTree(void)
{
    EcsUiTreeSnapshot tree = {
        .root = 100u,
        .scale = 2.0f,
        .count = 4u,
        .truncated = true,
    };

    tree.nodes[0] = (EcsUiTreeNodeSnapshot){
        .entity = 100u,
        .kind = ECS_UI_NODE_ROOT,
        .parent_index = ECS_UI_TREE_INVALID_INDEX,
        .first_child = 1u,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .visual = {.opacity = 1.0f},
    };
    (void)snprintf(tree.nodes[0].id, sizeof(tree.nodes[0].id), "Root");

    tree.nodes[1] = (EcsUiTreeNodeSnapshot){
        .entity = 101u,
        .parent = 100u,
        .kind = ECS_UI_NODE_CUSTOM,
        .depth = 1u,
        .parent_index = 0u,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = 2u,
        .visual = {
            .opacity = 0.5f,
            .offset_x = 3.0f,
            .offset_y = -2.0f,
        },
        .placement = {
            .offset_x = 4.0f,
            .offset_y = 5.0f,
            .width = 60.0f,
            .height = 30.0f,
        },
        .layout_x = 10.0f,
        .layout_y = 20.0f,
        .layout_width = 30.0f,
        .layout_height = 40.0f,
        .hit_test = {.mode = ECS_UI_HIT_TEST_CAPTURE},
        .has_placement = true,
        .has_layout = true,
    };
    (void)snprintf(tree.nodes[1].id, sizeof(tree.nodes[1].id), "Dup");

    tree.nodes[2] = (EcsUiTreeNodeSnapshot){
        .entity = 102u,
        .parent = 100u,
        .kind = ECS_UI_NODE_BUTTON,
        .depth = 1u,
        .parent_index = 0u,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = 3u,
        .visual = {.opacity = 1.0f},
        .hit_test = {.mode = ECS_UI_HIT_TEST_NONE},
        .text_field_view = {.focused = true},
        .has_text_field_view = true,
    };
    (void)snprintf(tree.nodes[2].id, sizeof(tree.nodes[2].id), "Dup");

    tree.nodes[3] = (EcsUiTreeNodeSnapshot){
        .entity = 103u,
        .parent = 100u,
        .kind = ECS_UI_NODE_TEXT,
        .depth = 1u,
        .parent_index = 0u,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .visual = {.opacity = 1.0f},
        .hit_test = {.mode = ECS_UI_HIT_TEST_CHILDREN},
    };
    (void)snprintf(tree.nodes[3].id, sizeof(tree.nodes[3].id), "Text");

    return tree;
}

static int TestSnapshotDumpGolden(void)
{
    const char *golden =
        "tree root=100 scale=2 count=4 truncated=true\n"
        "[0] parent=none children=1..3 count=3 depth=0 entity=100 "
        "id=\"Root\" kind=root hit=auto visual(opacity=1 offset=0,0) "
        "placement=no layout=none\n"
        "[1] parent=0 children=none depth=1 entity=101 id=\"Dup\" "
        "kind=custom hit=capture visual(opacity=0.5 offset=3,-2) "
        "placement=yes layout=(10,20 30x40)\n"
        "[2] parent=0 children=none depth=1 entity=102 id=\"Dup\" "
        "kind=button hit=none visual(opacity=1 offset=0,0) "
        "placement=no text_field=focused layout=none\n"
        "[3] parent=0 children=none depth=1 entity=103 id=\"Text\" "
        "kind=text hit=children visual(opacity=1 offset=0,0) "
        "placement=no layout=none\n";

    EcsUiTreeSnapshot tree = SyntheticTree();
    char buffer[2048] = {0};
    EcsUiTreeDebugDumpResult result =
        EcsUiTreeDebugDumpSnapshot(&tree, buffer, sizeof(buffer));
    int failure = 0;
    failure |= Require(!result.truncated, "golden dump should fit");
    failure |= Require(
        result.bytes_written == strlen(golden),
        "golden dump byte count mismatch");
    failure |= Require(
        strcmp(buffer, golden) == 0,
        "snapshot dump golden mismatch");
    return failure;
}

static int TestSnapshotDumpBufferTruncation(void)
{
    EcsUiTreeSnapshot tree = SyntheticTree();
    char buffer[32] = {0};
    EcsUiTreeDebugDumpResult result =
        EcsUiTreeDebugDumpSnapshot(&tree, buffer, sizeof(buffer));

    int failure = 0;
    failure |= Require(result.truncated, "small dump should truncate");
    failure |= Require(
        result.bytes_written == sizeof(buffer) - 1u,
        "small dump should report copied bytes");
    failure |= Require(
        buffer[sizeof(buffer) - 1u] == '\0',
        "small dump should stay terminated");
    failure |= Require(
        strncmp(buffer, "tree root=100", strlen("tree root=100")) == 0,
        "small dump prefix mismatch");
    return failure;
}

int main(void)
{
    int failure = 0;
    failure |= TestSnapshotDumpGolden();
    failure |= TestSnapshotDumpBufferTruncation();
    return failure;
}
