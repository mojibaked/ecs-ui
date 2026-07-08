#include "ecs_ui_interaction.h"
#include "ecs_ui_scroll_state.h"
#include "ecs_ui_style.h"

#include <stdio.h>
#include <string.h>

typedef struct EcsUiInteractionRect {
    float x;
    float y;
    float width;
    float height;
} EcsUiInteractionRect;

typedef struct EcsUiInteractionClip {
    bool enabled;
    EcsUiInteractionRect rect;
} EcsUiInteractionClip;

typedef struct EcsUiInteractionBuildContext {
    EcsUiInteractionFrame *frame;
    const EcsUiTreeSnapshot *tree;
    const EcsUiFrameLayoutOptions *layout;
    const EcsUiScrollUpdate *scroll_reports;
    uint32_t scroll_report_count;
    int16_t base_z_index;
    int16_t root_z_index;
    uint32_t root_order;
    uint32_t root_next_order;
    uint32_t root_item_order;
    EcsUiInteractionClip active_clip;
    float root_origin_x;
    float root_origin_y;
} EcsUiInteractionBuildContext;

typedef struct EcsUiInteractionRootState {
    int16_t root_z_index;
    uint32_t root_order;
    uint32_t root_item_order;
    EcsUiInteractionClip active_clip;
} EcsUiInteractionRootState;

static float EcsUiInteractionScale(const EcsUiTreeSnapshot *tree)
{
    return tree != NULL && tree->scale > 0.0f ? tree->scale : 1.0f;
}

static int16_t EcsUiInteractionZIndex(int16_t base, int relative)
{
    const int value = (int)base + relative;
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static bool EcsUiInteractionHasVisualOffset(
    const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->kind != ECS_UI_NODE_ROOT &&
        (node->visual.offset_x < -0.01f || node->visual.offset_x > 0.01f ||
            node->visual.offset_y < -0.01f || node->visual.offset_y > 0.01f);
}

static EcsUiInteractionRootState EcsUiInteractionSaveRoot(
    const EcsUiInteractionBuildContext *ctx)
{
    return (EcsUiInteractionRootState){
        .root_z_index = ctx != NULL ? ctx->root_z_index : 0,
        .root_order = ctx != NULL ? ctx->root_order : 0u,
        .root_item_order = ctx != NULL ? ctx->root_item_order : 0u,
        .active_clip = ctx != NULL ? ctx->active_clip : (EcsUiInteractionClip){0},
    };
}

static void EcsUiInteractionRestoreRoot(
    EcsUiInteractionBuildContext *ctx,
    EcsUiInteractionRootState state)
{
    if (ctx == NULL) {
        return;
    }
    ctx->root_z_index = state.root_z_index;
    ctx->root_order = state.root_order;
    ctx->root_item_order = state.root_item_order;
    ctx->active_clip = state.active_clip;
}

static void EcsUiInteractionBeginRoot(
    EcsUiInteractionBuildContext *ctx,
    int16_t z_index,
    EcsUiInteractionClip active_clip)
{
    if (ctx == NULL) {
        return;
    }
    ctx->root_z_index = z_index;
    ctx->root_order = ctx->root_next_order;
    ctx->root_next_order += 1u;
    ctx->root_item_order = 0u;
    ctx->active_clip = active_clip;
}

static EcsUiInteractionRect EcsUiInteractionNodePhysicalRect(
    const EcsUiInteractionBuildContext *ctx,
    const EcsUiTreeNodeSnapshot *node)
{
    const float scale = EcsUiInteractionScale(ctx != NULL ? ctx->tree : NULL);
    return node != NULL ?
        (EcsUiInteractionRect){
            .x = (node->layout_x * scale) +
                (ctx != NULL ? ctx->root_origin_x : 0.0f),
            .y = (node->layout_y * scale) +
                (ctx != NULL ? ctx->root_origin_y : 0.0f),
            .width = node->layout_width * scale,
            .height = node->layout_height * scale,
        } :
        (EcsUiInteractionRect){0};
}

static bool EcsUiInteractionNodeCapturesSelf(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->hit_test.mode == ECS_UI_HIT_TEST_NONE ||
        node->hit_test.mode == ECS_UI_HIT_TEST_CHILDREN) {
        return false;
    }
    if (node->hit_test.mode == ECS_UI_HIT_TEST_CAPTURE) {
        return true;
    }

    switch (node->kind) {
    case ECS_UI_NODE_BUTTON:
        return !node->button.disabled;
    case ECS_UI_NODE_PRESSABLE:
        return !node->pressable.disabled;
    case ECS_UI_NODE_CUSTOM:
        return node->on_click != 0;
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return false;
    }
}

static bool EcsUiInteractionNodeIsPressableTarget(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->hit_test.mode == ECS_UI_HIT_TEST_NONE ||
        node->hit_test.mode == ECS_UI_HIT_TEST_CHILDREN) {
        return false;
    }

    switch (node->kind) {
    case ECS_UI_NODE_BUTTON:
        return !node->button.disabled;
    case ECS_UI_NODE_PRESSABLE:
        return !node->pressable.disabled;
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        return node->on_click != 0;
    case ECS_UI_NODE_CUSTOM:
        return node->on_click != 0;
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return false;
    }
}

