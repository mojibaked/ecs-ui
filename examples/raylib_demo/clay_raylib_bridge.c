#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "clay_renderer_raylib.c"

Clay_Dimensions EcsUiClayRaylibMeasureText(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *user_data)
{
    return Raylib_MeasureText(text, config, user_data);
}
