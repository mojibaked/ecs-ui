#include "demo_terminal.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(DemoTerminalViewport);

static const char *DEMO_TERMINAL_CUSTOM_KIND = "demo-terminal";
static const char *DEMO_DRAG_HANDLE_CUSTOM_KIND = "demo-drag-handle";

static void DemoTerminalCopyString(
    char *out,
    size_t out_size,
    const char *value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *source = value != NULL ? value : "";
    size_t i = 0u;
    for (; i + 1u < out_size && source[i] != '\0'; i += 1u) {
        out[i] = source[i];
    }
    out[i] = '\0';
}

static Color DemoTerminalApplyOpacity(Color color, float opacity)
{
    float alpha = opacity;
    if (alpha < 0.0f) {
        alpha = 0.0f;
    }
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    color.a = (unsigned char)((float)color.a * alpha);
    return color;
}

static void DemoTerminalDrawLine(
    const char *line,
    Vector2 position,
    float opacity)
{
    DrawTextEx(
        GetFontDefault(),
        line != NULL ? line : "",
        position,
        14.0f,
        1.0f,
        DemoTerminalApplyOpacity((Color){172, 238, 211, 255}, opacity));
}

void DemoTerminalRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoTerminalViewport);
}

ecs_entity_t DemoTerminalBuildPreview(
    ecs_world_t *world,
    EcsUiBuilder *builder)
{
    if (world == NULL || builder == NULL) {
        return 0;
    }

    /*
     * Custom nodes reserve layout space in the ECS UI tree but leave the pixels
     * to the app. The renderer snapshots this custom kind and later calls
     * DemoTerminalDrawCustom with computed bounds and inherited opacity.
     */
    ecs_entity_t node = EcsUiAddCustom(
        builder,
        (EcsUiCustomDesc){
            .id = "TerminalViewport",
            .kind = DEMO_TERMINAL_CUSTOM_KIND,
            .preferred_height = 118.0f,
        });
    if (node == 0) {
        return 0;
    }

    DemoTerminalViewport terminal = {0};
    DemoTerminalCopyString(
        terminal.title,
        sizeof(terminal.title),
        "glowfish terminal");
    DemoTerminalCopyString(
        terminal.line_a,
        sizeof(terminal.line_a),
        "$ ecs-ui inspect --renderer raylib");
    DemoTerminalCopyString(
        terminal.line_b,
        sizeof(terminal.line_b),
        "layout: custom viewport reserved by ECS node");
    DemoTerminalCopyString(
        terminal.line_c,
        sizeof(terminal.line_c),
        "draw: app callback owns terminal-specific rendering");
    ecs_set_ptr(world, node, DemoTerminalViewport, &terminal);
    return node;
}

void DemoTerminalDrawCustom(
    const EcsUiTreeNodeSnapshot *node,
    Rectangle bounds,
    float opacity,
    void *user_data)
{
    if (node == NULL) {
        return;
    }

    /*
     * One callback handles multiple custom kinds. The drag handle is an
     * interactive gesture affordance with no app data, while the terminal
     * viewport reads ECS component data from the snapshot's entity.
     */
    if (strcmp(node->custom.kind, DEMO_DRAG_HANDLE_CUSTOM_KIND) == 0) {
        const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
        Color fill = hovered ?
            (Color){155, 184, 187, 255} :
            (Color){103, 128, 133, 255};
        fill = DemoTerminalApplyOpacity(fill, opacity);

        const float handle_width = 48.0f;
        const float handle_height = 5.0f;
        Rectangle handle = {
            .x = bounds.x + ((bounds.width - handle_width) * 0.5f),
            .y = bounds.y + ((bounds.height - handle_height) * 0.5f),
            .width = handle_width,
            .height = handle_height,
        };
        DrawRectangleRounded(handle, 1.0f, 8, fill);
        return;
    }

    ecs_world_t *world = user_data;
    if (world == NULL ||
        strcmp(node->custom.kind, DEMO_TERMINAL_CUSTOM_KIND) != 0) {
        return;
    }

    const DemoTerminalViewport *terminal =
        ecs_get(world, node->entity, DemoTerminalViewport);
    if (terminal == NULL) {
        return;
    }

    DrawRectangleRounded(
        bounds,
        0.10f,
        8,
        DemoTerminalApplyOpacity((Color){8, 14, 15, 255}, opacity));
    DrawRectangleRoundedLines(
        bounds,
        0.10f,
        8,
        DemoTerminalApplyOpacity((Color){49, 211, 186, 180}, opacity));

    Rectangle header = {
        .x = bounds.x,
        .y = bounds.y,
        .width = bounds.width,
        .height = 30.0f,
    };
    DrawRectangleRec(
        header,
        DemoTerminalApplyOpacity((Color){19, 31, 32, 230}, opacity));

    Vector2 title_position = {
        .x = bounds.x + 12.0f,
        .y = bounds.y + 8.0f,
    };
    DrawTextEx(
        GetFontDefault(),
        terminal->title,
        title_position,
        14.0f,
        1.0f,
        DemoTerminalApplyOpacity((Color){243, 247, 247, 255}, opacity));

    Vector2 line_position = {
        .x = bounds.x + 14.0f,
        .y = bounds.y + 40.0f,
    };
    DemoTerminalDrawLine(terminal->line_a, line_position, opacity);
    line_position.y += 20.0f;
    DemoTerminalDrawLine(terminal->line_b, line_position, opacity);
    line_position.y += 20.0f;
    DemoTerminalDrawLine(terminal->line_c, line_position, opacity);
}
