#ifndef ECS_UI_RAYLIB_DEMO_NAV_H
#define ECS_UI_RAYLIB_DEMO_NAV_H

#include "demo_app.h"

extern ECS_TAG_DECLARE(DemoRoute);
extern ECS_TAG_DECLARE(DemoPresentation);
extern ECS_TAG_DECLARE(DemoPresentationRoute);
extern ECS_TAG_DECLARE(DemoPresentationUiNode);
extern ECS_TAG_DECLARE(DemoActivePresentation);
extern ECS_TAG_DECLARE(DemoPresentRouteRequest);
extern ECS_TAG_DECLARE(DemoDismissPresentationRequest);

void DemoNavRegister(ecs_world_t *world);
ecs_entity_t DemoNavRoot(ecs_world_t *world);
ecs_entity_t DemoNavAddItemRoute(ecs_world_t *world);
void DemoNavRequestPresentRoute(ecs_world_t *world, ecs_entity_t route);
void DemoNavRequestDismissPresentation(ecs_world_t *world);

#endif
