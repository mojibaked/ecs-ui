#ifndef ECS_UI_ECS_UI_ANIMATION_H
#define ECS_UI_ECS_UI_ANIMATION_H

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EcsUiAnimatedFloat {
    float value;
} EcsUiAnimatedFloat;

typedef struct EcsUiLinear1f {
    float from;
    float to;
    float elapsed;
    float duration;
} EcsUiLinear1f;

extern ECS_COMPONENT_DECLARE(EcsUiAnimatedFloat);
extern ECS_COMPONENT_DECLARE(EcsUiLinear1f);
extern ECS_TAG_DECLARE(EcsUiAnimationComplete);

void EcsUiAnimationImport(ecs_world_t *world);

float EcsUiAnimationClamp01(float value);
float EcsUiAnimationValue(
    const ecs_world_t *world,
    ecs_entity_t entity,
    float fallback);
void EcsUiAnimationStartLinear1f(
    ecs_world_t *world,
    ecs_entity_t entity,
    float from,
    float to,
    float duration);
void EcsUiAnimationSetValue(
    ecs_world_t *world,
    ecs_entity_t entity,
    float value);
void EcsUiAnimationApplyFadeSlideY(
    ecs_world_t *world,
    ecs_entity_t node,
    float value,
    float offset_y);
void EcsUiAnimationApplyHighlight(
    ecs_world_t *world,
    ecs_entity_t node,
    float value);

#ifdef __cplusplus
}
#endif

#endif
