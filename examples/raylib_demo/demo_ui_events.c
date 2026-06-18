#include "demo_ui_internal.h"

#include "demo_nav.h"
#include "demo_text_input.h"

#include <raylib.h>

static uint32_t DemoUiEventItemId(
    ecs_world_t *ui_world,
    const EcsUiEvent *event)
{
    if (ui_world == NULL || event == NULL) {
        return 0u;
    }

    /*
     * Row buttons point to UI-world source proxies. The proxy stores the stable
     * app item id, which can be resolved in app_world without leaking app-world
     * entity ids into UI relationships.
     */
    ecs_entity_t source = EcsUiProjectionGetSource(ui_world, event->node);
    const DemoUiItemSource *source_data =
        source != 0 ? ecs_get(ui_world, source, DemoUiItemSource) : NULL;
    return source_data != NULL ? source_data->id : 0u;
}

void DemoUiApplyEvents(
    ecs_world_t *ui_world,
    ecs_world_t *app_world,
    const EcsUiEventList *events)
{
    if (ui_world == NULL || app_world == NULL || events == NULL) {
        return;
    }

    const DemoUiRefs *refs = ecs_singleton_get(ui_world, DemoUiRefs);
    for (uint32_t i = 0u; i < events->count; i += 1u) {
        const EcsUiEvent *event = &events->events[i];
        if (refs == NULL) {
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_INPUT) {
            /*
             * Text events are global input events. The demo routes them to a
             * field only when text input state says one field is focused, then
             * text systems mutate the field entity and refresh its projected UI.
             */
            if (DemoTextInputHasFocusedField(ui_world)) {
                DemoTextInputRequestInsert(ui_world, event->codepoint);
            }
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_DELETE) {
            if (DemoTextInputHasFocusedField(ui_world)) {
                DemoTextInputRequestDelete(ui_world);
            }
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_CANCEL) {
            if (DemoTextInputHasFocusedField(ui_world)) {
                DemoTextInputRequestBlur(ui_world);
            }
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_SUBMIT) {
            if (DemoTextInputHasFocusedField(ui_world)) {
                const char *label = DemoTextInputAddItemNameValue(ui_world);
                TraceLog(LOG_INFO, "DEMO: submit text field requested");
                DemoAppRequestAddNamedItem(app_world, label);
                DemoTextInputClearAddItemName(ui_world);
                DemoTextInputRequestBlur(ui_world);
                DemoNavRequestDismissPresentation(ui_world);
            }
            continue;
        }

        if (event->action == refs->drag_presentation_action) {
            /*
             * Drag callbacks use the same action token as clicks, but preserve
             * gesture phase and velocity. Navigation systems turn these requests
             * into direct animation values while the gesture is active, then
             * choose either dismissal or snap-back on release.
             */
            if (event->type == ECS_UI_EVENT_DRAG_STARTED) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: presentation drag started from %s",
                    event->node_id);
                DemoNavRequestBeginPresentationDrag(ui_world);
                continue;
            }
            if (event->type == ECS_UI_EVENT_DRAGGED) {
                DemoNavRequestUpdatePresentationDrag(ui_world, event->delta_y);
                continue;
            }
            if (event->type == ECS_UI_EVENT_DRAG_ENDED) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: presentation drag ended from %s at %.1fpx %.1fpx/s",
                    event->node_id,
                    event->delta_y,
                    event->velocity_y);
                DemoNavRequestEndPresentationDrag(
                    ui_world,
                    event->delta_y,
                    event->velocity_y);
                continue;
            }
        }

        if (event->type != ECS_UI_EVENT_CLICKED) {
            continue;
        }

        if (event->action == refs->focus_text_field_action) {
            ecs_entity_t field =
                EcsUiTextInputUiField(ui_world, event->node);
            if (field != 0) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: focus text field requested from %s",
                    event->node_id);
                DemoTextInputRequestFocusField(ui_world, field);
            }
            continue;
        }

        if (event->action == refs->present_add_item_action) {
            TraceLog(
                LOG_INFO,
                "DEMO: present add item requested from %s",
                event->node_id);
            DemoNavRequestPresentRoute(
                ui_world,
                DemoNavAddItemRoute(ui_world));
            continue;
        }

        if (event->action == refs->dismiss_presentation_action) {
            TraceLog(
                LOG_INFO,
                "DEMO: dismiss presentation requested from %s",
                event->node_id);
            DemoTextInputRequestBlur(ui_world);
            DemoNavRequestDismissPresentation(ui_world);
            continue;
        }

        if (event->action == refs->add_item_action) {
            /*
             * Submitting the sheet spans three subsystems: app request for the
             * item, text request/state cleanup, and navigation request to dismiss.
             * Keeping each as a request lets their registered systems own the
             * actual mutation and projection refresh.
             */
            TraceLog(
                LOG_INFO,
                "DEMO: add item requested from %s",
                event->node_id);
            DemoAppRequestAddNamedItem(
                app_world,
                DemoTextInputAddItemNameValue(ui_world));
            DemoTextInputClearAddItemName(ui_world);
            DemoTextInputRequestBlur(ui_world);
            DemoNavRequestDismissPresentation(ui_world);
            continue;
        }

        if (event->action == refs->select_item_action) {
            uint32_t item_id = DemoUiEventItemId(ui_world, event);
            ecs_entity_t item = DemoAppFindItemById(app_world, item_id);
            const DemoItem *item_data =
                item != 0 ? ecs_get(app_world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: select item requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestSelectItemId(app_world, item_id);
            }
            continue;
        }

        if (event->action == refs->rename_item_action) {
            uint32_t item_id = DemoUiEventItemId(ui_world, event);
            ecs_entity_t item = DemoAppFindItemById(app_world, item_id);
            const DemoItem *item_data =
                item != 0 ? ecs_get(app_world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: rename item requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestRenameItemId(app_world, item_id);
            }
            continue;
        }

        if (event->action == refs->move_item_up_action) {
            uint32_t item_id = DemoUiEventItemId(ui_world, event);
            ecs_entity_t item = DemoAppFindItemById(app_world, item_id);
            const DemoItem *item_data =
                item != 0 ? ecs_get(app_world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: move up requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestMoveItemUpId(app_world, item_id);
            }
            continue;
        }

        if (event->action == refs->move_item_down_action) {
            uint32_t item_id = DemoUiEventItemId(ui_world, event);
            ecs_entity_t item = DemoAppFindItemById(app_world, item_id);
            const DemoItem *item_data =
                item != 0 ? ecs_get(app_world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: move down requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestMoveItemDownId(app_world, item_id);
            }
            continue;
        }

        if (event->action == refs->delete_item_action) {
            uint32_t item_id = DemoUiEventItemId(ui_world, event);
            ecs_entity_t item = DemoAppFindItemById(app_world, item_id);
            const DemoItem *item_data =
                item != 0 ? ecs_get(app_world, item, DemoItem) : NULL;
            if (item_data != NULL) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: delete item requested from %s for %s",
                    event->node_id,
                    item_data->label);
                DemoAppRequestDeleteItemId(app_world, item_id);
            }
        }
    }
}
