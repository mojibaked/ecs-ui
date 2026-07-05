#include "ecs_ui/ecs_ui_projection.h"

#include "ecs_ui_projection_internal.h"

#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(EcsUiProjectionKey);
ECS_TAG_DECLARE(EcsUiProjectionRoot);
ECS_TAG_DECLARE(EcsUiProjectionSource);
ECS_TAG_DECLARE(EcsUiProjectionSlot);
ECS_TAG_DECLARE(EcsUiProjectionRootNode);
ECS_TAG_DECLARE(EcsUiProjectionPreserveChildren);

static bool EcsUiProjectionReady(void)
{
    return ecs_id(EcsUiProjectionKey) != 0 && EcsUiProjectionRoot != 0 &&
        EcsUiProjectionSource != 0 && EcsUiProjectionSlot != 0 &&
        EcsUiProjectionRootNode != 0 &&
        EcsUiProjectionPreserveChildren != 0;
}

void EcsUiProjectionImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiProjectionKey);
    ECS_TAG_DEFINE(world, EcsUiProjectionRoot);
    ECS_TAG_DEFINE(world, EcsUiProjectionSource);
    ECS_TAG_DEFINE(world, EcsUiProjectionSlot);
    ECS_TAG_DEFINE(world, EcsUiProjectionRootNode);
    ECS_TAG_DEFINE(world, EcsUiProjectionPreserveChildren);

    ecs_add_id(world, EcsUiProjectionRoot, EcsExclusive);
    ecs_add_id(world, EcsUiProjectionSource, EcsExclusive);
}

bool EcsUiProjectionSetSource(
    ecs_world_t *world,
    ecs_entity_t ui_node,
    ecs_entity_t source)
{
    if (world == NULL || ui_node == 0 || source == 0 ||
        !EcsUiProjectionReady()) {
        return false;
    }

    ecs_add_pair(world, ui_node, EcsUiProjectionSource, source);
    return true;
}

bool EcsUiProjectionLink(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t ui_root)
{
    if (world == NULL || source == 0 || ui_root == 0 ||
        !EcsUiProjectionReady()) {
        return false;
    }

    ecs_add_pair(world, source, EcsUiProjectionRoot, ui_root);
    ecs_add_id(world, ui_root, EcsUiProjectionRootNode);
    return EcsUiProjectionSetSource(world, ui_root, source);
}

ecs_entity_t EcsUiProjectionGetRoot(
    const ecs_world_t *world,
    ecs_entity_t source)
{
    if (world == NULL || source == 0 || !EcsUiProjectionReady()) {
        return 0;
    }
    return ecs_get_target(world, source, EcsUiProjectionRoot, 0);
}

ecs_entity_t EcsUiProjectionGetSource(
    const ecs_world_t *world,
    ecs_entity_t ui_node)
{
    if (world == NULL || ui_node == 0 || !EcsUiProjectionReady()) {
        return 0;
    }
    return ecs_get_target(world, ui_node, EcsUiProjectionSource, 0);
}

bool EcsUiProjectionDelete(ecs_world_t *world, ecs_entity_t source)
{
    if (world == NULL || source == 0 || !EcsUiProjectionReady()) {
        return false;
    }

    ecs_entity_t ui_root = EcsUiProjectionGetRoot(world, source);
    if (ui_root == 0) {
        return false;
    }

    ecs_delete(world, ui_root);
    ecs_remove_pair(world, source, EcsUiProjectionRoot, EcsWildcard);
    return true;
}

bool EcsUiProjectionRegisterSlot(ecs_world_t *world, ecs_entity_t slot)
{
    if (world == NULL || slot == 0 || !EcsUiProjectionReady()) {
        return false;
    }

    ecs_add_id(world, slot, EcsUiProjectionSlot);
    ecs_add_id(world, slot, EcsExclusive);
    return true;
}

bool EcsUiProjectionSetNode(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t slot,
    ecs_entity_t ui_node)
{
    if (world == NULL || source == 0 || slot == 0 || ui_node == 0) {
        return false;
    }
    if (!EcsUiProjectionRegisterSlot(world, slot)) {
        return false;
    }

    ecs_add_pair(world, source, slot, ui_node);
    return true;
}

