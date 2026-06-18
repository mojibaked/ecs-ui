#include "demo_app.h"
#include "demo_anim.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <raylib.h>

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(900, 560, "ecs-ui raylib demo");

    ecs_world_t *app_world = ecs_init();
    DemoAppRegister(app_world);
    (void)DemoAppItemRoot(app_world);

    ecs_world_t *ui_world = ecs_init();
    EcsUiImport(ui_world);
    DemoUiRegister(ui_world);
    DemoTerminalRegister(ui_world);
    DemoTextInputRegister(ui_world);
    DemoNavRegister(ui_world);
    DemoAnimRegister(ui_world);
    (void)DemoNavRoot(ui_world);
    ecs_entity_t root = DemoUiBuild(ui_world);
    EcsUiRaylibTheme theme = EcsUiRaylibThemeDefault();
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
            (void)EcsUiReadTree(ui_world, root, &tree);
        }

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
