#include "demo_app.h"

#include <raylib.h>
#include <stdio.h>

ECS_COMPONENT_DECLARE(DemoItem);
ECS_COMPONENT_DECLARE(DemoItemSequence);
ECS_TAG_DECLARE(DemoAddItemRequest);
ECS_TAG_DECLARE(DemoSelectItemRequest);
ECS_TAG_DECLARE(DemoDeleteItemRequest);
ECS_TAG_DECLARE(DemoRenameItemRequest);
ECS_TAG_DECLARE(DemoMoveItemUpRequest);
ECS_TAG_DECLARE(DemoMoveItemDownRequest);
ECS_TAG_DECLARE(DemoItemOrderDirty);
ECS_TAG_DECLARE(DemoSelectedItem);
ECS_TAG_DECLARE(DemoItemUiNode);
ECS_TAG_DECLARE(DemoItemSelectUiNode);
ECS_TAG_DECLARE(DemoItemLabelUiNode);
ECS_TAG_DECLARE(DemoItemMetaUiNode);
ECS_TAG_DECLARE(DemoUiForItem);

ecs_entity_t DemoAppItemRoot(ecs_world_t *world)
{
    ecs_entity_t root = ecs_entity(world, {.name = "DemoItems"});
    ecs_add_id(world, root, EcsOrderedChildren);
    return root;
}

ecs_entity_t DemoAppSelectionRoot(ecs_world_t *world)
{
    return ecs_entity(world, {.name = "DemoSelection"});
}

void DemoAppRequestAddItem(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    (void)ecs_new_w_id(world, DemoAddItemRequest);
}

void DemoAppRequestSelectItem(ecs_world_t *world, ecs_entity_t item)
{
    if (world == NULL || item == 0 || !ecs_has(world, item, DemoItem)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoSelectItemRequest, item);
}

void DemoAppRequestDeleteItem(ecs_world_t *world, ecs_entity_t item)
{
    if (world == NULL || item == 0 || !ecs_has(world, item, DemoItem)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoDeleteItemRequest, item);
}

void DemoAppRequestRenameItem(ecs_world_t *world, ecs_entity_t item)
{
    if (world == NULL || item == 0 || !ecs_has(world, item, DemoItem)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoRenameItemRequest, item);
}

void DemoAppRequestMoveItemUp(ecs_world_t *world, ecs_entity_t item)
{
    if (world == NULL || item == 0 || !ecs_has(world, item, DemoItem)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoMoveItemUpRequest, item);
}

void DemoAppRequestMoveItemDown(ecs_world_t *world, ecs_entity_t item)
{
    if (world == NULL || item == 0 || !ecs_has(world, item, DemoItem)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoMoveItemDownRequest, item);
}

static void DemoAppMarkItemOrderDirty(ecs_world_t *world)
{
    ecs_add_id(world, DemoAppItemRoot(world), DemoItemOrderDirty);
}

static bool DemoAppMoveItemInOrder(
    ecs_world_t *world,
    ecs_entity_t item,
    bool up,
    ecs_entity_t *out_neighbor)
{
    ecs_entity_t item_root = DemoAppItemRoot(world);
    ecs_entities_t children = ecs_get_ordered_children(world, item_root);
    if (children.count <= 1 || children.count > (int32_t)ECS_UI_TREE_NODE_MAX) {
        return false;
    }

    int32_t item_index = -1;
    for (int32_t i = 0; i < children.count; i += 1) {
        if (children.ids[i] == item) {
            item_index = i;
            break;
        }
    }

    const int32_t neighbor_index = up ? item_index - 1 : item_index + 1;
    if (item_index < 0 || neighbor_index < 0 ||
        neighbor_index >= children.count) {
        return false;
    }

    ecs_entity_t ordered[ECS_UI_TREE_NODE_MAX] = {0};
    for (int32_t i = 0; i < children.count; i += 1) {
        ordered[i] = children.ids[i];
    }

    ecs_entity_t neighbor = ordered[neighbor_index];
    ordered[neighbor_index] = ordered[item_index];
    ordered[item_index] = neighbor;
    ecs_set_child_order(world, item_root, ordered, children.count);

    if (out_neighbor != NULL) {
        *out_neighbor = neighbor;
    }
    DemoAppMarkItemOrderDirty(world);
    return true;
}