static bool EcsUiInteractionNodeIsDisabledTarget(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return false;
    }
    if (node->kind == ECS_UI_NODE_BUTTON) {
        return node->button.disabled;
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE) {
        return node->pressable.disabled;
    }
    return false;
}

static bool EcsUiInteractionNodeIsBlockingTarget(
    const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL && node->hit_test.mode == ECS_UI_HIT_TEST_CAPTURE;
}

static const EcsUiScrollUpdate *EcsUiInteractionFindScrollReport(
    const EcsUiInteractionBuildContext *ctx,
    uint32_t node_index,
    ecs_entity_t node)
{
    if (ctx == NULL || ctx->scroll_reports == NULL) {
        return NULL;
    }
    for (uint32_t i = 0u; i < ctx->scroll_report_count; i += 1u) {
        const EcsUiScrollUpdate *report = &ctx->scroll_reports[i];
        if (report->node == node || report->node_index == node_index) {
            return report;
        }
    }
    return NULL;
}

static void EcsUiInteractionFallbackScrollContentVisit(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const EcsUiTreeNodeSnapshot *container,
    float *out_width,
    float *out_height)
{
    if (tree == NULL || index >= tree->count || container == NULL ||
            out_width == NULL || out_height == NULL) {
        return;
    }
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (node->has_layout) {
        const float right =
            (node->layout_x - container->layout_x) + node->layout_width;
        const float bottom =
            (node->layout_y - container->layout_y) + node->layout_height;
        if (right > *out_width) {
            *out_width = right;
        }
        if (bottom > *out_height) {
            *out_height = bottom;
        }
    }
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        EcsUiInteractionFallbackScrollContentVisit(
            tree,
            child,
            container,
            out_width,
            out_height);
    }
}

static void EcsUiInteractionScrollContent(
    const EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    float *out_width,
    float *out_height)
{
    if (out_width != NULL) {
        *out_width = 0.0f;
    }
    if (out_height != NULL) {
        *out_height = 0.0f;
    }
    if (ctx == NULL || ctx->tree == NULL || index >= ctx->tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    const EcsUiScrollUpdate *report = EcsUiInteractionFindScrollReport(
        ctx,
        index,
        node->entity);
    if (report != NULL) {
        if (out_width != NULL) {
            *out_width = report->content_w;
        }
        if (out_height != NULL) {
            *out_height = report->content_h;
        }
        return;
    }

    if (node->has_scroll_state &&
            (node->scroll_state.content_w > 0.0f ||
             node->scroll_state.content_h > 0.0f)) {
        if (out_width != NULL) {
            *out_width = node->scroll_state.content_w;
        }
        if (out_height != NULL) {
            *out_height = node->scroll_state.content_h;
        }
        return;
    }

    float width = 0.0f;
    float height = 0.0f;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < ctx->tree->count;
         child = ctx->tree->nodes[child].next_sibling) {
        EcsUiInteractionFallbackScrollContentVisit(
            ctx->tree,
            child,
            node,
            &width,
            &height);
    }
    if (out_width != NULL) {
        *out_width = width;
    }
    if (out_height != NULL) {
        *out_height = height;
    }
}

