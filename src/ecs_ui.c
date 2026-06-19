#include "ecs_ui/ecs_ui.h"

#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(EcsUiNodeId);
ECS_COMPONENT_DECLARE(EcsUiNode);
ECS_COMPONENT_DECLARE(EcsUiStack);
ECS_COMPONENT_DECLARE(EcsUiBoxStyle);
ECS_COMPONENT_DECLARE(EcsUiTextStyle);
ECS_COMPONENT_DECLARE(EcsUiButton);
ECS_COMPONENT_DECLARE(EcsUiPressable);
ECS_COMPONENT_DECLARE(EcsUiText);
ECS_COMPONENT_DECLARE(EcsUiIcon);
ECS_COMPONENT_DECLARE(EcsUiCustom);
ECS_COMPONENT_DECLARE(EcsUiVisual);
ECS_COMPONENT_DECLARE(EcsUiHitTest);
ECS_COMPONENT_DECLARE(EcsUiTextFieldView);

ECS_TAG_DECLARE(EcsUiRoot);
ECS_TAG_DECLARE(EcsUiInteractive);
ECS_TAG_DECLARE(EcsUiOnClick);
ECS_TAG_DECLARE(EcsUiUsesStyle);
ECS_TAG_DECLARE(EcsUiTheme);
ECS_TAG_DECLARE(EcsUiActiveTheme);
ECS_TAG_DECLARE(EcsUiThemeStyle);

static void EcsUiCopyString(char *out, size_t out_size, const char *value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *source = value != NULL ? value : "";
    size_t i = 0u;
    for (; i + 1u < out_size && source[i] != '\0'; i += 1u) {
        out[i] = source[i];
    }
    out[i] = '\0';
}

static void EcsUiSetNodeId(ecs_world_t *world, ecs_entity_t entity, const char *id)
{
    EcsUiNodeId node_id = {0};
    EcsUiCopyString(node_id.value, sizeof(node_id.value), id);
    ecs_set_ptr(world, entity, EcsUiNodeId, &node_id);
}

static ecs_entity_t EcsUiCurrentParent(EcsUiBuilder *builder)
{
    if (builder == NULL || builder->depth == 0u) {
        return 0;
    }
    return builder->parent_stack[builder->depth - 1u];
}

static const EcsUiBoxStyle *EcsUiResolveBoxStyle(
    const ecs_world_t *world,
    ecs_entity_t entity)
{
    const EcsUiBoxStyle *box_style =
        entity != 0 ? ecs_get(world, entity, EcsUiBoxStyle) : NULL;
    if (box_style != NULL) {
        return box_style;
    }

    ecs_entity_t style_token =
        entity != 0 ? ecs_get_target(world, entity, EcsUiUsesStyle, 0) : 0;
    return style_token != 0 ?
        ecs_get(world, style_token, EcsUiBoxStyle) :
        NULL;
}

static const EcsUiTextStyle *EcsUiResolveTextStyle(
    const ecs_world_t *world,
    ecs_entity_t entity)
{
    const EcsUiTextStyle *text_style =
        entity != 0 ? ecs_get(world, entity, EcsUiTextStyle) : NULL;
    if (text_style != NULL) {
        return text_style;
    }

    ecs_entity_t style_token =
        entity != 0 ? ecs_get_target(world, entity, EcsUiUsesStyle, 0) : 0;
    return style_token != 0 ?
        ecs_get(world, style_token, EcsUiTextStyle) :
        NULL;
}

static void EcsUiClearKindComponents(ecs_world_t *world, ecs_entity_t entity)
{
    ecs_remove(world, entity, EcsUiStack);
    ecs_remove(world, entity, EcsUiBoxStyle);
    ecs_remove(world, entity, EcsUiTextStyle);
    ecs_remove(world, entity, EcsUiButton);
    ecs_remove(world, entity, EcsUiPressable);
    ecs_remove(world, entity, EcsUiText);
    ecs_remove(world, entity, EcsUiIcon);
    ecs_remove(world, entity, EcsUiCustom);
    ecs_remove(world, entity, EcsUiTextFieldView);
    ecs_remove_id(world, entity, EcsUiInteractive);
    ecs_remove_pair(world, entity, EcsUiOnClick, EcsWildcard);
    ecs_remove_pair(world, entity, EcsUiUsesStyle, EcsWildcard);
}

