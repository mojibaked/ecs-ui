#include "demo_ui_internal.h"

#include "demo_anim.h"

#include <stdio.h>

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
    if (item == selected_item) {
        DemoAnimStartSelectionHighlight(world, select_button);
    }
}

void DemoUiRefreshSelectionStyles(ecs_world_t *world)
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

static uint32_t DemoUiItemPosition(ecs_world_t *world, ecs_entity_t item)
{
    if (world == NULL || item == 0) {
        return 0u;
    }

    uint32_t position = 1u;
    ecs_entities_t children =
        ecs_get_ordered_children(world, DemoAppItemRoot(world));
    for (int32_t i = 0; i < children.count; i += 1) {
        if (!ecs_has(world, children.ids[i], DemoItem)) {
            continue;
        }
        if (children.ids[i] == item) {
            return position;
        }
        position += 1u;
    }

    return 0u;
}

static void DemoUiUpdateItemRowWithPosition(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data,
    uint32_t position)
{
    if (world == NULL || item == 0 || item_data == NULL) {
        return;
    }

    ecs_entity_t label = ecs_get_target(world, item, DemoItemLabelUiNode, 0);
    DemoUiUpdateTextNode(world, label, item_data->label, ECS_UI_TEXT_LABEL);

    char meta_text[ECS_UI_TEXT_MAX] = {0};
    if (position != 0u) {
        (void)snprintf(meta_text, sizeof(meta_text), "#%u", position);
    } else {
        (void)snprintf(meta_text, sizeof(meta_text), "#?");
    }
    ecs_entity_t meta = ecs_get_target(world, item, DemoItemMetaUiNode, 0);
    DemoUiUpdateTextNode(world, meta, meta_text, ECS_UI_TEXT_CAPTION);
}

void DemoUiUpdateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data)
{
    DemoUiUpdateItemRowWithPosition(
        world,
        item,
        item_data,
        DemoUiItemPosition(world, item));
}

void DemoUiApplyItemOrderToList(ecs_world_t *world, ecs_entity_t item_list)
{
    if (world == NULL || item_list == 0) {
        return;
    }

    ecs_entity_t ordered[ECS_UI_TREE_NODE_MAX] = {0};
    const int32_t ordered_cap = (int32_t)ECS_UI_TREE_NODE_MAX;
    int32_t ordered_count = 0;

    ecs_entities_t ui_children = ecs_get_ordered_children(world, item_list);
    for (int32_t i = 0; i < ui_children.count; i += 1) {
        if (ecs_get_target(world, ui_children.ids[i], DemoUiForItem, 0) != 0) {
            continue;
        }
        if (ordered_count >= ordered_cap) {
            return;
        }
        ordered[ordered_count] = ui_children.ids[i];
        ordered_count += 1;
    }

    uint32_t position = 1u;
    ecs_entities_t items =
        ecs_get_ordered_children(world, DemoAppItemRoot(world));
    for (int32_t i = 0; i < items.count; i += 1) {
        const DemoItem *item_data = ecs_get(world, items.ids[i], DemoItem);
        if (item_data == NULL) {
            continue;
        }

        ecs_entity_t row = ecs_get_target(world, items.ids[i], DemoItemUiNode, 0);
        if (row != 0) {
            if (ordered_count >= ordered_cap) {
                return;
            }
            ordered[ordered_count] = row;
            ordered_count += 1;
            DemoUiUpdateItemRowWithPosition(
                world,
                items.ids[i],
                item_data,
                position);
        }
        position += 1u;
    }

    ecs_set_child_order(world, item_list, ordered, ordered_count);
}

ecs_entity_t DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data,
    const DemoUiRefs *refs)
{
    if (world == NULL || item == 0 || item_data == NULL || refs == NULL ||
        refs->item_list == 0 || refs->select_item_action == 0 ||
        refs->delete_item_action == 0 || refs->rename_item_action == 0 ||
        refs->move_item_up_action == 0 || refs->move_item_down_action == 0) {
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
    (void)snprintf(
        meta_text,
        sizeof(meta_text),
        "#%u",
        DemoUiItemPosition(world, item));

    EcsUiBuilder builder = EcsUiBuilderBegin(world, refs->item_list);
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
            .on_click = refs->select_item_action,
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
            .on_click = refs->rename_item_action,
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
            .on_click = refs->move_item_up_action,
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
            .on_click = refs->move_item_down_action,
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
            .on_click = refs->delete_item_action,
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
        DemoAnimStartRowInsert(world, row);
    }
    return row;
}
