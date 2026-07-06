#ifndef ECS_UI_ECS_UI_RAYLIB_H
#define ECS_UI_ECS_UI_RAYLIB_H

#include <raylib.h>

#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_runner.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Raylib is a render bridge, so callback geometry is physical pixels. The core
 * tree snapshot remains logical; `scale` is the tree scale used by the bridge.
 * `physical_bounds` is the node's draw box, `physical_root_bounds` is the root
 * box passed to EcsUiRaylibDrawTreeEx, and `logical_origin` is that root origin
 * in window-origin logical units (`physical_root_bounds.xy / scale`).
 */
typedef struct EcsUiRaylibRenderContext {
    Rectangle physical_bounds;
    Rectangle physical_root_bounds;
    Vector2 logical_origin;
    float scale;
} EcsUiRaylibRenderContext;

typedef void (*EcsUiRaylibCustomDrawFn)(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data);

typedef struct EcsUiRaylibDrawOptions {
    EcsUiRaylibCustomDrawFn custom_draw;
    void *user_data;
    EcsUiRaylibCustomDrawFn icon_draw;
    EcsUiRaylibCustomDrawFn nine_slice_draw;
} EcsUiRaylibDrawOptions;

typedef enum EcsUiRaylibWakeReasonKind {
    ECS_UI_RAYLIB_WAKE_NONE = 0,
    ECS_UI_RAYLIB_WAKE_HOT = 1,
    ECS_UI_RAYLIB_WAKE_FD = 2,
    ECS_UI_RAYLIB_WAKE_POST = 3,
    ECS_UI_RAYLIB_WAKE_DEADLINE = 4,
    ECS_UI_RAYLIB_WAKE_CAPABILITY_DISABLED = 5,
    ECS_UI_RAYLIB_WAKE_ERROR = 6,
} EcsUiRaylibWakeReasonKind;

typedef struct EcsUiRaylibWakeReason {
    EcsUiRaylibWakeReasonKind kind;
    EcsUiWakeHandle handle;
    int fd;
    uint32_t fd_interests;
    char label[ECS_UI_ID_MAX];
} EcsUiRaylibWakeReason;

typedef struct EcsUiRaylibParkerCapabilities {
    bool post_empty_event_available;
    bool post_wake_enabled;
    bool fd_wake_enabled;
    bool deadline_wake_enabled;
    bool capability_disabled;
} EcsUiRaylibParkerCapabilities;

typedef void (*EcsUiRaylibPostEmptyEventFn)(void *ctx);

typedef struct EcsUiRaylibParkerDesc {
    EcsUiRaylibPostEmptyEventFn post_empty_event;
    void *post_empty_event_ctx;
} EcsUiRaylibParkerDesc;

typedef struct EcsUiRaylibParker EcsUiRaylibParker;

typedef bool (*EcsUiRaylibStepHook)(void *ctx);
typedef bool (*EcsUiRaylibStepTickHook)(double dt, void *ctx);

typedef struct EcsUiRaylibStepHooks {
    EcsUiRaylibStepHook pre_input_pump;
    EcsUiRaylibStepHook input_pump;
    EcsUiRaylibStepTickHook tick;
    EcsUiRaylibStepHook render;
    EcsUiRaylibStepHook post_blit_screenshot;
    /*
     * Called after render/screenshot when the frame is presented. Raylib users
     * normally put EndDrawing here so Step can arm park before the backend
     * reaches raylib's event-waiting point.
     */
    EcsUiRaylibStepHook present;
    EcsUiRaylibStepHook after_present_cleanup;
    /*
     * Called after a PARK frame arms the parker. Raylib users should enter the
     * backend wait here. With default raylib frame control this usually means a
     * BeginDrawing/EndDrawing pair, optionally redrawing cached/current content
     * before EndDrawing if the app does not have a presentation cache.
     */
    EcsUiRaylibStepHook park;
    void *ctx;
} EcsUiRaylibStepHooks;

typedef struct EcsUiRaylibStepDesc {
    EcsUiFrameSignalAccumulator *frame_signals;
    EcsUiWakeRegistry *wake_registry;
    EcsUiRaylibParker *parker;
    uint64_t now_ns;
    double dt;
    bool present_when_clean;
    const char *present_policy_label;
    EcsUiRaylibStepHooks hooks;
} EcsUiRaylibStepDesc;

typedef struct EcsUiRaylibStepCounters {
    uint64_t steps;
    uint64_t rendered;
    uint64_t skipped_render;
    uint64_t present_only;
    uint64_t classified_park;
    uint64_t park_armed;
    uint64_t continued;
    uint64_t immediate;
    uint64_t capability_disabled;
} EcsUiRaylibStepCounters;

typedef struct EcsUiRaylibStepState {
    EcsUiRaylibStepCounters counters;
} EcsUiRaylibStepState;

