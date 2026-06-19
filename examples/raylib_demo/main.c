#include "demo_app.h"
#include "demo_anim.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <raylib.h>

static EcsUiTheme DemoMainRaylibTheme(const ecs_world_t *ui_world)
{
    EcsUiTheme theme = EcsUiThemeDefault();
    if (!DemoThemeIsLight(ui_world)) {
        return theme;
    }

    theme.root_background = (EcsUiColor){239u, 244u, 242u, 255u};
    theme.surface = (EcsUiColor){251u, 253u, 252u, 255u};
    theme.surface_subtle = (EcsUiColor){232u, 240u, 238u, 255u};
    theme.button = (EcsUiColor){218u, 232u, 229u, 255u};
    theme.button_primary = (EcsUiColor){25u, 171u, 151u, 255u};
    theme.button_subtle = (EcsUiColor){207u, 221u, 219u, 255u};
    theme.button_danger = (EcsUiColor){218u, 82u, 62u, 255u};
    theme.button_disabled = (EcsUiColor){204u, 211u, 211u, 255u};
    theme.text = (EcsUiColor){20u, 31u, 34u, 255u};
    theme.text_muted = (EcsUiColor){80u, 99u, 102u, 255u};
    theme.text_inverse = (EcsUiColor){245u, 252u, 250u, 255u};
    return theme;
}

static Color DemoMainClearColor(const EcsUiTheme *theme)
{
    if (theme == NULL) {
        return BLACK;
    }
    return (Color){
        .r = theme->root_background.r,
        .g = theme->root_background.g,
        .b = theme->root_background.b,
        .a = theme->root_background.a,
    };
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

        EcsUiTheme theme = DemoMainRaylibTheme(ui_world);
        BeginDrawing();
        ClearBackground(DemoMainClearColor(&theme));
        EcsUiRaylibDrawTreeEx(&tree, bounds, &theme, &draw_options);
        EndDrawing();
    }

    ecs_fini(ui_world);
    ecs_fini(app_world);
    CloseWindow();
    return 0;
}
