#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#define CLAY_IMPLEMENTATION
#include "ecs_ui/ecs_ui_clay.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "ecs_ui_frame_internal.h"
#include "ecs_ui_solver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(
    ECS_UI_INTERACTION_TARGET_MAX == ECS_UI_CLAY_INTERACTION_TARGET_MAX,
    "neutral and clay interaction target capacities must match");

typedef struct EcsUiFrameBackend {
    bool initialized;
    void *arena_memory;
    size_t arena_size;
    EcsUiFrameBackendDesc desc;
    EcsUiDrawList draw_list;
    EcsUiSolverArena solver_arena;
    EcsUiClayInteractionState clay_state;
    EcsUiClayInteractionFrame clay_frame;
    EcsUiInteractionFrame *active_frame;
    EcsUiFrameInternalBackend selected_backend;
} EcsUiFrameBackend;

static EcsUiFrameBackend g_ecs_ui_frame_backend;

void EcsUiFrameInternalSelectBackend(EcsUiFrameInternalBackend backend)
{
    g_ecs_ui_frame_backend.selected_backend = backend;
}

EcsUiFrameInternalBackend EcsUiFrameInternalSelectedBackend(void)
{
    return g_ecs_ui_frame_backend.selected_backend;
}

static void EcsUiFrameReportError(
    const EcsUiFrameBackendDesc *desc,
    EcsUiFrameErrorKind kind,
    const char *message)
{
    if (desc == NULL || desc->error == NULL) {
        return;
    }
    desc->error(kind, message != NULL ? message : "", desc->error_user_data);
}

static EcsUiFrameErrorKind EcsUiFrameClayErrorKind(Clay_ErrorType type)
{
    switch (type) {
    case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED:
        return ECS_UI_FRAME_ERROR_MEASURE_TEXT_MISSING;
    case CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED:
        return ECS_UI_FRAME_ERROR_ARENA_CAPACITY;
    case CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED:
        return ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY;
    case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED:
        return ECS_UI_FRAME_ERROR_TEXT_MEASURE_CAPACITY;
    case CLAY_ERROR_TYPE_DUPLICATE_ID:
        return ECS_UI_FRAME_ERROR_DUPLICATE_ID;
    case CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND:
        return ECS_UI_FRAME_ERROR_FLOATING_PARENT_NOT_FOUND;
    case CLAY_ERROR_TYPE_PERCENTAGE_OVER_1:
        return ECS_UI_FRAME_ERROR_INVALID_PERCENT;
    case CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE:
        return ECS_UI_FRAME_ERROR_UNBALANCED_LAYOUT;
    case CLAY_ERROR_TYPE_INTERNAL_ERROR:
    default:
        return ECS_UI_FRAME_ERROR_INTERNAL;
    }
}

static void EcsUiFrameCopyClayString(
    char *out,
    size_t out_size,
    Clay_String value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    size_t length = 0u;
    if (value.length > 0) {
        length = (size_t)value.length;
    }
    if (length + 1u > out_size) {
        length = out_size - 1u;
    }
    if (length > 0u && value.chars != NULL) {
        memcpy(out, value.chars, length);
    }
    out[length] = '\0';
}

static void EcsUiFrameClayHandleError(Clay_ErrorData error_data)
{
    EcsUiFrameBackend *backend = error_data.userData;
    if (backend == NULL) {
        backend = &g_ecs_ui_frame_backend;
    }

    char message[512] = {0};
    EcsUiFrameCopyClayString(
        message,
        sizeof(message),
        error_data.errorText);
    EcsUiFrameReportError(
        &backend->desc,
        EcsUiFrameClayErrorKind(error_data.errorType),
        message);
}

static Clay_Dimensions EcsUiFrameClayMeasureText(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *user_data)
{
    EcsUiFrameBackend *backend = user_data;
    if (backend == NULL || backend->desc.measure_text == NULL) {
        return (Clay_Dimensions){0};
    }

    const EcsUiTextMeasureSpec spec = {
        .font_id = config != NULL ? config->fontId : 0u,
        .font_size = config != NULL ? (float)config->fontSize : 0.0f,
        .letter_spacing =
            config != NULL ? (float)config->letterSpacing : 1.0f,
        .line_height = config != NULL ? (float)config->lineHeight : 0.0f,
    };
    const EcsUiSize size = backend->desc.measure_text(
        text.chars,
        text.length,
        &spec,
        backend->desc.measure_user_data);
    return (Clay_Dimensions){
        .width = size.width,
        .height = size.height,
    };
}

