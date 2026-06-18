#include "demo_ui_internal.h"

#include "demo_nav.h"

#include <raylib.h>

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
            DemoNavRequestDismissPresentation(world);
            continue;
        }

        if (event->action == refs->add_item_action) {
            TraceLog(
                LOG_INFO,
                "DEMO: add item requested from %s",
                event->node_id);
            DemoAppRequestAddItem(world);
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
