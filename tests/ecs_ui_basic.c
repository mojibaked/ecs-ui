#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_animation.h"
#include "ecs_ui/ecs_ui_projection.h"

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

typedef struct TestProjectionItem {
    uint64_t key;
    char label[ECS_UI_TEXT_MAX];
} TestProjectionItem;

typedef struct TestProjectionContext {
    ecs_entity_t ui_parent;
    ecs_entity_t label_slot;
    uint32_t build_count;
    uint32_t update_count;
} TestProjectionContext;

ECS_COMPONENT_DECLARE(TestProjectionItem);

static void TestProjectionSyncSource(
    ecs_world_t *world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx)
{
    (void)ctx;

    const TestProjectionItem *data =
        item != NULL ? item->data : NULL;
    if (data != NULL) {
        ecs_set_ptr(world, source, TestProjectionItem, data);
    }
}

static ecs_entity_t TestProjectionBuildRoot(
    ecs_world_t *world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx)
{
    TestProjectionContext *projection = ctx;
    const TestProjectionItem *data =
        item != NULL ? item->data : NULL;
    if (projection == NULL || data == NULL) {
        return 0;
    }

    char row_id[ECS_UI_ID_MAX] = {0};
    char label_id[ECS_UI_ID_MAX] = {0};
    (void)snprintf(
        row_id,
        sizeof(row_id),
        "CollectionRow%llu",
        (unsigned long long)data->key);
    (void)snprintf(
        label_id,
        sizeof(label_id),
        "CollectionLabel%llu",
        (unsigned long long)data->key);

    EcsUiBuilder builder =
        EcsUiBuilderBegin(world, projection->ui_parent);
    ecs_entity_t row = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = row_id,
        });
    ecs_entity_t label = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = label_id,
            .text = data->label,
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);

    if (EcsUiBuilderOk(&builder) && row != 0 && label != 0) {
        (void)EcsUiProjectionSetNode(
            world,
            source,
            projection->label_slot,
            label);
        projection->build_count += 1u;
        return row;
    }
    return 0;
}

static void TestProjectionUpdateRoot(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t ui_root,
    const EcsUiProjectionCollectionSource *item,
    uint32_t position,
    uint32_t count,
    void *ctx)
{
    (void)ui_root;
    (void)count;

    TestProjectionContext *projection = ctx;
    const TestProjectionItem *data =
        item != NULL ? item->data : NULL;
    if (projection == NULL || data == NULL) {
        return;
    }

    ecs_entity_t label =
        EcsUiProjectionGetNode(world, source, projection->label_slot);
    EcsUiText *text =
        label != 0 ? ecs_get_mut(world, label, EcsUiText) : NULL;
    if (text != NULL) {
        (void)snprintf(
            text->text,
            sizeof(text->text),
            "%u:%.200s",
            position,
            data->label);
        text->role = ECS_UI_TEXT_BODY;
        ecs_modified(world, label, EcsUiText);
    }
    projection->update_count += 1u;
}