static void DemoAppAddItemSystem(ecs_iter_t *it)
{
    DemoItemSequence *sequence =
        ecs_singleton_get_mut(it->world, DemoItemSequence);
    if (sequence == NULL) {
        return;
    }

    ecs_entity_t item_root = DemoAppItemRoot(it->world);
    for (int32_t i = 0; i < it->count; i += 1) {
        const uint32_t item_id = sequence->next_item_id;
        sequence->next_item_id += 1u;

        char entity_name[ECS_UI_ID_MAX] = {0};
        (void)snprintf(entity_name, sizeof(entity_name), "Item%u", item_id);

        ecs_entity_t item = ecs_entity(it->world, {
            .parent = item_root,
            .name = entity_name,
            .sep = "",
        });

        DemoItem item_data = {
            .id = item_id,
            .rename_count = 0u,
        };
        (void)snprintf(
            item_data.label,
            sizeof(item_data.label),
            "item %u",
            item_id);
        ecs_set_ptr(it->world, item, DemoItem, &item_data);
        ecs_delete(it->world, it->entities[i]);
        DemoAppMarkItemOrderDirty(it->world);
        TraceLog(LOG_INFO, "DEMO: added %s", item_data.label);
    }
    ecs_singleton_modified(it->world, DemoItemSequence);
}