static bool EcsUiThemeReady(void)
{
    return EcsUiTheme != 0 && EcsUiActiveTheme != 0 &&
        EcsUiThemeStyle != 0;
}

static ecs_entity_t EcsUiThemeRootEntity(const ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    return ecs_lookup(world, "EcsUiThemes");
}

static ecs_entity_t EcsUiFindThemeStyleSource(
    const ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token)
{
    if (world == NULL || theme == 0 || style_token == 0 ||
        !EcsUiThemeReady()) {
        return 0;
    }

    ecs_iter_t it = ecs_children(world, theme);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            ecs_entity_t child = it.entities[i];
            if (ecs_get_target(world, child, EcsUiThemeStyle, 0) ==
                style_token) {
                return child;
            }
        }
    }
    return 0;
}

static ecs_entity_t EcsUiCreateNode(
    EcsUiBuilder *builder,
    const char *id,
    EcsUiNodeKind kind,
    bool can_have_children)
{
    if (builder == NULL || builder->world == NULL || builder->failed) {
        return 0;
    }

    ecs_entity_t parent = EcsUiCurrentParent(builder);
    if (parent == 0) {
        builder->failed = true;
        return 0;
    }

    ecs_world_t *world = builder->world;
    ecs_entity_t entity = 0;
    if (id != NULL && id[0] != '\0') {
        entity = ecs_entity(world, {
            .parent = parent,
            .name = id,
            .sep = "",
        });
    } else {
        entity = ecs_new_w_pair(world, EcsChildOf, parent);
    }

    if (entity == 0) {
        builder->failed = true;
        return 0;
    }

    ecs_add_pair(world, entity, EcsChildOf, parent);
    EcsUiSetNodeId(world, entity, id);
    ecs_set(world, entity, EcsUiNode, {.kind = kind});
    EcsUiClearKindComponents(world, entity);

    if (can_have_children) {
        ecs_add_id(world, entity, EcsOrderedChildren);
    }

    return entity;
}

static void EcsUiPushParent(EcsUiBuilder *builder, ecs_entity_t entity)
{
    if (builder == NULL || entity == 0) {
        return;
    }
    if (builder->depth >= ECS_UI_BUILDER_STACK_MAX) {
        builder->failed = true;
        return;
    }
    builder->parent_stack[builder->depth] = entity;
    builder->depth += 1u;
}

static ecs_entity_t EcsUiBeginStack(
    EcsUiBuilder *builder,
    EcsUiStackDesc desc,
    EcsUiNodeKind kind,
    EcsUiAxis axis)
{
    ecs_entity_t entity = EcsUiCreateNode(builder, desc.id, kind, true);
    if (entity == 0) {
        return 0;
    }

    EcsUiStack stack = {
        .axis = axis,
        .gap = desc.gap,
        .padding = desc.padding,
    };
    ecs_set_ptr(builder->world, entity, EcsUiStack, &stack);
    EcsUiPushParent(builder, entity);
    return entity;
}

void EcsUiImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiNodeId);
    ECS_COMPONENT_DEFINE(world, EcsUiNode);
    ECS_COMPONENT_DEFINE(world, EcsUiStack);
    ECS_COMPONENT_DEFINE(world, EcsUiBoxStyle);
    ECS_COMPONENT_DEFINE(world, EcsUiTextStyle);
    ECS_COMPONENT_DEFINE(world, EcsUiButton);
    ECS_COMPONENT_DEFINE(world, EcsUiPressable);
    ECS_COMPONENT_DEFINE(world, EcsUiText);
    ECS_COMPONENT_DEFINE(world, EcsUiIcon);
    ECS_COMPONENT_DEFINE(world, EcsUiCustom);
    ECS_COMPONENT_DEFINE(world, EcsUiVisual);
    ECS_COMPONENT_DEFINE(world, EcsUiHitTest);
    ECS_COMPONENT_DEFINE(world, EcsUiTextFieldView);

    ECS_TAG_DEFINE(world, EcsUiRoot);
    ECS_TAG_DEFINE(world, EcsUiInteractive);
    ECS_TAG_DEFINE(world, EcsUiOnClick);
    ECS_TAG_DEFINE(world, EcsUiUsesStyle);
    ECS_TAG_DEFINE(world, EcsUiTheme);
    ECS_TAG_DEFINE(world, EcsUiActiveTheme);
    ECS_TAG_DEFINE(world, EcsUiThemeStyle);
    ecs_add_id(world, EcsUiOnClick, EcsExclusive);
    ecs_add_id(world, EcsUiUsesStyle, EcsExclusive);
    ecs_add_id(world, EcsUiActiveTheme, EcsExclusive);
    ecs_add_id(world, EcsUiThemeStyle, EcsExclusive);
}

