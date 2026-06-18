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

static void DemoUiApplyItemSelectionStyle(
    ecs_world_t *world,
    ecs_entity_t item,
    ecs_entity_t selected_item)
{
    ecs_entity_t select_button =
        ecs_get_target(world, item, DemoItemSelectUiNode, 0);
    EcsUiButton *button =
        select_button != 0 ? ecs_get_mut(world, select_button, EcsUiButton) : NULL;
    if (button == NULL) {
        return;
    }

    const EcsUiButtonVariant variant =
        item == selected_item ? ECS_UI_BUTTON_PRIMARY : ECS_UI_BUTTON_SUBTLE;
    if (button->variant == variant) {
        return;
    }

    button->variant = variant;
    ecs_modified(world, select_button, EcsUiButton);
}

static void DemoUiRefreshSelectionStyles(ecs_world_t *world)
{
    ecs_entity_t selected_item = ecs_get_target(
        world,
        DemoAppSelectionRoot(world),
        DemoSelectedItem,
        0);

    ecs_iter_t it = ecs_each(world, DemoItem);
    while (ecs_each_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            DemoUiApplyItemSelectionStyle(
                world,
                it.entities[i],
                selected_item);
        }
    }
}

static void DemoUiUpdateTextNode(
    ecs_world_t *world,
    ecs_entity_t node,
    const char *value,
    EcsUiTextRole role)
{
    EcsUiText *text = node != 0 ? ecs_get_mut(world, node, EcsUiText) : NULL;
    if (text == NULL) {
        return;
    }

    (void)snprintf(text->text, sizeof(text->text), "%s", value != NULL ? value : "");
    text->role = role;
    ecs_modified(world, node, EcsUiText);
}

static void DemoUiUpdateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data)
{
    if (world == NULL || item == 0 || item_data == NULL) {
        return;
    }

    ecs_entity_t label = ecs_get_target(world, item, DemoItemLabelUiNode, 0);
    DemoUiUpdateTextNode(world, label, item_data->label, ECS_UI_TEXT_LABEL);

    char meta_text[ECS_UI_TEXT_MAX] = {0};
    (void)snprintf(meta_text, sizeof(meta_text), "#%u", item_data->order);
    ecs_entity_t meta = ecs_get_target(world, item, DemoItemMetaUiNode, 0);
    DemoUiUpdateTextNode(world, meta, meta_text, ECS_UI_TEXT_CAPTION);
}

typedef struct DemoUiOrderedRow {
    ecs_entity_t row;
    uint32_t order;
    uint32_t id;
} DemoUiOrderedRow;

static bool DemoUiOrderedRowsContain(
    const DemoUiOrderedRow *rows,
    int32_t row_count,
    ecs_entity_t row)
{
    for (int32_t i = 0; i < row_count; i += 1) {
        if (rows[i].row == row) {
            return true;
        }
    }
    return false;
}

