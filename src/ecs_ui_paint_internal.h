#ifndef ECS_UI_PAINT_INTERNAL_H
#define ECS_UI_PAINT_INTERNAL_H

#include "ecs_ui/ecs_ui_paint.h"

bool EcsUiPaintListBuildWithCapacity(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
    int16_t base_z_index,
    uint32_t item_capacity);

#endif
