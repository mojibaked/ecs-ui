#ifndef ECS_UI_FRAME_INTERNAL_H
#define ECS_UI_FRAME_INTERNAL_H

#include "ecs_ui/ecs_ui_clay.h"
#include "ecs_ui/ecs_ui_frame.h"

typedef enum EcsUiFrameInternalBackend {
    ECS_UI_FRAME_INTERNAL_BACKEND_CLAY = 0,
    ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE = 1,
    ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DIVERGE = 2,
    ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DEEP_DIVERGE = 3,
} EcsUiFrameInternalBackend;

struct EcsUiDrawList {
    Clay_RenderCommandArray commands;
};

const Clay_RenderCommandArray *EcsUiFrameDrawListClayCommands(
    const EcsUiDrawList *draw_list);
EcsUiClayLayoutOptions EcsUiFrameInternalClayLayoutOptions(
    const EcsUiFrameLayoutOptions *options);
void EcsUiFrameInternalSelectBackend(EcsUiFrameInternalBackend backend);
EcsUiFrameInternalBackend EcsUiFrameInternalSelectedBackend(void);

#endif