ecs_entity_t EcsUiProjectionGetNode(
    const ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t slot)
{
    if (world == NULL || source == 0 || slot == 0) {
        return 0;
    }
    return ecs_get_target(world, source, slot, 0);
}

static bool EcsUiProjectionAppendChild(
    ecs_entity_t *ordered,
    int32_t *count,
    ecs_entity_t child)
{
    if (ordered == NULL || count == NULL || child == 0 ||
        *count >= (int32_t)ECS_UI_TREE_NODE_MAX) {
        return false;
    }

    ordered[*count] = child;
    *count += 1;
    return true;
}

bool EcsUiProjectionSyncOrderedChildren(
    ecs_world_t *world,
    EcsUiProjectionOrderSyncDesc desc)
{
    if (world == NULL || desc.source_parent == 0 || desc.ui_parent == 0 ||
        !EcsUiProjectionReady()) {
        return false;
    }

    ecs_entity_t ordered[ECS_UI_TREE_NODE_MAX] = {0};
    int32_t ordered_count = 0;

    ecs_entities_t ui_children =
        ecs_get_ordered_children(world, desc.ui_parent);
    if (desc.preserve_unprojected_ui_children) {
        for (int32_t i = 0; i < ui_children.count; i += 1) {
            if (ecs_has_id(
                    world,
                    ui_children.ids[i],
                    EcsUiProjectionRootNode)) {
                continue;
            }
            if (!EcsUiProjectionAppendChild(
                    ordered,
                    &ordered_count,
                    ui_children.ids[i])) {
                return false;
            }
        }
    }

    uint32_t position = 1u;
    ecs_entities_t sources =
        ecs_get_ordered_children(world, desc.source_parent);
    for (int32_t i = 0; i < sources.count; i += 1) {
        ecs_entity_t source = sources.ids[i];
        if (desc.source_filter != 0 && !ecs_has_id(world, source, desc.source_filter)) {
            continue;
        }

        ecs_entity_t ui_root = EcsUiProjectionGetRoot(world, source);
        if (ui_root != 0) {
            if (!EcsUiProjectionAppendChild(
                    ordered,
                    &ordered_count,
                    ui_root)) {
                return false;
            }
            if (desc.on_projected != NULL) {
                desc.on_projected(
                    world,
                    source,
                    ui_root,
                    position,
                    desc.ctx);
            }
        }
        position += 1u;
    }

    if (ordered_count != ui_children.count) {
        return false;
    }
    if (ordered_count == 0) {
        return true;
    }
    ecs_set_child_order(
        world,
        desc.ui_parent,
        ordered,
        ordered_count);
    return true;
}

void EcsUiProjectionCollectionBufferInit(
    EcsUiProjectionCollectionBuffer *buffer,
    size_t item_size)
{
    if (buffer == NULL) {
        return;
    }

    buffer->item_size = item_size;
    buffer->item_count = 0u;
    buffer->truncated = item_size == 0u ||
        item_size > (size_t)ECS_UI_PROJECTION_ITEM_MAX;
}

bool EcsUiProjectionCollectionBufferPush(
    EcsUiProjectionCollectionBuffer *buffer,
    uint64_t key,
    const void *item)
{
    if (buffer == NULL || key == 0u || item == NULL ||
        buffer->item_size == 0u ||
        buffer->item_size > (size_t)ECS_UI_PROJECTION_ITEM_MAX) {
        if (buffer != NULL) {
            buffer->truncated = true;
        }
        return false;
    }

    if (buffer->item_count >= (uint32_t)ECS_UI_TREE_NODE_MAX) {
        buffer->truncated = true;
        return false;
    }

    unsigned char *storage = buffer->storage[buffer->item_count].bytes;
    (void)memcpy(storage, item, buffer->item_size);
    buffer->items[buffer->item_count] = (EcsUiProjectionCollectionSource){
        .key = key,
        .data = storage,
    };
    buffer->item_count += 1u;
    return true;
}

