#include "demo_ui_internal.h"

#include "demo_nav.h"
#include "demo_text_input.h"

#include <raylib.h>

void DemoUiApplyEvents(ecs_world_t *world, const EcsUiEventList *events)
{
    if (world == NULL || events == NULL) {
        return;
    }

    const DemoUiRefs *refs = ecs_singleton_get(world, DemoUiRefs);
    for (uint32_t i = 0u; i < events->count; i += 1u) {
        const EcsUiEvent *event = &events->events[i];
        if (refs == NULL) {
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_INPUT) {
            if (DemoTextInputHasFocusedField(world)) {
                DemoTextInputRequestInsert(world, event->codepoint);
            }
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_DELETE) {
            if (DemoTextInputHasFocusedField(world)) {
                DemoTextInputRequestDelete(world);
            }
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_CANCEL) {
            if (DemoTextInputHasFocusedField(world)) {
                DemoTextInputRequestBlur(world);
            }
            continue;
        }

        if (event->type == ECS_UI_EVENT_TEXT_SUBMIT) {
            if (DemoTextInputHasFocusedField(world)) {
                const char *label = DemoTextInputAddItemNameValue(world);
                TraceLog(LOG_INFO, "DEMO: submit text field requested");
                DemoAppRequestAddNamedItem(world, label);
                DemoTextInputClearAddItemName(world);
                DemoTextInputRequestBlur(world);
                DemoNavRequestDismissPresentation(world);
            }
            continue;
        }

        if (event->action == refs->drag_presentation_action) {
            if (event->type == ECS_UI_EVENT_DRAG_STARTED) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: presentation drag started from %s",
                    event->node_id);
                DemoNavRequestBeginPresentationDrag(world);
                continue;
            }
            if (event->type == ECS_UI_EVENT_DRAGGED) {
                DemoNavRequestUpdatePresentationDrag(world, event->delta_y);
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
                    world,
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
                ecs_get_target(world, event->node, DemoUiForTextField, 0);
            if (field != 0) {
                TraceLog(
                    LOG_INFO,
                    "DEMO: focus text field requested from %s",
                    event->node_id);
                DemoTextInputRequestFocusField(world, field);
            }
            continue;
        }

        if (event->action == refs->present_add_item_action) {
            TraceLog(
                LOG_INFO,
                "DEMO: present add item requested from %s",
                event->node_id);
            DemoNavRequestPresentRoute(world, DemoNavAddItemRoute(world));
            continue;
        }

        if (event->action == refs->dismiss_presentation_action) {
            TraceLog(
                LOG_INFO,
                "DEMO: dismiss presentation requested from %s",
                event->node_id);
            DemoTextInputRequestBlur(world);
            DemoNavRequestDismissPresentation(world);
            continue;
        }

        if (event->action == refs->add_item_action) {
            TraceLog(
                LOG_INFO,
                "DEMO: add item requested from %s",
                event->node_id);
            DemoAppRequestAddNamedItem(
                world,
                DemoTextInputAddItemNameValue(world));
            DemoTextInputClearAddItemName(world);
            DemoTextInputRequestBlur(world);
            DemoNavRequestDismissPresentation(world);
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
