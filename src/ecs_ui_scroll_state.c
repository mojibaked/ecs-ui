#include "ecs_ui_scroll_state.h"

static float EcsUiScrollStateMaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static float EcsUiScrollStateClampFloat(
    float value,
    float min_value,
    float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

float EcsUiScrollStateClampOffset(
    float offset,
    float content,
    float viewport)
{
    const float max_scroll = EcsUiScrollStateMaxFloat(content - viewport, 0.0f);
    return EcsUiScrollStateClampFloat(offset, -max_scroll, 0.0f);
}

bool EcsUiScrollStateApplyUpdate(
    ecs_world_t *world,
    const EcsUiScrollUpdate *update)
{
    if (world == NULL || update == NULL || update->node == 0) {
        return false;
    }

    const EcsUiScrollState *existing =
        ecs_get(world, update->node, EcsUiScrollState);
    EcsUiScrollState state =
        existing != NULL ? *existing : (EcsUiScrollState){0};
    state.content_w = update->content_w;
    state.content_h = update->content_h;
    if ((update->axes & ECS_UI_SCROLL_AXIS_X) != 0u) {
        state.offset_x = EcsUiScrollStateClampOffset(
            update->offset_x,
            state.content_w,
            update->viewport_w);
    } else {
        state.offset_x = 0.0f;
    }
    if ((update->axes & ECS_UI_SCROLL_AXIS_Y) != 0u) {
        state.offset_y = EcsUiScrollStateClampOffset(
            update->offset_y,
            state.content_h,
            update->viewport_h);
    } else {
        state.offset_y = 0.0f;
    }
    ecs_set_ptr(world, update->node, EcsUiScrollState, &state);
    return true;
}
