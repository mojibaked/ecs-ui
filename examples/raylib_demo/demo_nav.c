#include "demo_nav.h"

#include "demo_anim.h"
#include "demo_ui.h"

#include <raylib.h>

ECS_TAG_DECLARE(DemoRoute);
ECS_TAG_DECLARE(DemoPresentation);
ECS_TAG_DECLARE(DemoPresentationRoute);
ECS_TAG_DECLARE(DemoPresentationUiNode);
ECS_TAG_DECLARE(DemoActivePresentation);
ECS_TAG_DECLARE(DemoPresentRouteRequest);
ECS_TAG_DECLARE(DemoDismissPresentationRequest);

ecs_entity_t DemoNavRoot(ecs_world_t *world)
{
    return ecs_entity(world, {.name = "DemoNavigation"});
}

ecs_entity_t DemoNavAddItemRoute(ecs_world_t *world)
{
    return ecs_entity(world, {.name = "DemoAddItemRoute"});
}

void DemoNavRequestPresentRoute(ecs_world_t *world, ecs_entity_t route)
{
    if (world == NULL || route == 0 || !ecs_has_id(world, route, DemoRoute)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoPresentRouteRequest, route);
}

void DemoNavRequestDismissPresentation(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    (void)ecs_new_w_id(world, DemoDismissPresentationRequest);
}

static void DemoNavDeletePresentationUi(
    ecs_world_t *world,
    ecs_entity_t presentation)
{
    ecs_entity_t ui =
        ecs_get_target(world, presentation, DemoPresentationUiNode, 0);
    if (ui != 0) {
        ecs_delete(world, ui);
    }
}

static ecs_entity_t DemoNavCreateAddItemSheet(
    ecs_world_t *world,
    ecs_entity_t presentation,
    const DemoUiRefs *refs)
{
    if (world == NULL || presentation == 0 || refs == NULL ||
        refs->presentation_host == 0 || refs->add_item_action == 0 ||
        refs->dismiss_presentation_action == 0) {
        return 0;
    }

    ecs_entity_t existing =
        ecs_get_target(world, presentation, DemoPresentationUiNode, 0);
    if (existing != 0) {
        return existing;
    }

    EcsUiBuilder builder = EcsUiBuilderBegin(world, refs->presentation_host);
    ecs_entity_t sheet =
        EcsUiBeginVStack(
            &builder,
            (EcsUiStackDesc){
                .id = "AddItemSheet",
                .gap = 12.0f,
                .padding = 24.0f,
            });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "AddItemSheetTitle",
            .text = "Add item",
            .role = ECS_UI_TEXT_TITLE,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "AddItemSheetBody",
            .text = "This presentation is created from ECS navigation state.",
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "AddItemSheetActions",
            .gap = 10.0f,
            .padding = 0.0f,
        });
    EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "CreateItem",
            .variant = ECS_UI_BUTTON_PRIMARY,
            .on_click = refs->add_item_action,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "CreateItemIcon",
            .name = "+",
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "CreateItemLabel",
            .text = "create item",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "DismissPresentation",
            .variant = ECS_UI_BUTTON_SUBTLE,
            .on_click = refs->dismiss_presentation_action,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DismissPresentationLabel",
            .text = "dismiss",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);

    if (EcsUiBuilderOk(&builder) && sheet != 0) {
        ecs_add_pair(world, presentation, DemoPresentationUiNode, sheet);
        return sheet;
    }
    return 0;
}

static void DemoNavProjectPresentationUiSystem(ecs_iter_t *it)
{
    const DemoUiRefs *refs = ecs_singleton_get(it->world, DemoUiRefs);
    if (refs == NULL) {
        return;
    }

    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t route =
            ecs_get_target(it->world, it->entities[i], DemoPresentationRoute, 0);
        ecs_entity_t sheet = 0;
        if (route == DemoNavAddItemRoute(it->world)) {
            sheet = DemoNavCreateAddItemSheet(it->world, it->entities[i], refs);
        }
        if (sheet != 0) {
            DemoAnimApplyVisualToNode(
                it->world,
                sheet,
                DemoAnimPresentationValue(it->world, it->entities[i]));
        }
    }
}

