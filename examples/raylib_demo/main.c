#include "demo_app.h"
#include "demo_anim.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <raylib.h>

static EcsUiRaylibTheme DemoMainRaylibTheme(const ecs_world_t *ui_world)
{
    EcsUiRaylibTheme theme = EcsUiRaylibThemeDefault();
    if (!DemoThemeIsLight(ui_world)) {
        return theme;
    }

    theme.root_background = (Color){239, 244, 242, 255};
    theme.surface = (Color){251, 253, 252, 255};
    theme.surface_subtle = (Color){232, 240, 238, 255};
    theme.button = (Color){218, 232, 229, 255};
    theme.button_primary = (Color){25, 171, 151, 255};
    theme.button_subtle = (Color){207, 221, 219, 255};
    theme.button_danger = (Color){218, 82, 62, 255};
    theme.button_disabled = (Color){204, 211, 211, 255};
    theme.text = (Color){20, 31, 34, 255};
    theme.text_muted = (Color){80, 99, 102, 255};
    theme.text_inverse = (Color){245, 252, 250, 255};
    return theme;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(900, 560, "ecs-ui raylib demo");

    ecs_world_t *app_world = ecs_init();
    DemoAppRegister(app_world);
    (void)DemoAppItemRoot(app_world);

    ecs_world_t *ui_world = ecs_init();
    EcsUiImport(ui_world);
    DemoThemeRegister(ui_world);
    DemoUiRegister(ui_world);
    DemoTerminalRegister(ui_world);
    DemoTextInputRegister(ui_world);
    DemoNavRegister(ui_world);
    DemoAnimRegister(ui_world);
    (void)DemoNavRoot(ui_world);
    ecs_entity_t root = DemoUiBuild(ui_world);
    EcsUiRaylibDrawOptions draw_options = {
        .custom_draw = DemoTerminalDrawCustom,
        .user_data = ui_world,
    };

    while (!WindowShouldClose()) {
        const float margin = 40.0f;
        Rectangle bounds = {
            .x = margin,
            .y = margin,
            .width = (float)GetScreenWidth() - (margin * 2.0f),
            .height = (float)GetScreenHeight() - (margin * 2.0f),
        };

        EcsUiTreeSnapshot tree = {0};
        if (root != 0) {
            /*
             * The renderer consumes immutable snapshots, not live ECS tables.
             * Each frame snapshots once for hit testing, translates raylib input
             * into app/UI ECS request entities, progresses app state, bridges
             * app item snapshots into the UI world, progresses UI systems, then
             * snapshots again so drawing reflects this frame's mutations.
             */
            (void)EcsUiReadTree(ui_world, root, &tree);
            EcsUiEventList events = {0};
            EcsUiRaylibCollectEvents(&tree, bounds, &events);
            const float dt = GetFrameTime();
            DemoUiApplyEvents(ui_world, app_world, &events);
            (void)ecs_progress(app_world, dt);
            DemoUiSyncProjection(ui_world, app_world);
            (void)ecs_progress(ui_world, dt);
            char clipboard_text[ECS_UI_TEXT_MAX] = {0};
            while (DemoTextInputPopClipboardWrite(
                ui_world,
                clipboard_text,
                sizeof(clipboard_text))) {
                SetClipboardText(clipboard_text);
            }
            (void)EcsUiReadTree(ui_world, root, &tree);
        }

        EcsUiRaylibTheme theme = DemoMainRaylibTheme(ui_world);
        BeginDrawing();
        ClearBackground(theme.root_background);
        EcsUiRaylibDrawTreeEx(&tree, bounds, &theme, &draw_options);
        EndDrawing();
    }

    ecs_fini(ui_world);
    ecs_fini(app_world);
    CloseWindow();
    return 0;
}
