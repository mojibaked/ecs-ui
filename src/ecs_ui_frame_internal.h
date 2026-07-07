#ifndef ECS_UI_FRAME_INTERNAL_H
#define ECS_UI_FRAME_INTERNAL_H

#include "ecs_ui/ecs_ui_clay.h"
#include "ecs_ui/ecs_ui_frame.h"

struct EcsUiDrawList {
    Clay_RenderCommandArray commands;
};

const Clay_RenderCommandArray *EcsUiFrameDrawListClayCommands(
    const EcsUiDrawList *draw_list);
EcsUiClayLayoutOptions EcsUiFrameInternalClayLayoutOptions(
    const EcsUiFrameLayoutOptions *options);

#endif
