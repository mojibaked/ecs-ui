#include "demo_theme.h"

#include <string.h>

ecs_entity_t DemoThemeTextInputFieldStyleToken(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    return ecs_entity(world, {
        .name = "DemoTextInputFieldStyle",
        .sep = "",
    });
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

    ecs_entity_t field_style = DemoThemeTextInputFieldStyleToken(world);
    ecs_entity_t dark = DemoThemeDark(world);
    ecs_entity_t light = DemoThemeLight(world);
    if (field_style == 0 || dark == 0 || light == 0) {
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