uint32_t EcsUiProjectionCollectionBufferCount(
    const EcsUiProjectionCollectionBuffer *buffer)
{
    return buffer != NULL ? buffer->item_count : 0u;
}

bool EcsUiProjectionCollectionBufferOk(
    const EcsUiProjectionCollectionBuffer *buffer)
{
    return buffer != NULL && !buffer->truncated &&
        buffer->item_size > 0u &&
        buffer->item_size <= (size_t)ECS_UI_PROJECTION_ITEM_MAX;
}

static bool EcsUiProjectionKeyInItems(
    const EcsUiProjectionCollectionSource *items,
    uint32_t item_count,
    uint64_t key)
{
    for (uint32_t i = 0u; i < item_count; i += 1u) {
        if (items[i].key == key) {
            return true;
        }
    }
    return false;
}

typedef struct EcsUiProjectionOrderedEntityItem {
    uint64_t key;
    ecs_entity_t source;
} EcsUiProjectionOrderedEntityItem;

static bool EcsUiProjectionKeyInEntityItems(
    const EcsUiProjectionOrderedEntityItem *items,
    uint32_t item_count,
    uint64_t key)
{
    for (uint32_t i = 0u; i < item_count; i += 1u) {
        if (items[i].key == key) {
            return true;
        }
    }
    return false;
}

static ecs_entity_t EcsUiProjectionFindCollectionSource(
    ecs_world_t *world,
    ecs_entity_t source_parent,
    uint64_t key)
{
    ecs_iter_t it = ecs_each(world, EcsUiProjectionKey);
    while (ecs_each_next(&it)) {
        const EcsUiProjectionKey *keys =
            ecs_field(&it, EcsUiProjectionKey, 0);
        for (int32_t i = 0; i < it.count; i += 1) {
            if (keys[i].value == key &&
                ecs_get_parent(world, it.entities[i]) == source_parent) {
                return it.entities[i];
            }
        }
    }
    return 0;
}

static ecs_entity_t EcsUiProjectionEnsureCollectionSource(
    ecs_world_t *world,
    EcsUiProjectionCollectionDesc *desc,
    const EcsUiProjectionCollectionSource *item)
{
    if (world == NULL || desc == NULL || item == NULL || item->key == 0u) {
        return 0;
    }

    ecs_entity_t source =
        EcsUiProjectionFindCollectionSource(
            world,
            desc->source_parent,
            item->key);
    if (source == 0) {
        char name[ECS_UI_ID_MAX] = {0};
        const char *prefix = desc->source_name_prefix != NULL ?
            desc->source_name_prefix :
            "ProjectionSource";
        (void)snprintf(
            name,
            sizeof(name),
            "%s%llu",
            prefix,
            (unsigned long long)item->key);
        source = ecs_entity(world, {
            .parent = desc->source_parent,
            .name = name,
            .sep = "",
        });
        ecs_set(
            world,
            source,
            EcsUiProjectionKey,
            {
                .value = item->key,
            });
    }

    if (source != 0 && desc->source_filter != 0) {
        ecs_add_id(world, source, desc->source_filter);
    }
    if (source != 0 && desc->sync_source != NULL) {
        desc->sync_source(world, source, item, desc->ctx);
    }
    return source;
}

static bool EcsUiProjectionDeleteStaleCollectionSources(
    ecs_world_t *world,
    EcsUiProjectionCollectionDesc *desc)
{
    ecs_entity_t stale_sources[ECS_UI_TREE_NODE_MAX] = {0};
    int32_t stale_count = 0;
    ecs_entities_t children =
        ecs_get_ordered_children(world, desc->source_parent);
    for (int32_t i = 0; i < children.count; i += 1) {
        const EcsUiProjectionKey *key =
            ecs_get(world, children.ids[i], EcsUiProjectionKey);
        if (key == NULL ||
            EcsUiProjectionKeyInItems(
                desc->items,
                desc->item_count,
                key->value)) {
            continue;
        }
        if (stale_count >= (int32_t)ECS_UI_TREE_NODE_MAX) {
            return false;
        }
        stale_sources[stale_count] = children.ids[i];
        stale_count += 1;
    }

    for (int32_t i = 0; i < stale_count; i += 1) {
        ecs_entity_t ui_root =
            EcsUiProjectionGetRoot(world, stale_sources[i]);
        if (ui_root != 0) {
            ecs_delete(world, ui_root);
        }
        ecs_delete(world, stale_sources[i]);
    }
    return true;
}

