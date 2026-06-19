#include "demo_ui_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct DemoUiItemProjectionContext {
    const DemoUiRefs *refs;
    uint32_t selected_item_id;
} DemoUiItemProjectionContext;

static bool DemoUiItemSourceFromApp(
    ecs_world_t *app_world,
    ecs_entity_t item,
    DemoUiItemSource *out)
{
    if (app_world == NULL || item == 0 || out == NULL) {
        return false;
    }
    const DemoItem *item_data = ecs_get(app_world, item, DemoItem);
    if (item_data == NULL || item_data->id == 0u) {
        return false;
    }

    *out = (DemoUiItemSource){
        .id = item_data->id,
        .rename_count = item_data->rename_count,
    };
    (void)snprintf(
        out->label,
        sizeof(out->label),
        "%s",
        item_data->label);
    return true;
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

static uint64_t DemoUiItemProjectionKey(
    ecs_world_t *app_world,
    ecs_entity_t item,
    void *ctx)
{
    (void)ctx;

    DemoUiItemSource source = {0};
    return DemoUiItemSourceFromApp(app_world, item, &source) ?
        (uint64_t)source.id :
        0u;
}

static void DemoUiSyncItemSource(
    ecs_world_t *ui_world,
    ecs_entity_t source,
    ecs_world_t *app_world,
    ecs_entity_t app_item,
    void *ctx)
{
    (void)ctx;

    DemoUiItemSource next = {0};
    if (ui_world == NULL || source == 0 ||
        !DemoUiItemSourceFromApp(app_world, app_item, &next)) {
        return;
    }

    DemoUiItemSource *current =
        ecs_get_mut(ui_world, source, DemoUiItemSource);
    if (current == NULL) {
        ecs_set_ptr(ui_world, source, DemoUiItemSource, &next);
        return;
    }

    if (current->id == next.id &&
        current->rename_count == next.rename_count &&
        strcmp(current->label, next.label) == 0) {
        return;
    }

    *current = next;
    ecs_modified(ui_world, source, DemoUiItemSource);
}

static ecs_entity_t DemoUiBuildItemRoot(
    ecs_world_t *ui_world,
    ecs_entity_t source,
    ecs_world_t *app_world,
    ecs_entity_t app_item,
    void *ctx)
{
    (void)app_world;
    (void)app_item;

    DemoUiItemProjectionContext *projection_ctx = ctx;
    const DemoUiItemSource *item_data =
        ecs_get(ui_world, source, DemoUiItemSource);
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
    ecs_world_t *app_world,
    ecs_entity_t app_item,
    uint32_t position,
    uint32_t count,
    void *ctx)
{
    (void)ui_root;
    (void)app_world;
    (void)app_item;

    DemoUiItemProjectionContext *projection_ctx = ctx;
    const DemoUiItemSource *item_data =
        ecs_get(ui_world, source, DemoUiItemSource);
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

    DemoUiItemProjectionContext ctx = {
        .refs = refs,
        .selected_item_id = DemoAppSelectedItemId(app_world),
    };
    uint32_t projected_count = 0u;
    (void)EcsUiProjectionSyncOrderedEntities(
        (EcsUiProjectionOrderedEntityDesc){
            .source_world = app_world,
            .source_parent = DemoAppItemRoot(app_world),
            .source_filter = ecs_id(DemoItem),
            .ui_world = ui_world,
            .ui_source_parent = DemoUiItemSourceRoot(ui_world),
            .ui_parent = refs->item_list,
            .ui_source_filter = ecs_id(DemoUiItemSource),
            .preserve_unprojected_ui_children = true,
            .ui_source_name_prefix = "ItemSource",
            .key = DemoUiItemProjectionKey,
            .sync_source = DemoUiSyncItemSource,
            .build_root = DemoUiBuildItemRoot,
            .update_root = DemoUiUpdateItemRoot,
            .out_projected_count = &projected_count,
            .ctx = &ctx,
        });

    ecs_entity_t item_root = DemoAppItemRoot(app_world);
    if (ecs_has_id(app_world, item_root, DemoItemOrderDirty)) {
        ecs_remove_id(app_world, item_root, DemoItemOrderDirty);
    }
    DemoUiSetStatusForItemCount(
        ui_world,
        projected_count,
        ctx.selected_item_id);
}