static void DemoUiRefreshItemOrder(
    ecs_world_t *world,
    ecs_entity_t item_list,
    const DemoUiOrderedRow *extra_rows,
    int32_t extra_row_count)
{
    if (world == NULL || item_list == 0) {
        return;
    }

    DemoUiOrderedRow rows[ECS_UI_TREE_NODE_MAX] = {0};
    const int32_t row_cap = (int32_t)ECS_UI_TREE_NODE_MAX;
    int32_t row_count = 0;
    ecs_iter_t item_it = ecs_each(world, DemoItem);
    while (ecs_each_next(&item_it)) {
        const DemoItem *items = ecs_field(&item_it, DemoItem, 0);
        for (int32_t i = 0; i < item_it.count; i += 1) {
            ecs_entity_t row =
                ecs_get_target(world, item_it.entities[i], DemoItemUiNode, 0);
            if (row == 0 || row_count >= row_cap) {
                continue;
            }
            rows[row_count] = (DemoUiOrderedRow){
                .row = row,
                .order = items[i].order,
                .id = items[i].id,
            };
            row_count += 1;
        }
    }

    for (int32_t i = 0; i < extra_row_count; i += 1) {
        if (extra_rows[i].row == 0 || row_count >= row_cap ||
            DemoUiOrderedRowsContain(rows, row_count, extra_rows[i].row)) {
            continue;
        }
        rows[row_count] = extra_rows[i];
        row_count += 1;
    }

    for (int32_t i = 1; i < row_count; i += 1) {
        DemoUiOrderedRow row = rows[i];
        int32_t j = i - 1;
        while (j >= 0 &&
               (rows[j].order > row.order ||
                (rows[j].order == row.order && rows[j].id > row.id))) {
            rows[j + 1] = rows[j];
            j -= 1;
        }
        rows[j + 1] = row;
    }

    ecs_entity_t ordered[ECS_UI_TREE_NODE_MAX] = {0};
    const int32_t ordered_cap = (int32_t)ECS_UI_TREE_NODE_MAX;
    int32_t ordered_count = 0;
    ecs_entities_t children = ecs_get_ordered_children(world, item_list);
    for (int32_t i = 0; i < children.count; i += 1) {
        bool is_item_row = false;
        for (int32_t row_index = 0; row_index < row_count; row_index += 1) {
            if (children.ids[i] == rows[row_index].row) {
                is_item_row = true;
                break;
            }
        }
        if (is_item_row) {
            continue;
        }
        if (ordered_count >= ordered_cap) {
            return;
        }
        ordered[ordered_count] = children.ids[i];
        ordered_count += 1;
    }

    for (int32_t i = 0; i < row_count; i += 1) {
        if (ordered_count >= ordered_cap) {
            return;
        }
        ordered[ordered_count] = rows[i].row;
        ordered_count += 1;
    }

    ecs_set_child_order(world, item_list, ordered, ordered_count);
}

