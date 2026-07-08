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

static uint32_t EcsUiPaintClayClampTextIndex(uint32_t index, size_t length)
{
    return index <= length ? index : (uint32_t)length;
}

static uint32_t EcsUiPaintClayInlineTextChildCount(
    uint32_t start,
    uint32_t end)
{
    return start != end ? 1u : 0u;
}

static bool EcsUiPaintClayIsDirectTextFieldValue(
    const EcsUiTreeNodeSnapshot *field,
    const EcsUiTreeNodeSnapshot *child)
{
    return field != NULL &&
        child != NULL &&
        field->kind == ECS_UI_NODE_PRESSABLE &&
        field->has_text_field_view &&
        field->text_field_view.value_node == child->entity &&
        child->kind == ECS_UI_NODE_TEXT;
}

static uint32_t EcsUiPaintClayTextFieldValueChildCount(
    const EcsUiTreeNodeSnapshot *field,
    const EcsUiTreeNodeSnapshot *value)
{
    if (field == NULL || value == NULL || value->kind != ECS_UI_NODE_TEXT) {
        return 0u;
    }

    const char *text = value->text.text;
    const size_t length = strlen(text);
    const EcsUiTextFieldView *view = &field->text_field_view;
    const uint32_t text_end = (uint32_t)length;
    const uint32_t cursor =
        EcsUiPaintClayClampTextIndex(view->cursor, length);
    uint32_t selection_start =
        EcsUiPaintClayClampTextIndex(view->selection_anchor, length);
    uint32_t selection_end =
        EcsUiPaintClayClampTextIndex(view->selection_focus, length);
    if (selection_start > selection_end) {
        uint32_t swap = selection_start;
        selection_start = selection_end;
        selection_end = swap;
    }

    if (!view->focused) {
        return EcsUiPaintClayInlineTextChildCount(0u, text_end);
    }

    if (selection_start >= selection_end) {
        return
            EcsUiPaintClayInlineTextChildCount(0u, cursor) +
            1u +
            EcsUiPaintClayInlineTextChildCount(cursor, text_end);
    }

    return
        EcsUiPaintClayInlineTextChildCount(0u, selection_start) +
        (cursor == selection_start ? 1u : 0u) +
        1u +
        (cursor != selection_start ? 1u : 0u) +
        EcsUiPaintClayInlineTextChildCount(selection_end, text_end);
}

static bool EcsUiPaintClayChildVisible(
    const EcsUiTreeNodeSnapshot *child)
{
    return child != NULL && child->visual.opacity > 0.01f;
}

static uint32_t EcsUiPaintClayChildCount(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    if (tree == NULL || node == NULL) {
        return 0u;
    }
    if (node->kind == ECS_UI_NODE_TEXT) {
        return 1u;
    }

    uint32_t count = 0u;
    bool zstack_first_child = true;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        if (node->kind == ECS_UI_NODE_ZSTACK) {
            if (zstack_first_child) {
                zstack_first_child = false;
                if (!child_node->has_placement &&
                        EcsUiPaintClayChildVisible(child_node)) {
                    count += 1u;
                }
            }
            continue;
        }

        if (EcsUiPaintClayIsDirectTextFieldValue(node, child_node)) {
            count += EcsUiPaintClayTextFieldValueChildCount(
                node,
                child_node);
            continue;
        }
        if (!EcsUiPaintClayChildVisible(child_node)) {
            continue;
        }
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

static Clay_StringSlice EcsUiPaintClayTextSlice(
    const EcsUiPaintTextRun *run)
{
    const char *text = run != NULL && run->text != NULL ? run->text : "";
    uint32_t start = run != NULL ? run->byte_start : 0u;
    uint32_t end = run != NULL ? run->byte_end : 0u;
    const size_t length = strlen(text);
    if (start > length) {
        start = (uint32_t)length;
    }
    if (end > length) {
        end = (uint32_t)length;
    }
    if (start > end) {
        uint32_t swap = start;
        start = end;
        end = swap;
    }
    return (Clay_StringSlice){
        .length = (int32_t)(end - start),
        .chars = &text[start],
        .baseChars = text,
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
    const EcsUiTreeNodeSnapshot *node,
    int16_t z_index)
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
            .zIndex = z_index,
            .commandType = CLAY_RENDER_COMMAND_TYPE_CUSTOM,
        });
}

static bool EcsUiPaintClayPushText(
    Clay_RenderCommandArray *out,
    const EcsUiPaintItem *item,
    const EcsUiPaintClayAdapterOptions *options,
    float scale,
    uint32_t id)
{
    return EcsUiPaintClayPush(
        out,
        (Clay_RenderCommand){
            .boundingBox = EcsUiPaintClayBounds(item->rect, options, scale),
            .renderData = {
                .text = {
                    .stringContents =
                        EcsUiPaintClayTextSlice(&item->payload.text_run),
                    .textColor = EcsUiPaintClayColor(
                        item->payload.text_run.color,
                        item->opacity),
                    .fontId = 0u,
                    .fontSize = item->payload.text_run.font_size,
                    .letterSpacing = 0u,
                    .lineHeight = 0u,
                },
            },
            .id = id,
            .zIndex = item->z_index,
            .commandType = CLAY_RENDER_COMMAND_TYPE_TEXT,
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

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE &&
                item->key.role == ECS_UI_PAINT_ROLE_CLIP_SCOPE) {
            if (!EcsUiPaintClayPush(
                    out,
                    (Clay_RenderCommand){
                        .boundingBox =
                            EcsUiPaintClayBounds(item->rect, options, scale),
                        .id = element_id.id,
                        .zIndex = item->z_index,
                        .commandType =
                            item->key.part == ECS_UI_PAINT_CLIP_SCOPE_START ?
                                CLAY_RENDER_COMMAND_TYPE_SCISSOR_START :
                                CLAY_RENDER_COMMAND_TYPE_SCISSOR_END,
                    })) {
                return false;
            }
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_TEXT_RUN &&
                item->key.role == ECS_UI_PAINT_ROLE_TEXT_RUN) {
            const uint32_t line_index = item->key.part & 0xffu;
            const Clay_ElementId text_id =
                Clay__HashNumber(line_index, element_id.id);
            if (!EcsUiPaintClayPushText(
                    out,
                    item,
                    options,
                    scale,
                    text_id.id)) {
                return false;
            }
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX &&
                (item->key.role == ECS_UI_PAINT_ROLE_CARET ||
                    item->key.role == ECS_UI_PAINT_ROLE_SELECTION)) {
            Clay_ElementId role_id = {0};
            (void)EcsUiPaintClayElementId(
                node,
                item->key.role == ECS_UI_PAINT_ROLE_CARET ?
                    "_Caret" :
                    "_Selection",
                &role_id);
            if (!EcsUiPaintClayPushRectangle(
                    out,
                    item->rect,
                    options,
                    scale,
                    role_id.id,
                    item->payload.box.fill,
                    item->opacity,
                    (Clay_CornerRadius){0},
                    item->z_index)) {
                return false;
            }
            continue;
        }

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
                        item->z_index)) {
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
                        .zIndex = item->z_index,
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
                    item->z_index)) {
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
                    node,
                    item->z_index)) {
                return false;
            }
        }
    }
    return true;
}
