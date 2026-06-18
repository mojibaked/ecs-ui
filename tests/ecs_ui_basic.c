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

static int RequireNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *id,
    EcsUiNodeKind kind)
{
    if (tree == NULL || index >= tree->count) {
        return Require(false, "node index out of range");
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (strcmp(node->id, id) != 0 || node->kind != kind) {
        (void)fprintf(
            stderr,
            "unexpected node at %u: id=%s kind=%d\n",
            index,
            node->id,
            (int)node->kind);
        return 1;
    }
    return 0;
}

int main(void)
{
    ecs_world_t *world = ecs_init();
    if (world == NULL) {
        return 1;
    }

    EcsUiImport(world);
    int result = 0;
    result |= Require(
        ecs_has_id(world, EcsUiOnClick, EcsExclusive),
        "EcsUiOnClick should be exclusive");

    ecs_entity_t root = EcsUiRootEntity(world, "Home");
    if (root == 0) {
        ecs_fini(world);
        return 2;
    }

    ecs_entity_t present_add_machine_action =
        ecs_entity(world, {.name = "PresentAddMachineAction"});

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    VStack(&builder, {.id = "HomeStack", .gap = 10.0f, .padding = 16.0f}) {
        Button(
            &builder,
            {
                .id = "AddMachine",
                .variant = ECS_UI_BUTTON_PRIMARY,
                .on_click = present_add_machine_action,
            }) {
            Text(
                &builder,
                {
                    .id = "AddLabel",
                    .text = "add machine",
                    .role = ECS_UI_TEXT_BUTTON,
                });
        }
        HStack(&builder, {.id = "Footer", .gap = 4.0f}) {
            Icon(&builder, {.id = "FooterIcon", .name = "plus"});
            Text(
                &builder,
                {
                    .id = "FooterLabel",
                    .text = "footer",
                    .role = ECS_UI_TEXT_CAPTION,
                });
        }
    }
    EcsUiBuilderEnd(&builder);

    result |= Require(EcsUiBuilderOk(&builder), "builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "read tree failed");
    result |= Require(tree.count == 7u, "unexpected tree count");
    result |= Require(!tree.truncated, "tree truncated");
    result |= RequireNode(&tree, 0u, "Home", ECS_UI_NODE_ROOT);
    result |= RequireNode(&tree, 1u, "HomeStack", ECS_UI_NODE_VSTACK);
    result |= RequireNode(&tree, 2u, "AddMachine", ECS_UI_NODE_BUTTON);
    result |= RequireNode(&tree, 3u, "AddLabel", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 4u, "Footer", ECS_UI_NODE_HSTACK);
    result |= RequireNode(&tree, 5u, "FooterIcon", ECS_UI_NODE_ICON);
    result |= RequireNode(&tree, 6u, "FooterLabel", ECS_UI_NODE_TEXT);

    result |= Require(
        tree.nodes[1u].first_child == 2u,
        "HomeStack first child should be AddMachine");
    result |= Require(
        tree.nodes[2u].next_sibling == 4u,
        "AddMachine next sibling should be Footer");
    result |= Require(
        strcmp(tree.nodes[3u].text.text, "add machine") == 0,
        "text payload not copied");
    result |= Require(
        tree.nodes[2u].visual.opacity == 1.0f,
        "visual opacity should default to 1");
    result |= Require(
        tree.nodes[2u].visual.offset_x == 0.0f &&
            tree.nodes[2u].visual.offset_y == 0.0f,
        "visual offset should default to 0");

    ecs_entity_t home_stack = tree.nodes[1u].entity;
    result |= Require(
        ecs_has_id(world, home_stack, EcsOrderedChildren),
        "HomeStack should have EcsOrderedChildren");
    result |= Require(
        ecs_has_pair(
            world,
            tree.nodes[2u].entity,
            EcsUiOnClick,
            present_add_machine_action),
        "button should have OnClick action pair");
    result |= Require(
        tree.nodes[2u].on_click == present_add_machine_action,
        "button snapshot should expose OnClick action");

    ecs_entities_t ordered = ecs_get_ordered_children(world, home_stack);
    result |= Require(ordered.count == 2, "ordered child count mismatch");
    result |= Require(
        ordered.ids[0] == tree.nodes[2u].entity &&
            ordered.ids[1] == tree.nodes[4u].entity,
        "ordered children mismatch");

    ecs_fini(world);
    return result;
}
