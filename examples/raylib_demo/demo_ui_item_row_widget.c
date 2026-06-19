#include "demo_ui_internal.h"

#include "demo_anim.h"
#include "demo_theme.h"

#include <stdio.h>
#include <string.h>

void DemoUiApplyItemSelectionStyle(
    ecs_world_t *world,
    ecs_entity_t source,
    uint32_t selected_item_id)
{
    ecs_entity_t select_button =
        EcsUiProjectionGetNode(world, source, DemoItemSelectUiNode);
    EcsUiButton *button =
        select_button != 0 ? ecs_get_mut(world, select_button, EcsUiButton) : NULL;
    if (button == NULL) {
        return;
    }

    const DemoUiItemSource *item_data =
        source != 0 ? ecs_get(world, source, DemoUiItemSource) : NULL;
    if (item_data == NULL) {
        return;
    }

    const EcsUiButtonVariant variant =
        item_data->id == selected_item_id ?
            ECS_UI_BUTTON_PRIMARY :
            ECS_UI_BUTTON_SUBTLE;
    const ecs_entity_t style_token =
        item_data->id == selected_item_id ?
            DemoThemePrimaryActionStyleToken(world) :
            DemoThemeSubtleActionStyleToken(world);
    const ecs_entity_t current_style =
        ecs_get_target(world, select_button, EcsUiUsesStyle, 0);
    if (button->variant == variant && current_style == style_token) {
        return;
    }

    const bool variant_changed = button->variant != variant;
    if (variant_changed) {
        button->variant = variant;
        ecs_modified(world, select_button, EcsUiButton);
    }
    if (current_style != style_token && style_token != 0) {
        (void)EcsUiSetStyleToken(world, select_button, style_token);
    }
    if (variant_changed && item_data->id == selected_item_id) {
        DemoAnimStartSelectionHighlight(world, select_button);
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

    const char *next_value = value != NULL ? value : "";
    if (text->role == role && strcmp(text->text, next_value) == 0) {
        return;
    }

    (void)snprintf(text->text, sizeof(text->text), "%s", next_value);
    text->role = role;
    ecs_modified(world, node, EcsUiText);
}

static void DemoUiSetButtonDisabled(
    ecs_world_t *world,
    ecs_entity_t node,
    bool disabled)
{
    EcsUiButton *button =
        node != 0 ? ecs_get_mut(world, node, EcsUiButton) : NULL;
    if (button == NULL || button->disabled == disabled) {
        return;
    }

    button->disabled = disabled;
    ecs_modified(world, node, EcsUiButton);
}

static uint32_t DemoUiItemCount(ecs_world_t *world)
{
    uint32_t count = 0u;
    ecs_iter_t it = ecs_each(world, DemoUiItemSource);
    while (ecs_each_next(&it)) {
        count += (uint32_t)it.count;
    }
    return count;
}

static uint32_t DemoUiItemPosition(ecs_world_t *world, ecs_entity_t source)
{
    if (world == NULL || source == 0) {
        return 0u;
    }

    uint32_t position = 1u;
    ecs_entities_t children =
        ecs_get_ordered_children(world, DemoUiItemSourceRoot(world));
    for (int32_t i = 0; i < children.count; i += 1) {
        if (!ecs_has(world, children.ids[i], DemoUiItemSource)) {
            continue;
        }
        if (children.ids[i] == source) {
            return position;
        }
        position += 1u;
    }

    return 0u;
}

void DemoUiUpdateItemRowWithPosition(
    ecs_world_t *world,
    ecs_entity_t source,
    const DemoUiItemSource *item_data,
    uint32_t position,
    uint32_t item_count)
{
    if (world == NULL || source == 0 || item_data == NULL) {
        return;
    }

    ecs_entity_t label =
        EcsUiProjectionGetNode(world, source, DemoItemLabelUiNode);
    DemoUiUpdateTextNode(world, label, item_data->label, ECS_UI_TEXT_LABEL);

    char meta_text[ECS_UI_TEXT_MAX] = {0};
    if (position != 0u) {
        (void)snprintf(meta_text, sizeof(meta_text), "#%u", position);
    } else {
        (void)snprintf(meta_text, sizeof(meta_text), "#?");
    }
    ecs_entity_t meta =
        EcsUiProjectionGetNode(world, source, DemoItemMetaUiNode);
    DemoUiUpdateTextNode(world, meta, meta_text, ECS_UI_TEXT_CAPTION);

    ecs_entity_t up_button =
        EcsUiProjectionGetNode(world, source, DemoItemUpUiNode);
    DemoUiSetButtonDisabled(world, up_button, position <= 1u);

    ecs_entity_t down_button =
        EcsUiProjectionGetNode(world, source, DemoItemDownUiNode);
    DemoUiSetButtonDisabled(
        world,
        down_button,
        position == 0u || item_count == 0u || position >= item_count);
}

void DemoUiUpdateItemRow(
    ecs_world_t *world,
    ecs_entity_t source,
    const DemoUiItemSource *item_data)
{
    DemoUiUpdateItemRowWithPosition(
        world,
        source,
        item_data,
        DemoUiItemPosition(world, source),
        DemoUiItemCount(world));
}

ecs_entity_t DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t source,
    const DemoUiItemSource *item_data,
    const DemoUiRefs *refs)
{
    if (world == NULL || source == 0 || item_data == NULL || refs == NULL ||
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
        DemoUiItemPosition(world, source));

    /*
     * Rows are built once as retained UI. The root row is linked to a UI-world
     * source proxy, selected child nodes are stored in projection slots, and
     * action buttons point back to that proxy. Cross-world app requests use the
     * stable item id stored on DemoUiItemSource rather than app entity ids.
     */
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
            .style_token = DemoThemeSubtleActionStyleToken(world),
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
            .style_token = DemoThemeDangerActionStyleToken(world),
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
        (void)EcsUiProjectionLink(world, source, row);
        if (label_text != 0) {
            (void)EcsUiProjectionSetNode(
                world,
                source,
                DemoItemLabelUiNode,
                label_text);
        }
        if (meta_text_node != 0) {
            (void)EcsUiProjectionSetNode(
                world,
                source,
                DemoItemMetaUiNode,
                meta_text_node);
        }
        if (select_button != 0) {
            (void)EcsUiProjectionSetNode(
                world,
                source,
                DemoItemSelectUiNode,
                select_button);
            (void)EcsUiProjectionSetSource(world, select_button, source);
        }
        if (rename_button != 0) {
            (void)EcsUiProjectionSetSource(world, rename_button, source);
        }
        if (up_button != 0) {
            (void)EcsUiProjectionSetNode(
                world,
                source,
                DemoItemUpUiNode,
                up_button);
            (void)EcsUiProjectionSetSource(world, up_button, source);
        }
        if (down_button != 0) {
            (void)EcsUiProjectionSetNode(
                world,
                source,
                DemoItemDownUiNode,
                down_button);
            (void)EcsUiProjectionSetSource(world, down_button, source);
        }
        if (delete_button != 0) {
            (void)EcsUiProjectionSetSource(world, delete_button, source);
        }
        DemoAnimStartRowInsert(world, row);
    }
    return row;
}
