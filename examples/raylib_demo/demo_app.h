#ifndef ECS_UI_RAYLIB_DEMO_APP_H
#define ECS_UI_RAYLIB_DEMO_APP_H

#include "ecs_ui/ecs_ui.h"

#include <stdint.h>

typedef struct DemoItem {
    uint32_t id;
    uint32_t rename_count;
    char label[ECS_UI_TEXT_MAX];
} DemoItem;

typedef struct DemoItemSequence {
    uint32_t next_item_id;
} DemoItemSequence;

typedef struct DemoAddItemRequest {
    char label[ECS_UI_TEXT_MAX];
} DemoAddItemRequest;

extern ECS_COMPONENT_DECLARE(DemoItem);
extern ECS_COMPONENT_DECLARE(DemoItemSequence);
extern ECS_COMPONENT_DECLARE(DemoAddItemRequest);
extern ECS_TAG_DECLARE(DemoSelectItemRequest);
extern ECS_TAG_DECLARE(DemoDeleteItemRequest);
extern ECS_TAG_DECLARE(DemoRenameItemRequest);
extern ECS_TAG_DECLARE(DemoMoveItemUpRequest);
extern ECS_TAG_DECLARE(DemoMoveItemDownRequest);
extern ECS_TAG_DECLARE(DemoItemOrderDirty);
extern ECS_TAG_DECLARE(DemoSelectedItem);
extern ECS_TAG_DECLARE(DemoItemUiNode);
extern ECS_TAG_DECLARE(DemoItemSelectUiNode);
extern ECS_TAG_DECLARE(DemoItemLabelUiNode);
extern ECS_TAG_DECLARE(DemoItemMetaUiNode);
extern ECS_TAG_DECLARE(DemoUiForItem);

void DemoAppRegister(ecs_world_t *world);
ecs_entity_t DemoAppItemRoot(ecs_world_t *world);
ecs_entity_t DemoAppSelectionRoot(ecs_world_t *world);
void DemoAppRequestAddItem(ecs_world_t *world);
void DemoAppRequestAddNamedItem(ecs_world_t *world, const char *label);
void DemoAppRequestSelectItem(ecs_world_t *world, ecs_entity_t item);
void DemoAppRequestDeleteItem(ecs_world_t *world, ecs_entity_t item);
void DemoAppRequestRenameItem(ecs_world_t *world, ecs_entity_t item);
void DemoAppRequestMoveItemUp(ecs_world_t *world, ecs_entity_t item);
void DemoAppRequestMoveItemDown(ecs_world_t *world, ecs_entity_t item);

#endif
