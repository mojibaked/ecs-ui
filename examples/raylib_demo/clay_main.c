#include "clay_raylib_bridge.h"
#include "demo_anim.h"
#include "demo_app.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_animation.h"
#include "ecs_ui/ecs_ui_clay.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

#define DEMO_CLAY_NEXT_FRAME_NS 16666667u

typedef struct DemoClayRunCtx {
    ecs_world_t *app_world;
    ecs_world_t *ui_world;
    ecs_entity_t root;
    EcsUiClayInteractionState *interaction_state;
    Font *fonts;
    EcsUiFrameSignalAccumulator *frame_signals;
    EcsUiFrameSignalHandle tree_signal;
    EcsUiWakeRegistry *wake_registry;
    EcsUiWakeHandle animation_deadline;
    EcsUiTreeSnapshot tree;
    EcsUiTheme theme;
    EcsUiClayLayoutOptions layout_options;
    EcsUiClayInteractionFrame interaction_frame;
    Clay_RenderCommandArray render_commands;
    EcsUiEventList events;
    EcsUiClayRaylibRenderOptions render_options;
    uint64_t tree_revision;
    Vector2 previous_mouse;
    bool has_previous_mouse;
    bool previous_primary_down;
    bool previous_secondary_down;
    bool drawing;
} DemoClayRunCtx;

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

static void DemoClayBumpRevision(DemoClayRunCtx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->tree_revision += 1u;
    (void)EcsUiFrameSignalSetRevision(
        ctx->frame_signals,
        ctx->tree_signal,
        ctx->tree_revision);
}

static bool DemoClayHasNonHoverEvent(const EcsUiEventList *events)
{
    if (events == NULL || events->truncated) {
        return events != NULL && events->truncated;
    }
    for (uint32_t i = 0u; i < events->count; i += 1u) {
        if (events->events[i].type != ECS_UI_EVENT_HOVERED) {
            return true;
        }
    }
    return false;
}

static bool DemoClayRaylibInputChanged(
    DemoClayRunCtx *ctx,
    Vector2 mouse,
    Vector2 scroll)
{
    const bool primary_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool secondary_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (!ctx->has_previous_mouse) {
        ctx->previous_mouse = mouse;
        ctx->previous_primary_down = primary_down;
        ctx->previous_secondary_down = secondary_down;
        ctx->has_previous_mouse = true;
        return scroll.x != 0.0f || scroll.y != 0.0f;
    }

    const bool changed =
        mouse.x != ctx->previous_mouse.x ||
        mouse.y != ctx->previous_mouse.y ||
        primary_down != ctx->previous_primary_down ||
        secondary_down != ctx->previous_secondary_down ||
        scroll.x != 0.0f ||
        scroll.y != 0.0f;
    ctx->previous_mouse = mouse;
    ctx->previous_primary_down = primary_down;
    ctx->previous_secondary_down = secondary_down;
    return changed;
}

static void DemoClayBuildRenderOptions(DemoClayRunCtx *ctx)
{
    const float render_scale = ctx->tree.scale > 0.0f ? ctx->tree.scale : 1.0f;
    ctx->render_options = (EcsUiClayRaylibRenderOptions){
        .custom_draw = DemoTerminalDrawCustom,
        .physical_root_bounds = {
            .x = ctx->layout_options.bounds.x,
            .y = ctx->layout_options.bounds.y,
            .width = ctx->layout_options.bounds.width,
            .height = ctx->layout_options.bounds.height,
        },
        .logical_origin = {
            .x = ctx->layout_options.bounds.x / render_scale,
            .y = ctx->layout_options.bounds.y / render_scale,
        },
        .scale = render_scale,
        .user_data = ctx->ui_world,
    };
}

static bool DemoClayPreInputPump(void *user_data)
{
    DemoClayRunCtx *ctx = (DemoClayRunCtx *)user_data;
    ctx->theme = DemoClayTheme(ctx->ui_world);
    ctx->layout_options = DemoClayLayoutOptions();
    ctx->events = (EcsUiEventList){0};
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = (float)GetScreenWidth(),
        .height = (float)GetScreenHeight(),
    });
    if (IsWindowResized()) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_WINDOW_EVENT);
        DemoClayBumpRevision(ctx);
    }

    Vector2 mouse = GetMousePosition();
    EcsUiClayPointerState pointer = {
        .x = mouse.x,
        .y = mouse.y,
        .time = GetTime(),
        .down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        .secondary_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
        .secondary_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        .secondary_released = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT),
    };
    Vector2 scroll = GetMouseWheelMoveV();
    Clay_UpdateScrollContainers(
        true,
        (Clay_Vector2){
            .x = scroll.x,
            .y = scroll.y,
        },
        GetFrameTime());

    if (ctx->root != 0) {
        (void)EcsUiReadTree(ctx->ui_world, ctx->root, &ctx->tree);
    } else {
        ctx->tree = (EcsUiTreeSnapshot){0};
    }
    EcsUiClayInteractionFrameBegin(
        &ctx->interaction_frame,
        ctx->interaction_state);
    (void)DemoClayEmitRenderCommands(
        &ctx->tree,
        &ctx->theme,
        &ctx->layout_options,
        &ctx->interaction_frame);
    Clay_SetPointerState(
        (Clay_Vector2){pointer.x, pointer.y},
        pointer.down);

    EcsUiClayCollectFrameEvents(
        &ctx->interaction_frame,
        pointer,
        &ctx->events);
    DemoClayCollectKeyboardEvents(&ctx->events);
    if (DemoClayRaylibInputChanged(ctx, mouse, scroll) ||
            DemoClayHasNonHoverEvent(&ctx->events)) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_INPUT);
        DemoClayBumpRevision(ctx);
    }
    return false;
}

