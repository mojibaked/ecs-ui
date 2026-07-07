#ifndef ECS_UI_SCROLL_STATE_H
#define ECS_UI_SCROLL_STATE_H

#include "ecs_ui/ecs_ui.h"

float EcsUiScrollStateClampOffset(
    float offset,
    float content,
    float viewport);

bool EcsUiScrollStateApplyUpdate(
    ecs_world_t *world,
    const EcsUiScrollUpdate *update);

#endif
