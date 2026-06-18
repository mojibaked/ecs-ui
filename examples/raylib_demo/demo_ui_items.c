#include "demo_ui_internal.h"

#include <stdio.h>

static uint32_t DemoUiCountItems(ecs_world_t *world)
{
    uint32_t count = 0u;
    ecs_iter_t it = ecs_each(world, DemoItem);
    while (ecs_each_next(&it)) {
        count += (uint32_t)it.count;
    }
    return count;
}

static void DemoUiSetStatusForItemCount(ecs_world_t *world, uint32_t count)
{
    ecs_entity_t selected = ecs_get_target(
        world,
        DemoAppSelectionRoot(world),
        DemoSelectedItem,
        0);
    const DemoItem *selected_item =
        selected != 0 ? ecs_get(world, selected, DemoItem) : NULL;

    char status[ECS_UI_TEXT_MAX] = {0};
    if (selected_item != NULL) {
        (void)snprintf(
            status,
            sizeof(status),
            "%u items | selected item %u",
            count,
            selected_item->id);
    } else {
        (void)snprintf(status, sizeof(status), "%u items", count);
    }
    DemoUiSetStatus(world, status);
}

static void DemoUiRefreshStatus(ecs_world_t *world)
{
    DemoUiSetStatusForItemCount(world, DemoUiCountItems(world));
}

static void DemoUiMaterializeItemRowObserver(ecs_iter_t *it)
{
    const DemoUiRefs *refs = ecs_singleton_get(it->world, DemoUiRefs);
    if (refs == NULL || refs->item_list == 0 ||
        refs->select_item_action == 0 || refs->delete_item_action == 0 ||
        refs->rename_item_action == 0 || refs->move_item_up_action == 0 ||
        refs->move_item_down_action == 0) {
        return;
    }

    const DemoItem *items = ecs_field(it, DemoItem, 0);
    for (int32_t i = 0; i < it->count; i += 1) {
        if (ecs_get_target(it->world, it->entities[i], DemoItemUiNode, 0) != 0) {
            DemoUiUpdateItemRow(it->world, it->entities[i], &items[i]);
            continue;
        }
        (void)DemoUiCreateItemRow(
            it->world,
            it->entities[i],
            &items[i],
            refs);
    }
    DemoUiRefreshStatus(it->world);
    DemoUiRefreshSelectionStyles(it->world);
}

static void DemoUiRemoveItemRowObserver(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t row =
            ecs_get_target(it->world, it->entities[i], DemoItemUiNode, 0);
        if (row != 0) {
            ecs_delete(it->world, row);
        }
    }

    uint32_t count = DemoUiCountItems(it->world);
    count = count >= (uint32_t)it->count ? count - (uint32_t)it->count : 0u;
    DemoUiSetStatusForItemCount(it->world, count);
}

static void DemoUiSelectionStatusObserver(ecs_iter_t *it)
{
    DemoUiRefreshStatus(it->world);
    DemoUiRefreshSelectionStyles(it->world);
}

static void DemoUiApplyItemOrderSystem(ecs_iter_t *it)
{
    const DemoUiRefs *refs = ecs_singleton_get(it->world, DemoUiRefs);
    if (refs == NULL || refs->item_list == 0) {
        return;
    }

    for (int32_t i = 0; i < it->count; i += 1) {
        DemoUiApplyItemOrderToList(it->world, refs->item_list);
        ecs_remove_id(it->world, it->entities[i], DemoItemOrderDirty);
    }
}

void DemoUiRegisterItemProjection(ecs_world_t *world)
{
    (void)ecs_observer(world, {
        .entity = ecs_entity(world, {
            .name = "DemoUiMaterializeItemRowObserver",
        }),
        .query.terms = {
            {.id = ecs_id(DemoItem)},
        },
        .events = {EcsOnSet},
        .callback = DemoUiMaterializeItemRowObserver,
    });

    (void)ecs_observer(world, {
        .entity = ecs_entity(world, {
            .name = "DemoUiRemoveItemRowObserver",
        }),
        .query.terms = {
            {.id = ecs_id(DemoItem)},
        },
        .events = {EcsOnRemove},
        .callback = DemoUiRemoveItemRowObserver,
    });

    (void)ecs_observer(world, {
        .entity = ecs_entity(world, {
            .name = "DemoUiSelectionStatusObserver",
        }),
        .query.terms = {
            {.id = ecs_pair(DemoSelectedItem, EcsWildcard)},
        },
        .events = {EcsOnAdd, EcsOnRemove},
        .callback = DemoUiSelectionStatusObserver,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoUiApplyItemOrderSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoItemOrderDirty},
        },
        .callback = DemoUiApplyItemOrderSystem,
    });
}
