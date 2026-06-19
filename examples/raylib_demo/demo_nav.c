#include "demo_nav.h"

#include "demo_anim.h"
#include "demo_text_input.h"
#include "demo_theme.h"
#include "demo_ui.h"

#include <raylib.h>

ECS_COMPONENT_DECLARE(DemoPresentationDrag);
ECS_TAG_DECLARE(DemoBeginPresentationDragRequest);
ECS_COMPONENT_DECLARE(DemoUpdatePresentationDragRequest);
ECS_COMPONENT_DECLARE(DemoEndPresentationDragRequest);

static float DemoNavClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float DemoNavDragValue(float start_value, float delta_y)
{
    const float downward = delta_y > 0.0f ? delta_y : 0.0f;
    return DemoNavClamp01(start_value - (downward / 180.0f));
}

ecs_entity_t DemoNavRoot(ecs_world_t *world)
{
    return EcsUiNavRoot(world);
}

ecs_entity_t DemoNavAddItemRoute(ecs_world_t *world)
{
    return EcsUiNavRoute(world, "DemoAddItemRoute");
}

void DemoNavRequestPresentRoute(ecs_world_t *world, ecs_entity_t route)
{
    /*
     * Navigation follows the same event-to-request pattern as app actions.
     * Routes are durable entities; presentation requests are transient entities
     * that tell the nav system which route should become active during the
     * next ecs_progress() pass.
     */
    (void)EcsUiNavRequestPresentRoute(world, route);
}

void DemoNavRequestDismissPresentation(ecs_world_t *world)
{
    (void)EcsUiNavRequestDismissPresentation(world);
}

void DemoNavRequestBeginPresentationDrag(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    (void)ecs_new_w_id(world, DemoBeginPresentationDragRequest);
}

void DemoNavRequestUpdatePresentationDrag(ecs_world_t *world, float delta_y)
{
    if (world == NULL) {
        return;
    }

    ecs_entity_t request = ecs_new(world);
    ecs_set(
        world,
        request,
        DemoUpdatePresentationDragRequest,
        {
            .delta_y = delta_y,
        });
}

void DemoNavRequestEndPresentationDrag(
    ecs_world_t *world,
    float delta_y,
    float velocity_y)
{
    if (world == NULL) {
        return;
    }

    ecs_entity_t request = ecs_new(world);
    ecs_set(
        world,
        request,
        DemoEndPresentationDragRequest,
        {
            .delta_y = delta_y,
            .velocity_y = velocity_y,
        });
}

static ecs_entity_t DemoNavActivePresentationEntity(ecs_world_t *world)
{
    return EcsUiNavActivePresentation(world);
}

static void DemoNavDeletePresentationUi(
    ecs_world_t *world,
    ecs_entity_t presentation)
{
    ecs_entity_t ui =
        EcsUiNavPresentationUiNode(world, presentation);
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
        EcsUiNavPresentationUiNode(world, presentation);
    if (existing != 0) {
        return existing;
    }

    /*
     * Presentation UI is mounted dynamically under the viewport host. The sheet
     * itself is retained and linked to the presentation entity, so the nav
     * projector can reapply animation values without rebuilding the subtree.
     */
    EcsUiBuilder builder = EcsUiBuilderBegin(world, refs->presentation_host);
    ecs_entity_t sheet =
        EcsUiBeginVStack(
            &builder,
            (EcsUiStackDesc){
                .id = "AddItemSheet",
                .gap = 12.0f,
                .padding = 24.0f,
            });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "SheetDragHandle",
            .kind = "demo-drag-handle",
            .preferred_height = 28.0f,
            .on_click = refs->drag_presentation_action,
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
            .text = "Name an item, then create it from ECS text field state.",
            .role = ECS_UI_TEXT_CAPTION,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "AddItemNameLabel",
            .text = "item name",
            .role = ECS_UI_TEXT_LABEL,
        });
    (void)DemoTextInputBuildAddItemNameField(world, &builder);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "AddItemNoteLabel",
            .text = "note",
            .role = ECS_UI_TEXT_LABEL,
        });
    (void)DemoTextInputBuildAddItemNoteField(world, &builder);
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
            .style_token = DemoThemePrimaryActionStyleToken(world),
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
            .style_token = DemoThemeSubtleActionStyleToken(world),
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
        ecs_set(
            world,
            sheet,
            EcsUiHitTest,
            {
                .mode = ECS_UI_HIT_TEST_CAPTURE,
            });
        (void)EcsUiNavSetPresentationUiNode(world, presentation, sheet);
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
        /*
         * This system is the presentation projector: route state lives on a
         * presentation entity, while this system materializes the corresponding
         * UI and applies the latest animation value to the retained sheet node.
         */
        ecs_entity_t route =
            EcsUiNavPresentationRoute(it->world, it->entities[i]);
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