static bool EcsUiProjectionDeleteStaleOrderedEntitySources(
    ecs_world_t *ui_world,
    ecs_entity_t ui_source_parent,
    const EcsUiProjectionOrderedEntityItem *items,
    uint32_t item_count)
{
    ecs_entity_t stale_sources[ECS_UI_TREE_NODE_MAX] = {0};
    int32_t stale_count = 0;
    ecs_entities_t children =
        ecs_get_ordered_children(ui_world, ui_source_parent);
    for (int32_t i = 0; i < children.count; i += 1) {
        const EcsUiProjectionKey *key =
            ecs_get(ui_world, children.ids[i], EcsUiProjectionKey);
        if (key == NULL ||
            EcsUiProjectionKeyInEntityItems(
                items,
                item_count,
                key->value)) {
            continue;
        }
        if (stale_count >= (int32_t)ECS_UI_TREE_NODE_MAX) {
            return false;
        }
        stale_sources[stale_count] = children.ids[i];
        stale_count += 1;
    }

    for (int32_t i = 0; i < stale_count; i += 1) {
        ecs_entity_t ui_root =
            EcsUiProjectionGetRoot(ui_world, stale_sources[i]);
        if (ui_root != 0) {
            ecs_delete(ui_world, ui_root);
        }
        ecs_delete(ui_world, stale_sources[i]);
    }
    return true;
}

static bool EcsUiProjectionBeginPreserveChildren(
    ecs_world_t *world,
    ecs_entity_t parent,
    bool *already_present)
{
    if (world == NULL || parent == 0 ||
        EcsUiProjectionPreserveChildren == 0 ||
        already_present == NULL) {
        return false;
    }

    *already_present =
        ecs_has_id(world, parent, EcsUiProjectionPreserveChildren);
    if (!*already_present) {
        ecs_add_id(world, parent, EcsUiProjectionPreserveChildren);
    }
    return true;
}

static void EcsUiProjectionEndPreserveChildren(
    ecs_world_t *world,
    ecs_entity_t parent,
    bool already_present)
{
    if (world == NULL || parent == 0 ||
        EcsUiProjectionPreserveChildren == 0 ||
        already_present) {
        return;
    }
    ecs_remove_id(world, parent, EcsUiProjectionPreserveChildren);
}

static ecs_entity_t EcsUiProjectionEnsureOrderedEntitySource(
    ecs_world_t *ui_world,
    EcsUiProjectionOrderedEntityDesc *desc,
    const EcsUiProjectionOrderedEntityItem *item)
{
    if (ui_world == NULL || desc == NULL || item == NULL || item->key == 0u) {
        return 0;
    }

    ecs_entity_t ui_source =
        EcsUiProjectionFindCollectionSource(
            ui_world,
            desc->ui_source_parent,
            item->key);
    if (ui_source == 0) {
        char name[ECS_UI_ID_MAX] = {0};
        const char *prefix = desc->ui_source_name_prefix != NULL ?
            desc->ui_source_name_prefix :
            "ProjectionSource";
        (void)snprintf(
            name,
            sizeof(name),
            "%s%llu",
            prefix,
            (unsigned long long)item->key);
        ui_source = ecs_entity(ui_world, {
            .parent = desc->ui_source_parent,
            .name = name,
            .sep = "",
        });
        ecs_set(
            ui_world,
            ui_source,
            EcsUiProjectionKey,
            {
                .value = item->key,
            });
    }

    if (ui_source != 0 && desc->ui_source_filter != 0) {
        ecs_add_id(ui_world, ui_source, desc->ui_source_filter);
    }
    if (ui_source != 0 && desc->sync_source != NULL) {
        desc->sync_source(
            ui_world,
            ui_source,
            desc->source_world,
            item->source,
            desc->ctx);
    }
    return ui_source;
}

