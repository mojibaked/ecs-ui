#include "clay_raylib_bridge.h"
#include "demo_anim.h"
#include "demo_app.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_clay.h"

#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

static void DemoClayHandleErrors(Clay_ErrorData error_data)
{
    char message[256] = {0};
    const int32_t length = error_data.errorText.length < (int32_t)sizeof(message) - 1 ?
        error_data.errorText.length :
        (int32_t)sizeof(message) - 1;
    for (int32_t i = 0; i < length; i += 1) {
        message[i] = error_data.errorText.chars[i];
    }
    TraceLog(LOG_WARNING, "CLAY: %s", message);
}

static EcsUiClayTheme DemoClayTheme(const ecs_world_t *ui_world)
{
    EcsUiClayTheme theme = EcsUiClayThemeDefault();
    if (!DemoThemeIsLight(ui_world)) {
        return theme;
    }

    theme.root_background = (Clay_Color){239.0f, 244.0f, 242.0f, 255.0f};
    theme.surface = (Clay_Color){251.0f, 253.0f, 252.0f, 255.0f};
    theme.button = (Clay_Color){218.0f, 232.0f, 229.0f, 255.0f};
    theme.button_primary = (Clay_Color){25.0f, 171.0f, 151.0f, 255.0f};
    theme.button_subtle = (Clay_Color){207.0f, 221.0f, 219.0f, 255.0f};
    theme.button_danger = (Clay_Color){218.0f, 82.0f, 62.0f, 255.0f};
    theme.text = (Clay_Color){20.0f, 31.0f, 34.0f, 255.0f};
    theme.text_inverse = (Clay_Color){245.0f, 252.0f, 250.0f, 255.0f};
    return theme;
}

static Color DemoClayClearColor(const EcsUiClayTheme *theme)
{
    if (theme == NULL) {
        return BLACK;
    }
    return (Color){
        (unsigned char)theme->root_background.r,
        (unsigned char)theme->root_background.g,
        (unsigned char)theme->root_background.b,
        (unsigned char)theme->root_background.a,
    };
}

static void DemoClayPushKeyboardEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    uint32_t codepoint)
{
    if (events == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .codepoint = codepoint,
    };
    (void)EcsUiEventListPush(events, &event);
}

static void DemoClayPushKeyboardTextEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    const char *text)
{
    if (events == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
    };
    const char *value = text != NULL ? text : "";
    (void)snprintf(event.text, sizeof(event.text), "%s", value);
    (void)EcsUiEventListPush(events, &event);
}

static void DemoClayCollectKeyboardEvents(EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }

    const bool shortcut_down =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    for (int key = GetCharPressed(); key > 0; key = GetCharPressed()) {
        if (!shortcut_down) {
            DemoClayPushKeyboardEvent(
                events,
                ECS_UI_EVENT_TEXT_INPUT,
                (uint32_t)key);
        }
    }
    if (shortcut_down && IsKeyPressed(KEY_C)) {
        DemoClayPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_COPY, 0u);
    }
    if (shortcut_down && IsKeyPressed(KEY_X)) {
        DemoClayPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_CUT, 0u);
    }
    if (shortcut_down && IsKeyPressed(KEY_V)) {
        DemoClayPushKeyboardTextEvent(
            events,
            ECS_UI_EVENT_TEXT_PASTE,
            GetClipboardText());
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        DemoClayPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_DELETE, 0u);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        DemoClayPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_SUBMIT, 0u);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        DemoClayPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_CANCEL, 0u);
    }
    const bool shift_down =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_TAB)) {
        DemoClayPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_FOCUS_PREVIOUS :
                ECS_UI_EVENT_TEXT_FOCUS_NEXT,
            0u);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        DemoClayPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_LEFT :
                ECS_UI_EVENT_TEXT_CURSOR_LEFT,
            0u);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        DemoClayPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_RIGHT :
                ECS_UI_EVENT_TEXT_CURSOR_RIGHT,
            0u);
    }
    if (IsKeyPressed(KEY_HOME)) {
        DemoClayPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_START :
                ECS_UI_EVENT_TEXT_CURSOR_START,
            0u);
    }
    if (IsKeyPressed(KEY_END)) {
        DemoClayPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_END :
                ECS_UI_EVENT_TEXT_CURSOR_END,
            0u);
    }
}

static Clay_RenderCommandArray DemoClayEmitRenderCommands(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme)
{
    Clay_BeginLayout();
    EcsUiClayEmitTree(tree, theme);
    return Clay_EndLayout();
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(900, 560, "ecs-ui Clay raylib demo");

    uint64_t clay_memory_size = Clay_MinMemorySize();
    void *clay_memory = malloc(clay_memory_size);
    if (clay_memory == NULL) {
        CloseWindow();
        return 1;
    }

    Clay_Arena clay_arena =
        Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, clay_memory);
    Clay_Initialize(
        clay_arena,
        (Clay_Dimensions){
            .width = (float)GetScreenWidth(),
            .height = (float)GetScreenHeight(),
        },
        (Clay_ErrorHandler){
            .errorHandlerFunction = DemoClayHandleErrors,
        });

    Font fonts[1] = {0};
    fonts[0] = GetFontDefault();
    Clay_SetMeasureTextFunction(EcsUiClayRaylibMeasureText, fonts);

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

    while (!WindowShouldClose()) {
        EcsUiClayTheme theme = DemoClayTheme(ui_world);
        Clay_SetLayoutDimensions((Clay_Dimensions){
            .width = (float)GetScreenWidth(),
            .height = (float)GetScreenHeight(),
        });

        Vector2 mouse = GetMousePosition();
        EcsUiClayPointerState pointer = {
            .x = mouse.x,
            .y = mouse.y,
            .down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        };
        Clay_SetPointerState(
            (Clay_Vector2){pointer.x, pointer.y},
            pointer.down);
        Vector2 scroll = GetMouseWheelMoveV();
        Clay_UpdateScrollContainers(
            true,
            (Clay_Vector2){
                .x = scroll.x,
                .y = scroll.y,
            },
            GetFrameTime());

        EcsUiTreeSnapshot tree = {0};
        if (root != 0) {
            (void)EcsUiReadTree(ui_world, root, &tree);
        }

        (void)DemoClayEmitRenderCommands(&tree, &theme);

        EcsUiEventList events = {0};
        EcsUiClayCollectEvents(&tree, pointer, &events);
        DemoClayCollectKeyboardEvents(&events);
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
        if (root != 0) {
            (void)EcsUiReadTree(ui_world, root, &tree);
        }

        theme = DemoClayTheme(ui_world);
        Clay_RenderCommandArray render_commands =
            DemoClayEmitRenderCommands(&tree, &theme);

        BeginDrawing();
        ClearBackground(DemoClayClearColor(&theme));
        Clay_Raylib_Render(render_commands, fonts);
        EndDrawing();
    }

    ecs_fini(ui_world);
    ecs_fini(app_world);
    Clay_Raylib_Close();
    free(clay_memory);
    return 0;
}
