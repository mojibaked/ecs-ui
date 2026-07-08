#ifndef ECS_UI_INTERACTION_H
#define ECS_UI_INTERACTION_H

#include "ecs_ui/ecs_ui_frame.h"

typedef struct EcsUiInteractionBuildOptions {
    const EcsUiFrameLayoutOptions *layout;
    const EcsUiScrollUpdate *scroll_reports;
    uint32_t scroll_report_count;
} EcsUiInteractionBuildOptions;

void EcsUiInteractionFrameBegin(
    EcsUiInteractionFrame *frame,
    EcsUiInteractionState *state);

bool EcsUiInteractionFrameBuild(
    EcsUiInteractionFrame *frame,
    const EcsUiTreeSnapshot *tree,
    const EcsUiInteractionBuildOptions *options);

void EcsUiInteractionCollectFrameEvents(
    EcsUiInteractionFrame *frame,
    EcsUiPointerState pointer,
    EcsUiEventList *events);

#endif
