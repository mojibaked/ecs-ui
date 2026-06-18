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

typedef struct DemoPresentationDrag {
    float start_value;
} DemoPresentationDrag;

typedef struct DemoUpdatePresentationDragRequest {
    float delta_y;
} DemoUpdatePresentationDragRequest;

typedef struct DemoEndPresentationDragRequest {
    float delta_y;
    float velocity_y;
} DemoEndPresentationDragRequest;

extern ECS_COMPONENT_DECLARE(DemoPresentationDrag);
extern ECS_TAG_DECLARE(DemoBeginPresentationDragRequest);
extern ECS_COMPONENT_DECLARE(DemoUpdatePresentationDragRequest);
extern ECS_COMPONENT_DECLARE(DemoEndPresentationDragRequest);

void DemoNavRegister(ecs_world_t *world);
ecs_entity_t DemoNavRoot(ecs_world_t *world);
ecs_entity_t DemoNavAddItemRoute(ecs_world_t *world);
void DemoNavRequestPresentRoute(ecs_world_t *world, ecs_entity_t route);
void DemoNavRequestDismissPresentation(ecs_world_t *world);
void DemoNavRequestBeginPresentationDrag(ecs_world_t *world);
void DemoNavRequestUpdatePresentationDrag(ecs_world_t *world, float delta_y);
void DemoNavRequestEndPresentationDrag(
    ecs_world_t *world,
    float delta_y,
    float velocity_y);

#endif