ecs_entity_t EcsUiThemeRoot(ecs_world_t *world)
{
    if (world == NULL || !EcsUiThemeReady()) {
        return 0;
    }

    ecs_entity_t root = ecs_entity(world, {
        .name = "EcsUiThemes",
        .sep = "",
    });
    if (root != 0) {
        ecs_add_id(world, root, EcsOrderedChildren);
    }
    return root;
}

ecs_entity_t EcsUiStyleTokenRoot(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }

    ecs_entity_t root = ecs_entity(world, {
        .name = "EcsUiStyleTokens",
        .sep = "",
    });
    if (root != 0) {
        ecs_add_id(world, root, EcsOrderedChildren);
    }
    return root;
}

ecs_entity_t EcsUiStyleToken(ecs_world_t *world, const char *id)
{
    if (world == NULL || id == NULL || id[0] == '\0') {
        return 0;
    }

    ecs_entity_t root = EcsUiStyleTokenRoot(world);
    if (root == 0) {
        return 0;
    }

    return ecs_entity(world, {
        .parent = root,
        .name = id,
        .sep = "",
    });
}

bool EcsUiSetStyleToken(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t style_token)
{
    if (world == NULL || entity == 0 || style_token == 0 ||
        EcsUiUsesStyle == 0) {
        return false;
    }

    ecs_add_pair(world, entity, EcsUiUsesStyle, style_token);
    return true;
}

ecs_entity_t EcsUiThemeEntity(ecs_world_t *world, const char *id)
{
    if (world == NULL || !EcsUiThemeReady()) {
        return 0;
    }

    ecs_entity_t root = EcsUiThemeRoot(world);
    if (root == 0) {
        return 0;
    }

    ecs_entity_t theme = ecs_entity(world, {
        .parent = root,
        .name = id != NULL && id[0] != '\0' ? id : "EcsUiTheme",
        .sep = "",
    });
    if (theme != 0) {
        ecs_add_id(world, theme, EcsUiTheme);
        ecs_add_id(world, theme, EcsOrderedChildren);
    }
    return theme;
}

bool EcsUiSetActiveTheme(ecs_world_t *world, ecs_entity_t theme)
{
    if (world == NULL || theme == 0 || !EcsUiThemeReady() ||
        !ecs_has_id(world, theme, EcsUiTheme)) {
        return false;
    }

    ecs_entity_t root = EcsUiThemeRoot(world);
    if (root == 0) {
        return false;
    }

    ecs_add_pair(world, root, EcsUiActiveTheme, theme);
    return true;
}

ecs_entity_t EcsUiGetActiveTheme(const ecs_world_t *world)
{
    if (world == NULL || !EcsUiThemeReady()) {
        return 0;
    }

    ecs_entity_t root = EcsUiThemeRootEntity(world);
    if (root == 0) {
        return 0;
    }
    return ecs_get_target(world, root, EcsUiActiveTheme, 0);
}

bool EcsUiThemeSetBoxStyle(
    ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token,
    EcsUiBoxStyle style)
{
    if (world == NULL || theme == 0 || style_token == 0 ||
        !EcsUiThemeReady() || !ecs_has_id(world, theme, EcsUiTheme)) {
        return false;
    }

    ecs_entity_t source =
        EcsUiFindThemeStyleSource(world, theme, style_token);
    if (source == 0) {
        source = ecs_new_w_pair(world, EcsChildOf, theme);
        if (source == 0) {
            return false;
        }
        ecs_add_pair(world, source, EcsUiThemeStyle, style_token);
    }
    ecs_set_ptr(world, source, EcsUiBoxStyle, &style);
    return true;
}

