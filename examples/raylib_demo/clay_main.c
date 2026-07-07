#include "demo_anim.h"
#include "demo_app.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_animation.h"
#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <raylib.h>
#include <stdio.h>

#define DEMO_CLAY_NEXT_FRAME_NS 16666667u

typedef struct DemoClayRunCtx {
    ecs_world_t *app_world;
    ecs_world_t *ui_world;
    ecs_entity_t root;
    EcsUiInteractionState *interaction_state;
    Font *fonts;
    EcsUiFrameSignalAccumulator *frame_signals;
    EcsUiFrameSignalHandle tree_signal;
    EcsUiWakeRegistry *wake_registry;
    EcsUiWakeHandle animation_deadline;
    EcsUiTreeSnapshot tree;
    EcsUiTheme theme;
    EcsUiFrameLayoutOptions layout_options;
    EcsUiInteractionFrame interaction_frame;
    const EcsUiDrawList *draw_list;
    EcsUiEventList events;
    EcsUiRaylibRenderContext render_context;
    EcsUiRaylibDrawOptions render_options;
    uint64_t tree_revision;
    Vector2 previous_mouse;
    bool has_previous_mouse;
    bool previous_primary_down;
    bool previous_secondary_down;
    bool drawing;
} DemoClayRunCtx;

static void DemoClayHandleErrors(
    EcsUiFrameErrorKind kind,
    const char *message,
    void *user_data)
{
    (void)user_data;
    TraceLog(LOG_WARNING, "FRAME[%d]: %s", (int)kind, message);
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

static EcsUiFrameLayoutOptions DemoClayLayoutOptions(void)
{
    const float margin = 40.0f;
    return (EcsUiFrameLayoutOptions){
        .physical_bounds = {
            .x = margin,
            .y = margin,
            .width = (float)GetScreenWidth() - (margin * 2.0f),
            .height = (float)GetScreenHeight() - (margin * 2.0f),
        },
    };
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
    ctx->render_context = (EcsUiRaylibRenderContext){
        .physical_root_bounds = {
            .x = ctx->layout_options.physical_bounds.x,
            .y = ctx->layout_options.physical_bounds.y,
            .width = ctx->layout_options.physical_bounds.width,
            .height = ctx->layout_options.physical_bounds.height,
        },
        .logical_origin = {
            .x = ctx->layout_options.physical_bounds.x / render_scale,
            .y = ctx->layout_options.physical_bounds.y / render_scale,
        },
        .scale = render_scale,
    };
    ctx->render_options = (EcsUiRaylibDrawOptions){
        .custom_draw = DemoTerminalDrawCustom,
        .user_data = ctx->ui_world,
    };
}

static bool DemoClayPreInputPump(void *user_data)
{
    DemoClayRunCtx *ctx = (DemoClayRunCtx *)user_data;
    ctx->theme = DemoClayTheme(ctx->ui_world);
    ctx->layout_options = DemoClayLayoutOptions();
    ctx->events = (EcsUiEventList){0};
    EcsUiFrameBackendSetSurfaceSize(
        (float)GetScreenWidth(),
        (float)GetScreenHeight());
    if (IsWindowResized()) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_WINDOW_EVENT);
        DemoClayBumpRevision(ctx);
    }

    Vector2 mouse = GetMousePosition();
    Vector2 scroll = GetMouseWheelMoveV();
    EcsUiPointerState pointer = {
        .x = mouse.x,
        .y = mouse.y,
        .time = GetTime(),
        .down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        .secondary_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
        .secondary_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        .secondary_released = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT),
        .middle_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE),
        .middle_pressed = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE),
        .middle_released = IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE),
        .scroll_x = scroll.x,
        .scroll_y = scroll.y,
    };

    if (ctx->root != 0) {
        (void)EcsUiReadTree(ctx->ui_world, ctx->root, &ctx->tree);
    } else {
        ctx->tree = (EcsUiTreeSnapshot){0};
    }
    ctx->interaction_frame = (EcsUiInteractionFrame){
        .state = ctx->interaction_state,
    };
    (void)EcsUiFrameRun(
        &ctx->tree,
        &ctx->theme,
        &ctx->layout_options,
        &pointer,
        &ctx->interaction_frame
    );
    EcsUiFrameCollectEvents(
        &ctx->interaction_frame,
        pointer,
        &ctx->events);
    EcsUiFrameSettleScroll(GetFrameTime());
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
    ctx->draw_list = EcsUiFrameRun(
        &ctx->tree,
        &ctx->theme,
        &ctx->layout_options,
        NULL,
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
    EcsUiRaylibRenderDrawList(
        ctx->draw_list,
        ctx->fonts,
        &ctx->render_context,
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

    Font fonts[1] = {0};
    fonts[0] = GetFontDefault();
    if (!EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = (float)GetScreenWidth(),
                .surface_height = (float)GetScreenHeight(),
                .measure_text = EcsUiRaylibMeasureText,
                .measure_user_data = fonts,
                .error = DemoClayHandleErrors,
            })) {
        CloseWindow();
        return 1;
    }
    EcsUiInteractionState interaction_state = {0};
    EcsUiFrameInteractionStateInit(&interaction_state);

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
    EcsUiRaylibReleaseDrawListRenderer();
    EcsUiFrameBackendShutdown();
    return 0;
}
