#ifndef ECS_UI_ECS_UI_CLAY_H
#define ECS_UI_ECS_UI_CLAY_H

#include <clay.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EcsUiClayTheme {
    Clay_Color root_background;
    Clay_Color surface;
    Clay_Color button;
    Clay_Color button_primary;
    Clay_Color button_subtle;
    Clay_Color button_danger;
    Clay_Color text;
    Clay_Color text_inverse;
} EcsUiClayTheme;

EcsUiClayTheme EcsUiClayThemeDefault(void);
void EcsUiClayEmitTree(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme);

#ifdef __cplusplus
}
#endif

#endif

