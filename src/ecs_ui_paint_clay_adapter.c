#include "ecs_ui_paint_clay_adapter.h"

#include <stdio.h>
#include <string.h>

extern Clay_ElementId Clay__HashNumber(uint32_t offset, uint32_t seed);

static Clay_String EcsUiPaintClayString(const char *text)
{
    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = text != NULL ? (int32_t)strlen(text) : 0,
        .chars = text != NULL ? text : "",
    };
}

bool EcsUiPaintClayElementId(
    const EcsUiTreeNodeSnapshot *node,
    const char *suffix,
    Clay_ElementId *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (Clay_ElementId){0};
    if (node == NULL) {
        return false;
    }

    char clay_id[ECS_UI_ID_MAX * 2u] = {0};
    const char *authored_id = node->id[0] != '\0' ? node->id : "Node";
    (void)snprintf(
        clay_id,
        sizeof(clay_id),
        "%s_%llu%s",
        authored_id,
        (unsigned long long)node->entity,
        suffix != NULL ? suffix : "");
    *out = Clay_GetElementId(EcsUiPaintClayString(clay_id));
    return out->id != 0u;
}

static uint32_t EcsUiPaintClayChildCount(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    if (tree == NULL || node == NULL) {
        return 0u;
    }

    uint32_t count = 0u;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        count += 1u;
    }
    return count;
}

bool EcsUiPaintClayBorderCommandId(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node,
    Clay_ElementId *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (Clay_ElementId){0};
    Clay_ElementId element_id = {0};
    if (!EcsUiPaintClayElementId(node, NULL, &element_id)) {
        return false;
    }

    *out = Clay__HashNumber(element_id.id, EcsUiPaintClayChildCount(tree, node));
    return out->id != 0u;
}

static const EcsUiTreeNodeSnapshot *EcsUiPaintClayFindNode(
    const EcsUiTreeSnapshot *tree,
    ecs_entity_t entity)
{
    if (tree == NULL || entity == 0) {
        return NULL;
    }
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (tree->nodes[i].entity == entity) {
            return &tree->nodes[i];
        }
    }
    return NULL;
}

static float EcsUiPaintClayScale(const EcsUiPaintClayAdapterOptions *options)
{
    return options != NULL && options->scale > 0.0f ? options->scale : 1.0f;
}

static Clay_Color EcsUiPaintClayColor(EcsUiColorF color, float opacity)
{
    float alpha = opacity;
    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    return (Clay_Color){
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a * alpha,
    };
}

static Clay_CornerRadius EcsUiPaintClayRadius(
    EcsUiPaintCornerRadius radius,
    float scale)
{
    return (Clay_CornerRadius){
        .topLeft = radius.top_left * scale,
        .topRight = radius.top_right * scale,
        .bottomLeft = radius.bottom_left * scale,
        .bottomRight = radius.bottom_right * scale,
    };
}

static uint16_t EcsUiPaintClayU16(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= 65535.0f) {
        return 65535u;
    }
    return (uint16_t)value;
}

static Clay_BorderWidth EcsUiPaintClayBorderWidth(
    EcsUiPaintBorder border,
    float scale)
{
    return (Clay_BorderWidth){
        .left = EcsUiPaintClayU16(border.left * scale),
        .right = EcsUiPaintClayU16(border.right * scale),
        .top = EcsUiPaintClayU16(border.top * scale),
        .bottom = EcsUiPaintClayU16(border.bottom * scale),
    };
}

static bool EcsUiPaintClayBorderWidthAny(Clay_BorderWidth width)
{
    return width.left != 0u ||
        width.right != 0u ||
        width.top != 0u ||
        width.bottom != 0u;
}

static const char *EcsUiPaintClayBevelSuffix(uint16_t part)
{
    switch (part) {
    case ECS_UI_PAINT_BEVEL_EDGE_TOP:
        return "BevelTop";
    case ECS_UI_PAINT_BEVEL_EDGE_LEFT:
        return "BevelLeft";
    case ECS_UI_PAINT_BEVEL_EDGE_BOTTOM:
        return "BevelBottom";
    case ECS_UI_PAINT_BEVEL_EDGE_RIGHT:
        return "BevelRight";
    default:
        return "";
    }
}

static bool EcsUiPaintClayColorVisible(EcsUiColorF color, float opacity)
{
    float alpha = opacity;
    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    return color.a * alpha > 0.0f;
}

static bool EcsUiPaintClayPush(
    Clay_RenderCommandArray *out,
    Clay_RenderCommand command)
{
    if (out == NULL || out->internalArray == NULL ||
            out->length >= out->capacity) {
        return false;
    }
    out->internalArray[out->length] = command;
    out->length += 1;
    return true;
}

static Clay_BoundingBox EcsUiPaintClayBounds(
    EcsUiPaintRect rect,
    const EcsUiPaintClayAdapterOptions *options,
    float scale)
{
    return (Clay_BoundingBox){
        .x = (options != NULL ? options->physical_x : 0.0f) + rect.x * scale,
        .y = (options != NULL ? options->physical_y : 0.0f) + rect.y * scale,
        .width = rect.width * scale,
        .height = rect.height * scale,
    };
}

