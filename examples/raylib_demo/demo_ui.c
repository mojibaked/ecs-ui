#include "demo_ui_internal.h"

#include <string.h>

ECS_COMPONENT_DECLARE(DemoUiRefs);

ecs_entity_t DemoUiFindNodeById(ecs_world_t *world, const char *id)
{
    if (world == NULL || id == NULL) {
        return 0;
    }

    ecs_iter_t it = ecs_each(world, EcsUiNodeId);
    while (ecs_each_next(&it)) {
        const EcsUiNodeId *ids = ecs_field(&it, EcsUiNodeId, 0);
        for (int32_t i = 0; i < it.count; i += 1) {
            if (strcmp(ids[i].value, id) == 0) {
                return it.entities[i];
            }
        }
    }
    return 0;
}

void DemoUiRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoUiRefs);
    DemoUiRegisterItemProjection(world);
}
