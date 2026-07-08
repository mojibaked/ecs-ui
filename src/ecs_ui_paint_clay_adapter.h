#ifndef ECS_UI_PAINT_CLAY_ADAPTER_H
#define ECS_UI_PAINT_CLAY_ADAPTER_H

#include "ecs_ui/ecs_ui_clay.h"
#include "ecs_ui/ecs_ui_paint.h"

typedef struct EcsUiPaintClayAdapterOptions {
    float scale;
    float physical_x;
    float physical_y;
} EcsUiPaintClayAdapterOptions;

bool EcsUiPaintClayElementId(
    const EcsUiTreeNodeSnapshot *node,
    const char *suffix,
    Clay_ElementId *out);
bool EcsUiPaintClayBorderCommandId(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node,
    Clay_ElementId *out);
bool EcsUiPaintClayAdapterBuild(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintClayAdapterOptions *options,
    Clay_RenderCommand *storage,
    int32_t storage_capacity,
    Clay_RenderCommandArray *out);

#endif
