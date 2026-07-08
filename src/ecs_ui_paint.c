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

typedef struct EcsUiPaintTextContext {
    EcsUiTextStyle style;
    bool has_style;
    bool inverse;
    bool disabled;
} EcsUiPaintTextContext;

typedef struct EcsUiPaintBuildContext {
    EcsUiPaintList *list;
    const EcsUiTreeSnapshot *tree;
    const EcsUiTheme *theme;
    EcsUiMeasureTextFn measure_text;
    void *measure_user_data;
    uint32_t item_capacity;
} EcsUiPaintBuildContext;

static float EcsUiPaintScale(const EcsUiTreeSnapshot *tree)
{
    return tree != NULL && tree->scale > 0.0f ? tree->scale : 1.0f;
}

static uint16_t EcsUiPaintU16(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= 65535.0f) {
        return 65535u;
    }
    return (uint16_t)value;
}

static uint16_t EcsUiPaintFontSize(
    const EcsUiTreeSnapshot *tree,
    float logical_size)
{
    return EcsUiPaintU16(logical_size * EcsUiPaintScale(tree));
}

static float EcsUiPaintAlignFactor(EcsUiAlign align)
{
    switch (align) {
    case ECS_UI_ALIGN_CENTER:
        return 0.5f;
    case ECS_UI_ALIGN_END:
        return 1.0f;
    case ECS_UI_ALIGN_START:
    default:
        return 0.0f;
    }
}

static EcsUiTextLayout EcsUiPaintDefaultTextLayout(void)
{
    return (EcsUiTextLayout){
        .align_x = ECS_UI_ALIGN_START,
        .align_y = ECS_UI_ALIGN_CENTER,
    };
}

static uint16_t EcsUiPaintPart(uint32_t sub, uint32_t index)
{
    return (uint16_t)(((sub & 0xffu) << 8u) | (index & 0xffu));
}

static EcsUiPaintTextContext EcsUiPaintNodeTextContext(
    EcsUiPaintTextContext parent,
    const EcsUiTreeNodeSnapshot *node)
{
    if (node != NULL && node->has_text_style) {
        parent.style = node->text_style;
        parent.has_style = true;
    }
    return parent;
}

static uint32_t EcsUiPaintClampTextIndex(uint32_t index, size_t length)
{
    return index <= length ? index : (uint32_t)length;
}

static const EcsUiTreeNodeSnapshot *EcsUiPaintFindDirectTextChild(
    const EcsUiTreeSnapshot *tree,
    uint32_t parent_index,
    ecs_entity_t entity,
    uint32_t *out_index)
{
    if (out_index != NULL) {
        *out_index = ECS_UI_TREE_INVALID_INDEX;
    }
    if (tree == NULL || parent_index >= tree->count || entity == 0) {
        return NULL;
    }

    const EcsUiTreeNodeSnapshot *parent = &tree->nodes[parent_index];
    for (uint32_t child = parent->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        if (child_node->entity == entity &&
                child_node->kind == ECS_UI_NODE_TEXT) {
            if (out_index != NULL) {
                *out_index = child;
            }
            return child_node;
        }
    }
    return NULL;
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

static bool EcsUiPaintPushRoleBox(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    uint16_t role,
    uint16_t part,
    EcsUiPaintRect rect,
    EcsUiColorF fill,
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
        .part = part,
        .generation = list->generation,
    };
    item->primitive = ECS_UI_PAINT_PRIMITIVE_BOX;
    item->rect = rect;
    item->opacity = opacity;
    item->payload.box = (EcsUiPaintBox){
        .fill = fill,
    };
    return true;
}

