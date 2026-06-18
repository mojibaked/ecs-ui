#include "demo_ui.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(DemoUiRefs);

static ecs_entity_t DemoUiFindNodeById(ecs_world_t *world, const char *id)
{
    if (world == NULL || id == NULL) {
        return 0;
    }

    ecs_iter_t it = ecs_each(world, EcsUiNodeId);
    while (ecs_each_next(&it)) {
        const EcsUiNodeId *ids = ecs_field(&it, EcsUiNodeId, 0);
        for (int32_t i = 0; i < it.count; i += 1) {
            if (strcmp(ids[i].value, id) == 0) {
                return it.entities[i];
            }
        }
    }
    return 0;
}

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

static void DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data,
    ecs_entity_t item_list,
    ecs_entity_t select_item_action,
    ecs_entity_t delete_item_action)
{
    if (world == NULL || item == 0 || item_data == NULL || item_list == 0 ||
        select_item_action == 0 || delete_item_action == 0) {
        return;
    }

    char row_id[ECS_UI_ID_MAX] = {0};
    char select_id[ECS_UI_ID_MAX] = {0};
    char delete_id[ECS_UI_ID_MAX] = {0};
    char label_id[ECS_UI_ID_MAX] = {0};
    char meta_id[ECS_UI_ID_MAX] = {0};
    char meta_text[ECS_UI_TEXT_MAX] = {0};
    (void)snprintf(row_id, sizeof(row_id), "ItemRow%u", item_data->id);
    (void)snprintf(select_id, sizeof(select_id), "ItemSelect%u", item_data->id);
    (void)snprintf(delete_id, sizeof(delete_id), "ItemDelete%u", item_data->id);
    (void)snprintf(label_id, sizeof(label_id), "ItemLabel%u", item_data->id);
    (void)snprintf(meta_id, sizeof(meta_id), "ItemMeta%u", item_data->id);
    (void)snprintf(meta_text, sizeof(meta_text), "#%u", item_data->order);

    EcsUiBuilder builder = EcsUiBuilderBegin(world, item_list);
    ecs_entity_t row = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = row_id,
            .gap = 8.0f,
            .padding = 6.0f,
        });
    ecs_entity_t select_button = EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = select_id,
            .variant = ECS_UI_BUTTON_SUBTLE,
            .on_click = select_item_action,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = label_id,
            .text = item_data->label,
            .role = ECS_UI_TEXT_LABEL,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = meta_id,
            .text = meta_text,
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiEnd(&builder);
    ecs_entity_t delete_button = EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = delete_id,
            .variant = ECS_UI_BUTTON_DANGER,
            .on_click = delete_item_action,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DeleteLabel",
            .text = "delete",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);

    if (EcsUiBuilderOk(&builder) && row != 0) {
        ecs_add_pair(world, item, DemoItemUiNode, row);
        if (select_button != 0) {
            ecs_add_pair(world, select_button, DemoUiForItem, item);
        }
        if (delete_button != 0) {
            ecs_add_pair(world, delete_button, DemoUiForItem, item);
        }
    }
}

static void DemoUiMaterializeItemRowObserver(ecs_iter_t *it)
{
    const DemoUiRefs *refs = ecs_singleton_get(it->world, DemoUiRefs);
    if (refs == NULL || refs->item_list == 0 ||
        refs->select_item_action == 0 || refs->delete_item_action == 0) {
        return;
    }

    const DemoItem *items = ecs_field(it, DemoItem, 0);
    for (int32_t i = 0; i < it->count; i += 1) {
        if (ecs_get_target(it->world, it->entities[i], DemoItemUiNode, 0) != 0) {
            continue;
        }
        DemoUiCreateItemRow(
            it->world,
            it->entities[i],
            &items[i],
            refs->item_list,
            refs->select_item_action,
            refs->delete_item_action);
    }
    DemoUiRefreshStatus(it->world);
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
}

void DemoUiRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoUiRefs);

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
}