static void DemoNavRemovePresentationUiObserver(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        DemoNavDeletePresentationUi(it->world, it->entities[i]);
    }
}

static void DemoNavPresentRouteSystem(ecs_iter_t *it)
{
    ecs_entity_t nav_root = DemoNavRoot(it->world);
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t route =
            ecs_get_target(it->world, it->entities[i], DemoPresentRouteRequest, 0);
        if (route == 0 || !ecs_has_id(it->world, route, DemoRoute)) {
            ecs_delete(it->world, it->entities[i]);
            continue;
        }

        ecs_entity_t active =
            ecs_get_target(it->world, nav_root, DemoActivePresentation, 0);
        if (active != 0) {
            DemoNavDeletePresentationUi(it->world, active);
            ecs_delete(it->world, active);
        }

        ecs_entity_t presentation =
            ecs_new_w_pair(it->world, EcsChildOf, nav_root);
        ecs_add_id(it->world, presentation, DemoPresentation);
        DemoAnimStartPresentation(
            it->world,
            presentation,
            0.0f,
            1.0f,
            0.22f,
            false);
        ecs_add_pair(it->world, presentation, DemoPresentationRoute, route);
        ecs_add_pair(it->world, nav_root, DemoActivePresentation, presentation);
        TraceLog(LOG_INFO, "DEMO: presented route %s", ecs_get_name(it->world, route));
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoNavDismissPresentationSystem(ecs_iter_t *it)
{
    ecs_entity_t nav_root = DemoNavRoot(it->world);
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t active =
            ecs_get_target(it->world, nav_root, DemoActivePresentation, 0);
        if (active != 0) {
            DemoAnimStartPresentation(
                it->world,
                active,
                DemoAnimPresentationValue(it->world, active),
                0.0f,
                0.18f,
                true);
            TraceLog(LOG_INFO, "DEMO: dismissing presentation");
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

void DemoNavRegister(ecs_world_t *world)
{
    ECS_TAG_DEFINE(world, DemoRoute);
    ECS_TAG_DEFINE(world, DemoPresentation);
    ECS_TAG_DEFINE(world, DemoPresentationRoute);
    ECS_TAG_DEFINE(world, DemoPresentationUiNode);
    ECS_TAG_DEFINE(world, DemoActivePresentation);
    ECS_TAG_DEFINE(world, DemoPresentRouteRequest);
    ECS_TAG_DEFINE(world, DemoDismissPresentationRequest);

    ecs_add_id(world, DemoPresentationRoute, EcsExclusive);
    ecs_add_id(world, DemoPresentationUiNode, EcsExclusive);
    ecs_add_id(world, DemoActivePresentation, EcsExclusive);
    ecs_add_id(world, DemoPresentRouteRequest, EcsExclusive);

    (void)DemoNavRoot(world);
    ecs_add_id(world, DemoNavAddItemRoute(world), DemoRoute);

    (void)ecs_observer(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavRemovePresentationUiObserver",
        }),
        .query.terms = {
            {.id = DemoPresentation},
        },
        .events = {EcsOnRemove},
        .callback = DemoNavRemovePresentationUiObserver,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavProjectPresentationUiSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoPresentation},
            {.id = ecs_pair(DemoPresentationRoute, EcsWildcard)},
        },
        .callback = DemoNavProjectPresentationUiSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavPresentRouteSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoPresentRouteRequest, EcsWildcard)},
        },
        .callback = DemoNavPresentRouteSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavDismissPresentationSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoDismissPresentationRequest},
        },
        .callback = DemoNavDismissPresentationSystem,
    });
}