static Clay_FloatingAttachPointType EcsUiFrameClayAttachPoint(
    EcsUiAlign x,
    EcsUiAlign y)
{
    switch (x) {
    case ECS_UI_ALIGN_CENTER:
        switch (y) {
        case ECS_UI_ALIGN_CENTER:
            return CLAY_ATTACH_POINT_CENTER_CENTER;
        case ECS_UI_ALIGN_END:
            return CLAY_ATTACH_POINT_CENTER_BOTTOM;
        case ECS_UI_ALIGN_START:
        default:
            return CLAY_ATTACH_POINT_CENTER_TOP;
        }
    case ECS_UI_ALIGN_END:
        switch (y) {
        case ECS_UI_ALIGN_CENTER:
            return CLAY_ATTACH_POINT_RIGHT_CENTER;
        case ECS_UI_ALIGN_END:
            return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
        case ECS_UI_ALIGN_START:
        default:
            return CLAY_ATTACH_POINT_RIGHT_TOP;
        }
    case ECS_UI_ALIGN_START:
    default:
        switch (y) {
        case ECS_UI_ALIGN_CENTER:
            return CLAY_ATTACH_POINT_LEFT_CENTER;
        case ECS_UI_ALIGN_END:
            return CLAY_ATTACH_POINT_LEFT_BOTTOM;
        case ECS_UI_ALIGN_START:
        default:
            return CLAY_ATTACH_POINT_LEFT_TOP;
        }
    }
}

EcsUiClayLayoutOptions EcsUiFrameInternalClayLayoutOptions(
    const EcsUiFrameLayoutOptions *options)
{
    if (options == NULL) {
        return (EcsUiClayLayoutOptions){0};
    }

    return (EcsUiClayLayoutOptions){
        .bounds = {
            .x = options->physical_bounds.x,
            .y = options->physical_bounds.y,
            .width = options->physical_bounds.width,
            .height = options->physical_bounds.height,
        },
        .attach_points = {
            .element = EcsUiFrameClayAttachPoint(
                options->attach_points.element_x,
                options->attach_points.element_y),
            .parent = EcsUiFrameClayAttachPoint(
                options->attach_points.parent_x,
                options->attach_points.parent_y),
        },
        .z_index = options->z_index,
        .capture_pointer = options->capture_pointer,
    };
}

static EcsUiClayPointerState EcsUiFrameClayPointer(
    EcsUiPointerState pointer)
{
    return (EcsUiClayPointerState){
        .x = pointer.x,
        .y = pointer.y,
        .time = pointer.time,
        .down = pointer.down,
        .pressed = pointer.pressed,
        .released = pointer.released,
        .secondary_down = pointer.secondary_down,
        .secondary_pressed = pointer.secondary_pressed,
        .secondary_released = pointer.secondary_released,
        .middle_down = pointer.middle_down,
        .middle_pressed = pointer.middle_pressed,
        .middle_released = pointer.middle_released,
        .scroll_x = pointer.scroll_x,
        .scroll_y = pointer.scroll_y,
    };
}

static EcsUiClayPointerCapture EcsUiFrameClayCapture(
    EcsUiPointerCapture capture)
{
    EcsUiClayPointerCapture out = {
        .active = capture.active,
        .tree = capture.tree,
        .node = capture.node,
        .action = capture.action,
        .payload = capture.payload,
        .scale = capture.scale,
        .start_x = capture.start_x,
        .start_y = capture.start_y,
        .physical_start_x = capture.physical_start_x,
        .physical_start_y = capture.physical_start_y,
        .start_time = capture.start_time,
        .button = capture.button,
    };
    (void)snprintf(out.node_id, sizeof(out.node_id), "%s", capture.node_id);
    return out;
}

