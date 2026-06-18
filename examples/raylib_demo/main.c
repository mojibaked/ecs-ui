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

    ecs_world_t *world = ecs_init();
    EcsUiImport(world);
    DemoAppRegister(world);
    DemoUiRegister(world);
    DemoTerminalRegister(world);
    DemoTextInputRegister(world);
    DemoNavRegister(world);
    DemoAnimRegister(world);
    (void)DemoAppItemRoot(world);
    (void)DemoNavRoot(world);
    ecs_entity_t root = DemoUiBuild(world);
    EcsUiRaylibTheme theme = EcsUiRaylibThemeDefault();
    EcsUiRaylibDrawOptions draw_options = {
        .custom_draw = DemoTerminalDrawCustom,
        .user_data = world,
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
            (void)EcsUiReadTree(world, root, &tree);
            EcsUiEventList events = {0};
            EcsUiRaylibCollectEvents(&tree, bounds, &events);
            DemoUiApplyEvents(world, &events);
            (void)ecs_progress(world, GetFrameTime());
            (void)EcsUiReadTree(world, root, &tree);
        }

        BeginDrawing();
        ClearBackground(theme.root_background);
        EcsUiRaylibDrawTreeEx(&tree, bounds, &theme, &draw_options);
        EndDrawing();
    }

    ecs_fini(world);
    CloseWindow();
    return 0;
}
