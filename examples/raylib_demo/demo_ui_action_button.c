#include "demo_ui_action_button.h"

#include "demo_theme.h"

ecs_entity_t DemoUiActionButtonStyleToken(
    ecs_world_t *world,
    DemoUiActionButtonTone tone)
{
    switch (tone) {
    case DEMO_UI_ACTION_BUTTON_PRIMARY:
        return DemoThemePrimaryActionStyleToken(world);
    case DEMO_UI_ACTION_BUTTON_DANGER:
        return DemoThemeDangerActionStyleToken(world);
    case DEMO_UI_ACTION_BUTTON_SUBTLE:
    default:
        return DemoThemeSubtleActionStyleToken(world);
    }
}

ecs_entity_t DemoUiBeginActionButton(
    EcsUiBuilder *builder,
    DemoUiActionButtonDesc desc)
{
    ecs_entity_t style_token = desc.style_token;
    if (style_token == 0 && builder != NULL && builder->world != NULL) {
        style_token = DemoUiActionButtonStyleToken(builder->world, desc.tone);
    }

    return EcsUiBeginPressable(
        builder,
        (EcsUiPressableDesc){
            .id = desc.id,
            .on_click = desc.on_click,
            .disabled = desc.disabled,
            .style_token = style_token,
        });
}

bool DemoUiSetActionButtonTone(
    ecs_world_t *world,
    ecs_entity_t node,
    DemoUiActionButtonTone tone)
{
    if (world == NULL || node == 0 ||
        !ecs_has(world, node, EcsUiPressable)) {
        return false;
    }

    ecs_entity_t style_token = DemoUiActionButtonStyleToken(world, tone);
    if (style_token == 0 ||
        ecs_get_target(world, node, EcsUiUsesStyle, 0) == style_token) {
        return false;
    }
    return EcsUiSetStyleToken(world, node, style_token);
}

bool DemoUiSetActionButtonDisabled(
    ecs_world_t *world,
    ecs_entity_t node,
    bool disabled)
{
    if (world == NULL || node == 0) {
        return false;
    }

    EcsUiPressable *pressable = ecs_get_mut(world, node, EcsUiPressable);
    if (pressable == NULL || pressable->disabled == disabled) {
        return false;
    }

    pressable->disabled = disabled;
    ecs_modified(world, node, EcsUiPressable);
    return true;
}
