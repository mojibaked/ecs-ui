#include "ecs_ui/ecs_ui_raylib.h"

#include <stdio.h>
#include <string.h>

static float EcsUiRaylibMaxFloat(float a, float b)
{
    return a > b ? a : b;
}

static float EcsUiRaylibClampPositive(float value)
{
    return value > 0.0f ? value : 0.0f;
}

static float EcsUiRaylibClamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static Rectangle EcsUiRaylibOffset(Rectangle bounds, EcsUiVisual visual)
{
    bounds.x += visual.offset_x;
    bounds.y += visual.offset_y;
    return bounds;
}

static Rectangle EcsUiRaylibInset(Rectangle bounds, float amount)
{
    const float inset = EcsUiRaylibClampPositive(amount);
    const float width = bounds.width - (inset * 2.0f);
    const float height = bounds.height - (inset * 2.0f);
    return (Rectangle){
        .x = bounds.x + inset,
        .y = bounds.y + inset,
        .width = EcsUiRaylibClampPositive(width),
        .height = EcsUiRaylibClampPositive(height),
    };
}

static Color EcsUiRaylibApplyOpacity(Color color, float opacity)
{
    color.a = (unsigned char)((float)color.a * EcsUiRaylibClamp01(opacity));
    return color;
}

static Color EcsUiRaylibLerpColor(Color from, Color to, float amount)
{
    const float t = EcsUiRaylibClamp01(amount);
    return (Color){
        .r = (unsigned char)((float)from.r + ((float)to.r - (float)from.r) * t),
        .g = (unsigned char)((float)from.g + ((float)to.g - (float)from.g) * t),
        .b = (unsigned char)((float)from.b + ((float)to.b - (float)from.b) * t),
        .a = (unsigned char)((float)from.a + ((float)to.a - (float)from.a) * t),
    };
}

static uint32_t EcsUiRaylibChildCount(
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    uint32_t count = 0u;
    uint32_t child = tree->nodes[index].first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        count += 1u;
        child = tree->nodes[child].next_sibling;
    }
    return count;
}

static float EcsUiRaylibTextSize(EcsUiTextRole role)
{
    switch (role) {
    case ECS_UI_TEXT_TITLE:
        return 28.0f;
    case ECS_UI_TEXT_BUTTON:
    case ECS_UI_TEXT_LABEL:
        return 18.0f;
    case ECS_UI_TEXT_CAPTION:
        return 13.0f;
    case ECS_UI_TEXT_BODY:
    default:
        return 18.0f;
    }
}

static float EcsUiRaylibPreferredHeight(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    float width)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float padding = EcsUiRaylibClampPositive(node->stack.padding);
    const float gap = EcsUiRaylibClampPositive(node->stack.gap);

    switch (node->kind) {
    case ECS_UI_NODE_TEXT:
        return EcsUiRaylibTextSize(node->text.role) + 8.0f;
    case ECS_UI_NODE_ICON:
        return 24.0f;
    case ECS_UI_NODE_BUTTON:
        return 46.0f;
    case ECS_UI_NODE_HSTACK: {
        float height = 0.0f;
        uint32_t child_count = EcsUiRaylibChildCount(tree, index);
        const float child_width =
            child_count > 0u ? width / (float)child_count : width;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            height = EcsUiRaylibMaxFloat(
                height,
                EcsUiRaylibPreferredHeight(tree, child, child_width));
            child = tree->nodes[child].next_sibling;
        }
        return padding * 2.0f + EcsUiRaylibMaxFloat(height, 44.0f);
    }
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_ZSTACK: {
        float height = padding * 2.0f;
        uint32_t child_count = 0u;
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            if (child_count > 0u && node->kind != ECS_UI_NODE_ZSTACK) {
                height += gap;
            }
            height += EcsUiRaylibPreferredHeight(
                tree,
                child,
                width - (padding * 2.0f));
            child_count += 1u;
            child = tree->nodes[child].next_sibling;
        }
        return EcsUiRaylibMaxFloat(height, 44.0f);
    }
    case ECS_UI_NODE_NONE:
    default:
        return 0.0f;
    }
}

