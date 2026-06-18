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

static void DemoUiSetItemCountStatus(ecs_world_t *world, uint32_t count)
{
    char status[ECS_UI_TEXT_MAX] = {0};
    (void)snprintf(status, sizeof(status), "%u items", count);
    DemoUiSetStatus(world, status);
}

static void DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data,
    ecs_entity_t item_list)
{
    if (world == NULL || item == 0 || item_data == NULL || item_list == 0) {
        return;
    }

    char row_id[ECS_UI_ID_MAX] = {0};
    char label_id[ECS_UI_ID_MAX] = {0};
    char meta_id[ECS_UI_ID_MAX] = {0};
    char meta_text[ECS_UI_TEXT_MAX] = {0};
    (void)snprintf(row_id, sizeof(row_id), "ItemRow%u", item_data->id);
    (void)snprintf(label_id, sizeof(label_id), "ItemLabel%u", item_data->id);
    (void)snprintf(meta_id, sizeof(meta_id), "ItemMeta%u", item_data->id);
    (void)snprintf(meta_text, sizeof(meta_text), "#%u", item_data->order);

    EcsUiBuilder builder = EcsUiBuilderBegin(world, item_list);
    ecs_entity_t row = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = row_id,
            .gap = 10.0f,
            .padding = 10.0f,
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
    EcsUiBuilderEnd(&builder);

    if (EcsUiBuilderOk(&builder) && row != 0) {
        ecs_add_pair(world, item, DemoItemUiNode, row);
        ecs_add_pair(world, row, DemoItemUiFor, item);
    }
}

static void DemoUiMaterializeItemRowObserver(ecs_iter_t *it)
{
    const DemoUiRefs *refs = ecs_singleton_get(it->world, DemoUiRefs);
    if (refs == NULL || refs->item_list == 0) {
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
            refs->item_list);
    }
    DemoUiSetItemCountStatus(it->world, DemoUiCountItems(it->world));
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
    DemoUiSetItemCountStatus(it->world, count);
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
}

ecs_entity_t DemoUiBuild(ecs_world_t *world)
{
    ecs_entity_t root = EcsUiRootEntity(world, "RaylibDemo");
    ecs_entity_t add_item_action = ecs_entity(world, {.name = "AddItemAction"});
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
                    .text = "Click add item. App state and UI rows are Flecs entities.",
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

        if (refs != NULL &&
            (event->action == refs->add_item_action ||
             strcmp(event->node_id, "AddItem") == 0)) {
            TraceLog(LOG_INFO, "DEMO: add item requested from %s", event->node_id);
            DemoAppRequestAddItem(world);
        }
    }
}
