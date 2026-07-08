#ifndef ECS_UI_FRAME_ENABLE_CLAY
#define ECS_UI_FRAME_ENABLE_CLAY 0
#endif

#if ECS_UI_FRAME_ENABLE_CLAY
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
#endif

#include "ecs_ui_frame_internal.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "ecs_ui_interaction.h"
#include "ecs_ui_paint_internal.h"
#include "ecs_ui_scroll_state.h"
#include "ecs_ui_solver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ECS_UI_FRAME_ENABLE_CLAY
_Static_assert(
    ECS_UI_INTERACTION_TARGET_MAX == ECS_UI_CLAY_INTERACTION_TARGET_MAX,
    "neutral and clay interaction target capacities must match");
#endif

typedef struct EcsUiFrameBackend {
    bool initialized;
#if ECS_UI_FRAME_ENABLE_CLAY
    void *arena_memory;
    size_t arena_size;
#endif
    EcsUiFrameBackendDesc desc;
    EcsUiDrawList draw_list;
    EcsUiSolverArena solver_arena;
#if ECS_UI_FRAME_ENABLE_CLAY
    EcsUiClayInteractionState clay_state;
    EcsUiClayInteractionFrame clay_frame;
#endif
    EcsUiInteractionFrame *active_frame;
    EcsUiFrameInternalBackend selected_backend;
    const EcsUiSolverScrollOffset *solver_scroll_offsets;
    uint32_t solver_scroll_offset_count;
    EcsUiSolverScrollContent *solver_scroll_contents;
    uint32_t solver_scroll_content_count;
    EcsUiSolverScrollContent native_scroll_contents[ECS_UI_TREE_NODE_MAX];
    EcsUiScrollUpdate native_scroll_reports[ECS_UI_TREE_NODE_MAX];
    uint32_t native_scroll_report_count;
    EcsUiPaintList paint_lists[2];
    uint32_t active_paint_list;
    uint32_t paint_item_capacity;
    uint32_t generation;
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

const EcsUiPaintList *EcsUiFrameInternalPaintList(void)
{
    return &g_ecs_ui_frame_backend
        .paint_lists[g_ecs_ui_frame_backend.active_paint_list];
}

void EcsUiFrameInternalSetPaintItemCapacity(uint32_t capacity)
{
    g_ecs_ui_frame_backend.paint_item_capacity = capacity;
}

void EcsUiFrameInternalSetNativeScrollOffsets(
    const EcsUiSolverScrollOffset *offsets,
    uint32_t count)
{
    g_ecs_ui_frame_backend.solver_scroll_offsets = offsets;
    g_ecs_ui_frame_backend.solver_scroll_offset_count =
        offsets != NULL ? count : 0u;
}

void EcsUiFrameInternalSetNativeScrollContentOutput(
    EcsUiSolverScrollContent *contents,
    uint32_t count)
{
    g_ecs_ui_frame_backend.solver_scroll_contents = contents;
    g_ecs_ui_frame_backend.solver_scroll_content_count =
        contents != NULL ? count : 0u;
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

#if ECS_UI_FRAME_ENABLE_CLAY
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
#endif

static bool EcsUiFrameApplyPendingScrolls(
    ecs_world_t *world,
    const EcsUiScrollUpdate *updates,
    uint32_t count)
{
    bool ok = true;
    for (uint32_t i = 0u; i < count; i += 1u) {
        ok = EcsUiScrollStateApplyUpdate(world, &updates[i]) && ok;
    }
    return ok;
}

static void EcsUiFrameCopyNativeScrollContents(
    EcsUiFrameBackend *backend,
    const EcsUiSolverScrollContent *contents,
    uint32_t count)
{
    if (backend == NULL || backend->solver_scroll_contents == NULL ||
            contents == NULL) {
        return;
    }
    const uint32_t copy_count =
        backend->solver_scroll_content_count < count ?
            backend->solver_scroll_content_count :
            count;
    for (uint32_t i = 0u; i < copy_count; i += 1u) {
        backend->solver_scroll_contents[i] = contents[i];
    }
}

static void EcsUiFrameStoreNativeScrollReports(
    EcsUiFrameBackend *backend,
    const EcsUiTreeSnapshot *tree,
    const EcsUiSolverScrollContent *contents,
    uint32_t count)
{
    if (backend == NULL || tree == NULL || contents == NULL) {
        return;
    }
    backend->native_scroll_report_count = 0u;
    const uint32_t read_count = tree->count < count ? tree->count : count;
    for (uint32_t i = 0u; i < read_count; i += 1u) {
        const EcsUiSolverScrollContent *content = &contents[i];
        if (!content->valid || content->node_index >= tree->count) {
            continue;
        }
        const EcsUiTreeNodeSnapshot *node =
            &tree->nodes[content->node_index];
        if (!node->has_scroll_view ||
                backend->native_scroll_report_count >= ECS_UI_TREE_NODE_MAX) {
            continue;
        }
        const EcsUiScrollState *scroll_state =
            node->has_scroll_state ? &node->scroll_state : NULL;
        backend->native_scroll_reports[backend->native_scroll_report_count] =
            (EcsUiScrollUpdate){
                .tree = tree->root,
                .node = node->entity,
                .node_index = content->node_index,
                .axes = node->scroll_view.axes,
                .offset_x = scroll_state != NULL ? scroll_state->offset_x : 0.0f,
                .offset_y = scroll_state != NULL ? scroll_state->offset_y : 0.0f,
                .content_w = content->width,
                .content_h = content->height,
                .viewport_w = node->layout_width,
                .viewport_h = node->layout_height,
            };
        backend->native_scroll_report_count += 1u;
    }
}

static bool EcsUiFramePaint(
    EcsUiFrameBackend *backend,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    int16_t base_z_index)
{
    if (backend == NULL || tree == NULL || theme == NULL) {
        return false;
    }
    uint32_t next_generation = backend->generation + 1u;
    if (next_generation == 0u) {
        next_generation += 1u;
    }
    const uint32_t previous_generation =
        backend->paint_lists[backend->active_paint_list].generation;
    const uint32_t previous_tree_generation = tree->generation;
    const uint32_t scratch_index =
        backend->active_paint_list == 0u ? 1u : 0u;
    const uint32_t item_capacity =
        backend->paint_item_capacity > 0u ?
            backend->paint_item_capacity :
            ECS_UI_PAINT_ITEM_MAX;

    tree->generation = next_generation;
    if (!EcsUiPaintListBuildWithCapacity(
            &backend->paint_lists[scratch_index],
            tree,
            theme,
            backend->desc.measure_text,
            backend->desc.measure_user_data,
            base_z_index,
            item_capacity)) {
        const EcsUiPaintList *scratch = &backend->paint_lists[scratch_index];
        const bool capacity_exceeded =
            scratch->count >= item_capacity ||
            scratch->count >= ECS_UI_PAINT_ITEM_MAX;
        tree->generation = previous_generation != 0u ?
            previous_generation :
            previous_tree_generation;
        EcsUiFrameReportError(
            &backend->desc,
            ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY,
            capacity_exceeded ?
                "ecs-ui paint list capacity exceeded" :
                "ecs-ui paint text-field scope requires replaying pressable "
                "child flow for non-value children");
        return false;
    }

    backend->generation = next_generation;
    backend->active_paint_list = scratch_index;
    return true;
}

#if ECS_UI_FRAME_ENABLE_CLAY
static void EcsUiFrameApplyClayScrollReports(
    ecs_world_t *world,
    const EcsUiClayInteractionFrame *frame)
{
    if (world == NULL || frame == NULL) {
        return;
    }
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        const EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (!target->scroll_container || target->node == 0) {
            continue;
        }
        const float scale = target->scale > 0.0f ? target->scale : 1.0f;
        Clay_ScrollContainerData data =
            Clay_GetScrollContainerData(target->clay_id);
        if (!data.found) {
            continue;
        }
        Clay_ElementData element_data = Clay_GetElementData(target->clay_id);
        const float container_width =
            data.scrollContainerDimensions.width > 0.0f ?
                data.scrollContainerDimensions.width :
                (element_data.found ? element_data.boundingBox.width : 0.0f);
        const float container_height =
            data.scrollContainerDimensions.height > 0.0f ?
                data.scrollContainerDimensions.height :
                (element_data.found ? element_data.boundingBox.height : 0.0f);
        const EcsUiScrollState *existing =
            ecs_get(world, target->node, EcsUiScrollState);
        EcsUiScrollUpdate update = {
            .tree = target->tree,
            .node = target->node,
            .node_index = target->node_index,
            .axes = target->scroll_axes,
            .offset_x = existing != NULL ? existing->offset_x : 0.0f,
            .offset_y = existing != NULL ? existing->offset_y : 0.0f,
            .content_w = data.contentDimensions.width / scale,
            .content_h = data.contentDimensions.height / scale,
            .viewport_w = container_width / scale,
            .viewport_h = container_height / scale,
        };
        (void)EcsUiScrollStateApplyUpdate(world, &update);
    }
}

static void EcsUiFrameStoreClayScrollReports(
    EcsUiFrameBackend *backend,
    const EcsUiClayInteractionFrame *frame)
{
    if (backend == NULL || frame == NULL) {
        return;
    }
    backend->native_scroll_report_count = 0u;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        const EcsUiClayInteractionTarget *target = &frame->targets[i];
        if (!target->scroll_container || target->node == 0 ||
                backend->native_scroll_report_count >= ECS_UI_TREE_NODE_MAX) {
            continue;
        }
        const float scale = target->scale > 0.0f ? target->scale : 1.0f;
        Clay_ScrollContainerData data =
            Clay_GetScrollContainerData(target->clay_id);
        if (!data.found) {
            continue;
        }
        Clay_ElementData element_data = Clay_GetElementData(target->clay_id);
        const float container_width =
            data.scrollContainerDimensions.width > 0.0f ?
                data.scrollContainerDimensions.width :
                (element_data.found ? element_data.boundingBox.width : 0.0f);
        const float container_height =
            data.scrollContainerDimensions.height > 0.0f ?
                data.scrollContainerDimensions.height :
                (element_data.found ? element_data.boundingBox.height : 0.0f);
        const EcsUiTreeNodeSnapshot *node =
            target->tree_snapshot != NULL &&
                    target->node_index < target->tree_snapshot->count ?
                &target->tree_snapshot->nodes[target->node_index] :
                NULL;
        const EcsUiScrollState *scroll_state =
            node != NULL && node->has_scroll_state ? &node->scroll_state : NULL;
        backend->native_scroll_reports[backend->native_scroll_report_count] =
            (EcsUiScrollUpdate){
                .tree = target->tree,
                .node = target->node,
                .node_index = target->node_index,
                .axes = target->scroll_axes,
                .offset_x =
                    scroll_state != NULL ? scroll_state->offset_x : 0.0f,
                .offset_y =
                    scroll_state != NULL ? scroll_state->offset_y : 0.0f,
                .content_w = data.contentDimensions.width / scale,
                .content_h = data.contentDimensions.height / scale,
                .viewport_w = container_width / scale,
                .viewport_h = container_height / scale,
            };
        backend->native_scroll_report_count += 1u;
    }
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
#endif

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

    backend->desc = *desc;
    backend->draw_list = (EcsUiDrawList){0};
    backend->solver_arena = (EcsUiSolverArena){0};
#if ECS_UI_FRAME_ENABLE_CLAY
    const uint32_t clay_memory_size = Clay_MinMemorySize();
    void *memory = malloc((size_t)clay_memory_size);
    if (memory == NULL) {
        EcsUiFrameReportError(
            desc,
            ECS_UI_FRAME_ERROR_ALLOCATION_FAILED,
            "failed to allocate frame backend arena");
        *backend = (EcsUiFrameBackend){0};
        return false;
    }

    backend->arena_memory = memory;
    backend->arena_size = (size_t)clay_memory_size;
    backend->clay_state = (EcsUiClayInteractionState){0};
    backend->clay_frame = (EcsUiClayInteractionFrame){0};
#endif
    backend->active_frame = NULL;
#if ECS_UI_FRAME_ENABLE_CLAY
    backend->selected_backend = ECS_UI_FRAME_INTERNAL_BACKEND_CLAY;
#else
    backend->selected_backend = ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE;
#endif

    const float surface_width =
        desc->surface_width > 0.0f ? desc->surface_width : 1.0f;
    const float surface_height =
        desc->surface_height > 0.0f ? desc->surface_height : 1.0f;
    backend->desc.surface_width = surface_width;
    backend->desc.surface_height = surface_height;

#if ECS_UI_FRAME_ENABLE_CLAY
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
#endif
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
#if ECS_UI_FRAME_ENABLE_CLAY
    free(backend->arena_memory);
#endif
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
#if ECS_UI_FRAME_ENABLE_CLAY
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = backend->desc.surface_width,
        .height = backend->desc.surface_height,
    });
