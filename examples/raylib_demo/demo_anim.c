#include "demo_anim.h"

#include <raylib.h>

ECS_TAG_DECLARE(DemoAnimatedRow);
ECS_TAG_DECLARE(DemoAnimatedSelection);
ECS_TAG_DECLARE(DemoDismissPresentationOnAnimationComplete);

float DemoAnimPresentationValue(ecs_world_t *world, ecs_entity_t presentation)
{
    return EcsUiAnimationValue(world, presentation, 1.0f);
}

void DemoAnimApplyVisualToNode(
    ecs_world_t *world,
    ecs_entity_t node,
    float value)
{
    DemoAnimApplyVisualToNodeEx(world, node, value, 180.0f);
}

void DemoAnimApplyVisualToNodeEx(
    ecs_world_t *world,
    ecs_entity_t node,
    float value,
    float offset_y)
{
    /*
     * Animation writes normal UI visual components. Renderers only consume
     * opacity/offset/highlight values from retained UI nodes.
     */
    EcsUiAnimationApplyFadeSlideY(world, node, value, offset_y);
}

static void DemoAnimApplyHighlightToNode(
    ecs_world_t *world,
    ecs_entity_t node,
    float value)
{
    EcsUiAnimationApplyHighlight(world, node, value);
}

void DemoAnimApplyPresentationVisual(
    ecs_world_t *world,
    ecs_entity_t presentation,
    float value)
{
    ecs_entity_t sheet =
        ecs_get_target(world, presentation, DemoPresentationUiNode, 0);
    DemoAnimApplyVisualToNode(world, sheet, value);
}

void DemoAnimStartPresentation(
    ecs_world_t *world,
    ecs_entity_t presentation,
    float from,
    float to,
    float duration,
    bool dismiss_on_complete)
{
    if (world == NULL || presentation == 0) {
        return;
    }

    /*
     * A presentation entity is both navigation state and animation target. The
     * core animation component advances the scalar value; this demo marker says
     * whether completion should remove the presentation subtree.
     */
    EcsUiAnimationStartLinear1f(world, presentation, from, to, duration);
    if (dismiss_on_complete) {
        ecs_add_id(
            world,
            presentation,
            DemoDismissPresentationOnAnimationComplete);
    } else {
        ecs_remove_id(
            world,
            presentation,
            DemoDismissPresentationOnAnimationComplete);
    }
    DemoAnimApplyPresentationVisual(world, presentation, from);
}

void DemoAnimSetPresentationValue(
    ecs_world_t *world,
    ecs_entity_t presentation,
    float value)
{
    if (world == NULL || presentation == 0) {
        return;
    }

    /*
     * Gestures bypass the linear tween by setting the value directly and
     * removing completion behavior. On release, navigation starts a fresh tween
     * from this gesture-controlled value.
     */
    EcsUiAnimationSetValue(world, presentation, value);
    ecs_remove_id(world, presentation, DemoDismissPresentationOnAnimationComplete);
    DemoAnimApplyPresentationVisual(world, presentation, value);
}

static void DemoAnimStartNode(
    ecs_world_t *world,
    ecs_entity_t node,
    float from,
    float to,
    float duration)
{
    EcsUiAnimationStartLinear1f(world, node, from, to, duration);
}

void DemoAnimStartRowInsert(ecs_world_t *world, ecs_entity_t row)
{
    DemoAnimStartNode(world, row, 0.0f, 1.0f, 0.18f);
    ecs_add_id(world, row, DemoAnimatedRow);
    DemoAnimApplyVisualToNodeEx(world, row, 0.0f, 24.0f);
}

void DemoAnimStartSelectionHighlight(ecs_world_t *world, ecs_entity_t button)
{
    DemoAnimStartNode(world, button, 1.0f, 0.0f, 0.34f);
    ecs_add_id(world, button, DemoAnimatedSelection);
    DemoAnimApplyHighlightToNode(world, button, 1.0f);
}

static void DemoAnimDeletePresentationAfterExit(
    ecs_world_t *world,
    ecs_entity_t presentation)
{
    ecs_entity_t nav_root = DemoNavRoot(world);
    if (ecs_get_target(world, nav_root, DemoActivePresentation, 0) ==
        presentation) {
        ecs_remove_pair(
            world,
            nav_root,
            DemoActivePresentation,
            EcsWildcard);
    }
    ecs_entity_t ui =
        ecs_get_target(world, presentation, DemoPresentationUiNode, 0);
    if (ui != 0) {
        ecs_delete(world, ui);
    }
    ecs_delete(world, presentation);
}

static void DemoAnimProjectVisualsSystem(ecs_iter_t *it)
{
    const EcsUiAnimatedFloat *animated =
        ecs_field(it, EcsUiAnimatedFloat, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        const ecs_entity_t entity = it->entities[i];
        if (ecs_has_id(it->world, entity, DemoPresentation)) {
            DemoAnimApplyPresentationVisual(
                it->world,
                entity,
                animated[i].value);
        }
        if (ecs_has_id(it->world, entity, DemoAnimatedRow)) {
            DemoAnimApplyVisualToNodeEx(
                it->world,
                entity,
                animated[i].value,
                24.0f);
        }
        if (ecs_has_id(it->world, entity, DemoAnimatedSelection)) {
            DemoAnimApplyHighlightToNode(
                it->world,
                entity,
                animated[i].value);
        }
    }
}

static void DemoAnimCompleteSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        const ecs_entity_t entity = it->entities[i];
        const bool is_presentation =
            ecs_has_id(it->world, entity, DemoPresentation);
        const bool is_row =
            ecs_has_id(it->world, entity, DemoAnimatedRow);
        const bool is_selection =
            ecs_has_id(it->world, entity, DemoAnimatedSelection);
        const bool dismiss =
            ecs_has_id(
                it->world,
                entity,
                DemoDismissPresentationOnAnimationComplete);
        if (!is_presentation && !is_row && !is_selection && !dismiss) {
            continue;
        }

        if (dismiss) {
            /*
             * Exit animations own final cleanup. Deleting the presentation also
             * removes its dynamic UI subtree before the presentation entity
             * itself goes away.
             */
            TraceLog(LOG_INFO, "DEMO: presentation exit animation completed");
            DemoAnimDeletePresentationAfterExit(it->world, entity);
            continue;
        }

        if (is_presentation) {
            DemoAnimApplyPresentationVisual(it->world, entity, 1.0f);
        }
        if (is_row) {
            DemoAnimApplyVisualToNodeEx(it->world, entity, 1.0f, 0.0f);
            ecs_remove_id(it->world, entity, DemoAnimatedRow);
        }
        if (is_selection) {
            DemoAnimApplyHighlightToNode(it->world, entity, 0.0f);
            ecs_remove_id(it->world, entity, DemoAnimatedSelection);
        }
        ecs_remove_id(it->world, entity, EcsUiAnimationComplete);
    }
}

void DemoAnimRegister(ecs_world_t *world)
{
    EcsUiAnimationImport(world);

    ECS_TAG_DEFINE(world, DemoAnimatedRow);
    ECS_TAG_DEFINE(world, DemoAnimatedSelection);
    ECS_TAG_DEFINE(world, DemoDismissPresentationOnAnimationComplete);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAnimProjectVisualsSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(EcsUiAnimatedFloat)},
        },
        .callback = DemoAnimProjectVisualsSystem,
    });
    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAnimCompleteSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiAnimationComplete},
        },
        .callback = DemoAnimCompleteSystem,
    });
}
