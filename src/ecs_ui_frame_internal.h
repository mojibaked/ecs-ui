#ifndef ECS_UI_FRAME_INTERNAL_H
#define ECS_UI_FRAME_INTERNAL_H

#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "ecs_ui_solver.h"

#ifndef ECS_UI_FRAME_ENABLE_CLAY
#define ECS_UI_FRAME_ENABLE_CLAY 0
#endif

#if ECS_UI_FRAME_ENABLE_CLAY
#include "ecs_ui/ecs_ui_clay.h"
#endif

typedef enum EcsUiFrameInternalBackend {
    ECS_UI_FRAME_INTERNAL_BACKEND_CLAY = 0,
    ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE = 1,
    ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DIVERGE = 2,
    ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DEEP_DIVERGE = 3,
} EcsUiFrameInternalBackend;

struct EcsUiDrawList {
#if ECS_UI_FRAME_ENABLE_CLAY
    Clay_RenderCommandArray commands;
#else
    uint32_t reserved;
#endif
};

#if ECS_UI_FRAME_ENABLE_CLAY
const Clay_RenderCommandArray *EcsUiFrameDrawListClayCommands(
    const EcsUiDrawList *draw_list);
const Clay_RenderCommandArray *EcsUiFrameInternalClayCommands(void);
EcsUiClayLayoutOptions EcsUiFrameInternalClayLayoutOptions(
    const EcsUiFrameLayoutOptions *options);
#endif
void EcsUiFrameInternalSelectBackend(EcsUiFrameInternalBackend backend);
EcsUiFrameInternalBackend EcsUiFrameInternalSelectedBackend(void);
void EcsUiFrameInternalSetBackendDescForTest(
    const EcsUiFrameBackendDesc *desc);
const EcsUiPaintList *EcsUiFrameInternalPaintList(void);
void EcsUiFrameInternalSetPaintItemCapacity(uint32_t capacity);
void EcsUiFrameInternalSetTextMeasureLineCapacity(uint32_t capacity);
void EcsUiFrameInternalSetNativeScrollOffsets(
    const EcsUiSolverScrollOffset *offsets,
    uint32_t count);
void EcsUiFrameInternalSetNativeScrollContentOutput(
    EcsUiSolverScrollContent *contents,
    uint32_t count);

#endif
