#include "ecs_ui/ecs_ui_clay.h"

#include <string.h>

static Clay_String EcsUiClayString(const char *text)
{
    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = text != NULL ? (int32_t)strlen(text) : 0,
        .chars = text != NULL ? text : "",
    };
}

static uint16_t EcsUiClayU16(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= 65535.0f) {
        return 65535u;
    }
    return (uint16_t)value;
}

static Clay_TextElementConfig *EcsUiClayTextConfig(
    const EcsUiClayTheme *theme,
    EcsUiTextRole role)
{
    uint16_t font_size = 18u;
    switch (role) {
    case ECS_UI_TEXT_TITLE:
        font_size = 28u;
        break;
    case ECS_UI_TEXT_BUTTON:
    case ECS_UI_TEXT_LABEL:
        font_size = 16u;
        break;
    case ECS_UI_TEXT_CAPTION:
        font_size = 12u;
        break;
    case ECS_UI_TEXT_BODY:
    default:
        font_size = 18u;
        break;
    }

    return CLAY_TEXT_CONFIG({
        .textColor = theme->text,
        .fontSize = font_size,
        .wrapMode = CLAY_TEXT_WRAP_NONE,
    });
}

static Clay_Color EcsUiClayButtonColor(
    const EcsUiClayTheme *theme,
    EcsUiButtonVariant variant)
{
    switch (variant) {
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

static void EcsUiClayEmitNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index);

static void EcsUiClayEmitChildren(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index)
{
    uint32_t child = tree->nodes[index].first_child;
    while (child != ECS_UI_TREE_INVALID_INDEX) {
        EcsUiClayEmitNode(tree, theme, child);
        child = tree->nodes[child].next_sibling;
    }
}

static void EcsUiClayEmitStack(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index,
    Clay_Color background)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    Clay_LayoutDirection direction = CLAY_TOP_TO_BOTTOM;
    if (node->stack.axis == ECS_UI_AXIS_HORIZONTAL) {
        direction = CLAY_LEFT_TO_RIGHT;
    }

    CLAY(CLAY_SID(EcsUiClayString(node->id)), {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0),
            },
            .padding = CLAY_PADDING_ALL(EcsUiClayU16(node->stack.padding)),
            .layoutDirection = direction,
            .childGap = EcsUiClayU16(node->stack.gap),
        },
        .backgroundColor = background,
    }) {
        EcsUiClayEmitChildren(tree, theme, index);
    }
}

static void EcsUiClayEmitNode(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme,
    uint32_t index)
{
    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];

    switch (node->kind) {
    case ECS_UI_NODE_ROOT:
        EcsUiClayEmitStack(tree, theme, index, theme->root_background);
        break;
    case ECS_UI_NODE_VSTACK:
    case ECS_UI_NODE_HSTACK:
    case ECS_UI_NODE_ZSTACK:
        EcsUiClayEmitStack(tree, theme, index, theme->surface);
        break;
    case ECS_UI_NODE_BUTTON:
        CLAY(CLAY_SID(EcsUiClayString(node->id)), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_FIT(0),
                    .height = CLAY_SIZING_FIXED(44.0f),
                },
                .padding = {
                    .left = 14u,
                    .right = 14u,
                },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 8u,
                .childAlignment = {
                    .x = CLAY_ALIGN_X_CENTER,
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor =
                EcsUiClayButtonColor(theme, node->button.variant),
        }) {
            EcsUiClayEmitChildren(tree, theme, index);
        }
        break;
    case ECS_UI_NODE_TEXT:
        CLAY_TEXT(
            EcsUiClayString(node->text.text),
            EcsUiClayTextConfig(theme, node->text.role));
        break;
    case ECS_UI_NODE_ICON:
        CLAY_TEXT(
            EcsUiClayString(node->icon.name),
            EcsUiClayTextConfig(theme, ECS_UI_TEXT_LABEL));
        break;
    case ECS_UI_NODE_NONE:
    default:
        break;
    }
}

EcsUiClayTheme EcsUiClayThemeDefault(void)
{
    return (EcsUiClayTheme){
        .root_background = {16.0f, 20.0f, 25.0f, 255.0f},
        .surface = {24.0f, 32.0f, 37.0f, 255.0f},
        .button = {38.0f, 72.0f, 76.0f, 255.0f},
        .button_primary = {49.0f, 211.0f, 186.0f, 255.0f},
        .button_subtle = {88.0f, 111.0f, 116.0f, 255.0f},
        .button_danger = {255.0f, 125.0f, 95.0f, 255.0f},
        .text = {243.0f, 247.0f, 247.0f, 255.0f},
        .text_inverse = {16.0f, 20.0f, 25.0f, 255.0f},
    };
}

void EcsUiClayEmitTree(
    const EcsUiTreeSnapshot *tree,
    const EcsUiClayTheme *theme)
{
    if (tree == NULL || tree->count == 0u || theme == NULL) {
        return;
    }
    EcsUiClayEmitNode(tree, theme, 0u);
}