typedef struct EcsUiRaylibStepResult {
    EcsUiRaylibStepCounters counters;
    EcsUiRaylibWakeReason wake_reason;
    EcsUiFrameClassification frame_classification;
    EcsUiFrameReason frame_reason;
    bool rendered;
    bool presented;
    bool park_armed;
    bool immediate_next_step;
} EcsUiRaylibStepResult;

typedef bool (*EcsUiRaylibRunHook)(void *ctx);
typedef double (*EcsUiRaylibRunDeltaTimeFn)(void *ctx);
typedef uint64_t (*EcsUiRaylibRunNowNsFn)(void *ctx);

typedef struct EcsUiRaylibRunConfig {
    EcsUiFrameSignalAccumulator *frame_signals;
    EcsUiWakeRegistry *wake_registry;
    bool present_when_clean;
    const char *present_policy_label;
    /*
     * Run expects the caller to own InitWindow/CloseWindow. When true, Run
     * enables raylib event waiting for the duration of the loop and disables it
     * before returning.
     */
    bool enable_event_waiting;
} EcsUiRaylibRunConfig;

typedef struct EcsUiRaylibRunCallbacks {
    EcsUiRaylibStepHooks step;
    EcsUiRaylibRunHook should_quit;
    EcsUiRaylibRunHook window_should_close;
    EcsUiRaylibRunDeltaTimeFn delta_time;
    EcsUiRaylibRunNowNsFn now_ns;
} EcsUiRaylibRunCallbacks;

typedef struct EcsUiRaylibRunResult {
    EcsUiRaylibStepResult last_step;
    EcsUiRaylibStepCounters counters;
    EcsUiRaylibParkerCapabilities parker_capabilities;
    uint64_t steps;
    bool window_should_close;
    bool quit_requested;
} EcsUiRaylibRunResult;

/* `bounds` is the physical pixel root box for this render bridge. */
void EcsUiRaylibDrawTree(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme);
/* `bounds` is the physical pixel root box for this render bridge. */
void EcsUiRaylibDrawTreeEx(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options);
/*
 * Collect pointer/keyboard events for the direct raylib bridge. `bounds` and
 * raylib mouse input are physical pixels; emitted EcsUiEvent pointer coordinates
 * are window-origin logical units divided by tree->scale.
 */
void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events);

/*
 * Bounded wake shim for raylib's EnableEventWaiting/glfwWaitEvents path.
 * The loop thread builds an EcsUiWakeWaitSpec and arms the parker before the
 * raylib frame reaches its blocking wait. A watcher thread waits on the spec's
 * POSIX fds, a control pipe used by EcsUiRaylibParkerPost, and the nearest
 * deadline, then calls the configured post-empty-event callback. The default
 * creator wires that callback to glfwPostEmptyEvent when the raylib build
 * exposes GLFW; otherwise capability flags report fd/deadline wake as disabled.
 */
EcsUiRaylibParker *EcsUiRaylibParkerCreate(
    const EcsUiRaylibParkerDesc *desc);
EcsUiRaylibParker *EcsUiRaylibParkerCreateDefault(void);
void EcsUiRaylibParkerDestroy(EcsUiRaylibParker *parker);
EcsUiRaylibParkerCapabilities EcsUiRaylibParkerGetCapabilities(
    const EcsUiRaylibParker *parker);
bool EcsUiRaylibParkerArm(
    EcsUiRaylibParker *parker,
    const EcsUiWakeWaitSpec *spec);
bool EcsUiRaylibParkerPost(
    EcsUiRaylibParker *parker,
    const char *label);
uint64_t EcsUiRaylibParkerWakeSequence(const EcsUiRaylibParker *parker);
bool EcsUiRaylibParkerWaitForWake(
    EcsUiRaylibParker *parker,
    uint64_t after_sequence,
    uint64_t timeout_ns,
    EcsUiRaylibWakeReason *out);
bool EcsUiRaylibParkerLastWake(
    const EcsUiRaylibParker *parker,
    EcsUiRaylibWakeReason *out);

uint64_t EcsUiRaylibNowNs(void);
void EcsUiRaylibStepStateInit(EcsUiRaylibStepState *state);
bool EcsUiRaylibStep(
    EcsUiRaylibStepState *state,
    const EcsUiRaylibStepDesc *desc,
    EcsUiRaylibStepResult *out);
bool EcsUiRaylibRun(
    const EcsUiRaylibRunConfig *config,
    const EcsUiRaylibRunCallbacks *callbacks,
    EcsUiRaylibRunResult *out);

/*
 * Override the font used for all UI text. Pass a caller-owned font (e.g. a TTF
 * atlas) for crisp text at any size; the renderer does not take ownership and
 * the font must outlive its use. Defaults to raylib's bitmap GetFontDefault().
 */
void EcsUiRaylibSetFont(Font font);
/* Revert to GetFontDefault(). */
void EcsUiRaylibResetFont(void);

#ifdef __cplusplus
}
#endif

#endif
