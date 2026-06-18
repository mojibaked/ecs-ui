#include "ecs_ui/ecs_ui_projection.h"

#include <stdio.h>

ECS_COMPONENT_DECLARE(EcsUiProjectionKey);
ECS_TAG_DECLARE(EcsUiProjectionRoot);
ECS_TAG_DECLARE(EcsUiProjectionSource);
ECS_TAG_DECLARE(EcsUiProjectionSlot);
ECS_TAG_DECLARE(EcsUiProjectionRootNode);

static bool EcsUiProjectionReady(void)
{
    return ecs_id(EcsUiProjectionKey) != 0 && EcsUiProjectionRoot != 0 &&
        EcsUiProjectionSource != 0 && EcsUiProjectionSlot != 0 &&
        EcsUiProjectionRootNode != 0;
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
            ui_root = desc.build_root(world, source, item, desc.ctx);
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