static void EcsUiInteractionRegisterTarget(
    EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    bool area,
    bool pressable,
    bool blocking,
    bool scroll_container,
    bool scroll_subscribed,
    uint32_t scroll_axes,
    EcsUiInteractionRect rect)
{
    if (ctx == NULL || ctx->frame == NULL || ctx->tree == NULL ||
            index >= ctx->tree->count) {
        return;
    }
    if (ctx->frame->target_count >= ECS_UI_INTERACTION_TARGET_MAX) {
        ctx->frame->truncated = true;
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    const float scale = EcsUiInteractionScale(ctx->tree);
    EcsUiInteractionTarget *target =
        &ctx->frame->targets[ctx->frame->target_count];
    ctx->frame->target_count += 1u;
    *target = (EcsUiInteractionTarget){
        .tree = ctx->tree->root,
        .node = node->entity,
        .action = node->on_click,
        .payload = node->payload,
        .tree_snapshot = ctx->tree,
        .node_index = index,
        .emit_order = ctx->root_item_order + 1u,
        .depth = node->depth,
        .scale = scale,
        .area = area,
        .pressable = pressable,
        .blocking = blocking,
        .scroll_container = scroll_container,
        .scroll_subscribed = scroll_subscribed,
        .scroll_axes = scroll_axes,
        .disabled = EcsUiInteractionNodeIsDisabledTarget(node),
        .physical_x = rect.x,
        .physical_y = rect.y,
        .physical_width = rect.width,
        .physical_height = rect.height,
        .clip_enabled = ctx->active_clip.enabled,
        .clip_x = ctx->active_clip.rect.x,
        .clip_y = ctx->active_clip.rect.y,
        .clip_width = ctx->active_clip.rect.width,
        .clip_height = ctx->active_clip.rect.height,
        .z_index = ctx->root_z_index,
        .root_order = ctx->root_order,
        .order = ctx->root_item_order,
    };
    ctx->root_item_order += 1u;
    if (scroll_container) {
        float content_w = 0.0f;
        float content_h = 0.0f;
        EcsUiInteractionScrollContent(ctx, index, &content_w, &content_h);
        target->scroll_content_width = content_w * scale;
        target->scroll_content_height = content_h * scale;
        target->scroll_viewport_width = rect.width;
        target->scroll_viewport_height = rect.height;
    }
    (void)snprintf(target->node_id, sizeof(target->node_id), "%s", node->id);
}

static void EcsUiInteractionRegisterNodeTarget(
    EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    EcsUiInteractionRect rect,
    bool force_blocking)
{
    if (ctx == NULL || ctx->tree == NULL || index >= ctx->tree->count) {
        return;
    }
    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    const bool area = index == 0u;
    const bool pressable = EcsUiInteractionNodeIsPressableTarget(node);
    const bool blocking =
        force_blocking || EcsUiInteractionNodeIsBlockingTarget(node);
    const bool scroll_container = node->has_scroll_view;
    const bool scroll_subscribed = node->scroll_subscribed;
    const uint32_t scroll_axes =
        node->has_scroll_view ? node->scroll_view.axes : ECS_UI_SCROLL_AXIS_NONE;
    if (!area && !pressable && !blocking && !scroll_container &&
            !scroll_subscribed) {
        return;
    }
    EcsUiInteractionRegisterTarget(
        ctx,
        index,
        area,
        pressable,
        blocking,
        scroll_container,
        scroll_subscribed,
        scroll_axes,
        rect);
}

static void EcsUiInteractionRegisterWrapperBlocker(
    EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    EcsUiInteractionRect rect)
{
    if (ctx == NULL || ctx->tree == NULL || index >= ctx->tree->count) {
        return;
    }
    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    if (!EcsUiInteractionNodeCapturesSelf(node)) {
        return;
    }
    EcsUiInteractionRegisterTarget(
        ctx,
        index,
        false,
        false,
        true,
        false,
        false,
        ECS_UI_SCROLL_AXIS_NONE,
        rect);
}

static int EcsUiInteractionCompareTargets(const void *lhs, const void *rhs)
{
    const EcsUiInteractionTarget *a = (const EcsUiInteractionTarget *)lhs;
    const EcsUiInteractionTarget *b = (const EcsUiInteractionTarget *)rhs;
    if (a->z_index != b->z_index) {
        return a->z_index < b->z_index ? -1 : 1;
    }
    if (a->root_order != b->root_order) {
        return a->root_order < b->root_order ? -1 : 1;
    }
    if (a->order != b->order) {
        return a->order < b->order ? -1 : 1;
    }
    if (a->depth != b->depth) {
        return a->depth < b->depth ? -1 : 1;
    }
    return 0;
}

static void EcsUiInteractionSortTargets(EcsUiInteractionFrame *frame)
{
    if (frame == NULL || frame->target_count < 2u) {
        return;
    }
    qsort(
        frame->targets,
        frame->target_count,
        sizeof(frame->targets[0]),
        EcsUiInteractionCompareTargets);
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        frame->targets[i].emit_order = i + 1u;
    }
}

static bool EcsUiInteractionEmitNode(
    EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    float parent_opacity,
    bool allow_visual_offset_root);

static bool EcsUiInteractionEmitChildren(
    EcsUiInteractionBuildContext *ctx,
    const EcsUiTreeNodeSnapshot *node,
    float opacity)
{
    if (ctx == NULL || node == NULL) {
        return false;
    }
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < ctx->tree->count;
         child = ctx->tree->nodes[child].next_sibling) {
        if (!EcsUiInteractionEmitNode(ctx, child, opacity, true)) {
            return false;
        }
    }
    return true;
}

static bool EcsUiInteractionEmitZStackChildren(
    EcsUiInteractionBuildContext *ctx,
    const EcsUiTreeNodeSnapshot *node,
    float opacity)
{
    if (ctx == NULL || node == NULL) {
        return false;
    }
    bool first = true;
    int16_t z_index = 1;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < ctx->tree->count;
         child = ctx->tree->nodes[child].next_sibling) {
        const EcsUiTreeNodeSnapshot *child_node = &ctx->tree->nodes[child];
        const bool placed = child_node->has_placement;
        if (first && !placed) {
            if (!EcsUiInteractionEmitNode(ctx, child, opacity, true)) {
                return false;
            }
            first = false;
            continue;
        }

        const float child_opacity =
            opacity * EcsUiStyleClamp01(child_node->visual.opacity);
        if (child_opacity <= 0.01f) {
            continue;
        }
        const EcsUiInteractionRect rect =
            EcsUiInteractionNodePhysicalRect(ctx, child_node);
        EcsUiInteractionRegisterWrapperBlocker(ctx, child, rect);
        EcsUiInteractionRootState previous_root =
            EcsUiInteractionSaveRoot(ctx);
        EcsUiInteractionBeginRoot(
            ctx,
            EcsUiInteractionZIndex(ctx->base_z_index, z_index),
            (EcsUiInteractionClip){0});
        const bool ok =
            EcsUiInteractionEmitNode(ctx, child, opacity, false);
        EcsUiInteractionRestoreRoot(ctx, previous_root);
        if (!ok) {
            return false;
        }
        z_index += 1;
        first = false;
    }
    return true;
}