static void DemoNavPresentRouteSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t route =
            ecs_get_target(
                it->world,
                it->entities[i],
                EcsUiPresentRouteRequest,
                0);
        if (!EcsUiNavIsRoute(it->world, route)) {
            ecs_delete(it->world, it->entities[i]);
            continue;
        }

        ecs_entity_t active = EcsUiNavActivePresentation(it->world);
        if (active != 0) {
            /*
             * Only one presentation is active. Replacing it blurs text input and
             * deletes the previous sheet immediately because the new route owns
             * the overlay slot under the viewport.
             */
            (void)EcsUiTextInputRequestBlur(it->world);
            DemoNavDeletePresentationUi(it->world, active);
            ecs_delete(it->world, active);
        }

        ecs_entity_t presentation =
            EcsUiNavCreatePresentation(it->world, route);
        if (presentation == 0) {
            ecs_delete(it->world, it->entities[i]);
            continue;
        }
        DemoAnimStartPresentation(
            it->world,
            presentation,
            0.0f,
            1.0f,
            0.22f,
            false);
        TraceLog(LOG_INFO, "DEMO: presented route %s", ecs_get_name(it->world, route));
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoNavDismissPresentationSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t active = DemoNavActivePresentationEntity(it->world);
        if (active != 0) {
            (void)EcsUiTextInputRequestBlur(it->world);
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

static void DemoNavBeginPresentationDragSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t active = DemoNavActivePresentationEntity(it->world);
        if (active != 0) {
            /*
             * Starting a drag captures the current animation value and removes
             * any linear tween. While the pointer is down, gesture deltas drive
             * the presentation value directly.
             */
            const float value = DemoAnimPresentationValue(it->world, active);
            ecs_set(
                it->world,
                active,
                DemoPresentationDrag,
                {
                    .start_value = value,
                });
            DemoAnimSetPresentationValue(it->world, active, value);
            TraceLog(LOG_INFO, "DEMO: began presentation drag");
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoNavUpdatePresentationDragSystem(ecs_iter_t *it)
{
    const DemoUpdatePresentationDragRequest *requests =
        ecs_field(it, DemoUpdatePresentationDragRequest, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t active = DemoNavActivePresentationEntity(it->world);
        const DemoPresentationDrag *drag =
            active != 0 ? ecs_get(it->world, active, DemoPresentationDrag) : NULL;
        if (drag != NULL) {
            DemoAnimSetPresentationValue(
                it->world,
                active,
                DemoNavDragValue(drag->start_value, requests[i].delta_y));
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoNavEndPresentationDragSystem(ecs_iter_t *it)
{
    const DemoEndPresentationDragRequest *requests =
        ecs_field(it, DemoEndPresentationDragRequest, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t active = DemoNavActivePresentationEntity(it->world);
        const DemoPresentationDrag *drag =
            active != 0 ? ecs_get(it->world, active, DemoPresentationDrag) : NULL;
        if (drag != NULL) {
            const float current =
                DemoNavDragValue(drag->start_value, requests[i].delta_y);
            /*
             * Release policy intentionally mixes distance, velocity, and final
             * openness. The animation layer then finishes either toward 0
             * (dismiss and delete) or 1 (snap back to presented).
             */
            const bool should_dismiss = requests[i].delta_y > 96.0f ||
                requests[i].velocity_y > 900.0f ||
                current < 0.52f;
            ecs_remove(it->world, active, DemoPresentationDrag);
            if (should_dismiss) {
                (void)EcsUiTextInputRequestBlur(it->world);
            }
            DemoAnimStartPresentation(
                it->world,
                active,
                current,
                should_dismiss ? 0.0f : 1.0f,
                should_dismiss ? 0.16f : 0.18f,
                should_dismiss);
            TraceLog(
                LOG_INFO,
                "DEMO: ended presentation drag (%s)",
                should_dismiss ? "dismiss" : "cancel");
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

void DemoNavRegister(ecs_world_t *world)
{
    EcsUiNavigationImport(world);

    ECS_COMPONENT_DEFINE(world, DemoPresentationDrag);
    ECS_TAG_DEFINE(world, DemoBeginPresentationDragRequest);
    ECS_COMPONENT_DEFINE(world, DemoUpdatePresentationDragRequest);
    ECS_COMPONENT_DEFINE(world, DemoEndPresentationDragRequest);

    (void)DemoNavRoot(world);
    (void)DemoNavAddItemRoute(world);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavProjectPresentationUiSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiPresentation},
            {.id = ecs_pair(EcsUiPresentationRoute, EcsWildcard)},
        },
        .callback = DemoNavProjectPresentationUiSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavPresentRouteSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(EcsUiPresentRouteRequest, EcsWildcard)},
        },
        .callback = DemoNavPresentRouteSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavDismissPresentationSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiDismissPresentationRequest},
        },
        .callback = DemoNavDismissPresentationSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavBeginPresentationDragSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoBeginPresentationDragRequest},
        },
        .callback = DemoNavBeginPresentationDragSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavUpdatePresentationDragSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(DemoUpdatePresentationDragRequest)},
        },
        .callback = DemoNavUpdatePresentationDragSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoNavEndPresentationDragSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(DemoEndPresentationDragRequest)},
        },
        .callback = DemoNavEndPresentationDragSystem,
    });
}
