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
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_CUSTOM:
    case ECS_UI_NODE_NONE:
    default:
        return EcsUiPaintTransparent();
    }
}

static EcsUiPaintCornerRadius EcsUiPaintBoxRadius(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiTheme *theme,
    EcsUiColorF fill)
{
    if (node == NULL || theme == NULL) {
        return (EcsUiPaintCornerRadius){0};
    }

    float fallback = 0.0f;
    bool radius_enabled = fill.a != 0.0f;
    switch (node->kind) {
    case ECS_UI_NODE_BUTTON:
    case ECS_UI_NODE_PRESSABLE:
    case ECS_UI_NODE_CUSTOM:
        fallback = theme->radius;
        radius_enabled = true;
        break;
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
    default:
        break;
    }
    if (!radius_enabled) {
        return (EcsUiPaintCornerRadius){0};
    }

    const float radius = EcsUiStyleCornerRadius(node, fallback);
    return (EcsUiPaintCornerRadius){
        .top_left = radius,
        .top_right = radius,
        .bottom_left = radius,
        .bottom_right = radius,
    };
}

static EcsUiPaintRect EcsUiPaintNodeRect(const EcsUiTreeNodeSnapshot *node)
{
    return node != NULL ?
        (EcsUiPaintRect){
            .x = node->layout_x,
            .y = node->layout_y,
            .width = node->layout_width,
            .height = node->layout_height,
        } :
        (EcsUiPaintRect){0};
}

static EcsUiPaintItem *EcsUiPaintReserve(
    EcsUiPaintList *list,
    uint32_t item_capacity)
{
    if (list == NULL) {
        return NULL;
    }
    if (list->count >= item_capacity || list->count >= ECS_UI_PAINT_ITEM_MAX) {
        list->truncated = true;
        return NULL;
    }

    EcsUiPaintItem *item = &list->items[list->count];
    *item = (EcsUiPaintItem){0};
    item->order = list->count;
    list->count += 1u;
    return item;
}

static bool EcsUiPaintPushBox(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    EcsUiColorF fill,
    EcsUiPaintCornerRadius radius,
    EcsUiPaintBorder border,
    float opacity,
    uint32_t item_capacity)
{
    if (list == NULL || node == NULL) {
        return false;
    }
    EcsUiPaintItem *item = EcsUiPaintReserve(list, item_capacity);
    if (item == NULL) {
        return false;
    }

    item->key = (EcsUiPaintKey){
        .source = node->entity,
        .role = ECS_UI_PAINT_ROLE_BOX,
        .part = 0u,
        .generation = list->generation,
    };
    item->primitive = ECS_UI_PAINT_PRIMITIVE_BOX;
    item->rect = EcsUiPaintNodeRect(node);
    item->opacity = opacity;
    item->payload.box = (EcsUiPaintBox){
        .fill = fill,
        .radius = radius,
        .border = border,
    };
    return true;
}

static bool EcsUiPaintPushCustom(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    uint16_t role,
    EcsUiColorF color,
    float opacity,
    uint32_t item_capacity)
{
    if (list == NULL || node == NULL) {
        return false;
    }
    EcsUiPaintItem *item = EcsUiPaintReserve(list, item_capacity);
    if (item == NULL) {
        return false;
    }

    item->key = (EcsUiPaintKey){
        .source = node->entity,
        .role = role,
        .part = 0u,
        .generation = list->generation,
    };
    item->primitive = ECS_UI_PAINT_PRIMITIVE_CUSTOM;
    item->rect = EcsUiPaintNodeRect(node);
    item->opacity = opacity;
    item->payload.custom = (EcsUiPaintCustom){
        .source = node->entity,
        .color = color,
    };
    return true;
}

static EcsUiPaintRect EcsUiPaintBevelRect(
    const EcsUiTreeNodeSnapshot *node,
    uint16_t part)
{
    const EcsUiPaintRect rect = EcsUiPaintNodeRect(node);
    switch (part) {
    case ECS_UI_PAINT_BEVEL_EDGE_TOP:
        return (EcsUiPaintRect){
            .x = rect.x,
            .y = rect.y,
            .width = rect.width,
            .height = 1.0f,
        };
    case ECS_UI_PAINT_BEVEL_EDGE_LEFT:
        return (EcsUiPaintRect){
            .x = rect.x,
            .y = rect.y,
            .width = 1.0f,
            .height = rect.height,
        };
    case ECS_UI_PAINT_BEVEL_EDGE_BOTTOM:
        return (EcsUiPaintRect){
            .x = rect.x,
            .y = rect.y + rect.height - 1.0f,
            .width = rect.width,
            .height = 1.0f,
        };
    case ECS_UI_PAINT_BEVEL_EDGE_RIGHT:
    default:
        return (EcsUiPaintRect){
            .x = rect.x + rect.width - 1.0f,
            .y = rect.y,
            .width = 1.0f,
            .height = rect.height,
        };
    }
}

