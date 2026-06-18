#include "demo_ui_internal.h"

#include <string.h>

ECS_COMPONENT_DECLARE(DemoUiRefs);
ECS_COMPONENT_DECLARE(DemoUiItemSource);

ecs_entity_t DemoUiItemSourceRoot(ecs_world_t *world)
{
    ecs_entity_t root = ecs_entity(world, {.name = "DemoUiItemSources"});
    ecs_add_id(world, root, EcsOrderedChildren);
    return root;
}

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
    EcsUiProjectionImport(world);
    ECS_COMPONENT_DEFINE(world, DemoUiRefs);
    ECS_COMPONENT_DEFINE(world, DemoUiItemSource);
    ECS_TAG_DEFINE(world, DemoItemSelectUiNode);
    ECS_TAG_DEFINE(world, DemoItemLabelUiNode);
    ECS_TAG_DEFINE(world, DemoItemMetaUiNode);
    ECS_TAG_DEFINE(world, DemoItemUpUiNode);
    ECS_TAG_DEFINE(world, DemoItemDownUiNode);
    /*
     * Projection slots are typed links from a domain source entity to important
     * retained UI nodes inside its generated row. Later systems can ask for
     * "the label node for this item" without walking the tree by string id.
     */
    (void)EcsUiProjectionRegisterSlot(world, DemoItemSelectUiNode);
    (void)EcsUiProjectionRegisterSlot(world, DemoItemLabelUiNode);
    (void)EcsUiProjectionRegisterSlot(world, DemoItemMetaUiNode);
    (void)EcsUiProjectionRegisterSlot(world, DemoItemUpUiNode);
    (void)EcsUiProjectionRegisterSlot(world, DemoItemDownUiNode);
    (void)DemoUiItemSourceRoot(world);
}