static bool EcsUiPaintClayPushRectangle(
    Clay_RenderCommandArray *out,
    EcsUiPaintRect rect,
    const EcsUiPaintClayAdapterOptions *options,
    float scale,
    uint32_t id,
    EcsUiColorF color,
    float opacity,
    Clay_CornerRadius radius,
    int16_t z_index)
{
    return EcsUiPaintClayPush(
        out,
        (Clay_RenderCommand){
            .boundingBox = EcsUiPaintClayBounds(rect, options, scale),
            .renderData = {
                .rectangle = {
                    .backgroundColor = EcsUiPaintClayColor(color, opacity),
                    .cornerRadius = radius,
                },
            },
            .id = id,
            .zIndex = z_index,
            .commandType = CLAY_RENDER_COMMAND_TYPE_RECTANGLE,
        });
}

static bool EcsUiPaintClayPushCustom(
    Clay_RenderCommandArray *out,
    EcsUiPaintRect rect,
    const EcsUiPaintClayAdapterOptions *options,
    float scale,
    uint32_t id,
    EcsUiColorF color,
    float opacity,
    const EcsUiTreeNodeSnapshot *node)
{
    return EcsUiPaintClayPush(
        out,
        (Clay_RenderCommand){
            .boundingBox = EcsUiPaintClayBounds(rect, options, scale),
            .renderData = {
                .custom = {
                    .backgroundColor = EcsUiPaintClayColor(color, opacity),
                    .customData = (void *)node,
                },
            },
            .id = id,
            .zIndex = 0,
            .commandType = CLAY_RENDER_COMMAND_TYPE_CUSTOM,
        });
}

bool EcsUiPaintClayAdapterBuild(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintClayAdapterOptions *options,
    Clay_RenderCommand *storage,
    int32_t storage_capacity,
    Clay_RenderCommandArray *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (Clay_RenderCommandArray){
        .capacity = storage_capacity > 0 ? storage_capacity : 0,
        .length = 0,
        .internalArray = storage,
    };
    if (paint == NULL || tree == NULL || storage == NULL ||
            storage_capacity <= 0) {
        return false;
    }

    const float scale = EcsUiPaintClayScale(options);
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        const EcsUiPaintItem *item = &paint->items[i];
        const EcsUiTreeNodeSnapshot *node =
            EcsUiPaintClayFindNode(tree, item->key.source);
        Clay_ElementId element_id = {0};
        (void)EcsUiPaintClayElementId(node, NULL, &element_id);

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX &&
                item->key.role == ECS_UI_PAINT_ROLE_BOX) {
            const Clay_BoundingBox bounds =
                EcsUiPaintClayBounds(item->rect, options, scale);
            const Clay_CornerRadius radius =
                EcsUiPaintClayRadius(item->payload.box.radius, scale);
            if (EcsUiPaintClayColorVisible(
                    item->payload.box.fill,
                    item->opacity) &&
                    !EcsUiPaintClayPushRectangle(
                        out,
                        item->rect,
                        options,
                        scale,
                        element_id.id,
                        item->payload.box.fill,
                        item->opacity,
                        radius,
                        0)) {
                return false;
            }

            if (!item->payload.box.border.has_border) {
                continue;
            }
            const Clay_BorderWidth width =
                EcsUiPaintClayBorderWidth(item->payload.box.border, scale);
            if (!EcsUiPaintClayBorderWidthAny(width)) {
                continue;
            }
            Clay_ElementId border_id = {0};
            (void)EcsUiPaintClayBorderCommandId(tree, node, &border_id);
            if (!EcsUiPaintClayPush(
                    out,
                    (Clay_RenderCommand){
                        .boundingBox = bounds,
                        .renderData = {
                            .border = {
                                .color = EcsUiPaintClayColor(
                                    item->payload.box.border.color,
                                    item->opacity),
                                .cornerRadius = radius,
                                .width = width,
                            },
                        },
                        .id = border_id.id,
                        .zIndex = 0,
                        .commandType = CLAY_RENDER_COMMAND_TYPE_BORDER,
                    })) {
                return false;
            }
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX &&
                item->key.role == ECS_UI_PAINT_ROLE_BEVEL_EDGE) {
            Clay_ElementId bevel_id = {0};
            (void)EcsUiPaintClayElementId(
                node,
                EcsUiPaintClayBevelSuffix(item->key.part),
                &bevel_id);
            if (!EcsUiPaintClayPushRectangle(
                    out,
                    item->rect,
                    options,
                    scale,
                    bevel_id.id,
                    item->payload.bevel_edge.color,
                    item->opacity,
                    (Clay_CornerRadius){0},
                    20)) {
                return false;
            }
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_CUSTOM &&
                (item->key.role == ECS_UI_PAINT_ROLE_NINE_SLICE ||
                    item->key.role == ECS_UI_PAINT_ROLE_CUSTOM ||
                    item->key.role == ECS_UI_PAINT_ROLE_ICON)) {
            if (!EcsUiPaintClayPushCustom(
                    out,
                    item->rect,
                    options,
                    scale,
                    element_id.id,
                    item->payload.custom.color,
                    item->opacity,
                    node)) {
                return false;
            }
        }
    }
    return true;
}