#endif
}

void EcsUiFrameBackendSetCullingEnabled(bool enabled)
{
    if (!g_ecs_ui_frame_backend.initialized) {
        return;
    }
#if ECS_UI_FRAME_ENABLE_CLAY
    Clay_SetCullingEnabled(enabled);
#else
    (void)enabled;
#endif
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
    backend->native_scroll_report_count = 0u;
#if ECS_UI_FRAME_ENABLE_CLAY
    const bool use_native =
        backend->selected_backend == ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE ||
            backend->selected_backend ==
                ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DIVERGE ||
            backend->selected_backend ==
                ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DEEP_DIVERGE;
#else
    const bool use_native = true;
#endif
    if (use_native) {
        backend->active_frame = NULL;
        backend->draw_list = (EcsUiDrawList){0};
        char solver_message[256] = {0};
        const uint32_t native_content_count =
            tree->count < ECS_UI_TREE_NODE_MAX ?
                tree->count :
                ECS_UI_TREE_NODE_MAX;
        for (uint32_t i = 0u; i < native_content_count; i += 1u) {
            backend->native_scroll_contents[i] =
                (EcsUiSolverScrollContent){.node_index = i};
        }
        if (!EcsUiSolverRun(
                tree,
                &(EcsUiSolverRunOptions){
                    .layout = options,
                    .surface_width = backend->desc.surface_width,
                    .surface_height = backend->desc.surface_height,
                    .measure_text = backend->desc.measure_text,
                    .measure_user_data = backend->desc.measure_user_data,
                    .scroll_offsets = backend->solver_scroll_offsets,
                    .scroll_offset_count = backend->solver_scroll_offset_count,
                    .scroll_contents = backend->native_scroll_contents,
                    .scroll_content_count = native_content_count,
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
            tree->generation = EcsUiFrameInternalPaintList()->generation;
            return NULL;
        }
        EcsUiFrameCopyNativeScrollContents(
            backend,
            backend->native_scroll_contents,
            native_content_count);
        EcsUiFrameStoreNativeScrollReports(
            backend,
            tree,
            backend->native_scroll_contents,
            native_content_count);
        if (frame_or_null != NULL) {
            (void)EcsUiInteractionFrameBuild(
                frame_or_null,
                tree,
                &(EcsUiInteractionBuildOptions){
                    .layout = options,
                    .scroll_reports = backend->native_scroll_reports,
                    .scroll_report_count =
                        backend->native_scroll_report_count,
                });
            backend->active_frame = frame_or_null;
        }
        (void)EcsUiFramePaint(
            backend,
            tree,
            theme,
            options != NULL ? options->z_index : 0);
        (void)pointer_or_null;
        return &backend->draw_list;
    }

#if ECS_UI_FRAME_ENABLE_CLAY
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
    (void)EcsUiFramePaint(
        backend,
        tree,
        theme,
        options != NULL ? options->z_index : 0);

    if (pointer_or_null != NULL) {
        Clay_SetPointerState(
            (Clay_Vector2){
                .x = pointer_or_null->x,
                .y = pointer_or_null->y,
            },
            pointer_or_null->down);
    }
    EcsUiFrameStoreClayScrollReports(backend, &backend->clay_frame);
    if (frame_or_null != NULL) {
        (void)EcsUiInteractionFrameBuild(
            frame_or_null,
            tree,
            &(EcsUiInteractionBuildOptions){
                .layout = options,
                .scroll_reports = backend->native_scroll_reports,
                .scroll_report_count =
                    backend->native_scroll_report_count,
            });
        backend->active_frame = frame_or_null;
    }

    return &backend->draw_list;
#else
    EcsUiFrameReportError(
        &backend->desc,
        ECS_UI_FRAME_ERROR_INTERNAL,
        "requested clay frame backend is not available in this build");
    return NULL;
#endif
}

void EcsUiFrameSettleScroll(ecs_world_t *world, double dt)
{
    if (!g_ecs_ui_frame_backend.initialized) {
        return;
    }
#if ECS_UI_FRAME_ENABLE_CLAY
    Clay_UpdateScrollContainers(
        false,
        (Clay_Vector2){.x = 0.0f, .y = 0.0f},
        (float)dt);
    (void)EcsUiFrameApplyPendingScrolls(
        world,
        g_ecs_ui_frame_backend.clay_frame.pending_scrolls,
        g_ecs_ui_frame_backend.clay_frame.pending_scroll_count);
    EcsUiFrameApplyClayScrollReports(
        world,
        &g_ecs_ui_frame_backend.clay_frame);
#else
    (void)dt;
#endif
    (void)EcsUiFrameApplyPendingScrolls(
        world,
        g_ecs_ui_frame_backend.native_scroll_reports,
        g_ecs_ui_frame_backend.native_scroll_report_count);
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

    EcsUiInteractionCollectFrameEvents(frame, pointer, events);
}

bool EcsUiFrameApply(
    ecs_world_t *world,
    const EcsUiInteractionFrame *frame)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    bool ok = EcsUiApplyHoverState(
        world,
        frame != NULL ? frame->resolved_node : 0);
    if (frame != NULL) {
        ok = EcsUiFrameApplyPendingScrolls(
            world,
            frame->pending_scrolls,
            frame->pending_scroll_count) && ok;
    }
    if (frame != NULL && frame == backend->active_frame) {
#if ECS_UI_FRAME_ENABLE_CLAY
        EcsUiFrameApplyClayScrollReports(
            world,
            &backend->clay_frame);
#endif
    }
    if (world != NULL && backend->initialized) {
        const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
        ecs_singleton_set(
            world,
            EcsUiFrameArtifacts,
            {
                .paint = paint,
                .generation = paint != NULL ? paint->generation : 0u,
            });
    }
    return ok;
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

#if ECS_UI_FRAME_ENABLE_CLAY
const Clay_RenderCommandArray *EcsUiFrameDrawListClayCommands(
    const EcsUiDrawList *draw_list)
{
    return draw_list != NULL ? &draw_list->commands : NULL;
}
#endif
