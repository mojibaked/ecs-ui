#include "demo_app.h"

#include <raylib.h>
#include <stdio.h>

ECS_COMPONENT_DECLARE(DemoItem);
ECS_COMPONENT_DECLARE(DemoItemSequence);
ECS_TAG_DECLARE(DemoAddItemRequest);
ECS_TAG_DECLARE(DemoSelectItemRequest);
ECS_TAG_DECLARE(DemoDeleteItemRequest);
ECS_TAG_DECLARE(DemoSelectedItem);
ECS_TAG_DECLARE(DemoItemUiNode);
ECS_TAG_DECLARE(DemoItemSelectUiNode);
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
            .order = item_id,
        };
        (void)snprintf(
            item_data.label,
            sizeof(item_data.label),
            "item %u",
            item_id);
        ecs_set_ptr(it->world, item, DemoItem, &item_data);
        ecs_delete(it->world, it->entities[i]);
        TraceLog(LOG_INFO, "DEMO: added %s", item_data.label);
    }
    ecs_singleton_modified(it->world, DemoItemSequence);
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
    ECS_TAG_DEFINE(world, DemoSelectedItem);
    ECS_TAG_DEFINE(world, DemoItemUiNode);
    ECS_TAG_DEFINE(world, DemoItemSelectUiNode);
    ECS_TAG_DEFINE(world, DemoUiForItem);
    ecs_add_id(world, DemoSelectItemRequest, EcsExclusive);
    ecs_add_id(world, DemoDeleteItemRequest, EcsExclusive);
    ecs_add_id(world, DemoSelectedItem, EcsExclusive);
    ecs_add_id(world, DemoItemUiNode, EcsExclusive);
    ecs_add_id(world, DemoItemSelectUiNode, EcsExclusive);
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

    TraceLog(
        LOG_INFO,
        "DEMO: registered systems add=%llu select=%llu delete=%llu",
        (unsigned long long)add_item_system,
        (unsigned long long)select_item_system,
        (unsigned long long)delete_item_system);
}