static bool EcsUiPaintPushBevelEdge(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    uint16_t part,
    EcsUiColorF color,
    float opacity,
    uint32_t item_capacity)
{
    if (list == NULL || node == NULL) {
        return false;
    }
    EcsUiPaintItem *item = EcsUiPaintReserve(list, item_capacity);
    if (item == NULL) {
        return false;
    }

    item->key = (EcsUiPaintKey){
        .source = node->entity,
        .role = ECS_UI_PAINT_ROLE_BEVEL_EDGE,
        .part = part,
        .generation = list->generation,
    };
    item->primitive = ECS_UI_PAINT_PRIMITIVE_BOX;
    item->rect = EcsUiPaintBevelRect(node, part);
    item->opacity = opacity;
    item->payload.bevel_edge = (EcsUiPaintBevelEdge){
        .color = color,
    };
    return true;
}

static uint16_t EcsUiPaintCustomRole(const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL) {
        return ECS_UI_PAINT_ROLE_NONE;
    }
    if (node->kind == ECS_UI_NODE_ICON) {
        return ECS_UI_PAINT_ROLE_ICON;
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        return ECS_UI_PAINT_ROLE_CUSTOM;
    }
    if (EcsUiStyleHasNineSlice(node)) {
        return ECS_UI_PAINT_ROLE_NINE_SLICE;
    }
    return ECS_UI_PAINT_ROLE_NONE;
}

static EcsUiColorF EcsUiPaintCustomColor(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiTheme *theme)
{
    if (node == NULL || theme == NULL) {
        return EcsUiPaintTransparent();
    }
    if (node->kind == ECS_UI_NODE_ICON) {
        return EcsUiStyleIconColor();
    }
    if (EcsUiStyleHasNineSlice(node)) {
        return EcsUiStyleNineSliceTint(node);
    }
    if (node->kind == ECS_UI_NODE_CUSTOM) {
        return EcsUiStyleColorFrom(theme->surface_subtle);
    }
    return EcsUiPaintTransparent();
}

static bool EcsUiPaintEmitBevel(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    float opacity,
    uint32_t item_capacity)
{
    if (!EcsUiStyleHasDrawableBevel(node)) {
        return true;
    }

    const EcsUiColorF top_left = EcsUiStyleBevelTopLeftColor(node);
    const EcsUiColorF bottom_right = EcsUiStyleBevelBottomRightColor(node);
    return EcsUiPaintPushBevelEdge(
            list,
            node,
            ECS_UI_PAINT_BEVEL_EDGE_TOP,
            top_left,
            opacity,
            item_capacity) &&
        EcsUiPaintPushBevelEdge(
            list,
            node,
            ECS_UI_PAINT_BEVEL_EDGE_LEFT,
            top_left,
            opacity,
            item_capacity) &&
        EcsUiPaintPushBevelEdge(
            list,
            node,
            ECS_UI_PAINT_BEVEL_EDGE_BOTTOM,
            bottom_right,
            opacity,
            item_capacity) &&
        EcsUiPaintPushBevelEdge(
            list,
            node,
            ECS_UI_PAINT_BEVEL_EDGE_RIGHT,
            bottom_right,
            opacity,
            item_capacity);
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
        const EcsUiPaintBorder border = EcsUiStyleBorder(node);
        if (fill.a != 0.0f || border.has_border) {
            const EcsUiPaintCornerRadius radius =
                EcsUiPaintBoxRadius(node, theme, fill);
            if (!EcsUiPaintPushBox(
                    list,
                    node,
                    fill,
                    radius,
                    border,
                    opacity,
                    item_capacity)) {
                return false;
            }
        }

        const uint16_t custom_role = EcsUiPaintCustomRole(node);
        if (custom_role != ECS_UI_PAINT_ROLE_NONE) {
            if (!EcsUiPaintPushCustom(
                    list,
                    node,
                    custom_role,
                    EcsUiPaintCustomColor(node, theme),
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
    return EcsUiPaintEmitBevel(list, node, opacity, item_capacity);
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
