#ifndef ECS_UI_ECS_UI_PROJECTION_H
#define ECS_UI_ECS_UI_PROJECTION_H

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*EcsUiProjectionOrderVisit)(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t ui_root,
    uint32_t position,
    void *ctx);

typedef struct EcsUiProjectionOrderSyncDesc {
    ecs_entity_t source_parent;
    ecs_entity_t ui_parent;
    ecs_id_t source_filter;
    bool preserve_unprojected_ui_children;
    EcsUiProjectionOrderVisit on_projected;
    void *ctx;
} EcsUiProjectionOrderSyncDesc;

typedef struct EcsUiProjectionKey {
    uint64_t value;
} EcsUiProjectionKey;

typedef struct EcsUiProjectionCollectionSource {
    uint64_t key;
    const void *data;
} EcsUiProjectionCollectionSource;

typedef void (*EcsUiProjectionSyncSource)(
    ecs_world_t *world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx);

typedef ecs_entity_t (*EcsUiProjectionBuildRoot)(
    ecs_world_t *world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx);

typedef void (*EcsUiProjectionUpdateRoot)(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t ui_root,
    const EcsUiProjectionCollectionSource *item,
    uint32_t position,
    uint32_t count,
    void *ctx);

typedef struct EcsUiProjectionCollectionDesc {
    ecs_entity_t source_parent;
    ecs_entity_t ui_parent;
    ecs_id_t source_filter;
    const EcsUiProjectionCollectionSource *items;
    uint32_t item_count;
    bool preserve_unprojected_ui_children;
    const char *source_name_prefix;
    EcsUiProjectionSyncSource sync_source;
    EcsUiProjectionBuildRoot build_root;
    EcsUiProjectionUpdateRoot update_root;
    void *ctx;
} EcsUiProjectionCollectionDesc;

extern ECS_COMPONENT_DECLARE(EcsUiProjectionKey);
extern ECS_TAG_DECLARE(EcsUiProjectionRoot);
extern ECS_TAG_DECLARE(EcsUiProjectionSource);
extern ECS_TAG_DECLARE(EcsUiProjectionSlot);
extern ECS_TAG_DECLARE(EcsUiProjectionRootNode);

void EcsUiProjectionImport(ecs_world_t *world);

bool EcsUiProjectionLink(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t ui_root);
bool EcsUiProjectionSetSource(
    ecs_world_t *world,
    ecs_entity_t ui_node,
    ecs_entity_t source);
ecs_entity_t EcsUiProjectionGetRoot(
    const ecs_world_t *world,
    ecs_entity_t source);
ecs_entity_t EcsUiProjectionGetSource(
    const ecs_world_t *world,
    ecs_entity_t ui_node);
bool EcsUiProjectionDelete(ecs_world_t *world, ecs_entity_t source);

bool EcsUiProjectionRegisterSlot(ecs_world_t *world, ecs_entity_t slot);
bool EcsUiProjectionSetNode(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t slot,
    ecs_entity_t ui_node);
ecs_entity_t EcsUiProjectionGetNode(
    const ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t slot);

bool EcsUiProjectionSyncOrderedChildren(
    ecs_world_t *world,
    EcsUiProjectionOrderSyncDesc desc);

bool EcsUiProjectionSyncCollection(
    ecs_world_t *world,
    EcsUiProjectionCollectionDesc desc);

#ifdef __cplusplus
}
#endif

#endif
