#include "ecs_ui/ecs_ui_paint.h"
#include "ecs_ui_paint_internal.h"
#include "ecs_ui_style.h"

void EcsUiPaintListReset(
    EcsUiPaintList *list,
    ecs_entity_t tree,
    uint32_t generation)
{
    if (list == NULL) {
        return;
    }
    list->tree = tree;
    list->generation = generation;
    list->count = 0u;
    list->truncated = false;
}

static EcsUiColorF EcsUiPaintTransparent(void)
{
    return (EcsUiColorF){0};
}

static EcsUiColorF EcsUiPaintBoxFill(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiTheme *theme)
{
    if (node == NULL || theme == NULL) {
        return EcsUiPaintTransparent();
    }

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
        return EcsUiStyleContainerBackground(
            node,
            EcsUiStyleColorFrom(theme->root_background));
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        return EcsUiStyleContainerBackground(
            node,
            EcsUiPaintTransparent());
    case ECS_UI_NODE_BUTTON:
        return EcsUiStyleHasNineSlice(node) ?
            EcsUiPaintTransparent() :
            EcsUiStyleButtonColor(theme, node);
    case ECS_UI_NODE_PRESSABLE:
        return EcsUiStyleHasNineSlice(node) ?
            EcsUiPaintTransparent() :
            EcsUiStylePressableColor(theme, node);
    case ECS_UI_NODE_CUSTOM:
        return EcsUiStyleHasNineSlice(node) ?
            EcsUiPaintTransparent() :
            EcsUiStyleColorFrom(theme->surface_subtle);
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        return EcsUiPaintTransparent();
    }
}

static bool EcsUiPaintPushBox(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    EcsUiColorF fill,
    float opacity,
    uint32_t item_capacity)
{
    if (list == NULL || node == NULL) {
        return false;
    }
    if (list->count >= item_capacity || list->count >= ECS_UI_PAINT_ITEM_MAX) {
        list->truncated = true;
        return false;
    }

    const uint32_t order = list->count;
    list->items[list->count] = (EcsUiPaintItem){
        .key = {
            .source = node->entity,
            .role = ECS_UI_PAINT_ROLE_BOX,
            .part = 0u,
            .generation = list->generation,
        },
        .primitive = ECS_UI_PAINT_PRIMITIVE_BOX,
        .rect = {
            .x = node->layout_x,
            .y = node->layout_y,
            .width = node->layout_width,
            .height = node->layout_height,
        },
        .clip = {0},
        .opacity = opacity,
        .order = order,
        .payload = {
            .box = {
                .fill = fill,
            },
        },
    };
    list->count += 1u;
    return true;
}

static bool EcsUiPaintEmitNode(
    EcsUiPaintList *list,
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t index,
    float parent_opacity,
    uint32_t item_capacity)
{
    if (list == NULL || tree == NULL || index >= tree->count) {
        return false;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float opacity =
        parent_opacity * EcsUiStyleClamp01(node->visual.opacity);
    if (opacity <= 0.01f) {
        return true;
    }

    if (node->has_layout) {
        const EcsUiColorF fill = EcsUiPaintBoxFill(node, theme);
        if (fill.a != 0.0f) {
            if (!EcsUiPaintPushBox(
                    list,
                    node,
                    fill,
                    opacity,
                    item_capacity)) {
                return false;
            }
        }
    }

    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX;
         child = tree->nodes[child].next_sibling) {
        if (!EcsUiPaintEmitNode(
                list,
                tree,
                theme,
                child,
                opacity,
                item_capacity)) {
            return false;
        }
    }
    return true;
}

bool EcsUiPaintListBuildWithCapacity(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    uint32_t item_capacity)
{
    if (list == NULL || tree == NULL || theme == NULL) {
        return false;
    }

    if (item_capacity > ECS_UI_PAINT_ITEM_MAX) {
        item_capacity = ECS_UI_PAINT_ITEM_MAX;
    }
    EcsUiPaintListReset(list, tree->root, tree->generation);
    list->truncated = tree->truncated;
    /* 7.2 snapshots contain one root; all roots are therefore z=0 and stable. */
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (tree->nodes[i].parent != 0) {
            continue;
        }
        if (!EcsUiPaintEmitNode(
                list,
                tree,
                theme,
                i,
                1.0f,
                item_capacity)) {
            return false;
        }
    }
    return true;
}

bool EcsUiPaintListBuild(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme)
{
    return EcsUiPaintListBuildWithCapacity(
        list,
        tree,
        theme,
        ECS_UI_PAINT_ITEM_MAX);
}
