#ifndef ECS_UI_SOLVER_H
#define ECS_UI_SOLVER_H

#include <stdbool.h>
#include <stddef.h>

#include "ecs_ui/ecs_ui_frame.h"

typedef struct EcsUiSolverArena {
    unsigned char *data;
    size_t capacity;
    size_t used;
} EcsUiSolverArena;

typedef struct EcsUiSolverRunOptions {
    const EcsUiFrameLayoutOptions *layout;
    EcsUiMeasureTextFn measure_text;
    void *measure_user_data;
    bool force_divergence;
    bool force_deep_divergence;
    char *error_message;
    size_t error_message_size;
} EcsUiSolverRunOptions;

void EcsUiSolverArenaRelease(EcsUiSolverArena *arena);
bool EcsUiSolverRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiSolverRunOptions *options,
    EcsUiSolverArena *arena);

#endif
