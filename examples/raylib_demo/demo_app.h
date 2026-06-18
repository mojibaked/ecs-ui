#ifndef ECS_UI_RAYLIB_DEMO_APP_H
#define ECS_UI_RAYLIB_DEMO_APP_H

#include "ecs_ui/ecs_ui.h"

#include <stdint.h>

typedef struct DemoItem {
    uint32_t id;
    uint32_t order;
    char label[ECS_UI_TEXT_MAX];
} DemoItem;

typedef struct DemoItemSequence {
    uint32_t next_item_id;
} DemoItemSequence;

extern ECS_COMPONENT_DECLARE(DemoItem);
extern ECS_COMPONENT_DECLARE(DemoItemSequence);
extern ECS_TAG_DECLARE(DemoAddItemRequest);
extern ECS_TAG_DECLARE(DemoItemUiNode);
extern ECS_TAG_DECLARE(DemoItemUiFor);

void DemoAppRegister(ecs_world_t *world);
ecs_entity_t DemoAppItemRoot(ecs_world_t *world);
void DemoAppRequestAddItem(ecs_world_t *world);

#endif
