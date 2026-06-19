#include "demo_theme.h"

#include <string.h>

ecs_entity_t DemoThemeTextFieldStyleToken(ecs_world_t *world)
{
    return EcsUiStyleToken(world, "TextField");
}

ecs_entity_t DemoThemeTextInputFieldStyleToken(ecs_world_t *world)
{
    return DemoThemeTextFieldStyleToken(world);
}

ecs_entity_t DemoThemePrimaryActionStyleToken(ecs_world_t *world)
{
    return EcsUiStyleToken(world, "PrimaryAction");
}

ecs_entity_t DemoThemeSubtleActionStyleToken(ecs_world_t *world)
{
    return EcsUiStyleToken(world, "SubtleAction");
}

ecs_entity_t DemoThemeDangerActionStyleToken(ecs_world_t *world)
{
    return EcsUiStyleToken(world, "DangerAction");
}

static ecs_entity_t DemoThemeDark(ecs_world_t *world)
{
    return EcsUiThemeEntity(world, "DemoDarkTheme");
}

static ecs_entity_t DemoThemeLight(ecs_world_t *world)
{
    return EcsUiThemeEntity(world, "DemoLightTheme");
}

static bool DemoThemeInstallStyles(ecs_world_t *world)
{
    if (world == NULL) {
        return false;
    }

    ecs_entity_t field_style = DemoThemeTextFieldStyleToken(world);
    ecs_entity_t primary_action_style =
        DemoThemePrimaryActionStyleToken(world);
    ecs_entity_t subtle_action_style =
        DemoThemeSubtleActionStyleToken(world);
    ecs_entity_t danger_action_style =
        DemoThemeDangerActionStyleToken(world);
    ecs_entity_t dark = DemoThemeDark(world);
    ecs_entity_t light = DemoThemeLight(world);
    if (field_style == 0 || primary_action_style == 0 ||
        subtle_action_style == 0 || danger_action_style == 0 ||
        dark == 0 || light == 0) {
        return false;
    }

    bool ok = EcsUiThemeSetBoxStyle(
        world,
        dark,
        field_style,
        (EcsUiBoxStyle){
            .background = {35u, 52u, 56u, 255u},
            .hover_background = {42u, 68u, 72u, 255u},
            .disabled_background = {60u, 68u, 72u, 255u},
            .highlight_background = {49u, 211u, 186u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        });
    ok = EcsUiThemeSetBoxStyle(
        world,
        light,
        field_style,
        (EcsUiBoxStyle){
            .background = {236u, 245u, 243u, 255u},
            .hover_background = {224u, 238u, 235u, 255u},
            .disabled_background = {205u, 216u, 215u, 255u},
            .highlight_background = {49u, 211u, 186u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    ok = EcsUiThemeSetBoxStyle(
        world,
        dark,
        primary_action_style,
        (EcsUiBoxStyle){
            .background = {49u, 211u, 186u, 255u},
            .hover_background = {42u, 185u, 164u, 255u},
            .disabled_background = {70u, 78u, 82u, 255u},
            .highlight_background = {245u, 252u, 250u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    ok = EcsUiThemeSetBoxStyle(
        world,
        light,
        primary_action_style,
        (EcsUiBoxStyle){
            .background = {25u, 171u, 151u, 255u},
            .hover_background = {22u, 150u, 133u, 255u},
            .disabled_background = {204u, 211u, 211u, 255u},
            .highlight_background = {245u, 252u, 250u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    ok = EcsUiThemeSetBoxStyle(
        world,
        dark,
        subtle_action_style,
        (EcsUiBoxStyle){
            .background = {88u, 111u, 116u, 255u},
            .hover_background = {78u, 99u, 104u, 255u},
            .disabled_background = {70u, 78u, 82u, 255u},
            .highlight_background = {245u, 252u, 250u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    ok = EcsUiThemeSetBoxStyle(
        world,
        light,
        subtle_action_style,
        (EcsUiBoxStyle){
            .background = {207u, 221u, 219u, 255u},
            .hover_background = {194u, 211u, 208u, 255u},
            .disabled_background = {204u, 211u, 211u, 255u},
            .highlight_background = {20u, 31u, 34u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    ok = EcsUiThemeSetBoxStyle(
        world,
        dark,
        danger_action_style,
        (EcsUiBoxStyle){
            .background = {255u, 125u, 95u, 255u},
            .hover_background = {230u, 103u, 78u, 255u},
            .disabled_background = {70u, 78u, 82u, 255u},
            .highlight_background = {245u, 252u, 250u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    ok = EcsUiThemeSetBoxStyle(
        world,
        light,
        danger_action_style,
        (EcsUiBoxStyle){
            .background = {218u, 82u, 62u, 255u},
            .hover_background = {194u, 72u, 55u, 255u},
            .disabled_background = {204u, 211u, 211u, 255u},
            .highlight_background = {245u, 252u, 250u, 255u},
            .radius = 0.08f,
            .padding = 12.0f,
        }) && ok;
    return ok;
}

void DemoThemeRegister(ecs_world_t *world)
{
    if (world == NULL || !DemoThemeInstallStyles(world)) {
        return;
    }

    if (EcsUiGetActiveTheme(world) == 0) {
        (void)EcsUiSetActiveTheme(world, DemoThemeDark(world));
    }
    (void)EcsUiThemeApply(world);
}

void DemoThemeToggle(ecs_world_t *world)
{
    if (world == NULL || !DemoThemeInstallStyles(world)) {
        return;
    }

    ecs_entity_t next =
        DemoThemeIsLight(world) ? DemoThemeDark(world) : DemoThemeLight(world);
    if (next != 0 && EcsUiSetActiveTheme(world, next)) {
        (void)EcsUiThemeApply(world);
    }
}

bool DemoThemeIsLight(const ecs_world_t *world)
{
    if (world == NULL) {
        return false;
    }
    ecs_entity_t active = EcsUiGetActiveTheme(world);
    const char *name = active != 0 ? ecs_get_name(world, active) : NULL;
    return name != NULL && strcmp(name, "DemoLightTheme") == 0;
}

const char *DemoThemeName(const ecs_world_t *world)
{
    return DemoThemeIsLight(world) ? "light mode" : "dark mode";
}
