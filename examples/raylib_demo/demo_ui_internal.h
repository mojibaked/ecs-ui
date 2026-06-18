#ifndef ECS_UI_RAYLIB_DEMO_UI_INTERNAL_H
#define ECS_UI_RAYLIB_DEMO_UI_INTERNAL_H

#include "demo_ui.h"
#include "ecs_ui/ecs_ui_projection.h"

typedef struct DemoUiItemSource {
    uint32_t id;
    uint32_t rename_count;
    char label[ECS_UI_TEXT_MAX];
} DemoUiItemSource;

extern ECS_COMPONENT_DECLARE(DemoUiItemSource);

ecs_entity_t DemoUiFindNodeById(ecs_world_t *world, const char *id);
ecs_entity_t DemoUiItemSourceRoot(ecs_world_t *world);

ecs_entity_t DemoUiCreateItemRow(
    ecs_world_t *world,
    ecs_entity_t source,
    const DemoUiItemSource *item_data,
    const DemoUiRefs *refs);
void DemoUiUpdateItemRow(
    ecs_world_t *world,
    ecs_entity_t source,
    const DemoUiItemSource *item_data);
void DemoUiUpdateItemRowWithPosition(
    ecs_world_t *world,
    ecs_entity_t source,
    const DemoUiItemSource *item_data,
    uint32_t position,
    uint32_t item_count);
void DemoUiApplyItemSelectionStyle(
    ecs_world_t *world,
    ecs_entity_t source,
    uint32_t selected_item_id);

#endif