static bool EcsUiPaintPushTextRun(
    EcsUiPaintList *list,
    const EcsUiTreeNodeSnapshot *node,
    uint16_t part,
    EcsUiPaintRect rect,
    const char *text,
    uint32_t byte_start,
    uint32_t byte_end,
    uint16_t font_size,
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
        .role = ECS_UI_PAINT_ROLE_TEXT_RUN,
        .part = part,
        .generation = list->generation,
    };
    item->primitive = ECS_UI_PAINT_PRIMITIVE_TEXT_RUN;
    item->rect = rect;
    item->opacity = opacity;
    item->payload.text_run = (EcsUiPaintTextRun){
        .text = text != NULL ? text : "",
        .byte_start = byte_start,
        .byte_end = byte_end,
        .font_size = font_size,
        .color = color,
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

static bool EcsUiPaintEmitTextNode(
    const EcsUiPaintBuildContext *ctx,
    uint32_t index,
    EcsUiPaintTextContext text_context,
    float opacity)
{
    if (ctx == NULL || ctx->tree == NULL || index >= ctx->tree->count ||
            ctx->measure_text == NULL) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    if (!node->has_layout || node->kind != ECS_UI_NODE_TEXT) {
        return true;
    }

    text_context = EcsUiPaintNodeTextContext(text_context, node);
    const EcsUiTextLayout text_layout =
        node->has_text_layout ? node->text_layout : EcsUiPaintDefaultTextLayout();
    const float resolved_size = EcsUiStyleTextSize(
        node->text.role,
        text_context.style,
        text_context.has_style);
    const uint16_t font_size =
        EcsUiPaintFontSize(ctx->tree, resolved_size);
    const char *text = node->text.text;
    const uint32_t text_end = (uint32_t)strlen(text);
    if (text_end == 0u) {
        return true;
    }

    const EcsUiStyleTextRangeMeasure measure =
        EcsUiStyleMeasureTextRange(
            ctx->measure_text,
            ctx->measure_user_data,
            text,
            0u,
            text_end,
            font_size,
            EcsUiPaintScale(ctx->tree));
    if (measure.truncated) {
        ctx->list->truncated = true;
        return false;
    }
    if (measure.line_count == 0u) {
        return true;
    }

    const bool placed_text =
        node->parent_index != ECS_UI_TREE_INVALID_INDEX &&
        node->parent_index < ctx->tree->count &&
        ctx->tree->nodes[node->parent_index].kind == ECS_UI_NODE_ZSTACK &&
        node->has_placement;
    const bool has_newline = strchr(text, '\n') != NULL;
    const float element_width = measure.width;
    const float element_height =
        has_newline ? measure.height * (float)measure.line_count : measure.height;
    const EcsUiPaintRect wrapper = EcsUiPaintNodeRect(node);
    const float child_x = placed_text ?
        (wrapper.width - element_width) *
            EcsUiPaintAlignFactor(text_layout.align_x) :
        0.0f;
    const EcsUiPaintRect element = {
        .x = wrapper.x + child_x,
        .y = wrapper.y +
            (wrapper.height - element_height) *
                EcsUiPaintAlignFactor(text_layout.align_y),
        .width = element_width,
        .height = element_height,
    };
    const EcsUiColorF color = EcsUiStyleTextColor(
        ctx->theme,
        node->text.role,
        text_context.inverse,
        text_context.style,
        text_context.has_style,
        text_context.disabled);

    if (!has_newline) {
        return EcsUiPaintPushTextRun(
            ctx->list,
            node,
            EcsUiPaintPart(0u, 0u),
            element,
            text,
            0u,
            text_end,
            font_size,
            color,
            opacity,
            ctx->item_capacity);
    }

    for (uint32_t line = 0u; line < measure.line_count; line += 1u) {
        const EcsUiStyleTextLineMeasure *line_measure = &measure.lines[line];
        if (line_measure->byte_start == line_measure->byte_end) {
            continue;
        }
        const float offset =
            (element.width - line_measure->width) *
            EcsUiPaintAlignFactor(text_layout.align_x);
        const EcsUiPaintRect line_rect = {
            .x = element.x + offset,
            .y = element.y + measure.height * (float)line,
            .width = line_measure->width,
            .height = measure.height,
        };
        if (!EcsUiPaintPushTextRun(
                ctx->list,
                node,
                EcsUiPaintPart(0u, line),
                line_rect,
                text,
                line_measure->byte_start,
                line_measure->byte_end,
                font_size,
                color,
                opacity,
                ctx->item_capacity)) {
            return false;
        }
    }
    return true;
}

static bool EcsUiPaintTextFieldHasUnsupportedFlowChild(
    const EcsUiTreeSnapshot *tree,
    uint32_t field_index,
    uint32_t value_index)
{
    if (tree == NULL || field_index >= tree->count) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *field = &tree->nodes[field_index];
    for (uint32_t child = field->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        if (child == value_index) {
            continue;
        }
        if (tree->nodes[child].has_layout) {
            return true;
        }
    }
    return false;
}

static bool EcsUiPaintEmitTextFieldRange(
    const EcsUiPaintBuildContext *ctx,
    const EcsUiTreeNodeSnapshot *field_node,
    const EcsUiTreeNodeSnapshot *value_node,
    EcsUiPaintTextContext value_context,
    uint16_t font_size,
    EcsUiColorF color,
    float opacity,
    float *cursor_x,
    uint32_t *segment,
    uint32_t start,
    uint32_t end,
    bool selection)
{
    if (ctx == NULL || field_node == NULL || value_node == NULL ||
            cursor_x == NULL || segment == NULL || start == end) {
        return true;
    }
    (void)value_context;
    const EcsUiStyleTextRangeMeasure measure =
        EcsUiStyleMeasureTextRange(
            ctx->measure_text,
            ctx->measure_user_data,
            value_node->text.text,
            start,
            end,
            font_size,
            EcsUiPaintScale(ctx->tree));
    if (measure.truncated) {
        ctx->list->truncated = true;
        return false;
    }
    const EcsUiPaintRect rect = {
        .x = *cursor_x,
        .y = field_node->layout_y +
            (field_node->layout_height - measure.height) * 0.5f,
        .width = measure.width,
        .height = measure.height,
    };
    const uint16_t part = EcsUiPaintPart(*segment, 0u);
    if (selection) {
        if (!EcsUiPaintPushRoleBox(
                ctx->list,
                value_node,
                ECS_UI_PAINT_ROLE_SELECTION,
                part,
                rect,
                EcsUiStyleSelectionColor(ctx->theme),
                opacity,
                ctx->item_capacity)) {
            return false;
        }
    }
    if (!EcsUiPaintPushTextRun(
            ctx->list,
            value_node,
            part,
            rect,
            value_node->text.text,
            start,
            end,
            font_size,
            color,
            opacity,
            ctx->item_capacity)) {
        return false;
    }
    *cursor_x += measure.width;
    *segment += 1u;
    return true;
}

static bool EcsUiPaintEmitTextFieldCaret(
    const EcsUiPaintBuildContext *ctx,
    const EcsUiTreeNodeSnapshot *field_node,
    const EcsUiTreeNodeSnapshot *value_node,
    EcsUiColorF color,
    float resolved_size,
    float opacity,
    float *cursor_x,
    uint32_t *segment)
{
    if (ctx == NULL || field_node == NULL || value_node == NULL ||
            cursor_x == NULL || segment == NULL) {
        return false;
    }
    const float width = field_node->text_field_view.caret_width > 0.0f ?
        field_node->text_field_view.caret_width :
        2.0f;
    const float height = resolved_size + 8.0f;
    const EcsUiPaintRect rect = {
        .x = *cursor_x,
        .y = field_node->layout_y + (field_node->layout_height - height) * 0.5f,
        .width = width,
        .height = height,
    };
    if (!EcsUiPaintPushRoleBox(
            ctx->list,
            value_node,
            ECS_UI_PAINT_ROLE_CARET,
            EcsUiPaintPart(*segment, 0u),
            rect,
            color,
            opacity,
            ctx->item_capacity)) {
        return false;
    }
    *cursor_x += width;
    *segment += 1u;
    return true;
}

static bool EcsUiPaintEmitTextField(
    const EcsUiPaintBuildContext *ctx,
    uint32_t field_index,
    EcsUiPaintTextContext text_context,
    float opacity,
    bool *out_handled)
{
    if (out_handled != NULL) {
        *out_handled = false;
    }
    if (ctx == NULL || ctx->tree == NULL || field_index >= ctx->tree->count) {
        return false;
    }
    const EcsUiTreeNodeSnapshot *field_node = &ctx->tree->nodes[field_index];
    if (field_node->kind != ECS_UI_NODE_PRESSABLE ||
            !field_node->has_text_field_view) {
        return true;
    }

    uint32_t value_index = ECS_UI_TREE_INVALID_INDEX;
    const EcsUiTreeNodeSnapshot *value_node = EcsUiPaintFindDirectTextChild(
        ctx->tree,
        field_index,
        field_node->text_field_view.value_node,
        &value_index);
    if (value_node == NULL) {
        return true;
    }
    if (out_handled != NULL) {
        *out_handled = true;
    }
    if (EcsUiPaintTextFieldHasUnsupportedFlowChild(
            ctx->tree,
            field_index,
            value_index)) {
        ctx->list->truncated = true;
        return false;
    }

    EcsUiPaintTextContext value_context =
        EcsUiPaintNodeTextContext(text_context, value_node);
    value_context.inverse = false;
    value_context.disabled =
        value_context.disabled ||
        field_node->pressable.disabled ||
        field_node->text_field_view.disabled;
    const float resolved_size = EcsUiStyleTextSize(
        value_node->text.role,
        value_context.style,
        value_context.has_style);
    const uint16_t font_size =
        EcsUiPaintFontSize(ctx->tree, resolved_size);
    const EcsUiColorF color = EcsUiStyleTextColor(
        ctx->theme,
        value_node->text.role,
        value_context.inverse,
        value_context.style,
        value_context.has_style,
        value_context.disabled);
    const char *text = value_node->text.text;
    const size_t length = strlen(text);
    const uint32_t text_end = (uint32_t)length;
    const EcsUiTextFieldView *view = &field_node->text_field_view;
    const uint32_t cursor = EcsUiPaintClampTextIndex(view->cursor, length);
    uint32_t selection_start =
        EcsUiPaintClampTextIndex(view->selection_anchor, length);
    uint32_t selection_end =
        EcsUiPaintClampTextIndex(view->selection_focus, length);
    if (selection_start > selection_end) {
        uint32_t swap = selection_start;
        selection_start = selection_end;
        selection_end = swap;
    }
    const bool has_selection =
        view->focused && selection_start < selection_end;

    const float padding =
        field_node->has_box_style && field_node->box_style.padding > 0.0f ?
            field_node->box_style.padding :
            12.0f;
    float cursor_x = field_node->layout_x +
        EcsUiStyleScaledU16Logical(padding, EcsUiPaintScale(ctx->tree));
    uint32_t segment = 0u;
    if (!view->focused) {
        return EcsUiPaintEmitTextFieldRange(
            ctx,
            field_node,
            value_node,
            value_context,
            font_size,
            color,
            opacity,
            &cursor_x,
            &segment,
            0u,
            text_end,
            false);
    }

    if (!has_selection) {
        return EcsUiPaintEmitTextFieldRange(
                ctx,
                field_node,
                value_node,
                value_context,
                font_size,
                color,
                opacity,
                &cursor_x,
                &segment,
                0u,
                cursor,
                false) &&
            EcsUiPaintEmitTextFieldCaret(
                ctx,
                field_node,
                value_node,
                color,
                resolved_size,
                opacity,
                &cursor_x,
                &segment) &&
            EcsUiPaintEmitTextFieldRange(
                ctx,
                field_node,
                value_node,
                value_context,
                font_size,
                color,
                opacity,
                &cursor_x,
                &segment,
                cursor,
                text_end,
                false);
    }

    if (!EcsUiPaintEmitTextFieldRange(
            ctx,
            field_node,
            value_node,
            value_context,
            font_size,
            color,
            opacity,
            &cursor_x,
            &segment,
            0u,
            selection_start,
            false)) {
        return false;
    }
    if (cursor == selection_start &&
            !EcsUiPaintEmitTextFieldCaret(
                ctx,
                field_node,
                value_node,
                color,
                resolved_size,
                opacity,
                &cursor_x,
                &segment)) {
        return false;
    }
    if (!EcsUiPaintEmitTextFieldRange(
            ctx,
            field_node,
            value_node,
            value_context,
            font_size,
            color,
            opacity,
            &cursor_x,
            &segment,
            selection_start,
            selection_end,
            true)) {
        return false;
    }
    if (cursor != selection_start &&
            !EcsUiPaintEmitTextFieldCaret(
                ctx,
                field_node,
                value_node,
                color,
                resolved_size,
                opacity,
                &cursor_x,
                &segment)) {
        return false;
    }
    return EcsUiPaintEmitTextFieldRange(
        ctx,
        field_node,
        value_node,
        value_context,
        font_size,
        color,
        opacity,
        &cursor_x,
        &segment,
        selection_end,
        text_end,
        false);
}

static bool EcsUiPaintEmitNode(
    const EcsUiPaintBuildContext *ctx,
    uint32_t index,
    EcsUiPaintTextContext text_context,
    float parent_opacity,
    bool skip_children)
{
    if (ctx == NULL || ctx->list == NULL || ctx->tree == NULL ||
            index >= ctx->tree->count) {
        return false;
    }

    const EcsUiTreeNodeSnapshot *node = &ctx->tree->nodes[index];
    const float opacity =
        parent_opacity * EcsUiStyleClamp01(node->visual.opacity);
    if (opacity <= 0.01f) {
        return true;
    }
    text_context = EcsUiPaintNodeTextContext(text_context, node);

    if (node->has_layout) {
        const EcsUiColorF fill = EcsUiPaintBoxFill(node, ctx->theme);
        const EcsUiPaintBorder border = node->kind == ECS_UI_NODE_TEXT ?
            (EcsUiPaintBorder){0} :
            EcsUiStyleBorder(node);
        if (fill.a != 0.0f || border.has_border) {
            const EcsUiPaintCornerRadius radius =
                EcsUiPaintBoxRadius(node, ctx->theme, fill);
            if (!EcsUiPaintPushBox(
                    ctx->list,
                    node,
                    fill,
                    radius,
                    border,
                    opacity,
                    ctx->item_capacity)) {
                return false;
            }
        }

        const uint16_t custom_role = EcsUiPaintCustomRole(node);
        if (custom_role != ECS_UI_PAINT_ROLE_NONE) {
            if (!EcsUiPaintPushCustom(
                    ctx->list,
                    node,
                    custom_role,
                    EcsUiPaintCustomColor(node, ctx->theme),
                    opacity,
                    ctx->item_capacity)) {
                return false;
            }
        }

        if (node->kind == ECS_UI_NODE_TEXT &&
                !EcsUiPaintEmitTextNode(
                    ctx,
                    index,
                    text_context,
                    opacity)) {
            return false;
        }
        bool text_field_handled = false;
        if (!EcsUiPaintEmitTextField(
                ctx,
                index,
                text_context,
                opacity,
                &text_field_handled)) {
            return false;
        }
        if (text_field_handled) {
            skip_children = true;
        }
    }

    if (!skip_children) {
        EcsUiPaintTextContext child_text_context = text_context;
        if (node->kind == ECS_UI_NODE_BUTTON) {
            child_text_context.inverse =
                node->button.variant == ECS_UI_BUTTON_PRIMARY;
            child_text_context.disabled =
                child_text_context.disabled || node->button.disabled;
        } else if (node->kind == ECS_UI_NODE_PRESSABLE) {
            child_text_context.inverse = false;
            child_text_context.disabled =
                child_text_context.disabled || node->pressable.disabled;
        }
        for (uint32_t child = node->first_child;
             child != ECS_UI_TREE_INVALID_INDEX && child < ctx->tree->count;
             child = ctx->tree->nodes[child].next_sibling) {
            if (!EcsUiPaintEmitNode(
                    ctx,
                    child,
                    child_text_context,
                    opacity,
                    false)) {
                return false;
            }
        }
    }
    return EcsUiPaintEmitBevel(
        ctx->list,
        node,
        opacity,
        ctx->item_capacity);
}

bool EcsUiPaintListBuildWithCapacity(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data,
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
    const EcsUiPaintBuildContext ctx = {
        .list = list,
        .tree = tree,
        .theme = theme,
        .measure_text = measure_text,
        .measure_user_data = measure_user_data,
        .item_capacity = item_capacity,
    };
    /* 7.2 snapshots contain one root; all roots are therefore z=0 and stable. */
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (tree->nodes[i].parent != 0) {
            continue;
        }
        if (!EcsUiPaintEmitNode(
                &ctx,
                i,
                (EcsUiPaintTextContext){0},
                1.0f,
                false)) {
            return false;
        }
    }
    return true;
}

bool EcsUiPaintListBuild(
    EcsUiPaintList *list,
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    EcsUiMeasureTextFn measure_text,
    void *measure_user_data)
{
    return EcsUiPaintListBuildWithCapacity(
        list,
        tree,
        theme,
        measure_text,
        measure_user_data,
        ECS_UI_PAINT_ITEM_MAX);
}
