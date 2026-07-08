#ifndef ECS_UI_FRAME_INTERNAL_H
#define ECS_UI_FRAME_INTERNAL_H

#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "ecs_ui_solver.h"

void EcsUiFrameInternalSetBackendDescForTest(
    const EcsUiFrameBackendDesc *desc);
const EcsUiPaintList *EcsUiFrameInternalPaintList(void);
void EcsUiFrameInternalSetNativeDivergenceForTest(
    bool force_root,
    bool force_deep);
void EcsUiFrameInternalSetPaintItemCapacity(uint32_t capacity);
void EcsUiFrameInternalSetTextMeasureLineCapacity(uint32_t capacity);
void EcsUiFrameInternalSetNativeScrollOffsets(
    const EcsUiSolverScrollOffset *offsets,
    uint32_t count);
void EcsUiFrameInternalSetNativeScrollContentOutput(
    EcsUiSolverScrollContent *contents,
    uint32_t count);

#endif
