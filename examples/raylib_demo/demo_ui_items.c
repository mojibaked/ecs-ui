#include "demo_ui_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct DemoUiItemProjectionContext {
    const DemoUiRefs *refs;
    uint32_t selected_item_id;
} DemoUiItemProjectionContext;

static uint32_t DemoUiReadAppItems(
    ecs_world_t *app_world,
    DemoUiItemSource *out_items,
    uint32_t max_items)
{
    if (app_world == NULL || out_items == NULL || max_items == 0u) {
        return 0u;
    }

    uint32_t count = 0u;
    ecs_entities_t children =
        ecs_get_ordered_children(app_world, DemoAppItemRoot(app_world));
    for (int32_t i = 0; i < children.count && count < max_items; i += 1) {
        const DemoItem *item =
            ecs_get(app_world, children.ids[i], DemoItem);
        if (item == NULL) {
            continue;
        }

        out_items[count] = (DemoUiItemSource){
            .id = item->id,
            .rename_count = item->rename_count,
        };
        (void)snprintf(
            out_items[count].label,
            sizeof(out_items[count].label),
            "%s",
            item->label);
        count += 1u;
    }
    return count;
}

static void DemoUiSetStatusForItemCount(
    ecs_world_t *ui_world,
    uint32_t count,
    uint32_t selected_item_id)
{
    char status[ECS_UI_TEXT_MAX] = {0};
    if (selected_item_id != 0u) {
        (void)snprintf(
            status,
            sizeof(status),
            "%u items | selected item %u",
            count,
            selected_item_id);
    } else {
        (void)snprintf(status, sizeof(status), "%u items", count);
    }
    DemoUiSetStatus(ui_world, status);
}

static bool DemoUiRefsReady(const DemoUiRefs *refs)
{
    return refs != NULL && refs->item_list != 0 &&
        refs->select_item_action != 0 && refs->delete_item_action != 0 &&
        refs->rename_item_action != 0 && refs->move_item_up_action != 0 &&
        refs->move_item_down_action != 0;
}

static void DemoUiSyncItemSource(
    ecs_world_t *ui_world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx)
{
    (void)ctx;

    const DemoUiItemSource *next =
        item != NULL ? item->data : NULL;
    if (ui_world == NULL || source == 0 || next == NULL) {
        return;
    }

    DemoUiItemSource *current =
        ecs_get_mut(ui_world, source, DemoUiItemSource);
    if (current == NULL) {
        ecs_set_ptr(ui_world, source, DemoUiItemSource, next);
        return;
    }

    if (current->id == next->id &&
        current->rename_count == next->rename_count &&
        strcmp(current->label, next->label) == 0) {
        return;
    }

    *current = *next;
    ecs_modified(ui_world, source, DemoUiItemSource);
}

static ecs_entity_t DemoUiBuildItemRoot(
    ecs_world_t *ui_world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx)
{
    DemoUiItemProjectionContext *projection_ctx = ctx;
    const DemoUiItemSource *item_data =
        item != NULL ? item->data : NULL;
    if (projection_ctx == NULL || projection_ctx->refs == NULL ||
        item_data == NULL) {
        return 0;
    }

    return DemoUiCreateItemRow(
        ui_world,
        source,
        item_data,
        projection_ctx->refs);
}

static void DemoUiUpdateItemRoot(
    ecs_world_t *ui_world,
    ecs_entity_t source,
    ecs_entity_t ui_root,
    const EcsUiProjectionCollectionSource *item,
    uint32_t position,
    uint32_t count,
    void *ctx)
{
    (void)ui_root;

    DemoUiItemProjectionContext *projection_ctx = ctx;
    const DemoUiItemSource *item_data =
        item != NULL ? item->data : NULL;
    if (projection_ctx == NULL || item_data == NULL) {
        return;
    }

    DemoUiUpdateItemRowWithPosition(
        ui_world,
        source,
        item_data,
        position,
        count);
    DemoUiApplyItemSelectionStyle(
        ui_world,
        source,
        projection_ctx->selected_item_id);
}

void DemoUiSyncProjection(ecs_world_t *ui_world, ecs_world_t *app_world)
{
    if (ui_world == NULL || app_world == NULL) {
        return;
    }

    const DemoUiRefs *refs = ecs_singleton_get(ui_world, DemoUiRefs);
    if (!DemoUiRefsReady(refs)) {
        return;
    }

    DemoUiItemSource items[ECS_UI_TREE_NODE_MAX] = {0};
    EcsUiProjectionCollectionSource projection_items[ECS_UI_TREE_NODE_MAX] = {0};
    const uint32_t item_count =
        DemoUiReadAppItems(app_world, items, ECS_UI_TREE_NODE_MAX);
    for (uint32_t i = 0u; i < item_count; i += 1u) {
        projection_items[i] = (EcsUiProjectionCollectionSource){
            .key = items[i].id,
            .data = &items[i],
        };
    }

    DemoUiItemProjectionContext ctx = {
        .refs = refs,
        .selected_item_id = DemoAppSelectedItemId(app_world),
    };
    (void)EcsUiProjectionSyncCollection(
        ui_world,
        (EcsUiProjectionCollectionDesc){
            .source_parent = DemoUiItemSourceRoot(ui_world),
            .ui_parent = refs->item_list,
            .source_filter = ecs_id(DemoUiItemSource),
            .items = projection_items,
            .item_count = item_count,
            .preserve_unprojected_ui_children = true,
            .source_name_prefix = "ItemSource",
            .sync_source = DemoUiSyncItemSource,
            .build_root = DemoUiBuildItemRoot,
            .update_root = DemoUiUpdateItemRoot,
            .ctx = &ctx,
        });

    ecs_entity_t item_root = DemoAppItemRoot(app_world);
    if (ecs_has_id(app_world, item_root, DemoItemOrderDirty)) {
        ecs_remove_id(app_world, item_root, DemoItemOrderDirty);
    }
    DemoUiSetStatusForItemCount(ui_world, item_count, ctx.selected_item_id);
}
