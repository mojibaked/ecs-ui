#ifndef ECS_UI_RAYLIB_DEMO_UI_H
#define ECS_UI_RAYLIB_DEMO_UI_H

#include "demo_app.h"
#include "ecs_ui/ecs_ui.h"

typedef struct DemoUiRefs {
    ecs_entity_t add_item_action;
    ecs_entity_t select_item_action;
    ecs_entity_t delete_item_action;
    ecs_entity_t rename_item_action;
    ecs_entity_t move_item_up_action;
    ecs_entity_t move_item_down_action;
    ecs_entity_t item_list;
    ecs_entity_t status_text;
} DemoUiRefs;

extern ECS_COMPONENT_DECLARE(DemoUiRefs);

void DemoUiRegister(ecs_world_t *world);
ecs_entity_t DemoUiBuild(ecs_world_t *world);
void DemoUiApplyEvents(ecs_world_t *world, const EcsUiEventList *events);
void DemoUiSetStatus(ecs_world_t *world, const char *message);

#endif