static bool EcsUiInteractionEmitNodeContent(
    EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    float opacity)
{
    if (ctx == NULL || ctx->tree == NULL || index >= ctx->tree->count) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    if (!node->has_layout) {
        return true;
    }

    const EcsUiInteractionRect rect =
        EcsUiInteractionNodePhysicalRect(ctx, node);
    const bool root_capture =
        index == 0u && ctx->layout != NULL && ctx->layout->capture_pointer;
    EcsUiInteractionRegisterNodeTarget(ctx, index, rect, root_capture);

    EcsUiInteractionClip previous_clip = ctx->active_clip;
    bool opened_clip = false;
    if (node->has_scroll_view) {
        ctx->active_clip = (EcsUiInteractionClip){
            .enabled = true,
            .rect = rect,
        };
        opened_clip = true;
    }

    bool ok = true;
    if (node->kind == ECS_UI_NODE_ZSTACK) {
        ok = EcsUiInteractionEmitZStackChildren(ctx, node, opacity);
    } else {
        ok = EcsUiInteractionEmitChildren(ctx, node, opacity);
    }
    if (opened_clip) {
        ctx->active_clip = previous_clip;
    }
    return ok;
}

static bool EcsUiInteractionEmitNode(
    EcsUiInteractionBuildContext *ctx,
    uint32_t index,
    float parent_opacity,
    bool allow_visual_offset_root)
{
    if (ctx == NULL || ctx->tree == NULL || index >= ctx->tree->count) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    const float opacity =
        parent_opacity * EcsUiStyleClamp01(node->visual.opacity);
    if (opacity <= 0.01f) {
        return true;
    }

    if (allow_visual_offset_root && EcsUiInteractionHasVisualOffset(node)) {
        EcsUiInteractionRootState previous_root =
            EcsUiInteractionSaveRoot(ctx);
        EcsUiInteractionBeginRoot(ctx, 0, (EcsUiInteractionClip){0});
        const bool ok = EcsUiInteractionEmitNodeContent(ctx, index, opacity);
        EcsUiInteractionRestoreRoot(ctx, previous_root);
        return ok;
    }

    return EcsUiInteractionEmitNodeContent(ctx, index, opacity);
}

void EcsUiInteractionFrameBegin(
    EcsUiInteractionFrame *frame,
    EcsUiInteractionState *state)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    frame->state = state;
}

bool EcsUiInteractionFrameBuild(
    EcsUiInteractionFrame *frame,
    const EcsUiTreeSnapshot *tree,
    const EcsUiInteractionBuildOptions *options)
{
    if (frame == NULL || tree == NULL || tree->count == 0u) {
        return false;
    }
    EcsUiInteractionState *state = frame->state;
    EcsUiInteractionFrameBegin(frame, state);
    const EcsUiFrameLayoutOptions *layout =
        options != NULL ? options->layout : NULL;
    const float scale = EcsUiInteractionScale(tree);
    EcsUiInteractionBuildContext ctx = {
        .frame = frame,
        .tree = tree,
        .layout = layout,
        .scroll_reports = options != NULL ? options->scroll_reports : NULL,
        .scroll_report_count =
            options != NULL ? options->scroll_report_count : 0u,
        .base_z_index =
            EcsUiInteractionZIndex(layout != NULL ? layout->z_index : 0, 0),
        .root_z_index =
            EcsUiInteractionZIndex(layout != NULL ? layout->z_index : 0, 0),
        .root_order = 0u,
        .root_next_order = 1u,
        .root_item_order = 0u,
        .root_origin_x = layout != NULL ? layout->physical_bounds.x : 0.0f,
        .root_origin_y = layout != NULL ? layout->physical_bounds.y : 0.0f,
    };
    if (layout == NULL) {
        (void)scale;
    }
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (tree->nodes[i].parent != 0) {
            continue;
        }
        if (!EcsUiInteractionEmitNode(&ctx, i, 1.0f, true)) {
            return false;
        }
    }
    EcsUiInteractionSortTargets(frame);
    return true;
}

static bool EcsUiInteractionRectContains(
    EcsUiInteractionRect rect,
    float x,
    float y)
{
    return rect.width > 0.0f && rect.height > 0.0f &&
        x >= rect.x && y >= rect.y &&
        x < rect.x + rect.width && y < rect.y + rect.height;
}

static bool EcsUiInteractionTargetIsInside(
    const EcsUiInteractionTarget *target,
    EcsUiPointerState pointer)
{
    if (target == NULL) {
        return false;
    }
    if (!EcsUiInteractionRectContains(
            (EcsUiInteractionRect){
                .x = target->physical_x,
                .y = target->physical_y,
                .width = target->physical_width,
                .height = target->physical_height,
            },
            pointer.x,
            pointer.y)) {
        return false;
    }
    if (!target->clip_enabled) {
        return true;
    }
    return EcsUiInteractionRectContains(
        (EcsUiInteractionRect){
            .x = target->clip_x,
            .y = target->clip_y,
            .width = target->clip_width,
            .height = target->clip_height,
        },
        pointer.x,
        pointer.y);
}

