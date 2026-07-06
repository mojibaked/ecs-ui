#ifndef ECS_UI_RAYLIB_DEMO_TERMINAL_H
#define ECS_UI_RAYLIB_DEMO_TERMINAL_H

#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_raylib.h"

typedef struct DemoTerminalViewport {
    char title[ECS_UI_TEXT_MAX];
    char line_a[ECS_UI_TEXT_MAX];
    char line_b[ECS_UI_TEXT_MAX];
    char line_c[ECS_UI_TEXT_MAX];
} DemoTerminalViewport;

extern ECS_COMPONENT_DECLARE(DemoTerminalViewport);

void DemoTerminalRegister(ecs_world_t *world);
ecs_entity_t DemoTerminalBuildPreview(
    ecs_world_t *world,
    EcsUiBuilder *builder);
void DemoTerminalDrawCustom(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data);

#endif
