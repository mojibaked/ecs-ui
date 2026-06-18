#ifndef ECS_UI_ECS_UI_RAYLIB_H
#define ECS_UI_ECS_UI_RAYLIB_H

#include <raylib.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EcsUiRaylibTheme {
    Color root_background;
    Color surface;
    Color surface_subtle;
    Color button;
    Color button_primary;
    Color button_subtle;
    Color button_danger;
    Color button_disabled;
    Color text;
    Color text_muted;
    Color text_inverse;
    float radius;
} EcsUiRaylibTheme;

EcsUiRaylibTheme EcsUiRaylibThemeDefault(void);
void EcsUiRaylibDrawTree(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiRaylibTheme *theme);
void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events);

#ifdef __cplusplus
}
#endif

#endif