static void DemoAppRenameItemSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t item =
            ecs_get_target(it->world, it->entities[i], DemoRenameItemRequest, 0);
        DemoItem *item_data =
            item != 0 ? ecs_get_mut(it->world, item, DemoItem) : NULL;
        if (item_data != NULL) {
            item_data->rename_count += 1u;
            (void)snprintf(
                item_data->label,
                sizeof(item_data->label),
                "item %u.%u",
                item_data->id,
                item_data->rename_count);
            ecs_modified(it->world, item, DemoItem);
            TraceLog(LOG_INFO, "DEMO: renamed %s", item_data->label);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoAppMoveItemSystem(ecs_iter_t *it, ecs_entity_t request, bool up)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t item =
            ecs_get_target(it->world, it->entities[i], request, 0);
        const DemoItem *item_data =
            item != 0 ? ecs_get(it->world, item, DemoItem) : NULL;
        if (item_data != NULL) {
            ecs_entity_t neighbor = 0;
            const bool moved =
                DemoAppMoveItemInOrder(it->world, item, up, &neighbor);
            const DemoItem *neighbor_data =
                neighbor != 0 ? ecs_get(it->world, neighbor, DemoItem) : NULL;
            if (moved && neighbor_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: moved %s %s past %s",
                    item_data->label,
                    up ? "up" : "down",
                    neighbor_data->label);
            }
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoAppMoveItemUpSystem(ecs_iter_t *it)
{
    DemoAppMoveItemSystem(it, DemoMoveItemUpRequest, true);
}

static void DemoAppMoveItemDownSystem(ecs_iter_t *it)
{
    DemoAppMoveItemSystem(it, DemoMoveItemDownRequest, false);
}

static void DemoAppSelectItemSystem(ecs_iter_t *it)
{
    ecs_entity_t selection = DemoAppSelectionRoot(it->world);
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t item =
            ecs_get_target(it->world, it->entities[i], DemoSelectItemRequest, 0);
        const DemoItem *item_data =
            item != 0 ? ecs_get(it->world, item, DemoItem) : NULL;
        if (item_data != NULL) {
            ecs_add_pair(it->world, selection, DemoSelectedItem, item);
            TraceLog(LOG_INFO, "DEMO: selected %s", item_data->label);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoAppDeleteItemSystem(ecs_iter_t *it)
{
    ecs_entity_t selection = DemoAppSelectionRoot(it->world);
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t item =
            ecs_get_target(it->world, it->entities[i], DemoDeleteItemRequest, 0);
        const DemoItem *item_data =
            item != 0 ? ecs_get(it->world, item, DemoItem) : NULL;
        if (item_data != NULL) {
            if (ecs_get_target(it->world, selection, DemoSelectedItem, 0) == item) {
                ecs_remove_pair(
                    it->world,
                    selection,
                    DemoSelectedItem,
                    EcsWildcard);
            }
            TraceLog(LOG_INFO, "DEMO: deleted %s", item_data->label);
            ecs_delete(it->world, item);
            DemoAppMarkItemOrderDirty(it->world);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

void DemoAppRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoItem);
    ECS_COMPONENT_DEFINE(world, DemoItemSequence);
    ECS_TAG_DEFINE(world, DemoAddItemRequest);
    ECS_TAG_DEFINE(world, DemoSelectItemRequest);
    ECS_TAG_DEFINE(world, DemoDeleteItemRequest);
    ECS_TAG_DEFINE(world, DemoRenameItemRequest);
    ECS_TAG_DEFINE(world, DemoMoveItemUpRequest);
    ECS_TAG_DEFINE(world, DemoMoveItemDownRequest);
    ECS_TAG_DEFINE(world, DemoItemOrderDirty);
    ECS_TAG_DEFINE(world, DemoSelectedItem);
    ECS_TAG_DEFINE(world, DemoItemUiNode);
    ECS_TAG_DEFINE(world, DemoItemSelectUiNode);
    ECS_TAG_DEFINE(world, DemoItemLabelUiNode);
    ECS_TAG_DEFINE(world, DemoItemMetaUiNode);
    ECS_TAG_DEFINE(world, DemoUiForItem);
    ecs_add_id(world, DemoSelectItemRequest, EcsExclusive);
    ecs_add_id(world, DemoDeleteItemRequest, EcsExclusive);
    ecs_add_id(world, DemoRenameItemRequest, EcsExclusive);
    ecs_add_id(world, DemoMoveItemUpRequest, EcsExclusive);
    ecs_add_id(world, DemoMoveItemDownRequest, EcsExclusive);
    ecs_add_id(world, DemoSelectedItem, EcsExclusive);
    ecs_add_id(world, DemoItemUiNode, EcsExclusive);
    ecs_add_id(world, DemoItemSelectUiNode, EcsExclusive);
    ecs_add_id(world, DemoItemLabelUiNode, EcsExclusive);
    ecs_add_id(world, DemoItemMetaUiNode, EcsExclusive);
    ecs_add_id(world, DemoUiForItem, EcsExclusive);

    ecs_singleton_set(
        world,
        DemoItemSequence,
        {
            .next_item_id = 1u,
        });

    ecs_entity_t add_item_system = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAppAddItemSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoAddItemRequest},
        },
        .callback = DemoAppAddItemSystem,
    });

    ecs_entity_t select_item_system = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAppSelectItemSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoSelectItemRequest, EcsWildcard)},
        },
        .callback = DemoAppSelectItemSystem,
    });

    ecs_entity_t delete_item_system = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAppDeleteItemSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoDeleteItemRequest, EcsWildcard)},
        },
        .callback = DemoAppDeleteItemSystem,
    });

    ecs_entity_t rename_item_system = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAppRenameItemSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoRenameItemRequest, EcsWildcard)},
        },
        .callback = DemoAppRenameItemSystem,
    });

    ecs_entity_t move_item_up_system = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAppMoveItemUpSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoMoveItemUpRequest, EcsWildcard)},
        },
        .callback = DemoAppMoveItemUpSystem,
    });

    ecs_entity_t move_item_down_system = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAppMoveItemDownSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoMoveItemDownRequest, EcsWildcard)},
        },
        .callback = DemoAppMoveItemDownSystem,
    });

    TraceLog(
        LOG_INFO,
        "DEMO: registered systems add=%llu select=%llu delete=%llu rename=%llu up=%llu down=%llu",
        (unsigned long long)add_item_system,
        (unsigned long long)select_item_system,
        (unsigned long long)delete_item_system,
        (unsigned long long)rename_item_system,
        (unsigned long long)move_item_up_system,
        (unsigned long long)move_item_down_system);
}
