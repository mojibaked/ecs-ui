#ifndef ECS_UI_RAYLIB_DEMO_ANIM_H
#define ECS_UI_RAYLIB_DEMO_ANIM_H

#include "demo_nav.h"

typedef struct DemoAnimatedFloat {
    float value;
} DemoAnimatedFloat;

typedef struct DemoLinear1f {
    float from;
    float to;
    float elapsed;
    float duration;
} DemoLinear1f;

extern ECS_COMPONENT_DECLARE(DemoAnimatedFloat);
extern ECS_COMPONENT_DECLARE(DemoLinear1f);
extern ECS_TAG_DECLARE(DemoDismissPresentationOnAnimationComplete);

void DemoAnimRegister(ecs_world_t *world);
void DemoAnimStartPresentation(
    ecs_world_t *world,
    ecs_entity_t presentation,
    float from,
    float to,
    float duration,
    bool dismiss_on_complete);
float DemoAnimPresentationValue(ecs_world_t *world, ecs_entity_t presentation);
void DemoAnimApplyVisualToNode(
    ecs_world_t *world,
    ecs_entity_t node,
    float value);
void DemoAnimApplyPresentationVisual(
    ecs_world_t *world,
    ecs_entity_t presentation,
    float value);

#endif
