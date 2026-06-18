#ifndef ECS_UI_RAYLIB_DEMO_CLAY_RAYLIB_BRIDGE_H
#define ECS_UI_RAYLIB_DEMO_CLAY_RAYLIB_BRIDGE_H

#include <clay.h>
#include <raylib.h>

Clay_Dimensions EcsUiClayRaylibMeasureText(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *user_data);

void Clay_Raylib_Render(Clay_RenderCommandArray render_commands, Font *fonts);
void Clay_Raylib_Close(void);

#endif
