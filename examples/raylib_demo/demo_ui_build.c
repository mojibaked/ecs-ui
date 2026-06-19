#include "demo_ui_internal.h"

#include "demo_terminal.h"
#include "demo_theme.h"
#include "ecs_ui/ecs_ui_navigation.h"

#include <stdio.h>
#include <string.h>

ecs_entity_t DemoUiBuild(ecs_world_t *world)
{
    ecs_entity_t root = EcsUiRootEntity(world, "RaylibDemo");
    /*
     * Action entities are stable identity tokens attached to interactive UI
     * nodes. Event collection reports the token back, and DemoUiApplyEvents
     * decides which domain/navigation request to enqueue for that action.
     * Text input focus is routed by EcsUiForTextField instead of a demo token.
     */
    ecs_entity_t add_item_action = ecs_entity(world, {.name = "AddItemAction"});
    ecs_entity_t present_add_item_action =
        ecs_entity(world, {.name = "PresentAddItemAction"});
    ecs_entity_t dismiss_presentation_action =
        ecs_entity(world, {.name = "DismissPresentationAction"});
    ecs_entity_t drag_presentation_action =
        ecs_entity(world, {.name = "DragPresentationAction"});
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
    ecs_entity_t toggle_theme_action =
        ecs_entity(world, {.name = "ToggleThemeAction"});
    ecs_entity_t theme_text = 0;
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
                        .style_token = DemoThemePrimaryActionStyleToken(world),
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
                Button(
                    &builder,
                    {
                        .id = "ToggleTheme",
                        .variant = ECS_UI_BUTTON_SUBTLE,
                        .on_click = toggle_theme_action,
                        .style_token = DemoThemeSubtleActionStyleToken(world),
                    }) {
                    theme_text = Text(
                        &builder,
                        {
                            .id = "ThemeModeLabel",
                            .text = DemoThemeName(world),
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
    ecs_entity_t presentation_host = DemoUiFindNodeById(world, "DemoViewport");
    if (presentation_host != 0) {
        ecs_add_id(world, presentation_host, EcsUiPresentationHost);
        ecs_set(
            world,
            presentation_host,
            EcsUiHitTest,
            {
                .mode = ECS_UI_HIT_TEST_CAPTURE,
            });
    }

    ecs_singleton_set(
        world,
        DemoUiRefs,
        {
            .add_item_action = add_item_action,
            .present_add_item_action = present_add_item_action,
            .dismiss_presentation_action = dismiss_presentation_action,
            .drag_presentation_action = drag_presentation_action,
            .select_item_action = select_item_action,
            .delete_item_action = delete_item_action,
            .rename_item_action = rename_item_action,
            .move_item_up_action = move_item_up_action,
            .move_item_down_action = move_item_down_action,
            .toggle_theme_action = toggle_theme_action,
            .presentation_host = presentation_host,
            .item_list = DemoUiFindNodeById(world, "ItemList"),
            .status_text = DemoUiFindNodeById(world, "EventStatus"),
            .theme_text = theme_text,
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

void DemoUiRefreshThemeLabel(ecs_world_t *world)
{
    const DemoUiRefs *refs = ecs_singleton_get(world, DemoUiRefs);
    ecs_entity_t label = refs != NULL ? refs->theme_text : 0;
    EcsUiText *text =
        label != 0 ? ecs_get_mut(world, label, EcsUiText) : NULL;
    if (text == NULL) {
        return;
    }

    const char *mode = DemoThemeName(world);
    if (text->role == ECS_UI_TEXT_BUTTON && strcmp(text->text, mode) == 0) {
        return;
    }

    (void)snprintf(text->text, sizeof(text->text), "%s", mode);
    text->role = ECS_UI_TEXT_BUTTON;
    ecs_modified(world, label, EcsUiText);
}