static EcsUiPointerCapture EcsUiFramePublicCapture(
    EcsUiClayPointerCapture capture)
{
    EcsUiPointerCapture out = {
        .active = capture.active,
        .tree = capture.tree,
        .node = capture.node,
        .action = capture.action,
        .payload = capture.payload,
        .scale = capture.scale,
        .start_x = capture.start_x,
        .start_y = capture.start_y,
        .physical_start_x = capture.physical_start_x,
        .physical_start_y = capture.physical_start_y,
        .start_time = capture.start_time,
        .button = capture.button,
    };
    (void)snprintf(out.node_id, sizeof(out.node_id), "%s", capture.node_id);
    return out;
}

static EcsUiInteractionTarget EcsUiFramePublicTarget(
    const EcsUiClayInteractionTarget *target)
{
    if (target == NULL) {
        return (EcsUiInteractionTarget){0};
    }

    EcsUiInteractionTarget out = {
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .payload = target->payload,
        .tree_snapshot = target->tree_snapshot,
        .node_index = target->node_index,
        .emit_order = target->emit_order,
        .depth = target->depth,
        .scale = target->scale,
        .area = target->area,
        .pressable = target->pressable,
        .blocking = target->blocking,
        .scroll_container = target->scroll_container,
        .scroll_subscribed = target->scroll_subscribed,
        .scroll_axes = target->scroll_axes,
        .disabled = target->disabled,
        .inside = target->inside,
    };
    (void)snprintf(out.node_id, sizeof(out.node_id), "%s", target->node_id);
    return out;
}

static void EcsUiFrameCopyPublicFrame(
    EcsUiInteractionFrame *out,
    const EcsUiClayInteractionFrame *frame)
{
    if (out == NULL || frame == NULL) {
        return;
    }

    EcsUiInteractionState *state = out->state;
    memset(out, 0, sizeof(*out));
    out->state = state;
    if (out->state != NULL && frame->state != NULL) {
        out->state->capture = EcsUiFramePublicCapture(frame->state->capture);
    }

    const uint32_t target_count =
        frame->target_count < ECS_UI_INTERACTION_TARGET_MAX ?
            frame->target_count :
            ECS_UI_INTERACTION_TARGET_MAX;
    for (uint32_t i = 0u; i < target_count; i += 1u) {
        out->targets[i] = EcsUiFramePublicTarget(&frame->targets[i]);
    }

    out->target_count = target_count;
    out->inside_target_count = frame->inside_target_count;
    out->pressable_target_count = frame->pressable_target_count;
    out->resolved_tree = frame->resolved_tree;
    out->resolved_node = frame->resolved_node;
    out->resolved_action = frame->resolved_action;
    out->resolved_payload = frame->resolved_payload;
    (void)snprintf(
        out->resolved_node_id,
        sizeof(out->resolved_node_id),
        "%s",
        frame->resolved_node_id);
    out->resolved_pressable = frame->resolved_pressable;
    out->scroll_consumed = frame->scroll_consumed;
    out->truncated = frame->truncated ||
        frame->target_count > ECS_UI_INTERACTION_TARGET_MAX;
    out->capture_missing_target = frame->capture_missing_target;
    out->capture_missing_node = frame->capture_missing_node;
    out->capture_missing_action = frame->capture_missing_action;
    out->capture_missing_payload = frame->capture_missing_payload;
    (void)snprintf(
        out->capture_missing_node_id,
        sizeof(out->capture_missing_node_id),
        "%s",
        frame->capture_missing_node_id);
    out->capture_missed_release = frame->capture_missed_release;
    out->capture_missed_release_node = frame->capture_missed_release_node;
    out->capture_missed_release_action =
        frame->capture_missed_release_action;
    out->capture_missed_release_payload =
        frame->capture_missed_release_payload;
    (void)snprintf(
        out->capture_missed_release_node_id,
        sizeof(out->capture_missed_release_node_id),
        "%s",
        frame->capture_missed_release_node_id);
}

static void EcsUiFramePrimeClayState(EcsUiInteractionFrame *frame)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (frame == NULL || frame->state == NULL) {
        backend->clay_state = (EcsUiClayInteractionState){0};
        return;
    }
    backend->clay_state.capture =
        EcsUiFrameClayCapture(frame->state->capture);
}