static bool EcsUiInteractionTargetHasHigherPriority(
    const EcsUiInteractionTarget *candidate,
    const EcsUiInteractionTarget *current)
{
    if (candidate == NULL) {
        return false;
    }
    if (current == NULL) {
        return true;
    }
    if (candidate->emit_order != current->emit_order) {
        return candidate->emit_order > current->emit_order;
    }
    return candidate->depth > current->depth;
}

static bool EcsUiInteractionTargetPressEligible(
    const EcsUiInteractionTarget *target)
{
    return target != NULL &&
        target->inside &&
        !target->disabled &&
        (target->pressable || target->blocking);
}

static bool EcsUiInteractionTargetWheelEligible(
    const EcsUiInteractionTarget *target)
{
    return target != NULL &&
        target->inside &&
        !target->disabled &&
        (target->blocking ||
         target->scroll_container ||
         target->scroll_subscribed);
}

static EcsUiInteractionTarget *EcsUiInteractionResolvePressTarget(
    EcsUiInteractionFrame *frame)
{
    if (frame == NULL) {
        return NULL;
    }
    EcsUiInteractionTarget *resolved = NULL;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiInteractionTarget *target = &frame->targets[i];
        if (!EcsUiInteractionTargetPressEligible(target)) {
            continue;
        }
        if (EcsUiInteractionTargetHasHigherPriority(target, resolved)) {
            resolved = target;
        }
    }
    return resolved;
}

static EcsUiInteractionTarget *EcsUiInteractionResolveWheelTarget(
    EcsUiInteractionFrame *frame)
{
    if (frame == NULL) {
        return NULL;
    }
    EcsUiInteractionTarget *resolved = NULL;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiInteractionTarget *target = &frame->targets[i];
        if (!EcsUiInteractionTargetWheelEligible(target)) {
            continue;
        }
        if (EcsUiInteractionTargetHasHigherPriority(target, resolved)) {
            resolved = target;
        }
    }
    return resolved;
}

static EcsUiInteractionTarget *EcsUiInteractionFindCaptureTarget(
    EcsUiInteractionFrame *frame,
    const EcsUiPointerCapture *capture)
{
    if (frame == NULL || capture == NULL || !capture->active) {
        return NULL;
    }
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        EcsUiInteractionTarget *target = &frame->targets[i];
        if (target->tree == capture->tree && target->node == capture->node &&
            target->pressable && !target->disabled) {
            return target;
        }
    }
    return NULL;
}

static bool EcsUiInteractionCaptureCovered(
    const EcsUiInteractionTarget *captured_target,
    const EcsUiInteractionTarget *resolved)
{
    if (captured_target == NULL || resolved == NULL ||
        (resolved->tree == captured_target->tree &&
         resolved->node == captured_target->node)) {
        return false;
    }
    return resolved->blocking &&
        EcsUiInteractionTargetHasHigherPriority(resolved, captured_target);
}

static float EcsUiInteractionLogicalPointerValue(float value, float scale)
{
    const float safe_scale = scale > 0.0f ? scale : 1.0f;
    return value / safe_scale;
}

