#ifndef ECS_UI_ECS_UI_PROJECTION_H
#define ECS_UI_ECS_UI_PROJECTION_H

#include "ecs_ui/ecs_ui.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_PROJECTION_ITEM_MAX 512u

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

typedef union EcsUiProjectionCollectionItemStorage {
    uint64_t align_u64;
    double align_double;
    void *align_ptr;
    unsigned char bytes[ECS_UI_PROJECTION_ITEM_MAX];
} EcsUiProjectionCollectionItemStorage;

typedef struct EcsUiProjectionCollectionBuffer {
    size_t item_size;
    uint32_t item_count;
    bool truncated;
    EcsUiProjectionCollectionSource items[ECS_UI_TREE_NODE_MAX];
    EcsUiProjectionCollectionItemStorage storage[ECS_UI_TREE_NODE_MAX];
} EcsUiProjectionCollectionBuffer;

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

typedef struct EcsUiProjectionCollectionViewDesc {
    ecs_entity_t source_parent;
    ecs_entity_t ui_parent;
    ecs_id_t source_filter;
    const EcsUiProjectionCollectionBuffer *items;
    bool preserve_unprojected_ui_children;
    const char *source_name_prefix;
    EcsUiProjectionSyncSource sync_source;
    EcsUiProjectionBuildRoot build_root;
    EcsUiProjectionUpdateRoot update_root;
    void *ctx;
} EcsUiProjectionCollectionViewDesc;

typedef uint64_t (*EcsUiProjectionEntityKey)(
    ecs_world_t *source_world,
    ecs_entity_t source,
    void *ctx);

typedef void (*EcsUiProjectionSyncEntitySource)(
    ecs_world_t *ui_world,
    ecs_entity_t ui_source,
    ecs_world_t *source_world,
    ecs_entity_t source,
    void *ctx);

typedef ecs_entity_t (*EcsUiProjectionBuildEntityRoot)(
    ecs_world_t *ui_world,
    ecs_entity_t ui_source,
    ecs_world_t *source_world,
    ecs_entity_t source,
    void *ctx);

typedef void (*EcsUiProjectionUpdateEntityRoot)(
    ecs_world_t *ui_world,
    ecs_entity_t ui_source,
    ecs_entity_t ui_root,
    ecs_world_t *source_world,
    ecs_entity_t source,
    uint32_t position,
    uint32_t count,
    void *ctx);

typedef struct EcsUiProjectionOrderedEntityDesc {
    ecs_world_t *source_world;
    ecs_entity_t source_parent;
    ecs_id_t source_filter;
    ecs_world_t *ui_world;
    ecs_entity_t ui_source_parent;
    ecs_entity_t ui_parent;
    ecs_id_t ui_source_filter;
    bool preserve_unprojected_ui_children;
    const char *ui_source_name_prefix;
    EcsUiProjectionEntityKey key;
    EcsUiProjectionSyncEntitySource sync_source;
    EcsUiProjectionBuildEntityRoot build_root;
    EcsUiProjectionUpdateEntityRoot update_root;
    uint32_t *out_projected_count;
    void *ctx;
} EcsUiProjectionOrderedEntityDesc;

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

void EcsUiProjectionCollectionBufferInit(
    EcsUiProjectionCollectionBuffer *buffer,
    size_t item_size);
bool EcsUiProjectionCollectionBufferPush(
    EcsUiProjectionCollectionBuffer *buffer,
    uint64_t key,
    const void *item);
uint32_t EcsUiProjectionCollectionBufferCount(
    const EcsUiProjectionCollectionBuffer *buffer);
bool EcsUiProjectionCollectionBufferOk(
    const EcsUiProjectionCollectionBuffer *buffer);

bool EcsUiProjectionSyncCollection(
    ecs_world_t *world,
    EcsUiProjectionCollectionDesc desc);
bool EcsUiProjectionSyncCollectionView(
    ecs_world_t *world,
    EcsUiProjectionCollectionViewDesc desc);
bool EcsUiProjectionSyncOrderedEntities(
    EcsUiProjectionOrderedEntityDesc desc);

#ifdef __cplusplus
}
#endif

#endif
