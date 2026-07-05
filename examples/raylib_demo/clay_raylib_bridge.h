#ifndef ECS_UI_RAYLIB_DEMO_CLAY_RAYLIB_BRIDGE_H
#define ECS_UI_RAYLIB_DEMO_CLAY_RAYLIB_BRIDGE_H

#include <clay.h>
#include <raylib.h>

#include "ecs_ui/ecs_ui_raylib.h"

typedef struct EcsUiClayRaylibRenderOptions {
    EcsUiRaylibCustomDrawFn custom_draw;
    EcsUiRaylibCustomDrawFn icon_draw;
    EcsUiRaylibCustomDrawFn nine_slice_draw;
    void *user_data;
} EcsUiClayRaylibRenderOptions;

Clay_Dimensions EcsUiClayRaylibMeasureText(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *user_data);

void Clay_Raylib_Render(Clay_RenderCommandArray render_commands, Font *fonts);
void EcsUiClayRaylibRenderEx(
    Clay_RenderCommandArray render_commands,
    Font *fonts,
    const EcsUiClayRaylibRenderOptions *options);
void Clay_Raylib_Close(void);

#endif