static Color EcsUiRaylibButtonColor(
    const EcsUiRaylibTheme *theme,
    EcsUiButton button)
{
    if (button.disabled) {
        return theme->button_disabled;
    }

    switch (button.variant) {
    case ECS_UI_BUTTON_PRIMARY:
        return theme->button_primary;
    case ECS_UI_BUTTON_SUBTLE:
        return theme->button_subtle;
    case ECS_UI_BUTTON_DANGER:
        return theme->button_danger;
    case ECS_UI_BUTTON_DEFAULT:
    default:
        return theme->button;
    }
}

static Color EcsUiRaylibTextColor(
    const EcsUiRaylibTheme *theme,
    EcsUiTextRole role,
    bool inverse)
{
    if (inverse) {
        return theme->text_inverse;
    }
    if (role == ECS_UI_TEXT_CAPTION) {
        return theme->text_muted;
    }
    return theme->text;
}

static void EcsUiRaylibDrawTextLine(
    const char *text,
    Rectangle bounds,
    float font_size,
    Color color)
{
    const char *value = text != NULL ? text : "";
    Vector2 text_size = MeasureTextEx(GetFontDefault(), value, font_size, 1.0f);
    Vector2 position = {
        .x = bounds.x,
        .y = bounds.y + ((bounds.height - text_size.y) * 0.5f),
    };
    DrawTextEx(GetFontDefault(), value, position, font_size, 1.0f, color);
}

static void EcsUiRaylibDrawNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiRaylibTheme *theme,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    float opacity);

