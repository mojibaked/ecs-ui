#ifndef ECS_UI_ECS_UI_RAYLIB_H
#define ECS_UI_ECS_UI_RAYLIB_H

#include <raylib.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*EcsUiRaylibCustomDrawFn)(
    const EcsUiTreeNodeSnapshot *node,
    Rectangle bounds,
    float opacity,
    void *user_data);

typedef struct EcsUiRaylibDrawOptions {
    EcsUiRaylibCustomDrawFn custom_draw;
    void *user_data;
} EcsUiRaylibDrawOptions;

void EcsUiRaylibDrawTree(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme);
void EcsUiRaylibDrawTreeEx(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options);
void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events);

#ifdef __cplusplus
}
#endif

#endif
