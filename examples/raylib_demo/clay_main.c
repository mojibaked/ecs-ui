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

static EcsUiTheme DemoClayTheme(const ecs_world_t *ui_world)
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

static Color DemoClayClearColor(const EcsUiTheme *theme)
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

static EcsUiClayLayoutOptions DemoClayLayoutOptions(void)
{
    const float margin = 40.0f;
    return (EcsUiClayLayoutOptions){
        .bounds = {
            .x = margin,
            .y = margin,
            .width = (float)GetScreenWidth() - (margin * 2.0f),
            .height = (float)GetScreenHeight() - (margin * 2.0f),
        },
    };
}

static Clay_RenderCommandArray DemoClayEmitRenderCommands(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiClayLayoutOptions *layout_options,
    EcsUiClayInteractionFrame *frame)
{
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(tree, theme, layout_options, frame);
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
    EcsUiClayInteractionState interaction_state = {0};
    EcsUiClayInteractionStateInit(&interaction_state);

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
        EcsUiTheme theme = DemoClayTheme(ui_world);
        EcsUiClayLayoutOptions layout_options = DemoClayLayoutOptions();
        Clay_SetLayoutDimensions((Clay_Dimensions){
            .width = (float)GetScreenWidth(),
            .height = (float)GetScreenHeight(),
        });

        Vector2 mouse = GetMousePosition();
        EcsUiClayPointerState pointer = {
            .x = mouse.x,
            .y = mouse.y,
            .time = GetTime(),
            .down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        };
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

        EcsUiClayInteractionFrame interaction_frame = {0};
        EcsUiClayInteractionFrameBegin(
            &interaction_frame,
            &interaction_state);
        (void)DemoClayEmitRenderCommands(
            &tree,
            &theme,
            &layout_options,
            &interaction_frame);
        Clay_SetPointerState(
            (Clay_Vector2){pointer.x, pointer.y},
            pointer.down);

        EcsUiEventList events = {0};
        EcsUiClayCollectFrameEvents(&interaction_frame, pointer, &events);
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
            DemoClayEmitRenderCommands(&tree, &theme, &layout_options, NULL);
        EcsUiClayRaylibRenderOptions render_options = {
            .custom_draw = DemoTerminalDrawCustom,
            .user_data = ui_world,
        };

        BeginDrawing();
        ClearBackground(DemoClayClearColor(&theme));
        EcsUiClayRaylibRenderEx(render_commands, fonts, &render_options);
        EndDrawing();
    }

    ecs_fini(ui_world);
    ecs_fini(app_world);
    Clay_Raylib_Close();
    free(clay_memory);
    return 0;
}