static void EcsUiRaylibDrawChildrenVertical(
    const EcsUiTreeSnapshot *tree,
    const EcsUiRaylibTheme *theme,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    float y = bounds.y;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_height =
            EcsUiRaylibPreferredHeight(tree, child, bounds.width);
        const float remaining = bounds.y + bounds.height - y;
        if (remaining <= 0.0f) {
            return;
        }
        Rectangle child_bounds = {
            .x = bounds.x,
            .y = y,
            .width = bounds.width,
            .height = preferred_height < remaining ? preferred_height : remaining,
        };
        EcsUiRaylibDrawNode(
            tree,
            theme,
            child,
            child_bounds,
            inverse_text,
            opacity);
        y += child_bounds.height + gap;
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiRaylibDrawChildrenHorizontal(
    const EcsUiTreeSnapshot *tree,
    const EcsUiRaylibTheme *theme,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const uint32_t child_count = EcsUiRaylibChildCount(tree, index);
    if (child_count == 0u) {
        return;
    }

    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    const float total_gap = gap * (float)(child_count - 1u);
    const float child_width =
        EcsUiRaylibClampPositive(bounds.width - total_gap) / (float)child_count;
    float x = bounds.x;

    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        Rectangle child_bounds = {
            .x = x,
            .y = bounds.y,
            .width = child_width,
            .height = bounds.height,
        };
        EcsUiRaylibDrawNode(
            tree,
            theme,
            child,
            child_bounds,
            inverse_text,
            opacity);
        x += child_width + gap;
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiRaylibDrawNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiRaylibTheme *theme,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    float opacity)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float node_opacity =
        opacity * EcsUiRaylibClamp01(node->visual.opacity);
    if (node_opacity <= 0.01f) {
        return;
    }

    Rectangle node_bounds = EcsUiRaylibOffset(bounds, node->visual);

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
        DrawRectangleRec(
            node_bounds,
            EcsUiRaylibApplyOpacity(theme->root_background, node_opacity));
        EcsUiRaylibDrawChildrenVertical(
            tree,
            theme,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            inverse_text,
            node_opacity);
        break;
    case ECS_UI_NODE_VSTACK:
        DrawRectangleRounded(
            node_bounds,
            theme->radius,
            8,
            EcsUiRaylibApplyOpacity(theme->surface, node_opacity));
        EcsUiRaylibDrawChildrenVertical(
            tree,
            theme,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            inverse_text,
            node_opacity);
        break;
    case ECS_UI_NODE_HSTACK:
        DrawRectangleRounded(
            node_bounds,
            theme->radius,
            8,
            EcsUiRaylibApplyOpacity(theme->surface_subtle, node_opacity));
        EcsUiRaylibDrawChildrenHorizontal(
            tree,
            theme,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            inverse_text,
            node_opacity);
        break;
    case ECS_UI_NODE_ZSTACK: {
        DrawRectangleRounded(
            node_bounds,
            theme->radius,
            8,
            EcsUiRaylibApplyOpacity(theme->surface, node_opacity));
        Rectangle inner = EcsUiRaylibInset(node_bounds, node->stack.padding);
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            EcsUiRaylibDrawNode(
                tree,
                theme,
                child,
                inner,
                inverse_text,
                node_opacity);
            child = tree->nodes[child].next_sibling;
        }
        break;
    }
    case ECS_UI_NODE_BUTTON: {
        const bool hovered =
            !node->button.disabled &&
            CheckCollisionPointRec(GetMousePosition(), node_bounds);
        Color fill = EcsUiRaylibButtonColor(theme, node->button);
        if (hovered) {
            fill = ColorAlpha(fill, 0.86f);
        }
        fill = EcsUiRaylibLerpColor(
            fill,
            (Color){255, 255, 255, fill.a},
            EcsUiRaylibClamp01(node->visual.highlight) * 0.42f);
        DrawRectangleRounded(
            node_bounds,
            theme->radius,
            8,
            EcsUiRaylibApplyOpacity(fill, node_opacity));
        Rectangle inner = EcsUiRaylibInset(node_bounds, 12.0f);
        EcsUiRaylibDrawChildrenHorizontal(
            tree,
            theme,
            index,
            inner,
            node->button.variant == ECS_UI_BUTTON_PRIMARY,
            node_opacity);
        break;
    }
    case ECS_UI_NODE_TEXT:
        EcsUiRaylibDrawTextLine(
            node->text.text,
            node_bounds,
            EcsUiRaylibTextSize(node->text.role),
            EcsUiRaylibApplyOpacity(
                EcsUiRaylibTextColor(theme, node->text.role, inverse_text),
                node_opacity));
        break;
    case ECS_UI_NODE_ICON:
        EcsUiRaylibDrawTextLine(
            node->icon.name,
            node_bounds,
            18.0f,
            EcsUiRaylibApplyOpacity(
                EcsUiRaylibTextColor(theme, ECS_UI_TEXT_LABEL, inverse_text),
                node_opacity));
        break;
    case ECS_UI_NODE_NONE:
    default:
        break;
    }
}

typedef struct EcsUiRaylibHit {
    bool found;
    uint32_t index;
    Rectangle bounds;
} EcsUiRaylibHit;

static void EcsUiRaylibHitNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit);