bool EcsUiFrameBackendInit(const EcsUiFrameBackendDesc *desc)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (desc == NULL) {
        return false;
    }
    if (backend->initialized) {
        EcsUiFrameReportError(
            desc,
            ECS_UI_FRAME_ERROR_ALREADY_INITIALIZED,
            "ecs-ui frame backend is already initialized");
        return false;
    }
    if (desc->measure_text == NULL) {
        EcsUiFrameReportError(
            desc,
            ECS_UI_FRAME_ERROR_MEASURE_TEXT_MISSING,
            "ecs-ui frame backend requires a text measure callback");
        return false;
    }

    const uint32_t clay_memory_size = Clay_MinMemorySize();
    void *memory = malloc((size_t)clay_memory_size);
    if (memory == NULL) {
        EcsUiFrameReportError(
            desc,
            ECS_UI_FRAME_ERROR_ALLOCATION_FAILED,
            "failed to allocate frame backend arena");
        return false;
    }

    backend->arena_memory = memory;
    backend->arena_size = (size_t)clay_memory_size;
    backend->desc = *desc;
    backend->draw_list = (EcsUiDrawList){0};
    backend->solver_arena = (EcsUiSolverArena){0};
    backend->clay_state = (EcsUiClayInteractionState){0};
    backend->clay_frame = (EcsUiClayInteractionFrame){0};
    backend->active_frame = NULL;
    backend->selected_backend = ECS_UI_FRAME_INTERNAL_BACKEND_CLAY;

    const float surface_width =
        desc->surface_width > 0.0f ? desc->surface_width : 1.0f;
    const float surface_height =
        desc->surface_height > 0.0f ? desc->surface_height : 1.0f;
    backend->desc.surface_width = surface_width;
    backend->desc.surface_height = surface_height;

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(
        backend->arena_size,
        backend->arena_memory);
    Clay_Context *context = Clay_Initialize(
        arena,
        (Clay_Dimensions){
            .width = surface_width,
            .height = surface_height,
        },
        (Clay_ErrorHandler){
            .errorHandlerFunction = EcsUiFrameClayHandleError,
            .userData = backend,
        });
    if (context == NULL) {
        free(backend->arena_memory);
        *backend = (EcsUiFrameBackend){0};
        EcsUiFrameReportError(
            desc,
            ECS_UI_FRAME_ERROR_INTERNAL,
            "failed to initialize frame backend");
        return false;
    }

    Clay_SetMeasureTextFunction(EcsUiFrameClayMeasureText, backend);
    Clay_SetDebugModeEnabled(false);
    backend->initialized = true;
    return true;
}

void EcsUiFrameBackendShutdown(void)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (!backend->initialized) {
        return;
    }
    EcsUiSolverArenaRelease(&backend->solver_arena);
    free(backend->arena_memory);
    *backend = (EcsUiFrameBackend){0};
}

void EcsUiFrameBackendSetSurfaceSize(float width, float height)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (!backend->initialized) {
        return;
    }

    backend->desc.surface_width = width > 0.0f ? width : 1.0f;
    backend->desc.surface_height = height > 0.0f ? height : 1.0f;
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = backend->desc.surface_width,
        .height = backend->desc.surface_height,
    });
}

void EcsUiFrameBackendSetCullingEnabled(bool enabled)
{
    if (!g_ecs_ui_frame_backend.initialized) {
        return;
    }
    Clay_SetCullingEnabled(enabled);
}

