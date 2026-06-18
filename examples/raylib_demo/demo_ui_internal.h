#ifndef ECS_UI_RAYLIB_DEMO_UI_INTERNAL_H
#define ECS_UI_RAYLIB_DEMO_UI_INTERNAL_H

#include "demo_ui.h"

ecs_entity_t DemoUiFindNodeById(ecs_world_t *world, const char *id);
void DemoUiRegisterItemProjection(ecs_world_t *world);

ecs_entity_t DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data,
    const DemoUiRefs *refs);
void DemoUiUpdateItemRow(
    ecs_world_t *world,
    ecs_entity_t item,
    const DemoItem *item_data);
void DemoUiApplyItemOrderToList(ecs_world_t *world, ecs_entity_t item_list);
void DemoUiRefreshSelectionStyles(ecs_world_t *world);

#endif
