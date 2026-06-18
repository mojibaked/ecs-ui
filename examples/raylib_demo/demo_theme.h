#ifndef ECS_UI_RAYLIB_DEMO_THEME_H
#define ECS_UI_RAYLIB_DEMO_THEME_H

#include "ecs_ui/ecs_ui.h"

void DemoThemeRegister(ecs_world_t *world);
ecs_entity_t DemoThemeTextInputFieldStyleToken(ecs_world_t *world);
void DemoThemeToggle(ecs_world_t *world);
bool DemoThemeIsLight(const ecs_world_t *world);
const char *DemoThemeName(const ecs_world_t *world);

#endif