const EcsUiDrawList *EcsUiFrameRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options,
    const EcsUiPointerState *pointer_or_null,
    EcsUiInteractionFrame *frame_or_null)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (!backend->initialized) {
        EcsUiFrameReportError(
            &backend->desc,
            ECS_UI_FRAME_ERROR_NOT_INITIALIZED,
            "ecs-ui frame backend is not initialized");
        return NULL;
    }
    if (tree == NULL || theme == NULL) {
        EcsUiFrameReportError(
            &backend->desc,
            ECS_UI_FRAME_ERROR_INVALID_ARGUMENT,
            "ecs-ui frame run requires a tree and theme");
        return NULL;
    }
    if (backend->selected_backend == ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE ||
            backend->selected_backend ==
                ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DIVERGE ||
            backend->selected_backend ==
                ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DEEP_DIVERGE) {
        backend->active_frame = NULL;
        backend->draw_list = (EcsUiDrawList){0};
        char solver_message[256] = {0};
        if (!EcsUiSolverRun(
                tree,
                &(EcsUiSolverRunOptions){
                    .layout = options,
                    .measure_text = backend->desc.measure_text,
                    .measure_user_data = backend->desc.measure_user_data,
                    .force_divergence =
                        backend->selected_backend ==
                            ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DIVERGE,
                    .force_deep_divergence =
                        backend->selected_backend ==
                            ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DEEP_DIVERGE,
                    .error_message = solver_message,
                    .error_message_size = sizeof(solver_message),
                },
                &backend->solver_arena)) {
            EcsUiFrameReportError(
                &backend->desc,
                ECS_UI_FRAME_ERROR_INTERNAL,
                solver_message[0] != '\0' ?
                    solver_message :
                    "native layout solver failed");
            return NULL;
        }
        (void)theme;
        (void)pointer_or_null;
        (void)frame_or_null;
        return &backend->draw_list;
    }

    EcsUiClayLayoutOptions clay_options =
        EcsUiFrameInternalClayLayoutOptions(options);
    EcsUiFramePrimeClayState(frame_or_null);
    if (frame_or_null != NULL) {
        EcsUiClayInteractionFrameBegin(
            &backend->clay_frame,
            frame_or_null->state != NULL ? &backend->clay_state : NULL);
        backend->active_frame = frame_or_null;
    } else {
        backend->clay_frame = (EcsUiClayInteractionFrame){0};
        backend->active_frame = NULL;
    }

    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = backend->desc.surface_width,
        .height = backend->desc.surface_height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(
        tree,
        theme,
        options != NULL ? &clay_options : NULL,
        frame_or_null != NULL ? &backend->clay_frame : NULL);
    backend->draw_list.commands = Clay_EndLayout();
    (void)EcsUiClayEnrichSnapshotLayout(
        tree,
        options != NULL ? &clay_options : NULL);

    if (pointer_or_null != NULL) {
        Clay_SetPointerState(
            (Clay_Vector2){
                .x = pointer_or_null->x,
                .y = pointer_or_null->y,
            },
            pointer_or_null->down);
    }
    if (frame_or_null != NULL) {
        EcsUiFrameCopyPublicFrame(frame_or_null, &backend->clay_frame);
    }

    return &backend->draw_list;
}

void EcsUiFrameSettleScroll(double dt)
{
    if (!g_ecs_ui_frame_backend.initialized) {
        return;
    }
    Clay_UpdateScrollContainers(
        false,
        (Clay_Vector2){.x = 0.0f, .y = 0.0f},
        (float)dt);
}

void EcsUiFrameInteractionStateInit(EcsUiInteractionState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void EcsUiFrameCollectEvents(
    EcsUiInteractionFrame *frame,
    EcsUiPointerState pointer,
    EcsUiEventList *events)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (events == NULL) {
        return;
    }
    EcsUiEventListClear(events);
    if (!backend->initialized || frame == NULL || frame->state == NULL) {
        return;
    }
    if (frame != backend->active_frame) {
        EcsUiFrameReportError(
            &backend->desc,
            ECS_UI_FRAME_ERROR_STALE_INTERACTION_FRAME,
            "interaction frame is stale; collect before the next frame run");
        return;
    }

    EcsUiClayCollectFrameEvents(
        &backend->clay_frame,
        EcsUiFrameClayPointer(pointer),
        events);
    EcsUiFrameCopyPublicFrame(frame, &backend->clay_frame);
}

bool EcsUiFrameApply(
    ecs_world_t *world,
    const EcsUiInteractionFrame *frame)
{
    return EcsUiApplyHoverState(
        world,
        frame != NULL ? frame->resolved_node : 0);
}

bool EcsUiFrameTreePointerInside(
    const EcsUiInteractionFrame *frame,
    ecs_entity_t tree)
{
    if (frame == NULL || tree == 0) {
        return false;
    }
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        const EcsUiInteractionTarget *target = &frame->targets[i];
        if (target->tree == tree && target->area && target->inside) {
            return true;
        }
    }
    return false;
}

const Clay_RenderCommandArray *EcsUiFrameDrawListClayCommands(
    const EcsUiDrawList *draw_list)
{
    return draw_list != NULL ? &draw_list->commands : NULL;
}