bool EcsUiThemeSetTextStyle(
    ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token,
    EcsUiTextStyle style)
{
    if (world == NULL || theme == 0 || style_token == 0 ||
        !EcsUiThemeReady() || !ecs_has_id(world, theme, EcsUiTheme)) {
        return false;
    }

    ecs_entity_t source =
        EcsUiFindThemeStyleSource(world, theme, style_token);
    if (source == 0) {
        source = ecs_new_w_pair(world, EcsChildOf, theme);
        if (source == 0) {
            return false;
        }
        ecs_add_pair(world, source, EcsUiThemeStyle, style_token);
    }
    ecs_set_ptr(world, source, EcsUiTextStyle, &style);
    return true;
}

bool EcsUiThemeApply(ecs_world_t *world)
{
    if (world == NULL || !EcsUiThemeReady()) {
        return false;
    }

    ecs_entity_t active = EcsUiGetActiveTheme(world);
    if (active == 0) {
        return false;
    }

    ecs_iter_t it = ecs_children(world, active);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            ecs_entity_t source = it.entities[i];
            ecs_entity_t style_token =
                ecs_get_target(world, source, EcsUiThemeStyle, 0);
            const EcsUiBoxStyle *box_style =
                ecs_get(world, source, EcsUiBoxStyle);
            if (style_token == 0 || box_style == NULL) {
                const EcsUiTextStyle *text_style =
                    ecs_get(world, source, EcsUiTextStyle);
                if (style_token != 0 && text_style != NULL) {
                    ecs_set_ptr(world, style_token, EcsUiTextStyle, text_style);
                }
                continue;
            }
            ecs_set_ptr(world, style_token, EcsUiBoxStyle, box_style);
            const EcsUiTextStyle *text_style =
                ecs_get(world, source, EcsUiTextStyle);
            if (text_style != NULL) {
                ecs_set_ptr(world, style_token, EcsUiTextStyle, text_style);
            }
        }
    }
    return true;
}

ecs_entity_t EcsUiRootEntity(ecs_world_t *world, const char *id)
{
    if (world == NULL) {
        return 0;
    }

    const char *name = id != NULL && id[0] != '\0' ? id : "EcsUiRoot";
    ecs_entity_t root = ecs_entity(world, {
        .name = name,
        .sep = "",
    });
    if (root == 0) {
        return 0;
    }

    ecs_add_id(world, root, EcsUiRoot);
    ecs_add_id(world, root, EcsOrderedChildren);
    ecs_set(world, root, EcsUiNode, {.kind = ECS_UI_NODE_ROOT});
    EcsUiSetNodeId(world, root, name);
    return root;
}

EcsUiBuilder EcsUiBuilderBegin(ecs_world_t *world, ecs_entity_t root)
{
    EcsUiBuilder builder = {0};
    builder.world = world;
    builder.root = root;
    if (world == NULL || root == 0) {
        builder.failed = true;
        return builder;
    }

    ecs_add_id(world, root, EcsOrderedChildren);
    builder.parent_stack[0] = root;
    builder.depth = 1u;
    return builder;
}

void EcsUiBuilderEnd(EcsUiBuilder *builder)
{
    if (builder == NULL) {
        return;
    }
    if (builder->depth != 1u) {
        builder->failed = true;
    }
    builder->depth = 0u;
}

bool EcsUiBuilderOk(const EcsUiBuilder *builder)
{
    return builder != NULL && !builder->failed;
}

ecs_entity_t EcsUiBeginVStack(EcsUiBuilder *builder, EcsUiStackDesc desc)
{
    return EcsUiBeginStack(
        builder,
        desc,
        ECS_UI_NODE_VSTACK,
        ECS_UI_AXIS_VERTICAL);
}

ecs_entity_t EcsUiBeginHStack(EcsUiBuilder *builder, EcsUiStackDesc desc)
{
    return EcsUiBeginStack(
        builder,
        desc,
        ECS_UI_NODE_HSTACK,
        ECS_UI_AXIS_HORIZONTAL);
}

ecs_entity_t EcsUiBeginZStack(EcsUiBuilder *builder, EcsUiStackDesc desc)
{
    return EcsUiBeginStack(
        builder,
        desc,
        ECS_UI_NODE_ZSTACK,
        ECS_UI_AXIS_DEPTH);
}