int main(void)
{
    ecs_world_t *world = ecs_init();
    if (world == NULL) {
        return 1;
    }

    EcsUiImport(world);
    EcsUiAnimationImport(world);
    EcsUiProjectionImport(world);
    ECS_COMPONENT_DEFINE(world, TestProjectionItem);
    int result = 0;
    result |= Require(
        ecs_has_id(world, EcsUiOnClick, EcsExclusive),
        "EcsUiOnClick should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiProjectionRoot, EcsExclusive),
        "EcsUiProjectionRoot should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiProjectionSource, EcsExclusive),
        "EcsUiProjectionSource should be exclusive");
    result |= Require(
        ecs_id(EcsUiProjectionKey) != 0,
        "EcsUiProjectionKey should be registered");
    result |= Require(
        ecs_id(EcsUiAnimatedFloat) != 0,
        "EcsUiAnimatedFloat should be registered");
    result |= Require(
        ecs_id(EcsUiLinear1f) != 0,
        "EcsUiLinear1f should be registered");

    ecs_entity_t animation_target =
        ecs_entity(world, {.name = "AnimationTarget"});
    EcsUiAnimationStartLinear1f(world, animation_target, 0.0f, 1.0f, 1.0f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        0.0f,
        0.0001f,
        "animation start value mismatch");
    (void)ecs_progress(world, 0.25f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        0.25f,
        0.0001f,
        "animation quarter value mismatch");
    result |= Require(
        ecs_get(world, animation_target, EcsUiLinear1f) != NULL,
        "animation should still have linear component");
    result |= Require(
        !ecs_has_id(world, animation_target, EcsUiAnimationComplete),
        "animation should not complete early");
    (void)ecs_progress(world, 1.0f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        1.0f,
        0.0001f,
        "animation final value mismatch");
    result |= Require(
        ecs_get(world, animation_target, EcsUiLinear1f) == NULL,
        "animation should remove linear component on completion");
    result |= Require(
        ecs_has_id(world, animation_target, EcsUiAnimationComplete),
        "animation should add completion tag");
    EcsUiAnimationSetValue(world, animation_target, 1.5f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        1.0f,
        0.0001f,
        "manual animation value should clamp");
    result |= Require(
        !ecs_has_id(world, animation_target, EcsUiAnimationComplete),
        "manual animation value should clear completion tag");

    ecs_entity_t visual_target =
        ecs_entity(world, {.name = "AnimationVisualTarget"});
    EcsUiAnimationApplyFadeSlideY(world, visual_target, 0.25f, 80.0f);
    const EcsUiVisual *visual =
        ecs_get(world, visual_target, EcsUiVisual);
    result |= Require(
        visual != NULL,
        "fade slide visual should be set");
    if (visual != NULL) {
        result |= RequireNear(
            visual->opacity,
            0.25f,
            0.0001f,
            "fade slide opacity mismatch");
        result |= RequireNear(
            visual->offset_y,
            60.0f,
            0.0001f,
            "fade slide offset mismatch");
    }
    EcsUiAnimationApplyHighlight(world, visual_target, 0.5f);
    visual = ecs_get(world, visual_target, EcsUiVisual);
    result |= Require(
        visual != NULL,
        "highlight visual should be set");
    if (visual != NULL) {
        result |= RequireNear(
            visual->opacity,
            1.0f,
            0.0001f,
            "highlight opacity mismatch");
        result |= RequireNear(
            visual->highlight,
            0.5f,
            0.0001f,
            "highlight value mismatch");
    }

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
        Custom(
            &builder,
            {
                .id = "TerminalPreview",
                .kind = "terminal",
                .preferred_width = 320.0f,
                .preferred_height = 120.0f,
            });
    }
    EcsUiBuilderEnd(&builder);

    result |= Require(EcsUiBuilderOk(&builder), "builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "read tree failed");
    result |= Require(tree.count == 8u, "unexpected tree count");
    result |= Require(!tree.truncated, "tree truncated");
    result |= RequireNode(&tree, 0u, "Home", ECS_UI_NODE_ROOT);
    result |= RequireNode(&tree, 1u, "HomeStack", ECS_UI_NODE_VSTACK);
    result |= RequireNode(&tree, 2u, "AddMachine", ECS_UI_NODE_BUTTON);
    result |= RequireNode(&tree, 3u, "AddLabel", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 4u, "Footer", ECS_UI_NODE_HSTACK);
    result |= RequireNode(&tree, 5u, "FooterIcon", ECS_UI_NODE_ICON);
    result |= RequireNode(&tree, 6u, "FooterLabel", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 7u, "TerminalPreview", ECS_UI_NODE_CUSTOM);

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
        strcmp(tree.nodes[7u].custom.kind, "terminal") == 0,
        "custom kind not copied");
    result |= Require(
        tree.nodes[7u].custom.preferred_width == 320.0f &&
            tree.nodes[7u].custom.preferred_height == 120.0f,
        "custom preferred size not copied");
    result |= Require(
        tree.nodes[2u].visual.opacity == 1.0f,
        "visual opacity should default to 1");
    result |= Require(
        tree.nodes[2u].visual.offset_x == 0.0f &&
            tree.nodes[2u].visual.offset_y == 0.0f,
        "visual offset should default to 0");
    result |= Require(
        tree.nodes[2u].visual.highlight == 0.0f,
        "visual highlight should default to 0");

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
    result |= Require(ordered.count == 3, "ordered child count mismatch");
    result |= Require(
        ordered.ids[0] == tree.nodes[2u].entity &&
            ordered.ids[1] == tree.nodes[4u].entity &&
            ordered.ids[2] == tree.nodes[7u].entity,
        "ordered children mismatch");

    ecs_entity_t source_tag =
        ecs_entity(world, {.name = "TestProjectionSource"});
    ecs_entity_t source_parent =
        ecs_entity(world, {.name = "TestProjectionSources"});
    ecs_add_id(world, source_parent, EcsOrderedChildren);
    ecs_entity_t source_a = ecs_entity(world, {
        .parent = source_parent,
        .name = "SourceA",
        .sep = "",
    });
    ecs_entity_t source_b = ecs_entity(world, {
        .parent = source_parent,
        .name = "SourceB",
        .sep = "",
    });
    ecs_add_id(world, source_a, source_tag);
    ecs_add_id(world, source_b, source_tag);

    ecs_entity_t projection_root =
        EcsUiRootEntity(world, "ProjectionRoot");
    EcsUiBuilder projection_builder =
        EcsUiBuilderBegin(world, projection_root);
    Text(
        &projection_builder,
        {
            .id = "ProjectionHeader",
            .text = "header",
            .role = ECS_UI_TEXT_LABEL,
        });
    ecs_entity_t row_a = EcsUiBeginHStack(
        &projection_builder,
        (EcsUiStackDesc){
            .id = "ProjectedRowA",
        });
    ecs_entity_t label_a = EcsUiAddText(
        &projection_builder,
        (EcsUiTextDesc){
            .id = "ProjectedLabelA",
            .text = "A",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&projection_builder);
    ecs_entity_t row_b = EcsUiBeginHStack(
        &projection_builder,
        (EcsUiStackDesc){
            .id = "ProjectedRowB",
        });
    ecs_entity_t label_b = EcsUiAddText(
        &projection_builder,
        (EcsUiTextDesc){
            .id = "ProjectedLabelB",
            .text = "B",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&projection_builder);
    EcsUiBuilderEnd(&projection_builder);
    result |= Require(
        EcsUiBuilderOk(&projection_builder),
        "projection builder failed");

    ecs_entity_t label_slot =
        ecs_entity(world, {.name = "TestProjectionLabelSlot"});
    result |= Require(
        EcsUiProjectionLink(world, source_a, row_a),
        "projection link A failed");
    result |= Require(
        EcsUiProjectionLink(world, source_b, row_b),
        "projection link B failed");
    result |= Require(
        EcsUiProjectionSetNode(world, source_a, label_slot, label_a),
        "projection slot A failed");
    result |= Require(
        EcsUiProjectionSetNode(world, source_b, label_slot, label_b),
        "projection slot B failed");
    result |= Require(
        EcsUiProjectionGetRoot(world, source_a) == row_a,
        "projection root lookup failed");
    result |= Require(
        ecs_has_id(world, row_a, EcsUiProjectionRootNode) &&
            ecs_has_id(world, row_b, EcsUiProjectionRootNode),
        "projected row roots should be marked");
    result |= Require(
        EcsUiProjectionGetSource(world, row_b) == source_b,
        "projection source lookup failed");
    result |= Require(
        EcsUiProjectionGetNode(world, source_a, label_slot) == label_a,
        "projection slot lookup failed");
    result |= Require(
        ecs_has_id(world, label_slot, EcsUiProjectionSlot) &&
            ecs_has_id(world, label_slot, EcsExclusive),
        "projection slot should be registered");

    ecs_entity_t source_order[2] = {source_b, source_a};
    ecs_set_child_order(world, source_parent, source_order, 2);
    result |= Require(
        EcsUiProjectionSyncOrderedChildren(
            world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = source_parent,
                .ui_parent = projection_root,
                .source_filter = source_tag,
                .preserve_unprojected_ui_children = true,
            }),
        "projection order sync failed");
    ecs_entities_t projected_order =
        ecs_get_ordered_children(world, projection_root);
    result |= Require(
        projected_order.count == 3,
        "projection ordered child count mismatch");
    result |= Require(
        projected_order.ids[0] != row_a &&
            projected_order.ids[0] != row_b &&
            projected_order.ids[1] == row_b &&
            projected_order.ids[2] == row_a,
        "projection order mismatch");

    ecs_delete(world, source_b);
    result |= Require(
        !EcsUiProjectionSyncOrderedChildren(
            world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = source_parent,
                .ui_parent = projection_root,
                .source_filter = source_tag,
                .preserve_unprojected_ui_children = true,
            }),
        "projection order sync should reject stale projected rows");

    ecs_delete(world, row_b);
    result |= Require(
        EcsUiProjectionSyncOrderedChildren(
            world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = source_parent,
                .ui_parent = projection_root,
                .source_filter = source_tag,
                .preserve_unprojected_ui_children = true,
            }),
        "projection order sync after stale cleanup failed");
    projected_order = ecs_get_ordered_children(world, projection_root);
    result |= Require(
        projected_order.count == 2 &&
            projected_order.ids[0] != row_a &&
            projected_order.ids[1] == row_a,
        "projection order after stale cleanup mismatch");

    result |= Require(
        EcsUiProjectionDelete(world, source_a),
        "projection delete failed");
    result |= Require(
        EcsUiProjectionGetRoot(world, source_a) == 0 &&
            !ecs_is_alive(world, row_a),
        "projection delete cleanup failed");

    ecs_entity_t collection_source_parent =
        ecs_entity(world, {.name = "CollectionSources"});
    ecs_add_id(world, collection_source_parent, EcsOrderedChildren);
    ecs_entity_t collection_ui_parent =
        EcsUiRootEntity(world, "CollectionUi");
    EcsUiBuilder collection_builder =
        EcsUiBuilderBegin(world, collection_ui_parent);
    (void)EcsUiAddText(
        &collection_builder,
        (EcsUiTextDesc){
            .id = "CollectionHeader",
            .text = "collection",
            .role = ECS_UI_TEXT_LABEL,
        });
    EcsUiBuilderEnd(&collection_builder);
    result |= Require(
        EcsUiBuilderOk(&collection_builder),
        "collection builder failed");

    TestProjectionContext collection_ctx = {
        .ui_parent = collection_ui_parent,
        .label_slot = ecs_entity(world, {.name = "CollectionLabelSlot"}),
    };
    TestProjectionItem collection_data[2] = {
        {
            .key = 1u,
            .label = "alpha",
        },
        {
            .key = 2u,
            .label = "beta",
        },
    };
    EcsUiProjectionCollectionSource collection_items[2] = {
        {
            .key = collection_data[0].key,
            .data = &collection_data[0],
        },
        {
            .key = collection_data[1].key,
            .data = &collection_data[1],
        },
    };
    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = collection_items,
                .item_count = 2u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "collection sync failed");
    result |= Require(
        collection_ctx.build_count == 2u &&
            collection_ctx.update_count == 2u,
        "collection build/update counts mismatch");

    ecs_entities_t collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    result |= Require(
        collection_sources.count == 2,
        "collection source count mismatch");
    const EcsUiProjectionKey *first_key =
        ecs_get(world, collection_sources.ids[0], EcsUiProjectionKey);
    const EcsUiProjectionKey *second_key =
        ecs_get(world, collection_sources.ids[1], EcsUiProjectionKey);
    result |= Require(
        first_key != NULL && second_key != NULL &&
            first_key->value == 1u && second_key->value == 2u,
        "collection source key order mismatch");
    ecs_entity_t collection_row_1 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[0]);
    ecs_entity_t collection_row_2 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[1]);
    result |= Require(
        collection_row_1 != 0 && collection_row_2 != 0,
        "collection rows not linked");

    ecs_entities_t collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_order.count == 3 &&
            collection_order.ids[1] == collection_row_1 &&
            collection_order.ids[2] == collection_row_2,
        "collection row order mismatch");

    collection_data[0] = (TestProjectionItem){
        .key = 2u,
        .label = "beta updated",
    };
    collection_data[1] = (TestProjectionItem){
        .key = 1u,
        .label = "alpha",
    };
    collection_items[0] = (EcsUiProjectionCollectionSource){
        .key = collection_data[0].key,
        .data = &collection_data[0],
    };
    collection_items[1] = (EcsUiProjectionCollectionSource){
        .key = collection_data[1].key,
        .data = &collection_data[1],
    };
    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = collection_items,
                .item_count = 2u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "collection reorder sync failed");
    collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    collection_row_2 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[0]);
    collection_row_1 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[1]);
    collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_order.count == 3 &&
            collection_order.ids[1] == collection_row_2 &&
            collection_order.ids[2] == collection_row_1,
        "collection reorder mismatch");
    ecs_entity_t updated_label =
        EcsUiProjectionGetNode(
            world,
            collection_sources.ids[0],
            collection_ctx.label_slot);
    const EcsUiText *updated_text =
        updated_label != 0 ? ecs_get(world, updated_label, EcsUiText) : NULL;
    result |= Require(
        updated_text != NULL &&
            strcmp(updated_text->text, "1:beta updated") == 0,
        "collection row update mismatch");

    EcsUiProjectionCollectionSource one_item[1] = {
        {
            .key = collection_data[0].key,
            .data = &collection_data[0],
        },
    };
    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = one_item,
                .item_count = 1u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "collection stale delete sync failed");
    collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_sources.count == 1 &&
            collection_order.count == 2,
        "collection stale delete mismatch");

    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = NULL,
                .item_count = 0u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "empty collection sync failed");
    collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_sources.count == 0 &&
            collection_order.count == 1,
        "empty collection cleanup mismatch");

    ecs_fini(world);
    return result;
}
