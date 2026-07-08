#ifndef ECS_UI_PAINT_INTERNAL_H
#define ECS_UI_PAINT_INTERNAL_H

#include "ecs_ui/ecs_ui_paint.h"

bool EcsUiPaintListBuildWithCapacity(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t item_capacity);

#endif