ecs_entity_t EcsUiBeginButton(EcsUiBuilder *builder, EcsUiButtonDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(builder, desc.id, ECS_UI_NODE_BUTTON, true);
    if (entity == 0) {
        return 0;
    }

    EcsUiButton button = {
        .variant = desc.variant,
        .disabled = desc.disabled,
    };
    ecs_set_ptr(builder->world, entity, EcsUiButton, &button);
    ecs_add_id(builder->world, entity, EcsUiInteractive);
    if (desc.on_click != 0) {
        ecs_add_pair(builder->world, entity, EcsUiOnClick, desc.on_click);
    }
    if (desc.style_token != 0 &&
        !EcsUiSetStyleToken(builder->world, entity, desc.style_token)) {
        builder->failed = true;
    }
    EcsUiPushParent(builder, entity);
    return entity;
}

ecs_entity_t EcsUiBeginPressable(
    EcsUiBuilder *builder,
    EcsUiPressableDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(builder, desc.id, ECS_UI_NODE_PRESSABLE, true);
    if (entity == 0) {
        return 0;
    }

    EcsUiPressable pressable = {
        .disabled = desc.disabled,
    };
    ecs_set_ptr(builder->world, entity, EcsUiPressable, &pressable);
    ecs_add_id(builder->world, entity, EcsUiInteractive);
    if (desc.on_click != 0) {
        ecs_add_pair(builder->world, entity, EcsUiOnClick, desc.on_click);
    }
    if (desc.style_token != 0 &&
        !EcsUiSetStyleToken(builder->world, entity, desc.style_token)) {
        builder->failed = true;
    }
    EcsUiPushParent(builder, entity);
    return entity;
}

void EcsUiEnd(EcsUiBuilder *builder)
{
    if (builder == NULL) {
        return;
    }
    if (builder->depth <= 1u) {
        builder->failed = true;
        return;
    }
    builder->depth -= 1u;
    builder->parent_stack[builder->depth] = 0;
}

ecs_entity_t EcsUiAddText(EcsUiBuilder *builder, EcsUiTextDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(builder, desc.id, ECS_UI_NODE_TEXT, false);
    if (entity == 0) {
        return 0;
    }

    EcsUiText text = {
        .role = desc.role,
    };
    EcsUiCopyString(text.text, sizeof(text.text), desc.text);
    ecs_set_ptr(builder->world, entity, EcsUiText, &text);
    return entity;
}

ecs_entity_t EcsUiAddIcon(EcsUiBuilder *builder, EcsUiIconDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(builder, desc.id, ECS_UI_NODE_ICON, false);
    if (entity == 0) {
        return 0;
    }

    EcsUiIcon icon = {0};
    EcsUiCopyString(icon.name, sizeof(icon.name), desc.name);
    ecs_set_ptr(builder->world, entity, EcsUiIcon, &icon);
    return entity;
}

ecs_entity_t EcsUiAddCustom(EcsUiBuilder *builder, EcsUiCustomDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(builder, desc.id, ECS_UI_NODE_CUSTOM, false);
    if (entity == 0) {
        return 0;
    }

    EcsUiCustom custom = {
        .preferred_width = desc.preferred_width,
        .preferred_height = desc.preferred_height,
    };
    EcsUiCopyString(custom.kind, sizeof(custom.kind), desc.kind);
    ecs_set_ptr(builder->world, entity, EcsUiCustom, &custom);
    if (desc.on_click != 0) {
        ecs_add_id(builder->world, entity, EcsUiInteractive);
        ecs_add_pair(builder->world, entity, EcsUiOnClick, desc.on_click);
    }
    return entity;
}

