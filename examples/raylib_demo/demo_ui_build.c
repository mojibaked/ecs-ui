#include "demo_ui_internal.h"

#include "demo_terminal.h"

#include <stdio.h>
#include <string.h>

ecs_entity_t DemoUiBuild(ecs_world_t *world)
{
    ecs_entity_t root = EcsUiRootEntity(world, "RaylibDemo");
    /*
     * Action entities are stable identity tokens attached to interactive UI
     * nodes. Raylib event collection reports the token back, and DemoUiApplyEvents
     * decides which domain/navigation/text request to enqueue for that action.
     */
    ecs_entity_t add_item_action = ecs_entity(world, {.name = "AddItemAction"});
    ecs_entity_t present_add_item_action =
        ecs_entity(world, {.name = "PresentAddItemAction"});
    ecs_entity_t dismiss_presentation_action =
        ecs_entity(world, {.name = "DismissPresentationAction"});
    ecs_entity_t drag_presentation_action =
        ecs_entity(world, {.name = "DragPresentationAction"});
    ecs_entity_t focus_text_field_action =
        ecs_entity(world, {.name = "FocusTextFieldAction"});
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

    ZStack(&builder, {.id = "DemoViewport", .gap = 0.0f, .padding = 0.0f}) {
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
                        .on_click = present_add_item_action,
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
            (void)DemoTerminalBuildPreview(world, &builder);
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
    }

    EcsUiBuilderEnd(&builder);
    if (!EcsUiBuilderOk(&builder)) {
        return 0;
    }

    /*
     * DemoUiRefs is the bridge between static UI construction and runtime
     * systems. It keeps action tokens plus a few anchor nodes that dynamic
     * projections use as insertion points, avoiding repeated id scans later.
     */
    ecs_singleton_set(
        world,
        DemoUiRefs,
        {
            .add_item_action = add_item_action,
            .present_add_item_action = present_add_item_action,
            .dismiss_presentation_action = dismiss_presentation_action,
            .drag_presentation_action = drag_presentation_action,
            .focus_text_field_action = focus_text_field_action,
            .select_item_action = select_item_action,
            .delete_item_action = delete_item_action,
            .rename_item_action = rename_item_action,
            .move_item_up_action = move_item_up_action,
            .move_item_down_action = move_item_down_action,
            .presentation_host = DemoUiFindNodeById(world, "DemoViewport"),
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

    const char *next_message = message != NULL ? message : "";
    if (text->role == ECS_UI_TEXT_CAPTION &&
        strcmp(text->text, next_message) == 0) {
        return;
    }

    (void)snprintf(text->text, sizeof(text->text), "%s", next_message);
    text->role = ECS_UI_TEXT_CAPTION;
    ecs_modified(world, status, EcsUiText);
}
