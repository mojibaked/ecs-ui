#ifndef ECS_UI_RAYLIB_DEMO_UI_ACTION_BUTTON_H
#define ECS_UI_RAYLIB_DEMO_UI_ACTION_BUTTON_H

#include "ecs_ui/ecs_ui.h"

typedef enum DemoUiActionButtonTone {
    DEMO_UI_ACTION_BUTTON_SUBTLE = 0,
    DEMO_UI_ACTION_BUTTON_PRIMARY = 1,
    DEMO_UI_ACTION_BUTTON_DANGER = 2,
} DemoUiActionButtonTone;

typedef struct DemoUiActionButtonDesc {
    const char *id;
    DemoUiActionButtonTone tone;
    ecs_entity_t on_click;
    bool disabled;
    ecs_entity_t style_token;
} DemoUiActionButtonDesc;

ecs_entity_t DemoUiActionButtonStyleToken(
    ecs_world_t *world,
    DemoUiActionButtonTone tone);
ecs_entity_t DemoUiBeginActionButton(
    EcsUiBuilder *builder,
    DemoUiActionButtonDesc desc);
bool DemoUiSetActionButtonTone(
    ecs_world_t *world,
    ecs_entity_t node,
    DemoUiActionButtonTone tone);
bool DemoUiSetActionButtonDisabled(
    ecs_world_t *world,
    ecs_entity_t node,
    bool disabled);

#define DemoUiActionButton(builder, ...) \
    EcsUiScope( \
        DemoUiBeginActionButton( \
            (builder), \
            (DemoUiActionButtonDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#endif
