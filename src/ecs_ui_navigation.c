#include "ecs_ui/ecs_ui_navigation.h"

ECS_TAG_DECLARE(EcsUiRoute);
ECS_TAG_DECLARE(EcsUiPresentation);
ECS_TAG_DECLARE(EcsUiPresentationRoute);
ECS_TAG_DECLARE(EcsUiPresentationUiNode);
ECS_TAG_DECLARE(EcsUiPresentationHost);
ECS_TAG_DECLARE(EcsUiActivePresentation);
ECS_TAG_DECLARE(EcsUiPresentRouteRequest);
ECS_TAG_DECLARE(EcsUiDismissPresentationRequest);

static bool EcsUiNavigationReady(void)
{
    return EcsUiRoute != 0 && EcsUiPresentation != 0 &&
        EcsUiPresentationRoute != 0 && EcsUiPresentationUiNode != 0 &&
        EcsUiPresentationHost != 0 && EcsUiActivePresentation != 0 &&
        EcsUiPresentRouteRequest != 0 &&
        EcsUiDismissPresentationRequest != 0;
}

static ecs_entity_t EcsUiNavRootEntity(const ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    return ecs_lookup(world, "EcsUiNavigation");
}

void EcsUiNavigationImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_TAG_DEFINE(world, EcsUiRoute);
    ECS_TAG_DEFINE(world, EcsUiPresentation);
    ECS_TAG_DEFINE(world, EcsUiPresentationRoute);
    ECS_TAG_DEFINE(world, EcsUiPresentationUiNode);
    ECS_TAG_DEFINE(world, EcsUiPresentationHost);
    ECS_TAG_DEFINE(world, EcsUiActivePresentation);
    ECS_TAG_DEFINE(world, EcsUiPresentRouteRequest);
    ECS_TAG_DEFINE(world, EcsUiDismissPresentationRequest);

    ecs_add_id(world, EcsUiPresentationRoute, EcsExclusive);
    ecs_add_id(world, EcsUiPresentationUiNode, EcsExclusive);
    ecs_add_id(world, EcsUiActivePresentation, EcsExclusive);
    ecs_add_id(world, EcsUiPresentRouteRequest, EcsExclusive);
}

ecs_entity_t EcsUiNavRoot(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    return ecs_entity(world, {.name = "EcsUiNavigation"});
}

ecs_entity_t EcsUiNavRoute(ecs_world_t *world, const char *name)
{
    if (world == NULL || !EcsUiNavigationReady()) {
        return 0;
    }

    ecs_entity_t route = ecs_entity(world, {
        .name = name != NULL && name[0] != '\0' ? name : "EcsUiRoute",
    });
    if (route != 0) {
        ecs_add_id(world, route, EcsUiRoute);
    }
    return route;
}

bool EcsUiNavIsRoute(const ecs_world_t *world, ecs_entity_t route)
{
    return world != NULL && route != 0 && EcsUiNavigationReady() &&
        ecs_has_id(world, route, EcsUiRoute);
}

ecs_entity_t EcsUiNavActivePresentation(const ecs_world_t *world)
{
    if (world == NULL || !EcsUiNavigationReady()) {
        return 0;
    }
    ecs_entity_t root = EcsUiNavRootEntity(world);
    if (root == 0) {
        return 0;
    }
    return ecs_get_target(
        world,
        root,
        EcsUiActivePresentation,
        0);
}

ecs_entity_t EcsUiNavPresentationRoute(
    const ecs_world_t *world,
    ecs_entity_t presentation)
{
    if (world == NULL || presentation == 0 || !EcsUiNavigationReady()) {
        return 0;
    }
    return ecs_get_target(world, presentation, EcsUiPresentationRoute, 0);
}

ecs_entity_t EcsUiNavPresentationUiNode(
    const ecs_world_t *world,
    ecs_entity_t presentation)
{
    if (world == NULL || presentation == 0 || !EcsUiNavigationReady()) {
        return 0;
    }
    return ecs_get_target(world, presentation, EcsUiPresentationUiNode, 0);
}

ecs_entity_t EcsUiNavRequestPresentRoute(
    ecs_world_t *world,
    ecs_entity_t route)
{
    if (!EcsUiNavIsRoute(world, route)) {
        return 0;
    }
    return ecs_new_w_pair(world, EcsUiPresentRouteRequest, route);
}

ecs_entity_t EcsUiNavRequestDismissPresentation(ecs_world_t *world)
{
    if (world == NULL || !EcsUiNavigationReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiDismissPresentationRequest);
}

bool EcsUiNavSetActivePresentation(
    ecs_world_t *world,
    ecs_entity_t presentation)
{
    if (world == NULL || presentation == 0 || !EcsUiNavigationReady() ||
        !ecs_has_id(world, presentation, EcsUiPresentation)) {
        return false;
    }

    ecs_add_pair(
        world,
        EcsUiNavRoot(world),
        EcsUiActivePresentation,
        presentation);
    return true;
}

bool EcsUiNavClearActivePresentation(
    ecs_world_t *world,
    ecs_entity_t presentation)
{
    if (world == NULL || !EcsUiNavigationReady()) {
        return false;
    }

    ecs_entity_t root = EcsUiNavRoot(world);
    ecs_entity_t active =
        ecs_get_target(world, root, EcsUiActivePresentation, 0);
    if (active == 0 || (presentation != 0 && active != presentation)) {
        return false;
    }

    ecs_remove_pair(world, root, EcsUiActivePresentation, EcsWildcard);
    return true;
}

ecs_entity_t EcsUiNavCreatePresentation(
    ecs_world_t *world,
    ecs_entity_t route)
{
    if (!EcsUiNavIsRoute(world, route)) {
        return 0;
    }

    ecs_entity_t presentation =
        ecs_new_w_pair(world, EcsChildOf, EcsUiNavRoot(world));
    if (presentation == 0) {
        return 0;
    }

    ecs_add_id(world, presentation, EcsUiPresentation);
    ecs_add_pair(world, presentation, EcsUiPresentationRoute, route);
    ecs_add_pair(
        world,
        EcsUiNavRoot(world),
        EcsUiActivePresentation,
        presentation);
    return presentation;
}

bool EcsUiNavSetPresentationUiNode(
    ecs_world_t *world,
    ecs_entity_t presentation,
    ecs_entity_t ui_node)
{
    if (world == NULL || presentation == 0 || ui_node == 0 ||
        !EcsUiNavigationReady()) {
        return false;
    }

    ecs_add_pair(world, presentation, EcsUiPresentationUiNode, ui_node);
    return true;
}
