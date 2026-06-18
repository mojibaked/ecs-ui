#ifndef ECS_UI_ECS_UI_NAVIGATION_H
#define ECS_UI_ECS_UI_NAVIGATION_H

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ECS_TAG_DECLARE(EcsUiRoute);
extern ECS_TAG_DECLARE(EcsUiPresentation);
extern ECS_TAG_DECLARE(EcsUiPresentationRoute);
extern ECS_TAG_DECLARE(EcsUiPresentationUiNode);
extern ECS_TAG_DECLARE(EcsUiPresentationHost);
extern ECS_TAG_DECLARE(EcsUiActivePresentation);
extern ECS_TAG_DECLARE(EcsUiPresentRouteRequest);
extern ECS_TAG_DECLARE(EcsUiDismissPresentationRequest);

void EcsUiNavigationImport(ecs_world_t *world);

ecs_entity_t EcsUiNavRoot(ecs_world_t *world);
ecs_entity_t EcsUiNavRoute(ecs_world_t *world, const char *name);
bool EcsUiNavIsRoute(const ecs_world_t *world, ecs_entity_t route);
ecs_entity_t EcsUiNavActivePresentation(const ecs_world_t *world);
ecs_entity_t EcsUiNavPresentationRoute(
    const ecs_world_t *world,
    ecs_entity_t presentation);
ecs_entity_t EcsUiNavPresentationUiNode(
    const ecs_world_t *world,
    ecs_entity_t presentation);
ecs_entity_t EcsUiNavRequestPresentRoute(
    ecs_world_t *world,
    ecs_entity_t route);
ecs_entity_t EcsUiNavRequestDismissPresentation(ecs_world_t *world);
ecs_entity_t EcsUiNavCreatePresentation(
    ecs_world_t *world,
    ecs_entity_t route);
bool EcsUiNavSetActivePresentation(
    ecs_world_t *world,
    ecs_entity_t presentation);
bool EcsUiNavClearActivePresentation(
    ecs_world_t *world,
    ecs_entity_t presentation);
bool EcsUiNavSetPresentationUiNode(
    ecs_world_t *world,
    ecs_entity_t presentation,
    ecs_entity_t ui_node);

#ifdef __cplusplus
}
#endif

#endif