static uint32_t EcsUiReadNode(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t parent,
    uint32_t depth,
    uint32_t parent_index,
    EcsUiTreeSnapshot *out)
{
    if (world == NULL || entity == 0 || out == NULL) {
        return ECS_UI_TREE_INVALID_INDEX;
    }

    const EcsUiNode *node = ecs_get(world, entity, EcsUiNode);
    if (node == NULL) {
        return ECS_UI_TREE_INVALID_INDEX;
    }

    if (out->count >= ECS_UI_TREE_NODE_MAX) {
        out->truncated = true;
        return ECS_UI_TREE_INVALID_INDEX;
    }

    const uint32_t index = out->count;
    out->count += 1u;

    EcsUiTreeNodeSnapshot *snapshot = &out->nodes[index];
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->entity = entity;
    snapshot->parent = parent;
    snapshot->on_click = ecs_get_target(world, entity, EcsUiOnClick, 0);
    snapshot->kind = node->kind;
    snapshot->depth = depth;
    snapshot->parent_index = parent_index;
    snapshot->first_child = ECS_UI_TREE_INVALID_INDEX;
    snapshot->next_sibling = ECS_UI_TREE_INVALID_INDEX;
    snapshot->visual.opacity = 1.0f;

    const EcsUiNodeId *node_id = ecs_get(world, entity, EcsUiNodeId);
    if (node_id != NULL) {
        EcsUiCopyString(snapshot->id, sizeof(snapshot->id), node_id->value);
    }

    const EcsUiStack *stack = ecs_get(world, entity, EcsUiStack);
    if (stack != NULL) {
        snapshot->stack = *stack;
    }

    const EcsUiBoxStyle *box_style = EcsUiResolveBoxStyle(world, entity);
    if (box_style != NULL) {
        snapshot->box_style = *box_style;
        snapshot->has_box_style = true;
    }

    const EcsUiTextStyle *text_style = EcsUiResolveTextStyle(world, entity);
    if (text_style != NULL) {
        snapshot->text_style = *text_style;
        snapshot->has_text_style = true;
    }

    const EcsUiButton *button = ecs_get(world, entity, EcsUiButton);
    if (button != NULL) {
        snapshot->button = *button;
    }

    const EcsUiPressable *pressable = ecs_get(world, entity, EcsUiPressable);
    if (pressable != NULL) {
        snapshot->pressable = *pressable;
    }

    const EcsUiText *text = ecs_get(world, entity, EcsUiText);
    if (text != NULL) {
        snapshot->text = *text;
    }

    const EcsUiIcon *icon = ecs_get(world, entity, EcsUiIcon);
    if (icon != NULL) {
        snapshot->icon = *icon;
    }

    const EcsUiCustom *custom = ecs_get(world, entity, EcsUiCustom);
    if (custom != NULL) {
        snapshot->custom = *custom;
    }

    const EcsUiVisual *visual = ecs_get(world, entity, EcsUiVisual);
    if (visual != NULL) {
        snapshot->visual = *visual;
    }

    const EcsUiHitTest *hit_test = ecs_get(world, entity, EcsUiHitTest);
    if (hit_test != NULL) {
        snapshot->hit_test = *hit_test;
    }

    const EcsUiTextFieldView *text_field_view =
        ecs_get(world, entity, EcsUiTextFieldView);
    if (text_field_view != NULL) {
        snapshot->text_field_view = *text_field_view;
        snapshot->has_text_field_view = true;
    }

    uint32_t previous_child = ECS_UI_TREE_INVALID_INDEX;
    ecs_iter_t it = ecs_children(world, entity);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            const uint32_t child_index = EcsUiReadNode(
                world,
                it.entities[i],
                entity,
                depth + 1u,
                index,
                out);
            if (child_index == ECS_UI_TREE_INVALID_INDEX) {
                continue;
            }

            if (snapshot->first_child == ECS_UI_TREE_INVALID_INDEX) {
                snapshot->first_child = child_index;
            }
            if (previous_child != ECS_UI_TREE_INVALID_INDEX) {
                out->nodes[previous_child].next_sibling = child_index;
            }
            previous_child = child_index;
        }
    }

    return index;
}

bool EcsUiReadTree(
    const ecs_world_t *world,
    ecs_entity_t root,
    EcsUiTreeSnapshot *out)
{
    if (world == NULL || root == 0 || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->root = root;
    return EcsUiReadNode(
        world,
        root,
        0,
        0u,
        ECS_UI_TREE_INVALID_INDEX,
        out) != ECS_UI_TREE_INVALID_INDEX;
}

void EcsUiEventListClear(EcsUiEventList *events)
{
    if (events == NULL) {
        return;
    }
    memset(events, 0, sizeof(*events));
}

bool EcsUiEventListPush(EcsUiEventList *events, const EcsUiEvent *event)
{
    if (events == NULL || event == NULL) {
        return false;
    }
    if (events->count >= ECS_UI_EVENT_MAX) {
        events->truncated = true;
        return false;
    }

    events->events[events->count] = *event;
    events->count += 1u;
    return true;
}
