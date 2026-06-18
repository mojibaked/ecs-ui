#include "demo_app.h"

#include <raylib.h>
#include <stdio.h>

ECS_COMPONENT_DECLARE(DemoItem);
ECS_COMPONENT_DECLARE(DemoItemSequence);
ECS_TAG_DECLARE(DemoAddItemRequest);
ECS_TAG_DECLARE(DemoItemUiNode);
ECS_TAG_DECLARE(DemoItemUiFor);

ecs_entity_t DemoAppItemRoot(ecs_world_t *world)
{
    ecs_entity_t root = ecs_entity(world, {.name = "DemoItems"});
    ecs_add_id(world, root, EcsOrderedChildren);
    return root;
}

void DemoAppRequestAddItem(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    (void)ecs_new_w_id(world, DemoAddItemRequest);
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

void DemoAppRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoItem);
    ECS_COMPONENT_DEFINE(world, DemoItemSequence);
    ECS_TAG_DEFINE(world, DemoAddItemRequest);
    ECS_TAG_DEFINE(world, DemoItemUiNode);
    ECS_TAG_DEFINE(world, DemoItemUiFor);
    ecs_add_id(world, DemoItemUiNode, EcsExclusive);
    ecs_add_id(world, DemoItemUiFor, EcsExclusive);

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

    TraceLog(
        LOG_INFO,
        "DEMO: registered systems add=%llu",
        (unsigned long long)add_item_system);
}
