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

typedef struct EcsUiRaylibPointerCapture {
    bool active;
    ecs_entity_t node;
    ecs_entity_t action;
    char node_id[ECS_UI_ID_MAX];
    Vector2 start;
    double start_time;
} EcsUiRaylibPointerCapture;

static EcsUiRaylibPointerCapture g_ecs_ui_raylib_pointer_capture;

static float EcsUiRaylibDistanceSquared(Vector2 from, Vector2 to)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    return (dx * dx) + (dy * dy);
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

static float EcsUiRaylibPreferredCustomHeight(
    const EcsUiTreeNodeSnapshot *node)
{
    if (node == NULL || node->custom.preferred_height <= 0.0f) {
        return 96.0f;
    }
    return node->custom.preferred_height;
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
    case ECS_UI_NODE_PRESSABLE:
        return 46.0f;
    case ECS_UI_NODE_CUSTOM:
        return EcsUiRaylibPreferredCustomHeight(node);
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
    const EcsUiRaylibDrawOptions *options,
    uint32_t index,
    Rectangle bounds,
    bool inverse_text,
    float opacity);

static void EcsUiRaylibDrawChildrenVertical(
    const EcsUiTreeSnapshot *tree,
    const EcsUiRaylibTheme *theme,
    const EcsUiRaylibDrawOptions *options,
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
            options,
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
    const EcsUiRaylibDrawOptions *options,
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
            options,
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
    const EcsUiRaylibDrawOptions *options,
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
            options,
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
            options,
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
            options,
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
                options,
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
            options,
            index,
            inner,
            node->button.variant == ECS_UI_BUTTON_PRIMARY,
            node_opacity);
        break;
    }
    case ECS_UI_NODE_PRESSABLE: {
        const bool hovered =
            !node->pressable.disabled &&
            CheckCollisionPointRec(GetMousePosition(), node_bounds);
        Color fill = theme->button_subtle;
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
            options,
            index,
            inner,
            false,
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
    case ECS_UI_NODE_CUSTOM:
        if (options != NULL && options->custom_draw != NULL) {
            options->custom_draw(
                node,
                node_bounds,
                node_opacity,
                options->user_data);
        } else {
            DrawRectangleRounded(
                node_bounds,
                theme->radius,
                8,
                EcsUiRaylibApplyOpacity(theme->surface_subtle, node_opacity));
            DrawRectangleRoundedLines(
                node_bounds,
                theme->radius,
                8,
                EcsUiRaylibApplyOpacity(theme->text_muted, node_opacity));
            EcsUiRaylibDrawTextLine(
                node->custom.kind,
                EcsUiRaylibInset(node_bounds, 12.0f),
                EcsUiRaylibTextSize(ECS_UI_TEXT_CAPTION),
                EcsUiRaylibApplyOpacity(theme->text_muted, node_opacity));
        }
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
    case ECS_UI_NODE_PRESSABLE:
        if (!node->pressable.disabled &&
            CheckCollisionPointRec(point, node_bounds)) {
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
    case ECS_UI_NODE_CUSTOM:
        if (node->on_click != 0 && CheckCollisionPointRec(point, node_bounds)) {
            hit->found = true;
            hit->index = index;
            hit->bounds = node_bounds;
        }
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
        .start_x = point.x,
        .start_y = point.y,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", node->id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibStartPointerCapture(
    const EcsUiTreeNodeSnapshot *node,
    Vector2 point)
{
    if (node == NULL) {
        return;
    }

    g_ecs_ui_raylib_pointer_capture = (EcsUiRaylibPointerCapture){
        .active = true,
        .node = node->entity,
        .action = node->on_click,
        .start = point,
        .start_time = GetTime(),
    };
    (void)snprintf(
        g_ecs_ui_raylib_pointer_capture.node_id,
        sizeof(g_ecs_ui_raylib_pointer_capture.node_id),
        "%s",
        node->id);
}

static void EcsUiRaylibPushCapturedPointerEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    Vector2 point)
{
    const EcsUiRaylibPointerCapture *capture =
        &g_ecs_ui_raylib_pointer_capture;
    if (events == NULL || !capture->active) {
        return;
    }

    const float elapsed =
        (float)EcsUiRaylibMaxFloat((float)(GetTime() - capture->start_time), 0.001f);
    const float delta_x = point.x - capture->start.x;
    const float delta_y = point.y - capture->start.y;
    EcsUiEvent event = {
        .type = type,
        .node = capture->node,
        .action = capture->action,
        .x = point.x,
        .y = point.y,
        .start_x = capture->start.x,
        .start_y = capture->start.y,
        .delta_x = delta_x,
        .delta_y = delta_y,
        .elapsed = elapsed,
        .velocity_x = delta_x / elapsed,
        .velocity_y = delta_y / elapsed,
    };
    (void)snprintf(event.node_id, sizeof(event.node_id), "%s", capture->node_id);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibPushKeyboardEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    uint32_t codepoint)
{
    if (events == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
        .codepoint = codepoint,
    };
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibPushKeyboardTextEvent(
    EcsUiEventList *events,
    EcsUiEventType type,
    const char *text)
{
    if (events == NULL) {
        return;
    }

    EcsUiEvent event = {
        .type = type,
    };
    const char *value = text != NULL ? text : "";
    (void)snprintf(event.text, sizeof(event.text), "%s", value);
    (void)EcsUiEventListPush(events, &event);
}

static void EcsUiRaylibCollectKeyboardEvents(EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }

    const bool shortcut_down =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    for (int key = GetCharPressed(); key > 0; key = GetCharPressed()) {
        if (!shortcut_down) {
            EcsUiRaylibPushKeyboardEvent(
                events,
                ECS_UI_EVENT_TEXT_INPUT,
                (uint32_t)key);
        }
    }
    if (shortcut_down && IsKeyPressed(KEY_C)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_COPY, 0u);
    }
    if (shortcut_down && IsKeyPressed(KEY_X)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_CUT, 0u);
    }
    if (shortcut_down && IsKeyPressed(KEY_V)) {
        EcsUiRaylibPushKeyboardTextEvent(
            events,
            ECS_UI_EVENT_TEXT_PASTE,
            GetClipboardText());
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_DELETE, 0u);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_SUBMIT, 0u);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        EcsUiRaylibPushKeyboardEvent(events, ECS_UI_EVENT_TEXT_CANCEL, 0u);
    }
    const bool shift_down =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_TAB)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_FOCUS_PREVIOUS :
                ECS_UI_EVENT_TEXT_FOCUS_NEXT,
            0u);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_LEFT :
                ECS_UI_EVENT_TEXT_CURSOR_LEFT,
            0u);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_RIGHT :
                ECS_UI_EVENT_TEXT_CURSOR_RIGHT,
            0u);
    }
    if (IsKeyPressed(KEY_HOME)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_START :
                ECS_UI_EVENT_TEXT_CURSOR_START,
            0u);
    }
    if (IsKeyPressed(KEY_END)) {
        EcsUiRaylibPushKeyboardEvent(
            events,
            shift_down ?
                ECS_UI_EVENT_TEXT_SELECT_END :
                ECS_UI_EVENT_TEXT_CURSOR_END,
            0u);
    }
}

static void EcsUiRaylibCollectPointerEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events)
{
    if (tree == NULL || tree->count == 0u || events == NULL) {
        return;
    }

    const Vector2 point = GetMousePosition();
    if (g_ecs_ui_raylib_pointer_capture.active) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            EcsUiRaylibPushCapturedPointerEvent(
                events,
                ECS_UI_EVENT_DRAGGED,
                point);
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            const bool did_drag =
                EcsUiRaylibDistanceSquared(
                    g_ecs_ui_raylib_pointer_capture.start,
                    point) > 36.0f;
            EcsUiRaylibPushCapturedPointerEvent(
                events,
                ECS_UI_EVENT_DRAG_ENDED,
                point);
            if (!did_drag) {
                EcsUiRaylibPushCapturedPointerEvent(
                    events,
                    ECS_UI_EVENT_CLICKED,
                    point);
            }
            g_ecs_ui_raylib_pointer_capture =
                (EcsUiRaylibPointerCapture){0};
        }
        return;
    }

    EcsUiRaylibHit hit = {0};
    EcsUiRaylibHitNode(tree, 0u, bounds, point, &hit);
    if (!hit.found || hit.index >= tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[hit.index];
    EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_HOVERED, point);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        EcsUiRaylibPushPointerEvent(events, node, ECS_UI_EVENT_PRESSED, point);
        EcsUiRaylibStartPointerCapture(node, point);
        EcsUiRaylibPushCapturedPointerEvent(
            events,
            ECS_UI_EVENT_DRAG_STARTED,
            point);
    }
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
    EcsUiRaylibDrawTreeEx(tree, bounds, theme, NULL);
}

void EcsUiRaylibDrawTreeEx(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiRaylibTheme *theme,
    const EcsUiRaylibDrawOptions *options)
{
    if (tree == NULL || tree->count == 0u || theme == NULL) {
        return;
    }

    EcsUiRaylibDrawNode(tree, theme, options, 0u, bounds, false, 1.0f);
}

void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events)
{
    EcsUiEventListClear(events);
    if (events == NULL) {
        return;
    }

    EcsUiRaylibCollectPointerEvents(tree, bounds, events);
    EcsUiRaylibCollectKeyboardEvents(events);
}