static bool DemoClayTick(double dt, void *user_data)
{
    DemoClayRunCtx *ctx = (DemoClayRunCtx *)user_data;
    DemoUiApplyEvents(ctx->ui_world, ctx->app_world, &ctx->events);
    (void)ecs_progress(ctx->app_world, (float)dt);
    DemoUiSyncProjection(ctx->ui_world, ctx->app_world);
    (void)ecs_progress(ctx->ui_world, (float)dt);

    char clipboard_text[ECS_UI_TEXT_MAX] = {0};
    while (DemoTextInputPopClipboardWrite(
        ctx->ui_world,
        clipboard_text,
        sizeof(clipboard_text))) {
        SetClipboardText(clipboard_text);
        DemoClayBumpRevision(ctx);
    }
    if (ctx->root != 0) {
        (void)EcsUiReadTree(ctx->ui_world, ctx->root, &ctx->tree);
    }

    ctx->theme = DemoClayTheme(ctx->ui_world);
    ctx->render_commands =
        DemoClayEmitRenderCommands(
            &ctx->tree,
            &ctx->theme,
            &ctx->layout_options,
            NULL);
    DemoClayBuildRenderOptions(ctx);

    const bool animation_active = EcsUiAnimationHasActive(ctx->ui_world);
    (void)EcsUiAnimationArmNextFrameDeadline(
        ctx->ui_world,
        ctx->wake_registry,
        ctx->animation_deadline,
        EcsUiRaylibNowNs() + DEMO_CLAY_NEXT_FRAME_NS);
    if (animation_active) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_FORCE_RENDER);
    }
    return false;
}

static void DemoClayDrawCurrent(DemoClayRunCtx *ctx)
{
    BeginDrawing();
    ctx->drawing = true;
    ClearBackground(DemoClayClearColor(&ctx->theme));
    EcsUiClayRaylibRenderEx(
        ctx->render_commands,
        ctx->fonts,
        &ctx->render_options);
}

static bool DemoClayRender(void *user_data)
{
    DemoClayDrawCurrent((DemoClayRunCtx *)user_data);
    return false;
}

static bool DemoClayPresent(void *user_data)
{
    DemoClayRunCtx *ctx = (DemoClayRunCtx *)user_data;
    if (!ctx->drawing) {
        DemoClayDrawCurrent(ctx);
    }
    EndDrawing();
    ctx->drawing = false;
    return false;
}

static bool DemoClayPark(void *user_data)
{
    DemoClayRunCtx *ctx = (DemoClayRunCtx *)user_data;
    DemoClayDrawCurrent(ctx);
    EndDrawing();
    ctx->drawing = false;
    return false;
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
    EcsUiFrameSignalAccumulator *frame_signals =
        EcsUiFrameSignalAccumulatorCreate();
    EcsUiWakeRegistry *wake_registry = EcsUiWakeRegistryCreate();
    DemoClayRunCtx run_ctx = {
        .app_world = app_world,
        .ui_world = ui_world,
        .root = root,
        .interaction_state = &interaction_state,
        .fonts = fonts,
        .frame_signals = frame_signals,
        .tree_signal = EcsUiFrameSignalRegisterStable(
            frame_signals,
            1u,
            "demo.clay.tree",
            0u),
        .wake_registry = wake_registry,
        .animation_deadline =
            EcsUiWakeRegisterDeadline(wake_registry, "demo.clay.animation"),
        .theme = DemoClayTheme(ui_world),
        .layout_options = DemoClayLayoutOptions(),
    };

    (void)EcsUiRaylibRun(
        &(EcsUiRaylibRunConfig){
            .frame_signals = frame_signals,
            .wake_registry = wake_registry,
            .enable_event_waiting = true,
        },
        &(EcsUiRaylibRunCallbacks){
            .step = {
                .pre_input_pump = DemoClayPreInputPump,
                .tick = DemoClayTick,
                .render = DemoClayRender,
                .present = DemoClayPresent,
                .park = DemoClayPark,
                .ctx = &run_ctx,
            },
        },
        &(EcsUiRaylibRunResult){0});

    EcsUiWakeRegistryDestroy(wake_registry);
    EcsUiFrameSignalAccumulatorDestroy(frame_signals);
    ecs_fini(ui_world);
    ecs_fini(app_world);
    Clay_Raylib_Close();
    free(clay_memory);
    return 0;
}