bool EcsUiProjectionSyncCollection(
    ecs_world_t *world,
    EcsUiProjectionCollectionDesc desc)
{
    if (world == NULL || desc.source_parent == 0 || desc.ui_parent == 0 ||
        (desc.item_count > 0u && desc.items == NULL) ||
        desc.item_count > (uint32_t)ECS_UI_TREE_NODE_MAX ||
        !EcsUiProjectionReady()) {
        return false;
    }
    if (ecs_is_deferred(world)) {
        return false;
    }

    ecs_add_id(world, desc.source_parent, EcsOrderedChildren);
    if (!EcsUiProjectionDeleteStaleCollectionSources(world, &desc)) {
        return false;
    }

    ecs_entity_t ordered_sources[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t projected_count = 0u;
    for (uint32_t i = 0u; i < desc.item_count; i += 1u) {
        const EcsUiProjectionCollectionSource *item = &desc.items[i];
        ecs_entity_t source =
            EcsUiProjectionEnsureCollectionSource(world, &desc, item);
        if (source == 0) {
            continue;
        }
        ordered_sources[projected_count] = source;
        projected_count += 1u;

        ecs_entity_t ui_root = EcsUiProjectionGetRoot(world, source);
        if (ui_root == 0 && desc.build_root != NULL) {
            bool preserve_already_present = false;
            if (!EcsUiProjectionBeginPreserveChildren(
                    world,
                    desc.ui_parent,
                    &preserve_already_present)) {
                return false;
            }
            ui_root = desc.build_root(world, source, item, desc.ctx);
            EcsUiProjectionEndPreserveChildren(
                world,
                desc.ui_parent,
                preserve_already_present);
            if (ui_root != 0 && EcsUiProjectionGetRoot(world, source) == 0) {
                (void)EcsUiProjectionLink(world, source, ui_root);
            }
        }
        if (ui_root != 0 && desc.update_root != NULL) {
            desc.update_root(
                world,
                source,
                ui_root,
                item,
                i + 1u,
                desc.item_count,
                desc.ctx);
        }
    }

    if (projected_count > 0u) {
        ecs_set_child_order(
            world,
            desc.source_parent,
            ordered_sources,
            (int32_t)projected_count);
    } else {
        ecs_entities_t source_children =
            ecs_get_ordered_children(world, desc.source_parent);
        if (source_children.count != 0) {
            return false;
        }
    }

    return EcsUiProjectionSyncOrderedChildren(
        world,
        (EcsUiProjectionOrderSyncDesc){
            .source_parent = desc.source_parent,
            .ui_parent = desc.ui_parent,
            .source_filter = desc.source_filter,
            .preserve_unprojected_ui_children =
                desc.preserve_unprojected_ui_children,
        });
}

bool EcsUiProjectionSyncCollectionView(
    ecs_world_t *world,
    EcsUiProjectionCollectionViewDesc desc)
{
    if (desc.items == NULL ||
        !EcsUiProjectionCollectionBufferOk(desc.items)) {
        return false;
    }

    return EcsUiProjectionSyncCollection(
        world,
        (EcsUiProjectionCollectionDesc){
            .source_parent = desc.source_parent,
            .ui_parent = desc.ui_parent,
            .source_filter = desc.source_filter,
            .items = desc.items->items,
            .item_count = desc.items->item_count,
            .preserve_unprojected_ui_children =
                desc.preserve_unprojected_ui_children,
            .source_name_prefix = desc.source_name_prefix,
            .sync_source = desc.sync_source,
            .build_root = desc.build_root,
            .update_root = desc.update_root,
            .ctx = desc.ctx,
        });
}

bool EcsUiProjectionSyncOrderedEntities(
    EcsUiProjectionOrderedEntityDesc desc)
{
    if (desc.out_projected_count != NULL) {
        *desc.out_projected_count = 0u;
    }
    if (desc.source_world == NULL || desc.source_parent == 0 ||
        desc.ui_world == NULL || desc.ui_source_parent == 0 ||
        desc.ui_parent == 0 || desc.key == NULL ||
        !EcsUiProjectionReady()) {
        return false;
    }
    if (ecs_is_deferred(desc.source_world) || ecs_is_deferred(desc.ui_world)) {
        return false;
    }

    EcsUiProjectionOrderedEntityItem items[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t item_count = 0u;
    ecs_entities_t source_children =
        ecs_get_ordered_children(desc.source_world, desc.source_parent);
    for (int32_t i = 0; i < source_children.count; i += 1) {
        ecs_entity_t source = source_children.ids[i];
        if (desc.source_filter != 0 &&
            !ecs_has_id(desc.source_world, source, desc.source_filter)) {
            continue;
        }
        if (item_count >= (uint32_t)ECS_UI_TREE_NODE_MAX) {
            return false;
        }

        uint64_t key = desc.key(desc.source_world, source, desc.ctx);
        if (key == 0u ||
            EcsUiProjectionKeyInEntityItems(items, item_count, key)) {
            return false;
        }
        items[item_count] = (EcsUiProjectionOrderedEntityItem){
            .key = key,
            .source = source,
        };
        item_count += 1u;
    }

    ecs_add_id(desc.ui_world, desc.ui_source_parent, EcsOrderedChildren);
    if (!EcsUiProjectionDeleteStaleOrderedEntitySources(
            desc.ui_world,
            desc.ui_source_parent,
            items,
            item_count)) {
        return false;
    }

    ecs_entity_t ordered_sources[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t projected_count = 0u;
    for (uint32_t i = 0u; i < item_count; i += 1u) {
        const EcsUiProjectionOrderedEntityItem *item = &items[i];
        ecs_entity_t ui_source =
            EcsUiProjectionEnsureOrderedEntitySource(
                desc.ui_world,
                &desc,
                item);
        if (ui_source == 0) {
            return false;
        }
        ordered_sources[projected_count] = ui_source;
        projected_count += 1u;

        ecs_entity_t ui_root =
            EcsUiProjectionGetRoot(desc.ui_world, ui_source);
        if (ui_root == 0 && desc.build_root != NULL) {
            bool preserve_already_present = false;
            if (!EcsUiProjectionBeginPreserveChildren(
                    desc.ui_world,
                    desc.ui_parent,
                    &preserve_already_present)) {
                return false;
            }
            ui_root = desc.build_root(
                desc.ui_world,
                ui_source,
                desc.source_world,
                item->source,
                desc.ctx);
            EcsUiProjectionEndPreserveChildren(
                desc.ui_world,
                desc.ui_parent,
                preserve_already_present);
            if (ui_root != 0 &&
                EcsUiProjectionGetRoot(desc.ui_world, ui_source) == 0) {
                (void)EcsUiProjectionLink(desc.ui_world, ui_source, ui_root);
            }
        }
        if (ui_root == 0) {
            return false;
        }
        if (desc.update_root != NULL) {
            desc.update_root(
                desc.ui_world,
                ui_source,
                ui_root,
                desc.source_world,
                item->source,
                i + 1u,
                item_count,
                desc.ctx);
        }
    }

    if (projected_count > 0u) {
        ecs_set_child_order(
            desc.ui_world,
            desc.ui_source_parent,
            ordered_sources,
            (int32_t)projected_count);
    } else {
        ecs_entities_t ui_source_children =
            ecs_get_ordered_children(desc.ui_world, desc.ui_source_parent);
        if (ui_source_children.count != 0) {
            return false;
        }
    }

    if (!EcsUiProjectionSyncOrderedChildren(
            desc.ui_world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = desc.ui_source_parent,
                .ui_parent = desc.ui_parent,
                .source_filter = desc.ui_source_filter,
                .preserve_unprojected_ui_children =
                    desc.preserve_unprojected_ui_children,
            })) {
        return false;
    }

    if (desc.out_projected_count != NULL) {
        *desc.out_projected_count = projected_count;
    }
    return true;
}
