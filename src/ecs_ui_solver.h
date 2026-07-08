#ifndef ECS_UI_SOLVER_H
#define ECS_UI_SOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ecs_ui/ecs_ui_frame.h"

typedef struct EcsUiSolverArena {
    unsigned char *data;
    size_t capacity;
    size_t used;
} EcsUiSolverArena;

typedef struct EcsUiSolverScrollOffset {
    uint32_t node_index;
    float offset_x;
    float offset_y;
} EcsUiSolverScrollOffset;

typedef struct EcsUiSolverScrollContent {
    uint32_t node_index;
    float width;
    float height;
    bool valid;
} EcsUiSolverScrollContent;

typedef struct EcsUiSolverRunOptions {
    const EcsUiFrameLayoutOptions *layout;
    float surface_width;
    float surface_height;
    EcsUiMeasureTextFn measure_text;
    void *measure_user_data;
    /*
     * Scroll offsets are logical units. The Clay parity harness writes the
     * same values scaled to physical pixels into Clay's retained scroll state.
     * Reported content dimensions are logical units.
     */
    const EcsUiSolverScrollOffset *scroll_offsets;
    uint32_t scroll_offset_count;
    EcsUiSolverScrollContent *scroll_contents;
    uint32_t scroll_content_count;
    bool force_divergence;
    bool force_deep_divergence;
    uint32_t text_line_capacity;
    EcsUiFrameErrorKind *error_kind;
    char *error_message;
    size_t error_message_size;
} EcsUiSolverRunOptions;

void EcsUiSolverArenaRelease(EcsUiSolverArena *arena);
bool EcsUiSolverRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiSolverRunOptions *options,
    EcsUiSolverArena *arena);

#endif
