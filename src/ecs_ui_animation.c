#include "ecs_ui/ecs_ui_animation.h"

ECS_COMPONENT_DECLARE(EcsUiAnimatedFloat);
ECS_COMPONENT_DECLARE(EcsUiLinear1f);
ECS_TAG_DECLARE(EcsUiAnimationComplete);

float EcsUiAnimationClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float EcsUiAnimationMax(float a, float b)
{
    return a > b ? a : b;
}

static bool EcsUiAnimationReady(void)
{
    return ecs_id(EcsUiAnimatedFloat) != 0 &&
        ecs_id(EcsUiLinear1f) != 0 &&
        EcsUiAnimationComplete != 0;
}

bool EcsUiAnimationHasActive(ecs_world_t *world)
{
    if (world == NULL || !EcsUiAnimationReady()) {
        return false;
    }

    ecs_iter_t it = ecs_each(world, EcsUiLinear1f);
    const bool active = ecs_each_next(&it);
    if (active) {
        ecs_iter_fini(&it);
    }
    return active;
}

bool EcsUiAnimationArmNextFrameDeadline(
    ecs_world_t *world,
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    uint64_t next_frame_deadline_ns)
{
    if (registry == NULL || !EcsUiWakeHandleIsValid(handle)) {
        return false;
    }
    if (EcsUiAnimationHasActive(world)) {
        return EcsUiWakeArmDeadline(registry, handle, next_frame_deadline_ns);
    }
    return EcsUiWakeDisarmDeadline(registry, handle);
}

float EcsUiAnimationValue(
    const ecs_world_t *world,
    ecs_entity_t entity,
    float fallback)
{
    if (world == NULL || entity == 0 || !EcsUiAnimationReady()) {
        return fallback;
    }

    const EcsUiAnimatedFloat *animated =
        ecs_get(world, entity, EcsUiAnimatedFloat);
    return animated != NULL ? animated->value : fallback;
}

void EcsUiAnimationStartLinear1f(
    ecs_world_t *world,
    ecs_entity_t entity,
    float from,
    float to,
    float duration)
{
    if (world == NULL || entity == 0 || !EcsUiAnimationReady()) {
        return;
    }

    ecs_set(
        world,
        entity,
        EcsUiAnimatedFloat,
        {
            .value = EcsUiAnimationClamp01(from),
        });
    ecs_set(
        world,
        entity,
        EcsUiLinear1f,
        {
            .from = EcsUiAnimationClamp01(from),
            .to = EcsUiAnimationClamp01(to),
            .duration = EcsUiAnimationMax(duration, 0.001f),
        });
    ecs_remove_id(world, entity, EcsUiAnimationComplete);
}

void EcsUiAnimationSetValue(
    ecs_world_t *world,
    ecs_entity_t entity,
    float value)
{
    if (world == NULL || entity == 0 || !EcsUiAnimationReady()) {
        return;
    }

    ecs_set(
        world,
        entity,
        EcsUiAnimatedFloat,
        {
            .value = EcsUiAnimationClamp01(value),
        });
    ecs_remove(world, entity, EcsUiLinear1f);
    ecs_remove_id(world, entity, EcsUiAnimationComplete);
}

void EcsUiAnimationApplyFadeSlideY(
    ecs_world_t *world,
    ecs_entity_t node,
    float value,
    float offset_y)
{
    if (world == NULL || node == 0) {
        return;
    }

    const float visible = EcsUiAnimationClamp01(value);
    ecs_set(
        world,
        node,
        EcsUiVisual,
        {
            .opacity = visible,
            .offset_y = (1.0f - visible) * offset_y,
        });
}

void EcsUiAnimationApplyHighlight(
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
            .highlight = EcsUiAnimationClamp01(value),
        });
}

static void EcsUiAnimationAdvanceLinear1fSystem(ecs_iter_t *it)
{
    EcsUiAnimatedFloat *animated = ecs_field(it, EcsUiAnimatedFloat, 0);
    EcsUiLinear1f *linear = ecs_field(it, EcsUiLinear1f, 1);
    const float dt = it->delta_time > 0.0f ? it->delta_time : 1.0f / 60.0f;

    for (int32_t i = 0; i < it->count; i += 1) {
        linear[i].elapsed += dt;
        const float t =
            EcsUiAnimationClamp01(linear[i].elapsed / linear[i].duration);
        animated[i].value =
            linear[i].from + ((linear[i].to - linear[i].from) * t);

        if (t >= 1.0f) {
            ecs_remove(it->world, it->entities[i], EcsUiLinear1f);
            ecs_add_id(it->world, it->entities[i], EcsUiAnimationComplete);
        }
    }
}

void EcsUiAnimationImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiAnimatedFloat);
    ECS_COMPONENT_DEFINE(world, EcsUiLinear1f);
    ECS_TAG_DEFINE(world, EcsUiAnimationComplete);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiAnimationAdvanceLinear1fSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(EcsUiAnimatedFloat)},
            {.id = ecs_id(EcsUiLinear1f)},
        },
        .callback = EcsUiAnimationAdvanceLinear1fSystem,
    });
}