static void EcsUiRaylibHitChildrenVertical(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    float y = bounds.y;
    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        const float preferred_height =
            EcsUiRaylibPreferredHeight(tree, child, bounds.width);
        const float remaining = bounds.y + bounds.height - y;
        if (remaining <= 0.0f) {
            return;
        }
        Rectangle child_bounds = {
            .x = bounds.x,
            .y = y,
            .width = bounds.width,
            .height = preferred_height < remaining ? preferred_height : remaining,
        };
        EcsUiRaylibHitNode(tree, child, child_bounds, point, hit);
        y += child_bounds.height + gap;
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiRaylibHitChildrenHorizontal(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    const uint32_t child_count = EcsUiRaylibChildCount(tree, index);
    if (child_count == 0u) {
        return;
    }

    const float gap = EcsUiRaylibClampPositive(node->stack.gap);
    const float total_gap = gap * (float)(child_count - 1u);
    const float child_width =
        EcsUiRaylibClampPositive(bounds.width - total_gap) / (float)child_count;
    float x = bounds.x;

    uint32_t child = node->first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        Rectangle child_bounds = {
            .x = x,
            .y = bounds.y,
            .width = child_width,
            .height = bounds.height,
        };
        EcsUiRaylibHitNode(tree, child, child_bounds, point, hit);
        x += child_width + gap;
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiRaylibHitNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    Rectangle bounds,
    Vector2 point,
    EcsUiRaylibHit *hit)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Rectangle node_bounds = EcsUiRaylibOffset(bounds, node->visual);
    if (EcsUiRaylibClamp01(node->visual.opacity) <= 0.01f) {
        return;
    }

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
    case ECS_UI_NODE_VSTACK:
        EcsUiRaylibHitChildrenVertical(
            tree,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            point,
            hit);
        break;
    case ECS_UI_NODE_HSTACK:
        EcsUiRaylibHitChildrenHorizontal(
            tree,
            index,
            EcsUiRaylibInset(node_bounds, node->stack.padding),
            point,
            hit);
        break;
    case ECS_UI_NODE_ZSTACK: {
        Rectangle inner = EcsUiRaylibInset(node_bounds, node->stack.padding);
        uint32_t child = node->first_child;
        while (child != ECS_UI_TREE_INVALID_INDEX) {
            EcsUiRaylibHitNode(tree, child, inner, point, hit);
            child = tree->nodes[child].next_sibling;
        }
        break;
    }
    case ECS_UI_NODE_BUTTON:
        if (!node->button.disabled && CheckCollisionPointRec(point, node_bounds)) {
            hit->found = true;
            hit->index = index;
            hit->bounds = node_bounds;
        }
        EcsUiRaylibHitChildrenHorizontal(
            tree,
            index,
            EcsUiRaylibInset(node_bounds, 12.0f),
            point,
            hit);
        break;
    case ECS_UI_NODE_TEXT:
    case ECS_UI_NODE_ICON:
    case ECS_UI_NODE_NONE:
    default:
        break;
    }
}

static void EcsUiRaylibPushPointerEvent(
    EcsUiEventList *events,
    const EcsUiTreeNodeSnapshot *node,
    EcsUiEventType type,
    Vector2 point)
{
    if (events == NULL || node == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .node = node->entity,
        .action = node->on_click,
        .x = point.x,
        .y = point.y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", node->id);
    (void)EcsUiEventListPush(events, &event);
}

EcsUiRaylibTheme EcsUiRaylibThemeDefault(void)
{
    return (EcsUiRaylibTheme){
        .root_background = {16, 20, 25, 255},
        .surface = {24, 32, 37, 255},
        .surface_subtle = {18, 27, 31, 255},
        .button = {38, 72, 76, 255},
        .button_primary = {49, 211, 186, 255},
        .button_subtle = {88, 111, 116, 255},
        .button_danger = {255, 125, 95, 255},
        .button_disabled = {70, 78, 82, 255},
        .text = {243, 247, 247, 255},
        .text_muted = {142, 161, 164, 255},
        .text_inverse = {16, 20, 25, 255},
        .radius = 0.12f,
    };
}

void EcsUiRaylibDrawTree(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiRaylibTheme *theme)
{
    if (tree == NULL || tree->count == 0u || theme == NULL) {
        return;
    }

    EcsUiRaylibDrawNode(tree, theme, 0u, bounds, false, 1.0f);
}

void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events)
{
    EcsUiEventListClear(events);
    if (tree == NULL || tree->count == 0u || events == NULL) {
        return;
    }

    EcsUiRaylibHit hit = {0};
    const Vector2 point = GetMousePosition();
    EcsUiRaylibHitNode(tree, 0u, bounds, point, &hit);
    if (!hit.found || hit.index >= tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[hit.index];
    EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_HOVERED, point);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_PRESSED, point);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_CLICKED, point);
    }
}