static ecs_entity_t DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data,
    ecs_entity_t item_list,
    ecs_entity_t select_item_action,
    ecs_entity_t delete_item_action,
    ecs_entity_t rename_item_action,
    ecs_entity_t move_item_up_action,
    ecs_entity_t move_item_down_action)
{
    if (world == NULL || item == 0 || item_data == NULL || item_list == 0 ||
        select_item_action == 0 || delete_item_action == 0 ||
        rename_item_action == 0 || move_item_up_action == 0 ||
        move_item_down_action == 0) {
        return 0;
    }

    char row_id[ECS_UI_ID_MAX] = {0};
    char select_id[ECS_UI_ID_MAX] = {0};
    char rename_id[ECS_UI_ID_MAX] = {0};
    char up_id[ECS_UI_ID_MAX] = {0};
    char down_id[ECS_UI_ID_MAX] = {0};
    char delete_id[ECS_UI_ID_MAX] = {0};
    char label_id[ECS_UI_ID_MAX] = {0};
    char meta_id[ECS_UI_ID_MAX] = {0};
    char meta_text[ECS_UI_TEXT_MAX] = {0};
    (void)snprintf(row_id, sizeof(row_id), "ItemRow%u", item_data->id);
    (void)snprintf(select_id, sizeof(select_id), "ItemSelect%u", item_data->id);
    (void)snprintf(rename_id, sizeof(rename_id), "ItemRename%u", item_data->id);
    (void)snprintf(up_id, sizeof(up_id), "ItemUp%u", item_data->id);
    (void)snprintf(down_id, sizeof(down_id), "ItemDown%u", item_data->id);
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
    ecs_entity_t label_text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = label_id,
            .text = item_data->label,
            .role = ECS_UI_TEXT_LABEL,
        });
    ecs_entity_t meta_text_node = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = meta_id,
            .text = meta_text,
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiEnd(&builder);
    ecs_entity_t rename_button = EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = rename_id,
            .variant = ECS_UI_BUTTON_DEFAULT,
            .on_click = rename_item_action,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "RenameLabel",
            .text = "rename",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    ecs_entity_t up_button = EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = up_id,
            .variant = ECS_UI_BUTTON_DEFAULT,
            .on_click = move_item_up_action,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "UpLabel",
            .text = "up",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    ecs_entity_t down_button = EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = down_id,
            .variant = ECS_UI_BUTTON_DEFAULT,
            .on_click = move_item_down_action,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DownLabel",
            .text = "down",
            .role = ECS_UI_TEXT_BUTTON,
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
        ecs_add_pair(world, row, DemoUiForItem, item);
        if (label_text != 0) {
            ecs_add_pair(world, item, DemoItemLabelUiNode, label_text);
        }
        if (meta_text_node != 0) {
            ecs_add_pair(world, item, DemoItemMetaUiNode, meta_text_node);
        }
        if (select_button != 0) {
            ecs_add_pair(world, item, DemoItemSelectUiNode, select_button);
            ecs_add_pair(world, select_button, DemoUiForItem, item);
        }
        if (rename_button != 0) {
            ecs_add_pair(world, rename_button, DemoUiForItem, item);
        }
        if (up_button != 0) {
            ecs_add_pair(world, up_button, DemoUiForItem, item);
        }
        if (down_button != 0) {
            ecs_add_pair(world, down_button, DemoUiForItem, item);
        }
        if (delete_button != 0) {
            ecs_add_pair(world, delete_button, DemoUiForItem, item);
        }
    }
    return row;
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

    DemoUiOrderedRow new_rows[ECS_UI_TREE_NODE_MAX] = {0};
    int32_t new_row_count = 0;
    const int32_t new_row_cap = (int32_t)ECS_UI_TREE_NODE_MAX;
    const DemoItem *items = ecs_field(it, DemoItem, 0);
    for (int32_t i = 0; i < it->count; i += 1) {
        if (ecs_get_target(it->world, it->entities[i], DemoItemUiNode, 0) != 0) {
            DemoUiUpdateItemRow(it->world, it->entities[i], &items[i]);
            continue;
        }
        ecs_entity_t row = DemoUiCreateItemRow(
            it->world,
            it->entities[i],
            &items[i],
            refs->item_list,
            refs->select_item_action,
            refs->delete_item_action,
            refs->rename_item_action,
            refs->move_item_up_action,
            refs->move_item_down_action);
        if (row != 0 && new_row_count < new_row_cap) {
            new_rows[new_row_count] = (DemoUiOrderedRow){
                .row = row,
                .order = items[i].order,
                .id = items[i].id,
            };
            new_row_count += 1;
        }
    }
    DemoUiRefreshItemOrder(
        it->world,
        refs->item_list,
        new_rows,
        new_row_count);
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
    ecs_entity_t rename_item_action =
        ecs_entity(world, {.name = "RenameItemAction"});
    ecs_entity_t move_item_up_action =
        ecs_entity(world, {.name = "MoveItemUpAction"});
    ecs_entity_t move_item_down_action =
        ecs_entity(world, {.name = "MoveItemDownAction"});
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
                    .text = "Click add item, then select, rename, reorder, or delete a row.",
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
            .rename_item_action = rename_item_action,
            .move_item_up_action = move_item_up_action,
            .move_item_down_action = move_item_down_action,
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

        if (event->action == refs->rename_item_action) {
            ecs_entity_t item =
                ecs_get_target(world, event->node, DemoUiForItem, 0);
            const DemoItem *item_data =
                item != 0 ? ecs_get(world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: rename item requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestRenameItem(world, item);
            }
            continue;
        }

        if (event->action == refs->move_item_up_action) {
            ecs_entity_t item =
                ecs_get_target(world, event->node, DemoUiForItem, 0);
            const DemoItem *item_data =
                item != 0 ? ecs_get(world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: move up requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestMoveItemUp(world, item);
            }
            continue;
        }

        if (event->action == refs->move_item_down_action) {
            ecs_entity_t item =
                ecs_get_target(world, event->node, DemoUiForItem, 0);
            const DemoItem *item_data =
                item != 0 ? ecs_get(world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: move down requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestMoveItemDown(world, item);
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