static void EcsUiInteractionPushPointerEventWithAction(
    EcsUiEventList *events,
    const EcsUiInteractionTarget *target,
    EcsUiEventType type,
    EcsUiPointerState pointer,
    ecs_entity_t action,
    EcsUiPointerButton button)
{
    if (events == NULL || target == NULL) {
        return;
    }
    EcsUiEvent event = {
        .type = type,
        .tree = target->tree,
        .node = target->node,
        .action = action,
        .payload = target->payload,
        .x = EcsUiInteractionLogicalPointerValue(pointer.x, target->scale),
        .y = EcsUiInteractionLogicalPointerValue(pointer.y, target->scale),
        .start_x =
            EcsUiInteractionLogicalPointerValue(pointer.x, target->scale),
        .start_y =
            EcsUiInteractionLogicalPointerValue(pointer.y, target->scale),
        .button = button,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", target->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiInteractionPushPointerEvent(
    EcsUiEventList *events,
    const EcsUiInteractionTarget *target,
    EcsUiEventType type,
    EcsUiPointerState pointer)
{
    EcsUiInteractionPushPointerEventWithAction(
        events,
        target,
        type,
        pointer,
        target != NULL ? target->action : 0,
        ECS_UI_POINTER_BUTTON_PRIMARY);
}

static bool EcsUiInteractionPointerHasScroll(EcsUiPointerState pointer)
{
    return pointer.scroll_x != 0.0f || pointer.scroll_y != 0.0f;
}

static bool EcsUiInteractionScrollAxisCanConsume(
    float delta,
    bool enabled,
    float content_size,
    float container_size)
{
    return delta != 0.0f &&
        enabled &&
        content_size > container_size;
}

static bool EcsUiInteractionApplyScrollAxis(
    float delta,
    float content_size,
    float container_size,
    float *value)
{
    if (value == NULL || delta == 0.0f || content_size <= container_size) {
        return false;
    }
    const float old_value = *value;
    const float max_scroll =
        content_size > container_size ? content_size - container_size : 0.0f;
    const float next_value = EcsUiScrollStateClampOffset(
        old_value + (delta * 10.0f),
        content_size,
        container_size);
    if (next_value == old_value || max_scroll <= 0.0f) {
        return false;
    }
    *value = next_value;
    return true;
}

typedef struct EcsUiInteractionScrollWheelResult {
    bool consumed_x;
    bool consumed_y;
    bool mutated;
} EcsUiInteractionScrollWheelResult;

static void EcsUiInteractionPushPendingScrollUpdate(
    EcsUiInteractionFrame *frame,
    const EcsUiInteractionTarget *target,
    float offset_x,
    float offset_y,
    float content_width,
    float content_height,
    float container_width,
    float container_height)
{
    if (frame == NULL || target == NULL) {
        return;
    }
    const float scale = target->scale > 0.0f ? target->scale : 1.0f;
    EcsUiScrollUpdate update = {
        .tree = target->tree,
        .node = target->node,
        .node_index = target->node_index,
        .axes = target->scroll_axes,
        .offset_x = offset_x / scale,
        .offset_y = offset_y / scale,
        .content_w = content_width / scale,
        .content_h = content_height / scale,
        .viewport_w = container_width / scale,
        .viewport_h = container_height / scale,
    };
    for (uint32_t i = 0u; i < frame->pending_scroll_count; i += 1u) {
        if (frame->pending_scrolls[i].node == update.node) {
            frame->pending_scrolls[i] = update;
            return;
        }
    }
    if (frame->pending_scroll_count >= ECS_UI_SCROLL_UPDATE_MAX) {
        frame->truncated = true;
        return;
    }
    frame->pending_scrolls[frame->pending_scroll_count] = update;
    frame->pending_scroll_count += 1u;
}

static EcsUiInteractionScrollWheelResult
EcsUiInteractionApplyScrollContainerWheel(
    EcsUiInteractionFrame *frame,
    const EcsUiInteractionTarget *target,
    EcsUiPointerState pointer)
{
    EcsUiInteractionScrollWheelResult result = {0};
    if (frame == NULL || target == NULL || !target->scroll_container ||
        !EcsUiInteractionPointerHasScroll(pointer)) {
        return result;
    }

    const float content_width = target->scroll_content_width;
    const float content_height = target->scroll_content_height;
    const float container_width = target->scroll_viewport_width;
    const float container_height = target->scroll_viewport_height;
    const bool can_x = EcsUiInteractionScrollAxisCanConsume(
        pointer.scroll_x,
        (target->scroll_axes & ECS_UI_SCROLL_AXIS_X) != 0u,
        content_width,
        container_width);
    const bool can_y = EcsUiInteractionScrollAxisCanConsume(
        pointer.scroll_y,
        (target->scroll_axes & ECS_UI_SCROLL_AXIS_Y) != 0u,
        content_height,
        container_height);

    float next_x = 0.0f;
    float next_y = 0.0f;
    if (target->tree_snapshot != NULL &&
            target->node_index < target->tree_snapshot->count) {
        const EcsUiTreeNodeSnapshot *node =
            &target->tree_snapshot->nodes[target->node_index];
        if (node->has_scroll_state) {
            const float scale = target->scale > 0.0f ? target->scale : 1.0f;
            next_x = node->scroll_state.offset_x * scale;
            next_y = node->scroll_state.offset_y * scale;
        }
    }

    bool changed = false;
    if (can_x) {
        result.consumed_x = true;
        changed |= EcsUiInteractionApplyScrollAxis(
            pointer.scroll_x,
            content_width,
            container_width,
            &next_x);
    }
    if (can_y) {
        result.consumed_y = true;
        changed |= EcsUiInteractionApplyScrollAxis(
            pointer.scroll_y,
            content_height,
            container_height,
            &next_y);
    }
    if (changed) {
        EcsUiInteractionPushPendingScrollUpdate(
            frame,
            target,
            next_x,
            next_y,
            content_width,
            content_height,
            container_width,
            container_height);
        frame->scroll_consumed = true;
    }
    result.mutated = changed;
    return result;
}

static void EcsUiInteractionPushScrollEvent(
    EcsUiEventList *events,
    const EcsUiInteractionTarget *target,
    EcsUiPointerState pointer,
    float scroll_x,
    float scroll_y)
{
    if (events == NULL || target == NULL ||
        (scroll_x == 0.0f && scroll_y == 0.0f)) {
        return;
    }
    EcsUiEvent event = {
        .type = ECS_UI_EVENT_SCROLLED,
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .payload = target->payload,
        .x = EcsUiInteractionLogicalPointerValue(pointer.x, target->scale),
        .y = EcsUiInteractionLogicalPointerValue(pointer.y, target->scale),
        .start_x =
            EcsUiInteractionLogicalPointerValue(pointer.x, target->scale),
        .start_y =
            EcsUiInteractionLogicalPointerValue(pointer.y, target->scale),
        .scroll_x = scroll_x,
        .scroll_y = scroll_y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", target->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiInteractionRouteWheel(
    EcsUiInteractionFrame *frame,
    EcsUiPointerState pointer,
    EcsUiEventList *events)
{
    if (frame == NULL || events == NULL ||
        !EcsUiInteractionPointerHasScroll(pointer)) {
        return;
    }

    EcsUiInteractionTarget *target =
        EcsUiInteractionResolveWheelTarget(frame);
    if (target == NULL) {
        return;
    }
    float subscribed_scroll_x = pointer.scroll_x;
    float subscribed_scroll_y = pointer.scroll_y;
    if (target->scroll_container) {
        EcsUiInteractionScrollWheelResult result =
            EcsUiInteractionApplyScrollContainerWheel(frame, target, pointer);
        if (result.consumed_x) {
            subscribed_scroll_x = 0.0f;
        }
        if (result.consumed_y) {
            subscribed_scroll_y = 0.0f;
        }
        if (target->scroll_subscribed) {
            EcsUiInteractionPushScrollEvent(
                events,
                target,
                pointer,
                subscribed_scroll_x,
                subscribed_scroll_y);
        }
        return;
    }
    if (target->scroll_subscribed) {
        EcsUiInteractionPushScrollEvent(
            events,
            target,
            pointer,
            subscribed_scroll_x,
            subscribed_scroll_y);
        return;
    }
    if (target->blocking) {
        return;
    }
}

static void EcsUiInteractionStartPointerCapture(
    EcsUiInteractionState *state,
    const EcsUiInteractionTarget *target,
    EcsUiPointerState pointer,
    EcsUiPointerButton button)
{
    if (state == NULL || target == NULL) {
        return;
    }
    state->capture = (EcsUiPointerCapture){
        .active = true,
        .tree = target->tree,
        .node = target->node,
        .action = target->action,
        .payload = target->payload,
        .scale = target->scale,
        .start_x =
            EcsUiInteractionLogicalPointerValue(pointer.x, target->scale),
        .start_y =
            EcsUiInteractionLogicalPointerValue(pointer.y, target->scale),
        .physical_start_x = pointer.x,
        .physical_start_y = pointer.y,
        .start_time = pointer.time,
        .button = button,
    };
    (void)snprintf(
        state->capture.node_id,
        sizeof(state->capture.node_id),
        "%s",
        target->node_id);
}

static void EcsUiInteractionPushCapturedPointerEvent(
    EcsUiEventList *events,
    const EcsUiPointerCapture *capture,
    EcsUiEventType type,
    EcsUiPointerState pointer)
{
    if (events == NULL || capture == NULL || !capture->active) {
        return;
    }

    float elapsed = (float)(pointer.time - capture->start_time);
    if (elapsed < 0.001f) {
        elapsed = 0.001f;
    }
    const float x =
        EcsUiInteractionLogicalPointerValue(pointer.x, capture->scale);
    const float y =
        EcsUiInteractionLogicalPointerValue(pointer.y, capture->scale);
    const float delta_x = x - capture->start_x;
    const float delta_y = y - capture->start_y;
    EcsUiEvent event = {
        .type = type,
        .tree = capture->tree,
        .node = capture->node,
        .action = capture->action,
        .payload = capture->payload,
        .x = x,
        .y = y,
        .start_x = capture->start_x,
        .start_y = capture->start_y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .elapsed = elapsed,
        .velocity_x = delta_x / elapsed,
        .velocity_y = delta_y / elapsed,
        .button = capture->button,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", capture->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static bool EcsUiInteractionPointerDownForButton(
    EcsUiPointerState pointer,
    EcsUiPointerButton button)
{
    if (button == ECS_UI_POINTER_BUTTON_SECONDARY) {
        return pointer.secondary_down;
    }
    if (button == ECS_UI_POINTER_BUTTON_MIDDLE) {
        return pointer.middle_down;
    }
    return pointer.down;
}

static bool EcsUiInteractionPointerReleasedForButton(
    EcsUiPointerState pointer,
    EcsUiPointerButton button)
{
    if (button == ECS_UI_POINTER_BUTTON_SECONDARY) {
        return pointer.secondary_released;
    }
    if (button == ECS_UI_POINTER_BUTTON_MIDDLE) {
        return pointer.middle_released;
    }
    return pointer.released;
}

static float EcsUiInteractionDistanceSquared(
    EcsUiPointerState pointer,
    float start_x,
    float start_y)
{
    const float dx = pointer.x - start_x;
    const float dy = pointer.y - start_y;
    return (dx * dx) + (dy * dy);
}

static void EcsUiInteractionMarkPointerInsideTargets(
    EcsUiInteractionFrame *frame,
    EcsUiPointerState pointer)
{
    if (frame == NULL) {
        return;
    }
    frame->inside_target_count = 0u;
    frame->pressable_target_count = 0u;
    for (uint32_t i = 0u; i < frame->target_count; i += 1u) {
        if (frame->targets[i].pressable && !frame->targets[i].disabled) {
            frame->pressable_target_count += 1u;
        }
        frame->targets[i].inside =
            EcsUiInteractionTargetIsInside(&frame->targets[i], pointer);
        if (frame->targets[i].inside) {
            frame->inside_target_count += 1u;
        }
    }
}

void EcsUiInteractionCollectFrameEvents(
    EcsUiInteractionFrame *frame,
    EcsUiPointerState pointer,
    EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }
    EcsUiEventListClear(events);
    if (frame == NULL || frame->state == NULL) {
        return;
    }

    EcsUiInteractionMarkPointerInsideTargets(frame, pointer);
    EcsUiInteractionTarget *resolved =
        EcsUiInteractionResolvePressTarget(frame);
    frame->resolved_tree = resolved != NULL ? resolved->tree : 0;
    frame->resolved_node = resolved != NULL ? resolved->node : 0;
    frame->resolved_action = resolved != NULL ? resolved->action : 0;
    frame->resolved_payload = resolved != NULL ? resolved->payload : 0u;
    frame->resolved_pressable = resolved != NULL && resolved->pressable;
    frame->resolved_node_id[0] = '\0';
    if (resolved != NULL) {
        (void)snprintf(
            frame->resolved_node_id,
            sizeof(frame->resolved_node_id),
            "%s",
            resolved->node_id);
    }

    EcsUiInteractionRouteWheel(frame, pointer, events);

    EcsUiPointerCapture *capture = &frame->state->capture;
    if (capture->active) {
        EcsUiInteractionTarget *captured_target =
            EcsUiInteractionFindCaptureTarget(frame, capture);
        if (captured_target == NULL) {
            frame->capture_missing_target = true;
            frame->capture_missing_node = capture->node;
            frame->capture_missing_action = capture->action;
            frame->capture_missing_payload = capture->payload;
            (void)snprintf(
                frame->capture_missing_node_id,
                sizeof(frame->capture_missing_node_id),
                "%s",
                capture->node_id);
            *capture = (EcsUiPointerCapture){0};
        } else {
            if (EcsUiInteractionCaptureCovered(captured_target, resolved)) {
                EcsUiInteractionPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_DRAG_ENDED,
                    pointer);
                *capture = (EcsUiPointerCapture){0};
                return;
            }

            const bool button_down =
                EcsUiInteractionPointerDownForButton(pointer, capture->button);
            const bool button_released =
                EcsUiInteractionPointerReleasedForButton(
                    pointer,
                    capture->button);
            if (button_down) {
                EcsUiInteractionPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_DRAGGED,
                    pointer);
                return;
            }
            if (button_released || !button_down) {
                if (!button_released) {
                    frame->capture_missed_release = true;
                    frame->capture_missed_release_node = capture->node;
                    frame->capture_missed_release_action = capture->action;
                    frame->capture_missed_release_payload = capture->payload;
                    (void)snprintf(
                        frame->capture_missed_release_node_id,
                        sizeof(frame->capture_missed_release_node_id),
                        "%s",
                        capture->node_id);
                }
                const bool did_drag =
                    EcsUiInteractionDistanceSquared(
                        pointer,
                        capture->physical_start_x,
                        capture->physical_start_y) > 36.0f;
                const bool click_eligible = captured_target->inside;
                EcsUiInteractionPushCapturedPointerEvent(
                    events,
                    capture,
                    ECS_UI_EVENT_DRAG_ENDED,
                    pointer);
                if (!did_drag && click_eligible &&
                    capture->button != ECS_UI_POINTER_BUTTON_MIDDLE) {
                    EcsUiInteractionPushCapturedPointerEvent(
                        events,
                        capture,
                        ECS_UI_EVENT_CLICKED,
                        pointer);
                }
                *capture = (EcsUiPointerCapture){0};
            }
            return;
        }
    }

    if (resolved == NULL || !resolved->pressable) {
        return;
    }

    EcsUiInteractionPushPointerEvent(
        events,
        resolved,
        ECS_UI_EVENT_HOVERED,
        pointer);
    if (pointer.middle_pressed) {
        EcsUiInteractionStartPointerCapture(
            frame->state,
            resolved,
            pointer,
            ECS_UI_POINTER_BUTTON_MIDDLE);
        EcsUiInteractionPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    } else if (pointer.secondary_pressed) {
        EcsUiInteractionPushPointerEventWithAction(
            events,
            resolved,
            ECS_UI_EVENT_SECONDARY_PRESSED,
            pointer,
            resolved->action,
            ECS_UI_POINTER_BUTTON_SECONDARY);
        EcsUiInteractionStartPointerCapture(
            frame->state,
            resolved,
            pointer,
            ECS_UI_POINTER_BUTTON_SECONDARY);
        EcsUiInteractionPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    } else if (pointer.pressed) {
        EcsUiInteractionPushPointerEvent(
            events,
            resolved,
            ECS_UI_EVENT_PRESSED,
            pointer);
        EcsUiInteractionStartPointerCapture(
            frame->state,
            resolved,
            pointer,
            ECS_UI_POINTER_BUTTON_PRIMARY);
        EcsUiInteractionPushCapturedPointerEvent(
            events,
            &frame->state->capture,
            ECS_UI_EVENT_DRAG_STARTED,
            pointer);
    }
}
