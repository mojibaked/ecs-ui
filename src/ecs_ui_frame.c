#include "ecs_ui_frame_internal.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "ecs_ui_interaction.h"
#include "ecs_ui_paint_internal.h"
#include "ecs_ui_scroll_state.h"
#include "ecs_ui_solver.h"

#include <string.h>

typedef struct EcsUiFrameBackend {
    bool initialized;
    bool force_native_divergence;
    bool force_native_deep_divergence;
    EcsUiFrameBackendDesc desc;
    EcsUiSolverArena solver_arena;
    EcsUiInteractionFrame *active_frame;
    const EcsUiSolverScrollOffset *solver_scroll_offsets;
    uint32_t solver_scroll_offset_count;
    uint32_t solver_text_line_capacity;
    EcsUiSolverScrollContent *solver_scroll_contents;
    uint32_t solver_scroll_content_count;
    EcsUiSolverScrollContent native_scroll_contents[ECS_UI_TREE_NODE_MAX];
    EcsUiScrollUpdate native_scroll_reports[ECS_UI_TREE_NODE_MAX];
    uint32_t native_scroll_report_count;
    EcsUiPaintList paint_lists[2];
    uint32_t active_paint_list;
    uint32_t paint_item_capacity;
    uint32_t generation;
    bool current_paint_valid;
} EcsUiFrameBackend;

static EcsUiFrameBackend g_ecs_ui_frame_backend;

void EcsUiFrameInternalSetBackendDescForTest(
    const EcsUiFrameBackendDesc *desc)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (backend->initialized) {
        return;
    }
    backend->desc = desc != NULL ? *desc : (EcsUiFrameBackendDesc){0};
}

void EcsUiFrameInternalSetNativeDivergenceForTest(
    bool force_root,
    bool force_deep)
{
    g_ecs_ui_frame_backend.force_native_divergence = force_root;
    g_ecs_ui_frame_backend.force_native_deep_divergence = force_deep;
}

const EcsUiPaintList *EcsUiFrameInternalPaintList(void)
{
    const EcsUiPaintList *paint = &g_ecs_ui_frame_backend
        .paint_lists[g_ecs_ui_frame_backend.active_paint_list];
    return paint->generation != 0u ? paint : NULL;
}

const EcsUiPaintList *EcsUiFramePaintList(void)
{
    if (!g_ecs_ui_frame_backend.current_paint_valid) {
        return NULL;
    }
    return EcsUiFrameInternalPaintList();
}

void EcsUiFrameInternalSetPaintItemCapacity(uint32_t capacity)
{
    g_ecs_ui_frame_backend.paint_item_capacity = capacity;
}

void EcsUiFrameInternalSetTextMeasureLineCapacity(uint32_t capacity)
{
    g_ecs_ui_frame_backend.solver_text_line_capacity = capacity;
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
    backend->current_paint_valid = false;
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
    backend->current_paint_valid = true;
    return true;
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

    backend->desc = *desc;
    backend->solver_arena = (EcsUiSolverArena){0};
    backend->active_frame = NULL;

    const float surface_width =
        desc->surface_width > 0.0f ? desc->surface_width : 1.0f;
    const float surface_height =
        desc->surface_height > 0.0f ? desc->surface_height : 1.0f;
    backend->desc.surface_width = surface_width;
    backend->desc.surface_height = surface_height;

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
}

void EcsUiFrameBackendSetCullingEnabled(bool enabled)
{
    if (!g_ecs_ui_frame_backend.initialized) {
        return;
    }
    (void)enabled;
}

const EcsUiPaintList *EcsUiFrameRun(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options,
    const EcsUiPointerState *pointer_or_null,
    EcsUiInteractionFrame *frame_or_null)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    backend->current_paint_valid = false;
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
    backend->active_frame = NULL;
    char solver_message[256] = {0};
    EcsUiFrameErrorKind solver_error = ECS_UI_FRAME_ERROR_INTERNAL;
    const uint32_t native_content_count =
        tree->count < ECS_UI_TREE_NODE_MAX ? tree->count : ECS_UI_TREE_NODE_MAX;
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
                .text_line_capacity = backend->solver_text_line_capacity,
                .force_divergence = backend->force_native_divergence,
                .force_deep_divergence = backend->force_native_deep_divergence,
                .error_kind = &solver_error,
                .error_message = solver_message,
                .error_message_size = sizeof(solver_message),
            },
            &backend->solver_arena)) {
        EcsUiFrameReportError(
            &backend->desc,
            solver_error != ECS_UI_FRAME_ERROR_NONE ?
                solver_error :
                ECS_UI_FRAME_ERROR_INTERNAL,
            solver_message[0] != '\0' ?
                solver_message :
                "native layout solver failed");
        const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
        tree->generation = paint != NULL ? paint->generation : 0u;
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
    if (!EcsUiFramePaint(
            backend,
            tree,
            theme,
            options != NULL ? options->z_index : 0)) {
        return NULL;
    }
    (void)pointer_or_null;
    return EcsUiFramePaintList();
}

void EcsUiFrameSettleScroll(ecs_world_t *world, double dt)
{
    EcsUiFrameBackend *backend = &g_ecs_ui_frame_backend;
    if (!backend->initialized) {
        return;
    }
    (void)dt;
    (void)EcsUiFrameApplyPendingScrolls(
        world,
        backend->native_scroll_reports,
        backend->native_scroll_report_count);
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