ecs_entity_t DemoUiBuild(ecs_world_t *world)
{
    ecs_entity_t root = EcsUiRootEntity(world, "RaylibDemo");
    ecs_entity_t add_item_action = ecs_entity(world, {.name = "AddItemAction"});
    ecs_entity_t select_item_action =
        ecs_entity(world, {.name = "SelectItemAction"});
    ecs_entity_t delete_item_action =
        ecs_entity(world, {.name = "DeleteItemAction"});
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);

    VStack(&builder, {.id = "DemoStack", .gap = 12.0f, .padding = 24.0f}) {
        Text(
            &builder,
            {
                .id = "Title",
                .text = "ecs-ui",
                .role = ECS_UI_TEXT_TITLE,
            });
        Text(
            &builder,
            {
                .id = "Subtitle",
                .text = "Flecs-authored UI rendered with raylib",
                .role = ECS_UI_TEXT_CAPTION,
            });
        HStack(&builder, {.id = "Actions", .gap = 10.0f, .padding = 0.0f}) {
            Button(
                &builder,
                {
                    .id = "AddItem",
                    .variant = ECS_UI_BUTTON_PRIMARY,
                    .on_click = add_item_action,
                }) {
                Icon(&builder, {.id = "AddItemIcon", .name = "+"});
                Text(
                    &builder,
                    {
                        .id = "AddItemLabel",
                        .text = "add item",
                        .role = ECS_UI_TEXT_BUTTON,
                    });
            }
        }
        VStack(&builder, {.id = "Inspector", .gap = 6.0f, .padding = 14.0f}) {
            Text(
                &builder,
                {
                    .id = "InspectorTitle",
                    .text = "ordered children",
                    .role = ECS_UI_TEXT_LABEL,
                });
            Text(
                &builder,
                {
                    .id = "InspectorBody",
                    .text = "Click add item, then select or delete a row.",
                    .role = ECS_UI_TEXT_CAPTION,
                });
        }
        VStack(&builder, {.id = "ItemList", .gap = 6.0f, .padding = 14.0f}) {
            Text(
                &builder,
                {
                    .id = "ItemListTitle",
                    .text = "items",
                    .role = ECS_UI_TEXT_LABEL,
                });
        }
        Text(
            &builder,
            {
                .id = "EventStatus",
                .text = "0 items",
                .role = ECS_UI_TEXT_CAPTION,
            });
    }

    EcsUiBuilderEnd(&builder);
    if (!EcsUiBuilderOk(&builder)) {
        return 0;
    }

    ecs_singleton_set(
        world,
        DemoUiRefs,
        {
            .add_item_action = add_item_action,
            .select_item_action = select_item_action,
            .delete_item_action = delete_item_action,
            .item_list = DemoUiFindNodeById(world, "ItemList"),
            .status_text = DemoUiFindNodeById(world, "EventStatus"),
        });
    return root;
}

void DemoUiSetStatus(ecs_world_t *world, const char *message)
{
    const DemoUiRefs *refs = ecs_singleton_get(world, DemoUiRefs);
    ecs_entity_t status = refs != NULL && refs->status_text != 0 ?
        refs->status_text :
        DemoUiFindNodeById(world, "EventStatus");
    EcsUiText *text =
        status != 0 ? ecs_get_mut(world, status, EcsUiText) : NULL;
    if (text == NULL) {
        return;
    }

    (void)snprintf(text->text, sizeof(text->text), "%s", message);
    text->role = ECS_UI_TEXT_CAPTION;
    ecs_modified(world, status, EcsUiText);
}

void DemoUiApplyEvents(ecs_world_t *world, const EcsUiEventList *events)
{
    if (world == NULL || events == NULL) {
        return;
    }

    const DemoUiRefs *refs = ecs_singleton_get(world, DemoUiRefs);
    for (uint32_t i = 0u; i < events->count; i += 1u) {
        const EcsUiEvent *event = &events->events[i];
        if (event->type != ECS_UI_EVENT_CLICKED) {
            continue;
        }

        if (refs == NULL) {
            continue;
        }

        if (event->action == refs->add_item_action) {
            TraceLog(LOG_INFO, "DEMO: add item requested from %s", event->node_id);
            DemoAppRequestAddItem(world);
            continue;
        }

        if (event->action == refs->select_item_action) {
            ecs_entity_t item =
                ecs_get_target(world, event->node, DemoUiForItem, 0);
            const DemoItem *item_data =
                item != 0 ? ecs_get(world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: select item requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestSelectItem(world, item);
            }
            continue;
        }

        if (event->action == refs->delete_item_action) {
            ecs_entity_t item =
                ecs_get_target(world, event->node, DemoUiForItem, 0);
            const DemoItem *item_data =
                item != 0 ? ecs_get(world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: delete item requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestDeleteItem(world, item);
            }
        }
    }
}
