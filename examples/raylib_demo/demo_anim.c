#include "demo_anim.h"

#include <raylib.h>

ECS_COMPONENT_DECLARE(DemoAnimatedFloat);
ECS_COMPONENT_DECLARE(DemoLinear1f);
ECS_TAG_DECLARE(DemoAnimatedRow);
ECS_TAG_DECLARE(DemoAnimatedSelection);
ECS_TAG_DECLARE(DemoDismissPresentationOnAnimationComplete);

static float DemoAnimClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float DemoAnimMax(float a, float b)
{
    return a > b ? a : b;
}

float DemoAnimPresentationValue(ecs_world_t *world, ecs_entity_t presentation)
{
    const DemoAnimatedFloat *animated =
        presentation != 0 ? ecs_get(world, presentation, DemoAnimatedFloat) : NULL;
    return animated != NULL ? animated->value : 1.0f;
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
    if (world == NULL || node == 0) {
        return;
    }

    const float visible = DemoAnimClamp01(value);
    ecs_set(
        world,
        node,
        EcsUiVisual,
        {
            .opacity = visible,
            .offset_y = (1.0f - visible) * offset_y,
        });
}

static void DemoAnimApplyHighlightToNode(
    ecs_world_t *world,
    ecs_entity_t node,
    float value)
{
    if (world == NULL || node == 0) {
        return;
    }

    ecs_set(
        world,
        node,
        EcsUiVisual,
        {
            .opacity = 1.0f,
            .highlight = DemoAnimClamp01(value),
        });
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

    ecs_set(
        world,
        presentation,
        DemoAnimatedFloat,
        {
            .value = DemoAnimClamp01(from),
        });
    ecs_set(
        world,
        presentation,
        DemoLinear1f,
        {
            .from = DemoAnimClamp01(from),
            .to = DemoAnimClamp01(to),
            .duration = DemoAnimMax(duration, 0.001f),
        });

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

static void DemoAnimStartNode(
    ecs_world_t *world,
    ecs_entity_t node,
    float from,
    float to,
    float duration)
{
    if (world == NULL || node == 0) {
        return;
    }

    ecs_set(
        world,
        node,
        DemoAnimatedFloat,
        {
            .value = DemoAnimClamp01(from),
        });
    ecs_set(
        world,
        node,
        DemoLinear1f,
        {
            .from = DemoAnimClamp01(from),
            .to = DemoAnimClamp01(to),
            .duration = DemoAnimMax(duration, 0.001f),
        });
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
    ecs_delete(world, presentation);
}

static void DemoAnimAdvanceLinear1fSystem(ecs_iter_t *it)
{
    DemoAnimatedFloat *animated = ecs_field(it, DemoAnimatedFloat, 0);
    DemoLinear1f *linear = ecs_field(it, DemoLinear1f, 1);
    const float dt = it->delta_time > 0.0f ? it->delta_time : 1.0f / 60.0f;

    for (int32_t i = 0; i < it->count; i += 1) {
        linear[i].elapsed += dt;
        const float t = DemoAnimClamp01(linear[i].elapsed / linear[i].duration);
        animated[i].value =
            linear[i].from + ((linear[i].to - linear[i].from) * t);
        if (ecs_has_id(it->world, it->entities[i], DemoPresentation)) {
            DemoAnimApplyPresentationVisual(
                it->world,
                it->entities[i],
                animated[i].value);
        } else if (ecs_has_id(it->world, it->entities[i], DemoAnimatedRow)) {
            DemoAnimApplyVisualToNodeEx(
                it->world,
                it->entities[i],
                animated[i].value,
                24.0f);
        } else if (ecs_has_id(
                       it->world,
                       it->entities[i],
                       DemoAnimatedSelection)) {
            DemoAnimApplyHighlightToNode(
                it->world,
                it->entities[i],
                animated[i].value);
        }

        if (t >= 1.0f) {
            const bool dismiss =
                ecs_has_id(
                    it->world,
                    it->entities[i],
                    DemoDismissPresentationOnAnimationComplete);
            if (!dismiss) {
                DemoAnimApplyVisualToNodeEx(
                    it->world,
                    it->entities[i],
                    1.0f,
                    0.0f);
            }
            ecs_remove(it->world, it->entities[i], DemoLinear1f);
            if (ecs_has_id(it->world, it->entities[i], DemoAnimatedRow)) {
                ecs_remove_id(it->world, it->entities[i], DemoAnimatedRow);
            }
            if (ecs_has_id(it->world, it->entities[i], DemoAnimatedSelection)) {
                ecs_remove_id(
                    it->world,
                    it->entities[i],
                    DemoAnimatedSelection);
            }
            if (dismiss) {
                TraceLog(LOG_INFO, "DEMO: presentation exit animation completed");
                DemoAnimDeletePresentationAfterExit(it->world, it->entities[i]);
            }
        }
    }
}

void DemoAnimRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoAnimatedFloat);
    ECS_COMPONENT_DEFINE(world, DemoLinear1f);
    ECS_TAG_DEFINE(world, DemoAnimatedRow);
    ECS_TAG_DEFINE(world, DemoAnimatedSelection);
    ECS_TAG_DEFINE(world, DemoDismissPresentationOnAnimationComplete);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoAnimAdvanceLinear1fSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(DemoAnimatedFloat)},
            {.id = ecs_id(DemoLinear1f)},
        },
        .callback = DemoAnimAdvanceLinear1fSystem,
    });
}
