#include "demo_app.h"
#include "demo_anim.h"
#include "demo_nav.h"
#include "demo_terminal.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"
#include "ecs_ui/ecs_ui_animation.h"
#include "ecs_ui/ecs_ui_raylib.h"

#include <raylib.h>

#define DEMO_NEXT_FRAME_NS 16666667u

typedef struct DemoMainRunCtx {
    ecs_world_t *app_world;
    ecs_world_t *ui_world;
    ecs_entity_t root;
    EcsUiRaylibDrawOptions draw_options;
    EcsUiFrameSignalAccumulator *frame_signals;
    EcsUiFrameSignalHandle tree_signal;
    EcsUiWakeRegistry *wake_registry;
    EcsUiWakeHandle animation_deadline;
    EcsUiTreeSnapshot tree;
    EcsUiEventList events;
    EcsUiTheme theme;
    Rectangle bounds;
    uint64_t tree_revision;
    Vector2 previous_mouse;
    bool has_previous_mouse;
    bool previous_primary_down;
    bool previous_secondary_down;
    bool drawing;
} DemoMainRunCtx;

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

static Rectangle DemoMainBounds(void)
{
    const float margin = 40.0f;
    return (Rectangle){
        .x = margin,
        .y = margin,
        .width = (float)GetScreenWidth() - (margin * 2.0f),
        .height = (float)GetScreenHeight() - (margin * 2.0f),
    };
}

static void DemoMainBumpRevision(DemoMainRunCtx *ctx)
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

static bool DemoMainHasNonHoverEvent(const EcsUiEventList *events)
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

static bool DemoMainRaylibInputChanged(DemoMainRunCtx *ctx)
{
    Vector2 mouse = GetMousePosition();
    const bool primary_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool secondary_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (!ctx->has_previous_mouse) {
        ctx->previous_mouse = mouse;
        ctx->previous_primary_down = primary_down;
        ctx->previous_secondary_down = secondary_down;
        ctx->has_previous_mouse = true;
        return false;
    }

    const bool changed =
        mouse.x != ctx->previous_mouse.x ||
        mouse.y != ctx->previous_mouse.y ||
        primary_down != ctx->previous_primary_down ||
        secondary_down != ctx->previous_secondary_down;
    ctx->previous_mouse = mouse;
    ctx->previous_primary_down = primary_down;
    ctx->previous_secondary_down = secondary_down;
    return changed;
}

static bool DemoMainPreInputPump(void *user_data)
{
    DemoMainRunCtx *ctx = (DemoMainRunCtx *)user_data;
    ctx->bounds = DemoMainBounds();
    ctx->events = (EcsUiEventList){0};
    if (IsWindowResized()) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_WINDOW_EVENT);
        DemoMainBumpRevision(ctx);
    }
    if (ctx->root != 0) {
        (void)EcsUiReadTree(ctx->ui_world, ctx->root, &ctx->tree);
        EcsUiRaylibCollectEvents(&ctx->tree, ctx->bounds, &ctx->events);
    }
    if (DemoMainRaylibInputChanged(ctx) ||
            DemoMainHasNonHoverEvent(&ctx->events)) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_INPUT);
        DemoMainBumpRevision(ctx);
    }
    return false;
}

static bool DemoMainTick(double dt, void *user_data)
{
    DemoMainRunCtx *ctx = (DemoMainRunCtx *)user_data;
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
        DemoMainBumpRevision(ctx);
    }
    if (ctx->root != 0) {
        (void)EcsUiReadTree(ctx->ui_world, ctx->root, &ctx->tree);
    }
    ctx->theme = DemoMainRaylibTheme(ctx->ui_world);

    const bool animation_active = EcsUiAnimationHasActive(ctx->ui_world);
    (void)EcsUiAnimationArmNextFrameDeadline(
        ctx->ui_world,
        ctx->wake_registry,
        ctx->animation_deadline,
        EcsUiRaylibNowNs() + DEMO_NEXT_FRAME_NS);
    if (animation_active) {
        (void)EcsUiFrameSignalMark(
            ctx->frame_signals,
            ECS_UI_FRAME_MARK_FORCE_RENDER);
    }
    return false;
}

static void DemoMainDrawCurrent(DemoMainRunCtx *ctx)
{
    BeginDrawing();
    ctx->drawing = true;
    ClearBackground(DemoMainClearColor(&ctx->theme));
    EcsUiRaylibDrawTreeEx(
        &ctx->tree,
        ctx->bounds,
        &ctx->theme,
        &ctx->draw_options);
}

static bool DemoMainRender(void *user_data)
{
    DemoMainDrawCurrent((DemoMainRunCtx *)user_data);
    return false;
}

static bool DemoMainPresent(void *user_data)
{
    DemoMainRunCtx *ctx = (DemoMainRunCtx *)user_data;
    if (!ctx->drawing) {
        DemoMainDrawCurrent(ctx);
    }
    EndDrawing();
    ctx->drawing = false;
    return false;
}

static bool DemoMainPark(void *user_data)
{
    DemoMainRunCtx *ctx = (DemoMainRunCtx *)user_data;
    DemoMainDrawCurrent(ctx);
    EndDrawing();
    ctx->drawing = false;
    return false;
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
    EcsUiFrameSignalAccumulator *frame_signals =
        EcsUiFrameSignalAccumulatorCreate();
    EcsUiWakeRegistry *wake_registry = EcsUiWakeRegistryCreate();
    DemoMainRunCtx run_ctx = {
        .app_world = app_world,
        .ui_world = ui_world,
        .root = root,
        .draw_options = {
            .custom_draw = DemoTerminalDrawCustom,
            .user_data = ui_world,
        },
        .frame_signals = frame_signals,
        .tree_signal = EcsUiFrameSignalRegisterStable(
            frame_signals,
            1u,
            "demo.tree",
            0u),
        .wake_registry = wake_registry,
        .animation_deadline =
            EcsUiWakeRegisterDeadline(wake_registry, "demo.animation"),
        .theme = DemoMainRaylibTheme(ui_world),
        .bounds = DemoMainBounds(),
    };

    (void)EcsUiRaylibRun(
        &(EcsUiRaylibRunConfig){
            .frame_signals = frame_signals,
            .wake_registry = wake_registry,
            .enable_event_waiting = true,
        },
        &(EcsUiRaylibRunCallbacks){
            .step = {
                .pre_input_pump = DemoMainPreInputPump,
                .tick = DemoMainTick,
                .render = DemoMainRender,
                .present = DemoMainPresent,
                .park = DemoMainPark,
                .ctx = &run_ctx,
            },
        },
        &(EcsUiRaylibRunResult){0});

    EcsUiWakeRegistryDestroy(wake_registry);
    EcsUiFrameSignalAccumulatorDestroy(frame_signals);
    ecs_fini(ui_world);
    ecs_fini(app_world);
    CloseWindow();
    return 0;
}
