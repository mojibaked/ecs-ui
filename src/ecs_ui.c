#include "ecs_ui/ecs_ui.h"
#include "ecs_ui_projection_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(EcsUiNodeId);
ECS_COMPONENT_DECLARE(EcsUiKey);
ECS_COMPONENT_DECLARE(EcsUiAutoOrdinal);
ECS_COMPONENT_DECLARE(EcsUiDeclaration);
ECS_COMPONENT_DECLARE(EcsUiBuilderRootState);
ECS_COMPONENT_DECLARE(EcsUiActionPayload);
ECS_COMPONENT_DECLARE(EcsUiScale);
ECS_COMPONENT_DECLARE(EcsUiNode);
ECS_COMPONENT_DECLARE(EcsUiStack);
ECS_COMPONENT_DECLARE(EcsUiBoxStyle);
ECS_COMPONENT_DECLARE(EcsUiNineSliceStyle);
ECS_COMPONENT_DECLARE(EcsUiTextStyle);
ECS_COMPONENT_DECLARE(EcsUiTextLayout);
ECS_COMPONENT_DECLARE(EcsUiButton);
ECS_COMPONENT_DECLARE(EcsUiPressable);
ECS_COMPONENT_DECLARE(EcsUiText);
ECS_COMPONENT_DECLARE(EcsUiIcon);
ECS_COMPONENT_DECLARE(EcsUiCustom);
ECS_COMPONENT_DECLARE(EcsUiVisual);
ECS_COMPONENT_DECLARE(EcsUiPlacement);
ECS_COMPONENT_DECLARE(EcsUiHitTest);
ECS_COMPONENT_DECLARE(EcsUiScrollView);
ECS_COMPONENT_DECLARE(EcsUiTextFieldView);

ECS_TAG_DECLARE(EcsUiRoot);
ECS_TAG_DECLARE(EcsUiInteractive);
ECS_TAG_DECLARE(EcsUiOnClick);
ECS_TAG_DECLARE(EcsUiUsesStyle);
ECS_TAG_DECLARE(EcsUiHovered);
ECS_TAG_DECLARE(EcsUiHoverWithin);
ECS_TAG_DECLARE(EcsUiRevealedByHover);
ECS_TAG_DECLARE(EcsUiScrollSubscribed);
ECS_TAG_DECLARE(EcsUiThemeTag);
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

static float EcsUiNormalizeScale(float scale)
{
    return scale > 0.0f ? scale : 1.0f;
}

static bool EcsUiSetIdIfChanged(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id,
    size_t size,
    const void *value)
{
    if (world == NULL || entity == 0 || id == 0 || size == 0u ||
        value == NULL) {
        return false;
    }

    const void *existing = ecs_get_id(world, entity, id);
    if (existing != NULL && memcmp(existing, value, size) == 0) {
        return true;
    }

    ecs_set_id(world, entity, id, size, value);
    return true;
}

static bool EcsUiRemoveIdIfPresent(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id)
{
    if (world == NULL || entity == 0 || id == 0) {
        return false;
    }
    if (!ecs_has_id(world, entity, id)) {
        return true;
    }
    ecs_remove_id(world, entity, id);
    return true;
}

static bool EcsUiRemovePairTargetIfPresent(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t relation)
{
    if (world == NULL || entity == 0 || relation == 0) {
        return false;
    }
    ecs_entity_t target = ecs_get_target(world, entity, relation, 0);
    if (target == 0) {
        return true;
    }
    ecs_remove_pair(world, entity, relation, target);
    return true;
}

static void EcsUiSetNodeId(ecs_world_t *world, ecs_entity_t entity, const char *id)
{
    EcsUiNodeId node_id = {0};
    EcsUiCopyString(node_id.value, sizeof(node_id.value), id);
    (void)EcsUiSetIdIfChanged(
        world,
        entity,
        ecs_id(EcsUiNodeId),
        sizeof(node_id),
        &node_id);
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

static const EcsUiNineSliceStyle *EcsUiResolveNineSliceStyle(
    const ecs_world_t *world,
    ecs_entity_t entity)
{
    const EcsUiNineSliceStyle *nine_slice_style =
        entity != 0 ? ecs_get(world, entity, EcsUiNineSliceStyle) : NULL;
    if (nine_slice_style != NULL) {
        return nine_slice_style;
    }

    ecs_entity_t style_token =
        entity != 0 ? ecs_get_target(world, entity, EcsUiUsesStyle, 0) : 0;
    return style_token != 0 ?
        ecs_get(world, style_token, EcsUiNineSliceStyle) :
        NULL;
}

static void EcsUiSetVisualOpacity(
    ecs_world_t *world,
    ecs_entity_t entity,
    float opacity);

static void EcsUiClearKindComponents(ecs_world_t *world, ecs_entity_t entity)
{
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiStack));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiBoxStyle));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiNineSliceStyle));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiScrollView));
    (void)EcsUiRemoveIdIfPresent(world, entity, EcsUiScrollSubscribed);
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiButton));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiPressable));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiText));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiIcon));
    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiCustom));
    (void)EcsUiRemoveIdIfPresent(world, entity, EcsUiInteractive);
    (void)EcsUiRemovePairTargetIfPresent(world, entity, EcsUiOnClick);
    (void)EcsUiRemovePairTargetIfPresent(world, entity, EcsUiUsesStyle);
    (void)EcsUiRemoveIdIfPresent(
        world,
        entity,
        ecs_id(EcsUiActionPayload));
    if (EcsUiRevealedByHover != 0) {
        const bool had_reveal =
            ecs_get_target(world, entity, EcsUiRevealedByHover, 0) != 0;
        (void)EcsUiRemovePairTargetIfPresent(
            world,
            entity,
            EcsUiRevealedByHover);
        if (had_reveal) {
            EcsUiSetVisualOpacity(world, entity, 1.0f);
        }
    }
}

static void EcsUiClearDeclarationTransientComponents(
    ecs_world_t *world,
    ecs_entity_t entity)
{
    if (world == NULL || entity == 0 || EcsUiRevealedByHover == 0) {
        return;
    }

    const bool had_reveal =
        ecs_get_target(world, entity, EcsUiRevealedByHover, 0) != 0;
    (void)EcsUiRemovePairTargetIfPresent(world, entity, EcsUiRevealedByHover);
    if (had_reveal) {
        EcsUiSetVisualOpacity(world, entity, 1.0f);
    }
}

static bool EcsUiThemeReady(void)
{
    return EcsUiThemeTag != 0 && EcsUiActiveTheme != 0 &&
        EcsUiThemeStyle != 0;
}

EcsUiTheme EcsUiThemeDefault(void)
{
    return (EcsUiTheme){
        .root_background = {10u, 14u, 18u, 255u},
        .surface = {29u, 36u, 44u, 255u},
        .surface_subtle = {45u, 55u, 65u, 255u},
        .button = {57u, 67u, 78u, 255u},
        .button_primary = {49u, 211u, 186u, 255u},
        .button_subtle = {70u, 82u, 94u, 255u},
        .button_danger = {238u, 118u, 88u, 255u},
        .button_disabled = {45u, 52u, 60u, 255u},
        .text = {243u, 247u, 247u, 255u},
        .text_muted = {154u, 169u, 174u, 255u},
        .text_inverse = {10u, 14u, 18u, 255u},
        .radius = 0.05f,
    };
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

static bool EcsUiNodeKindCanHaveChildren(EcsUiNodeKind kind)
{
    return kind == ECS_UI_NODE_ROOT ||
        kind == ECS_UI_NODE_VSTACK ||
        kind == ECS_UI_NODE_HSTACK ||
        kind == ECS_UI_NODE_ZSTACK ||
        kind == ECS_UI_NODE_BUTTON ||
        kind == ECS_UI_NODE_PRESSABLE;
}

static EcsUiBuilderParentFrame *EcsUiCurrentParentFrame(
    EcsUiBuilder *builder)
{
    if (builder == NULL || builder->depth == 0u) {
        return NULL;
    }
    return &builder->parent_frames[builder->depth - 1u];
}

static bool EcsUiBuilderParentDeclared(
    const EcsUiBuilder *builder,
    ecs_entity_t parent)
{
    if (builder == NULL || parent == 0) {
        return false;
    }
    for (uint32_t i = 0u; i < builder->declared_parent_count; i += 1u) {
        if (builder->declared_parents[i] == parent) {
            return true;
        }
    }
    return false;
}

static bool EcsUiBuilderMarkParent(
    EcsUiBuilder *builder,
    ecs_entity_t parent)
{
    if (builder == NULL || parent == 0) {
        return false;
    }
    if (EcsUiBuilderParentDeclared(builder, parent)) {
        return true;
    }
    if (builder->declared_parent_count >= ECS_UI_TREE_NODE_MAX) {
        builder->failed = true;
        return false;
    }
    builder->declared_parents[builder->declared_parent_count] = parent;
    builder->declared_parent_count += 1u;
    return true;
}

static bool EcsUiBuilderKeyDeclared(
    const EcsUiBuilder *builder,
    ecs_entity_t parent,
    uint64_t key)
{
    if (builder == NULL || parent == 0 || key == 0u) {
        return false;
    }
    for (uint32_t i = 0u; i < builder->declared_child_count; i += 1u) {
        const EcsUiBuilderDeclaredChild *child =
            &builder->declared_children[i];
        if (child->parent == parent && child->key == key) {
            return true;
        }
    }
    return false;
}

static bool EcsUiBuilderMarkChild(
    EcsUiBuilder *builder,
    ecs_entity_t parent,
    ecs_entity_t child,
    uint64_t key,
    uint32_t auto_ordinal)
{
    if (builder == NULL || parent == 0 || child == 0) {
        return false;
    }
    if (key != 0u && EcsUiBuilderKeyDeclared(builder, parent, key)) {
        builder->failed = true;
        return false;
    }
    for (uint32_t i = 0u; i < builder->declared_child_count; i += 1u) {
        const EcsUiBuilderDeclaredChild *declared =
            &builder->declared_children[i];
        if (declared->parent == parent && declared->child == child) {
            builder->failed = true;
            return false;
        }
    }
    if (builder->declared_child_count >= ECS_UI_TREE_NODE_MAX) {
        builder->failed = true;
        return false;
    }
    builder->declared_children[builder->declared_child_count] =
        (EcsUiBuilderDeclaredChild){
            .parent = parent,
            .child = child,
            .key = key,
            .auto_ordinal = auto_ordinal,
        };
    builder->declared_child_count += 1u;
    return true;
}

static ecs_entity_t EcsUiFindKeyedChild(
    const ecs_world_t *world,
    ecs_entity_t parent,
    uint64_t key)
{
    if (world == NULL || parent == 0 || key == 0u) {
        return 0;
    }

    ecs_iter_t it = ecs_children(world, parent);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            const EcsUiKey *stored = ecs_get(world, it.entities[i], EcsUiKey);
            if (stored != NULL && stored->value == key) {
                return it.entities[i];
            }
        }
    }
    return 0;
}

static ecs_entity_t EcsUiFindAutoOrdinalChild(
    const ecs_world_t *world,
    ecs_entity_t parent,
    uint32_t ordinal)
{
    if (world == NULL || parent == 0 || ordinal == 0u) {
        return 0;
    }

    ecs_iter_t it = ecs_children(world, parent);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            const EcsUiAutoOrdinal *stored =
                ecs_get(world, it.entities[i], EcsUiAutoOrdinal);
            if (stored != NULL && stored->value == ordinal) {
                return it.entities[i];
            }
        }
    }
    return 0;
}

static void EcsUiDeleteChildren(ecs_world_t *world, ecs_entity_t parent)
{
    if (world == NULL || parent == 0) {
        return;
    }

    ecs_entity_t children[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t count = 0u;
    ecs_iter_t it = ecs_children(world, parent);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count &&
                count < (uint32_t)ECS_UI_TREE_NODE_MAX; i += 1) {
            children[count] = it.entities[i];
            count += 1u;
        }
    }

    for (uint32_t i = 0u; i < count; i += 1u) {
        ecs_delete(world, children[i]);
    }
}

static bool EcsUiSetChildParent(
    ecs_world_t *world,
    ecs_entity_t child,
    ecs_entity_t parent)
{
    if (world == NULL || child == 0 || parent == 0) {
        return false;
    }
    if (ecs_get_parent(world, child) == parent) {
        return true;
    }
    ecs_add_pair(world, child, EcsChildOf, parent);
    return true;
}

static bool EcsUiSetDeclarationIdentity(
    ecs_world_t *world,
    ecs_entity_t entity,
    uint64_t key,
    uint32_t auto_ordinal)
{
    if (world == NULL || entity == 0) {
        return false;
    }

    if (key != 0u) {
        EcsUiKey stored = {.value = key};
        (void)EcsUiSetIdIfChanged(
            world,
            entity,
            ecs_id(EcsUiKey),
            sizeof(stored),
            &stored);
        (void)EcsUiRemoveIdIfPresent(
            world,
            entity,
            ecs_id(EcsUiAutoOrdinal));
        return true;
    }

    (void)EcsUiRemoveIdIfPresent(world, entity, ecs_id(EcsUiKey));
    if (auto_ordinal != 0u) {
        EcsUiAutoOrdinal stored = {.value = auto_ordinal};
        (void)EcsUiSetIdIfChanged(
            world,
            entity,
            ecs_id(EcsUiAutoOrdinal),
            sizeof(stored),
            &stored);
    } else {
        (void)EcsUiRemoveIdIfPresent(
            world,
            entity,
            ecs_id(EcsUiAutoOrdinal));
    }
    return true;
}

static bool EcsUiStampDeclaration(
    ecs_world_t *world,
    ecs_entity_t entity,
    uint64_t generation)
{
    EcsUiDeclaration declaration = {.generation = generation};
    return EcsUiSetIdIfChanged(
        world,
        entity,
        ecs_id(EcsUiDeclaration),
        sizeof(declaration),
        &declaration);
}

static bool EcsUiSetNodeKind(
    ecs_world_t *world,
    ecs_entity_t entity,
    EcsUiNodeKind kind)
{
    if (world == NULL || entity == 0) {
        return false;
    }

    const EcsUiNode *existing = ecs_get(world, entity, EcsUiNode);
    const bool kind_changed = existing == NULL || existing->kind != kind;
    if (kind_changed) {
        EcsUiClearKindComponents(world, entity);
        if (!EcsUiNodeKindCanHaveChildren(kind)) {
            EcsUiDeleteChildren(world, entity);
            (void)EcsUiRemoveIdIfPresent(world, entity, EcsOrderedChildren);
        }
    }

    EcsUiNode node = {.kind = kind};
    (void)EcsUiSetIdIfChanged(
        world,
        entity,
        ecs_id(EcsUiNode),
        sizeof(node),
        &node);
    return true;
}

static ecs_entity_t EcsUiCreateNode(
    EcsUiBuilder *builder,
    const char *id,
    uint64_t key,
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
    uint32_t auto_ordinal = 0u;
    if (key != 0u) {
        entity = EcsUiFindKeyedChild(world, parent, key);
        if (entity == 0) {
            entity = ecs_new_w_pair(world, EcsChildOf, parent);
        }
    } else if (id != NULL && id[0] != '\0') {
        entity = ecs_entity(world, {
            .parent = parent,
            .name = id,
            .sep = "",
        });
    } else {
        EcsUiBuilderParentFrame *parent_frame =
            EcsUiCurrentParentFrame(builder);
        if (parent_frame == NULL) {
            builder->failed = true;
            return 0;
        }
        auto_ordinal = parent_frame->next_auto_ordinal;
        parent_frame->next_auto_ordinal += 1u;
        entity = EcsUiFindAutoOrdinalChild(world, parent, auto_ordinal);
        if (entity == 0) {
            entity = ecs_new_w_pair(world, EcsChildOf, parent);
        }
    }

    if (entity == 0) {
        builder->failed = true;
        return 0;
    }

    if (!EcsUiBuilderMarkChild(builder, parent, entity, key, auto_ordinal)) {
        return 0;
    }
    (void)EcsUiSetChildParent(world, entity, parent);
    (void)EcsUiSetDeclarationIdentity(world, entity, key, auto_ordinal);
    (void)EcsUiStampDeclaration(world, entity, builder->generation);
    EcsUiSetNodeId(world, entity, id);
    (void)EcsUiSetNodeKind(world, entity, kind);
    EcsUiClearDeclarationTransientComponents(world, entity);

    if (can_have_children) {
        if (!ecs_has_id(world, entity, EcsOrderedChildren)) {
            ecs_add_id(world, entity, EcsOrderedChildren);
        }
        (void)EcsUiBuilderMarkParent(builder, entity);
    }

    return entity;
}

static void EcsUiSetActionPair(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t relation,
    ecs_entity_t action,
    uint64_t payload,
    bool always_interactive)
{
    if (world == NULL || entity == 0 || relation == 0) {
        return;
    }
    if (always_interactive || action != 0) {
        if (!ecs_has_id(world, entity, EcsUiInteractive)) {
            ecs_add_id(world, entity, EcsUiInteractive);
        }
    } else {
        (void)EcsUiRemoveIdIfPresent(world, entity, EcsUiInteractive);
    }

    ecs_entity_t existing = ecs_get_target(world, entity, relation, 0);
    if (existing != action) {
        if (existing != 0) {
            ecs_remove_pair(world, entity, relation, existing);
        }
        if (action != 0) {
            ecs_add_pair(world, entity, relation, action);
        }
    }

    EcsUiActionPayload stored = {.value = payload};
    (void)EcsUiSetIdIfChanged(
        world,
        entity,
        ecs_id(EcsUiActionPayload),
        sizeof(stored),
        &stored);
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
    builder->parent_frames[builder->depth] = (EcsUiBuilderParentFrame){
        .parent = entity,
        .next_auto_ordinal = 1u,
    };
    builder->depth += 1u;
}

static bool EcsUiSetStyleTokenForNode(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t style_token)
{
    if (world == NULL || entity == 0 || EcsUiUsesStyle == 0) {
        return false;
    }

    ecs_entity_t existing = ecs_get_target(world, entity, EcsUiUsesStyle, 0);
    if (existing == style_token) {
        return true;
    }
    if (existing != 0) {
        ecs_remove_pair(world, entity, EcsUiUsesStyle, existing);
    }
    if (style_token != 0) {
        ecs_add_pair(world, entity, EcsUiUsesStyle, style_token);
    }
    return true;
}

static bool EcsUiSetScrollSubscribedForNode(
    ecs_world_t *world,
    ecs_entity_t entity,
    bool subscribed)
{
    if (world == NULL || entity == 0 || EcsUiScrollSubscribed == 0) {
        return false;
    }
    if (subscribed) {
        if (!ecs_has_id(world, entity, EcsUiScrollSubscribed)) {
            ecs_add_id(world, entity, EcsUiScrollSubscribed);
        }
        return true;
    }
    return EcsUiRemoveIdIfPresent(world, entity, EcsUiScrollSubscribed);
}

static ecs_entity_t EcsUiBeginStack(
    EcsUiBuilder *builder,
    EcsUiStackDesc desc,
    EcsUiNodeKind kind,
    EcsUiAxis axis,
    bool clear_scroll_view)
{
    ecs_entity_t entity = EcsUiCreateNode(builder, desc.id, desc.key, kind, true);
    if (entity == 0) {
        return 0;
    }

    EcsUiStack stack = {
        .axis = axis,
        .gap = desc.gap,
        .padding = desc.padding,
        .padding_left = desc.padding_left,
        .padding_top = desc.padding_top,
        .padding_right = desc.padding_right,
        .padding_bottom = desc.padding_bottom,
        .preferred_width = desc.preferred_width,
        .preferred_height = desc.preferred_height,
        .align_x = desc.align_x,
        .align_y = desc.align_y,
        .width_sizing = desc.width_sizing,
        .height_sizing = desc.height_sizing,
    };
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiStack),
        sizeof(stack),
        &stack);
    if (clear_scroll_view) {
        (void)EcsUiRemoveIdIfPresent(
            builder->world,
            entity,
            ecs_id(EcsUiScrollView));
    }
    if (!EcsUiSetStyleTokenForNode(
            builder->world,
            entity,
            desc.style_token)) {
        builder->failed = true;
    }
    if (desc.style != NULL) {
        (void)EcsUiSetIdIfChanged(
            builder->world,
            entity,
            ecs_id(EcsUiBoxStyle),
            sizeof(*desc.style),
            desc.style);
    } else {
        (void)EcsUiRemoveIdIfPresent(
            builder->world,
            entity,
            ecs_id(EcsUiBoxStyle));
    }
    if (desc.nine_slice_style != NULL) {
        (void)EcsUiSetIdIfChanged(
            builder->world,
            entity,
            ecs_id(EcsUiNineSliceStyle),
            sizeof(*desc.nine_slice_style),
            desc.nine_slice_style);
    } else {
        (void)EcsUiRemoveIdIfPresent(
            builder->world,
            entity,
            ecs_id(EcsUiNineSliceStyle));
    }
    if (!EcsUiSetScrollSubscribedForNode(
            builder->world,
            entity,
            desc.scroll_subscribed)) {
        builder->failed = true;
    }
    EcsUiPushParent(builder, entity);
    return entity;
}

static uint32_t EcsUiScrollAxesOrDefault(
    uint32_t axes,
    uint32_t default_axes)
{
    return axes != ECS_UI_SCROLL_AXIS_NONE ? axes : default_axes;
}

void EcsUiImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiNodeId);
    ECS_COMPONENT_DEFINE(world, EcsUiKey);
    ECS_COMPONENT_DEFINE(world, EcsUiAutoOrdinal);
    ECS_COMPONENT_DEFINE(world, EcsUiDeclaration);
    ECS_COMPONENT_DEFINE(world, EcsUiBuilderRootState);
    ECS_COMPONENT_DEFINE(world, EcsUiActionPayload);
    ECS_COMPONENT_DEFINE(world, EcsUiScale);
    ECS_COMPONENT_DEFINE(world, EcsUiNode);
    ECS_COMPONENT_DEFINE(world, EcsUiStack);
    ECS_COMPONENT_DEFINE(world, EcsUiBoxStyle);
    ECS_COMPONENT_DEFINE(world, EcsUiNineSliceStyle);
    ECS_COMPONENT_DEFINE(world, EcsUiTextStyle);
    ECS_COMPONENT_DEFINE(world, EcsUiTextLayout);
    ECS_COMPONENT_DEFINE(world, EcsUiButton);
    ECS_COMPONENT_DEFINE(world, EcsUiPressable);
    ECS_COMPONENT_DEFINE(world, EcsUiText);
    ECS_COMPONENT_DEFINE(world, EcsUiIcon);
    ECS_COMPONENT_DEFINE(world, EcsUiCustom);
    ECS_COMPONENT_DEFINE(world, EcsUiVisual);
    ECS_COMPONENT_DEFINE(world, EcsUiPlacement);
    ECS_COMPONENT_DEFINE(world, EcsUiHitTest);
    ECS_COMPONENT_DEFINE(world, EcsUiScrollView);
    ECS_COMPONENT_DEFINE(world, EcsUiTextFieldView);

    ECS_TAG_DEFINE(world, EcsUiRoot);
    ECS_TAG_DEFINE(world, EcsUiInteractive);
    ECS_TAG_DEFINE(world, EcsUiOnClick);
    ECS_TAG_DEFINE(world, EcsUiUsesStyle);
    ECS_TAG_DEFINE(world, EcsUiHovered);
    ECS_TAG_DEFINE(world, EcsUiHoverWithin);
    ECS_TAG_DEFINE(world, EcsUiRevealedByHover);
    ECS_TAG_DEFINE(world, EcsUiScrollSubscribed);
    ECS_TAG_DEFINE(world, EcsUiThemeTag);
    ECS_TAG_DEFINE(world, EcsUiActiveTheme);
    ECS_TAG_DEFINE(world, EcsUiThemeStyle);
    ecs_add_id(world, EcsUiOnClick, EcsExclusive);
    ecs_add_id(world, EcsUiUsesStyle, EcsExclusive);
    ecs_add_id(world, EcsUiRevealedByHover, EcsExclusive);
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
    if (world == NULL || entity == 0 || style_token == 0) {
        return false;
    }
    return EcsUiSetStyleTokenForNode(world, entity, style_token);
}

static void EcsUiSetVisualOpacity(
    ecs_world_t *world,
    ecs_entity_t entity,
    float opacity)
{
    if (world == NULL || entity == 0) {
        return;
    }

    EcsUiVisual visual = {
        .opacity = 1.0f,
    };
    const EcsUiVisual *existing = ecs_get(world, entity, EcsUiVisual);
    if (existing != NULL) {
        visual = *existing;
    }
    if (visual.opacity == opacity) {
        return;
    }
    visual.opacity = opacity;
    ecs_set_ptr(world, entity, EcsUiVisual, &visual);
}

static bool EcsUiRevealTriggerActive(
    const ecs_world_t *world,
    ecs_entity_t trigger)
{
    return world != NULL && trigger != 0 &&
        ecs_is_alive(world, trigger) &&
        ecs_has_id(world, trigger, EcsUiHoverWithin);
}

static void EcsUiApplyRevealForNode(
    ecs_world_t *world,
    ecs_entity_t entity)
{
    if (world == NULL || entity == 0 || EcsUiRevealedByHover == 0) {
        return;
    }

    ecs_entity_t trigger =
        ecs_get_target(world, entity, EcsUiRevealedByHover, 0);
    EcsUiSetVisualOpacity(
        world,
        entity,
        EcsUiRevealTriggerActive(world, trigger) ? 1.0f : 0.0f);
}

bool EcsUiSetRevealOnHover(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t trigger)
{
    if (world == NULL || entity == 0 || EcsUiRevealedByHover == 0) {
        return false;
    }

    if (trigger == 0) {
        trigger = ecs_get_target(world, entity, EcsChildOf, 0);
    }
    if (trigger == 0) {
        return false;
    }

    ecs_add_pair(world, entity, EcsUiRevealedByHover, trigger);
    EcsUiApplyRevealForNode(world, entity);
    return true;
}

bool EcsUiClearRevealOnHover(ecs_world_t *world, ecs_entity_t entity)
{
    if (world == NULL || entity == 0 || EcsUiRevealedByHover == 0) {
        return false;
    }

    ecs_remove_pair(world, entity, EcsUiRevealedByHover, EcsWildcard);
    EcsUiSetVisualOpacity(world, entity, 1.0f);
    return true;
}

static void EcsUiRemoveTagFromAll(ecs_world_t *world, ecs_entity_t tag)
{
    if (world == NULL || tag == 0) {
        return;
    }

    bool flush = ecs_defer_begin(world);
    ecs_iter_t it = ecs_each_id(world, tag);
    while (ecs_each_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            ecs_remove_id(world, it.entities[i], tag);
        }
    }
    if (flush) {
        (void)ecs_defer_end(world);
    }
}

bool EcsUiApplyHoverState(ecs_world_t *world, ecs_entity_t hovered_node)
{
    if (world == NULL || EcsUiHovered == 0 || EcsUiHoverWithin == 0 ||
        EcsUiRevealedByHover == 0) {
        return false;
    }

    EcsUiRemoveTagFromAll(world, EcsUiHovered);
    EcsUiRemoveTagFromAll(world, EcsUiHoverWithin);

    if (hovered_node != 0 && ecs_is_alive(world, hovered_node)) {
        ecs_add_id(world, hovered_node, EcsUiHovered);
        for (ecs_entity_t node = hovered_node; node != 0;
             node = ecs_get_target(world, node, EcsChildOf, 0)) {
            ecs_add_id(world, node, EcsUiHoverWithin);
        }
    }

    bool flush = ecs_defer_begin(world);
    ecs_iter_t it =
        ecs_each_id(world, ecs_pair(EcsUiRevealedByHover, EcsWildcard));
    while (ecs_each_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            EcsUiApplyRevealForNode(world, it.entities[i]);
        }
    }
    if (flush) {
        (void)ecs_defer_end(world);
    }

    return true;
}

ecs_entity_t EcsUiCurrentHoveredNode(const ecs_world_t *world)
{
    if (world == NULL || EcsUiHovered == 0) {
        return 0;
    }

    ecs_iter_t it = ecs_each_id(world, EcsUiHovered);
    while (ecs_each_next(&it)) {
        if (it.count > 0) {
            return it.entities[0];
        }
    }
    return 0;
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
        ecs_add_id(world, theme, EcsUiThemeTag);
        ecs_add_id(world, theme, EcsOrderedChildren);
    }
    return theme;
}

bool EcsUiSetActiveTheme(ecs_world_t *world, ecs_entity_t theme)
{
    if (world == NULL || theme == 0 || !EcsUiThemeReady() ||
        !ecs_has_id(world, theme, EcsUiThemeTag)) {
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
        !EcsUiThemeReady() || !ecs_has_id(world, theme, EcsUiThemeTag)) {
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
    return EcsUiSetIdIfChanged(
        world,
        source,
        ecs_id(EcsUiBoxStyle),
        sizeof(style),
        &style);
}

bool EcsUiThemeSetTextStyle(
    ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token,
    EcsUiTextStyle style)
{
    if (world == NULL || theme == 0 || style_token == 0 ||
        !EcsUiThemeReady() || !ecs_has_id(world, theme, EcsUiThemeTag)) {
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
    return EcsUiSetIdIfChanged(
        world,
        source,
        ecs_id(EcsUiTextStyle),
        sizeof(style),
        &style);
}

bool EcsUiThemeSetNineSliceStyle(
    ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token,
    EcsUiNineSliceStyle style)
{
    if (world == NULL || theme == 0 || style_token == 0 ||
        !EcsUiThemeReady() || !ecs_has_id(world, theme, EcsUiThemeTag)) {
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
    return EcsUiSetIdIfChanged(
        world,
        source,
        ecs_id(EcsUiNineSliceStyle),
        sizeof(style),
        &style);
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
            if (style_token == 0) {
                continue;
            }

            const EcsUiBoxStyle *box_style =
                ecs_get(world, source, EcsUiBoxStyle);
            if (box_style != NULL) {
                (void)EcsUiSetIdIfChanged(
                    world,
                    style_token,
                    ecs_id(EcsUiBoxStyle),
                    sizeof(*box_style),
                    box_style);
            }

            const EcsUiTextStyle *text_style =
                ecs_get(world, source, EcsUiTextStyle);
            if (text_style != NULL) {
                (void)EcsUiSetIdIfChanged(
                    world,
                    style_token,
                    ecs_id(EcsUiTextStyle),
                    sizeof(*text_style),
                    text_style);
            }

            const EcsUiNineSliceStyle *nine_slice_style =
                ecs_get(world, source, EcsUiNineSliceStyle);
            if (nine_slice_style != NULL) {
                (void)EcsUiSetIdIfChanged(
                    world,
                    style_token,
                    ecs_id(EcsUiNineSliceStyle),
                    sizeof(*nine_slice_style),
                    nine_slice_style);
            }
        }
    }
    return true;
}

static uint64_t EcsUiNextBuilderGeneration(
    ecs_world_t *world,
    ecs_entity_t root)
{
    if (world == NULL || root == 0) {
        return 0u;
    }

    EcsUiBuilderRootState state = {0};
    const EcsUiBuilderRootState *existing =
        ecs_get(world, root, EcsUiBuilderRootState);
    if (existing != NULL) {
        state = *existing;
    }
    state.next_generation += 1u;
    if (state.next_generation == 0u) {
        state.next_generation = 1u;
    }
    (void)EcsUiSetIdIfChanged(
        world,
        root,
        ecs_id(EcsUiBuilderRootState),
        sizeof(state),
        &state);
    return state.next_generation;
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

    if (!ecs_has_id(world, root, EcsUiRoot)) {
        ecs_add_id(world, root, EcsUiRoot);
    }
    if (!ecs_has_id(world, root, EcsOrderedChildren)) {
        ecs_add_id(world, root, EcsOrderedChildren);
    }
    if (!ecs_has(world, root, EcsUiScale)) {
        (void)EcsUiSetScale(world, root, 1.0f);
    }
    (void)EcsUiSetNodeKind(world, root, ECS_UI_NODE_ROOT);
    EcsUiSetNodeId(world, root, name);
    return root;
}

bool EcsUiSetScale(ecs_world_t *world, ecs_entity_t root, float scale)
{
    if (world == NULL || root == 0) {
        return false;
    }

    EcsUiScale value = {
        .value = EcsUiNormalizeScale(scale),
    };
    return EcsUiSetIdIfChanged(
        world,
        root,
        ecs_id(EcsUiScale),
        sizeof(value),
        &value);
}

float EcsUiGetScale(const ecs_world_t *world, ecs_entity_t root)
{
    if (world == NULL || root == 0) {
        return 1.0f;
    }
    const EcsUiScale *scale = ecs_get(world, root, EcsUiScale);
    return EcsUiNormalizeScale(scale != NULL ? scale->value : 1.0f);
}

static uint32_t EcsUiBuilderChildrenForParent(
    const EcsUiBuilder *builder,
    ecs_entity_t parent,
    ecs_entity_t *out,
    uint32_t out_count)
{
    if (builder == NULL || parent == 0 || out == NULL || out_count == 0u) {
        return 0u;
    }

    uint32_t count = 0u;
    for (uint32_t i = 0u; i < builder->declared_child_count; i += 1u) {
        const EcsUiBuilderDeclaredChild *declared =
            &builder->declared_children[i];
        if (declared->parent != parent) {
            continue;
        }
        if (count >= out_count) {
            return count;
        }
        out[count] = declared->child;
        count += 1u;
    }
    return count;
}

static bool EcsUiBuilderChildDeclaredForParent(
    const EcsUiBuilder *builder,
    ecs_entity_t parent,
    ecs_entity_t child)
{
    if (builder == NULL || parent == 0 || child == 0) {
        return false;
    }

    for (uint32_t i = 0u; i < builder->declared_child_count; i += 1u) {
        const EcsUiBuilderDeclaredChild *declared =
            &builder->declared_children[i];
        if (declared->parent == parent && declared->child == child) {
            return true;
        }
    }
    return false;
}

static bool EcsUiBuilderPreservesExistingChildren(
    const EcsUiBuilder *builder,
    ecs_entity_t parent)
{
    return builder != NULL &&
        builder->world != NULL &&
        parent != 0 &&
        EcsUiProjectionPreserveChildren != 0 &&
        ecs_has_id(builder->world, parent, EcsUiProjectionPreserveChildren);
}

static bool EcsUiBuilderPruneParent(
    EcsUiBuilder *builder,
    ecs_entity_t parent)
{
    if (builder == NULL || builder->world == NULL || parent == 0) {
        return false;
    }
    if (EcsUiBuilderPreservesExistingChildren(builder, parent)) {
        return true;
    }

    ecs_entity_t stale[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t stale_count = 0u;
    ecs_iter_t it = ecs_children(builder->world, parent);
    while (ecs_children_next(&it)) {
        for (int32_t i = 0; i < it.count; i += 1) {
            ecs_entity_t child = it.entities[i];
            if (EcsUiBuilderChildDeclaredForParent(builder, parent, child)) {
                continue;
            }
            const EcsUiDeclaration *declaration =
                ecs_get(builder->world, child, EcsUiDeclaration);
            if (declaration == NULL ||
                declaration->generation == builder->generation) {
                continue;
            }
            if (stale_count >= (uint32_t)ECS_UI_TREE_NODE_MAX) {
                builder->failed = true;
                return false;
            }
            stale[stale_count] = child;
            stale_count += 1u;
        }
    }

    for (uint32_t i = 0u; i < stale_count; i += 1u) {
        ecs_delete(builder->world, stale[i]);
    }
    return true;
}

static bool EcsUiBuilderEnforceParentOrder(
    EcsUiBuilder *builder,
    ecs_entity_t parent)
{
    if (builder == NULL || builder->world == NULL || parent == 0) {
        return false;
    }
    if (EcsUiBuilderPreservesExistingChildren(builder, parent)) {
        return true;
    }
    if (!ecs_has_id(builder->world, parent, EcsOrderedChildren)) {
        builder->failed = true;
        return false;
    }

    ecs_entity_t declared[ECS_UI_TREE_NODE_MAX] = {0};
    const uint32_t declared_count = EcsUiBuilderChildrenForParent(
        builder,
        parent,
        declared,
        (uint32_t)ECS_UI_TREE_NODE_MAX);
    ecs_entities_t current = ecs_get_ordered_children(builder->world, parent);

    uint32_t first_declared = (uint32_t)current.count;
    uint32_t unmanaged_count = 0u;
    ecs_entity_t unmanaged[ECS_UI_TREE_NODE_MAX] = {0};
    for (int32_t i = 0; i < current.count; i += 1) {
        const ecs_entity_t child = current.ids[i];
        if (EcsUiBuilderChildDeclaredForParent(builder, parent, child)) {
            if (first_declared == (uint32_t)current.count) {
                first_declared = (uint32_t)i;
            }
            continue;
        }
        if (unmanaged_count >= (uint32_t)ECS_UI_TREE_NODE_MAX) {
            builder->failed = true;
            return false;
        }
        unmanaged[unmanaged_count] = child;
        unmanaged_count += 1u;
    }

    if (unmanaged_count + declared_count > (uint32_t)ECS_UI_TREE_NODE_MAX) {
        builder->failed = true;
        return false;
    }

    for (uint32_t i = 0u; i < declared_count; i += 1u) {
        if (ecs_get_parent(builder->world, declared[i]) != parent) {
            builder->failed = true;
            return false;
        }
    }

    const uint32_t insert_at =
        first_declared <= unmanaged_count ? first_declared : unmanaged_count;
    ecs_entity_t ordered[ECS_UI_TREE_NODE_MAX] = {0};
    uint32_t ordered_count = 0u;
    for (uint32_t i = 0u; i < insert_at; i += 1u) {
        ordered[ordered_count] = unmanaged[i];
        ordered_count += 1u;
    }
    for (uint32_t i = 0u; i < declared_count; i += 1u) {
        ordered[ordered_count] = declared[i];
        ordered_count += 1u;
    }
    for (uint32_t i = insert_at; i < unmanaged_count; i += 1u) {
        ordered[ordered_count] = unmanaged[i];
        ordered_count += 1u;
    }

    if (current.count != (int32_t)ordered_count) {
        builder->failed = true;
        return false;
    }
    bool differs = false;
    for (uint32_t i = 0u; i < ordered_count; i += 1u) {
        if (current.ids[i] != ordered[i]) {
            differs = true;
            break;
        }
    }
    if (!differs) {
        return true;
    }

    ecs_set_child_order(
        builder->world,
        parent,
        ordered_count > 0u ? ordered : NULL,
        (int32_t)ordered_count);
    return true;
}

static bool EcsUiBuilderReconcile(EcsUiBuilder *builder)
{
    if (builder == NULL || builder->world == NULL || builder->failed) {
        return false;
    }

    for (uint32_t i = 0u; i < builder->declared_parent_count; i += 1u) {
        if (!EcsUiBuilderPruneParent(builder, builder->declared_parents[i])) {
            return false;
        }
    }
    for (uint32_t i = 0u; i < builder->declared_parent_count; i += 1u) {
        if (!EcsUiBuilderEnforceParentOrder(
                builder,
                builder->declared_parents[i])) {
            return false;
        }
    }
    return true;
}

EcsUiBuilder EcsUiBuilderBegin(ecs_world_t *world, ecs_entity_t root)
{
    EcsUiBuilder builder = {0};
    builder.world = world;
    builder.root = root;
    if (world == NULL || root == 0 || ecs_is_deferred(world)) {
        builder.failed = true;
        return builder;
    }

    if (!ecs_has_id(world, root, EcsOrderedChildren)) {
        ecs_add_id(world, root, EcsOrderedChildren);
    }
    builder.generation = EcsUiNextBuilderGeneration(world, root);
    (void)EcsUiStampDeclaration(world, root, builder.generation);
    (void)EcsUiBuilderMarkParent(&builder, root);
    builder.parent_stack[0] = root;
    builder.parent_frames[0] = (EcsUiBuilderParentFrame){
        .parent = root,
        .next_auto_ordinal = 1u,
    };
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
    if (!builder->failed) {
        (void)EcsUiBuilderReconcile(builder);
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
        ECS_UI_AXIS_VERTICAL,
        true);
}

ecs_entity_t EcsUiBeginHStack(EcsUiBuilder *builder, EcsUiStackDesc desc)
{
    return EcsUiBeginStack(
        builder,
        desc,
        ECS_UI_NODE_HSTACK,
        ECS_UI_AXIS_HORIZONTAL,
        true);
}

ecs_entity_t EcsUiBeginZStack(EcsUiBuilder *builder, EcsUiStackDesc desc)
{
    return EcsUiBeginStack(
        builder,
        desc,
        ECS_UI_NODE_ZSTACK,
        ECS_UI_AXIS_DEPTH,
        true);
}

ecs_entity_t EcsUiBeginVScrollView(
    EcsUiBuilder *builder,
    EcsUiScrollViewDesc desc)
{
    ecs_entity_t entity = EcsUiBeginStack(
        builder,
        desc.stack,
        ECS_UI_NODE_VSTACK,
        ECS_UI_AXIS_VERTICAL,
        false);
    if (entity == 0) {
        return 0;
    }

    EcsUiScrollView scroll_view = {
        .axes = EcsUiScrollAxesOrDefault(
            desc.axes,
            ECS_UI_SCROLL_AXIS_Y),
    };
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiScrollView),
        sizeof(scroll_view),
        &scroll_view);
    return entity;
}

ecs_entity_t EcsUiBeginHScrollView(
    EcsUiBuilder *builder,
    EcsUiScrollViewDesc desc)
{
    ecs_entity_t entity = EcsUiBeginStack(
        builder,
        desc.stack,
        ECS_UI_NODE_HSTACK,
        ECS_UI_AXIS_HORIZONTAL,
        false);
    if (entity == 0) {
        return 0;
    }

    EcsUiScrollView scroll_view = {
        .axes = EcsUiScrollAxesOrDefault(
            desc.axes,
            ECS_UI_SCROLL_AXIS_X),
    };
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiScrollView),
        sizeof(scroll_view),
        &scroll_view);
    return entity;
}

ecs_entity_t EcsUiBeginButton(EcsUiBuilder *builder, EcsUiButtonDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(
            builder,
            desc.id,
            desc.key,
            ECS_UI_NODE_BUTTON,
            true);
    if (entity == 0) {
        return 0;
    }

    EcsUiButton button = {
        .variant = desc.variant,
        .preferred_width = desc.preferred_width,
        .preferred_height = desc.preferred_height,
        .disabled = desc.disabled,
    };
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiButton),
        sizeof(button),
        &button);
    EcsUiSetActionPair(
        builder->world,
        entity,
        EcsUiOnClick,
        desc.on_click,
        desc.payload,
        true);
    if (!EcsUiSetStyleTokenForNode(
            builder->world,
            entity,
            desc.style_token)) {
        builder->failed = true;
    }
    if (!EcsUiSetScrollSubscribedForNode(
            builder->world,
            entity,
            desc.scroll_subscribed)) {
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
        EcsUiCreateNode(
            builder,
            desc.id,
            desc.key,
            ECS_UI_NODE_PRESSABLE,
            true);
    if (entity == 0) {
        return 0;
    }

    EcsUiPressable pressable = {
        .preferred_height = desc.preferred_height,
        .disabled = desc.disabled,
    };
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiPressable),
        sizeof(pressable),
        &pressable);
    EcsUiSetActionPair(
        builder->world,
        entity,
        EcsUiOnClick,
        desc.on_click,
        desc.payload,
        true);
    if (!EcsUiSetStyleTokenForNode(
            builder->world,
            entity,
            desc.style_token)) {
        builder->failed = true;
    }
    if (!EcsUiSetScrollSubscribedForNode(
            builder->world,
            entity,
            desc.scroll_subscribed)) {
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

bool EcsUiSetScrollView(
    ecs_world_t *world,
    ecs_entity_t entity,
    EcsUiScrollView scroll_view)
{
    if (world == NULL || entity == 0) {
        return false;
    }
    if (scroll_view.axes == ECS_UI_SCROLL_AXIS_NONE) {
        return EcsUiClearScrollView(world, entity);
    }

    ecs_set_ptr(world, entity, EcsUiScrollView, &scroll_view);
    return true;
}

bool EcsUiClearScrollView(ecs_world_t *world, ecs_entity_t entity)
{
    if (world == NULL || entity == 0) {
        return false;
    }

    ecs_remove(world, entity, EcsUiScrollView);
    return true;
}

ecs_entity_t EcsUiAddText(EcsUiBuilder *builder, EcsUiTextDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(
            builder,
            desc.id,
            desc.key,
            ECS_UI_NODE_TEXT,
            false);
    if (entity == 0) {
        return 0;
    }

    EcsUiText text = {
        .role = desc.role,
    };
    EcsUiCopyString(text.text, sizeof(text.text), desc.text);
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiText),
        sizeof(text),
        &text);
    return entity;
}

ecs_entity_t EcsUiAddIcon(EcsUiBuilder *builder, EcsUiIconDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(
            builder,
            desc.id,
            desc.key,
            ECS_UI_NODE_ICON,
            false);
    if (entity == 0) {
        return 0;
    }

    EcsUiIcon icon = {0};
    EcsUiCopyString(icon.name, sizeof(icon.name), desc.name);
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiIcon),
        sizeof(icon),
        &icon);
    return entity;
}

ecs_entity_t EcsUiAddCustom(EcsUiBuilder *builder, EcsUiCustomDesc desc)
{
    ecs_entity_t entity =
        EcsUiCreateNode(
            builder,
            desc.id,
            desc.key,
            ECS_UI_NODE_CUSTOM,
            false);
    if (entity == 0) {
        return 0;
    }

    EcsUiCustom custom = {
        .preferred_width = desc.preferred_width,
        .preferred_height = desc.preferred_height,
        .width_sizing = desc.width_sizing,
        .height_sizing = desc.height_sizing,
    };
    EcsUiCopyString(custom.kind, sizeof(custom.kind), desc.kind);
    (void)EcsUiSetIdIfChanged(
        builder->world,
        entity,
        ecs_id(EcsUiCustom),
        sizeof(custom),
        &custom);
    EcsUiSetActionPair(
        builder->world,
        entity,
        EcsUiOnClick,
        desc.on_click,
        desc.payload,
        false);
    if (!EcsUiSetScrollSubscribedForNode(
            builder->world,
            entity,
            desc.scroll_subscribed)) {
        builder->failed = true;
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
    const EcsUiActionPayload *payload =
        ecs_get(world, entity, EcsUiActionPayload);
    snapshot->payload = payload != NULL ? payload->value : 0u;
    snapshot->kind = node->kind;
    snapshot->depth = depth;
    snapshot->parent_index = parent_index;
    snapshot->first_child = ECS_UI_TREE_INVALID_INDEX;
    snapshot->next_sibling = ECS_UI_TREE_INVALID_INDEX;
    snapshot->visual.opacity = 1.0f;
    snapshot->hovered = EcsUiHovered != 0 &&
        ecs_has_id(world, entity, EcsUiHovered);
    snapshot->hover_within = EcsUiHoverWithin != 0 &&
        ecs_has_id(world, entity, EcsUiHoverWithin);

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

    const EcsUiNineSliceStyle *nine_slice_style =
        EcsUiResolveNineSliceStyle(world, entity);
    if (nine_slice_style != NULL && nine_slice_style->image[0] != '\0') {
        snapshot->nine_slice_style = *nine_slice_style;
        snapshot->has_nine_slice_style = true;
    }

    const EcsUiTextStyle *text_style = EcsUiResolveTextStyle(world, entity);
    if (text_style != NULL) {
        snapshot->text_style = *text_style;
        snapshot->has_text_style = true;
    }

    const EcsUiTextLayout *text_layout =
        ecs_get(world, entity, EcsUiTextLayout);
    if (text_layout != NULL) {
        snapshot->text_layout = *text_layout;
        snapshot->has_text_layout = true;
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

    const EcsUiPlacement *placement =
        ecs_get(world, entity, EcsUiPlacement);
    if (placement != NULL) {
        snapshot->placement = *placement;
        snapshot->has_placement = true;
    }

    const EcsUiHitTest *hit_test = ecs_get(world, entity, EcsUiHitTest);
    if (hit_test != NULL) {
        snapshot->hit_test = *hit_test;
    }

    const EcsUiScrollView *scroll_view =
        ecs_get(world, entity, EcsUiScrollView);
    if (scroll_view != NULL) {
        snapshot->scroll_view = *scroll_view;
        snapshot->has_scroll_view = true;
    }
    snapshot->scroll_subscribed = EcsUiScrollSubscribed != 0 &&
        ecs_has_id(world, entity, EcsUiScrollSubscribed);

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
    out->scale = EcsUiGetScale(world, root);
    return EcsUiReadNode(
        world,
        root,
        0,
        0u,
        ECS_UI_TREE_INVALID_INDEX,
        out) != ECS_UI_TREE_INVALID_INDEX;
}

const EcsUiTreeNodeSnapshot *EcsUiTreeSnapshotFindNodeById(
    const EcsUiTreeSnapshot *tree,
    const char *id)
{
    if (tree == NULL || id == NULL) {
        return NULL;
    }

    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (strcmp(tree->nodes[i].id, id) == 0) {
            return &tree->nodes[i];
        }
    }
    return NULL;
}

typedef struct EcsUiDebugDumpWriter {
    char *buffer;
    size_t size;
    size_t bytes_written;
    bool truncated;
} EcsUiDebugDumpWriter;

static const char *EcsUiDebugNodeKindName(EcsUiNodeKind kind)
{
    switch (kind) {
    case ECS_UI_NODE_ROOT:
        return "root";
    case ECS_UI_NODE_VSTACK:
        return "vstack";
    case ECS_UI_NODE_HSTACK:
        return "hstack";
    case ECS_UI_NODE_ZSTACK:
        return "zstack";
    case ECS_UI_NODE_BUTTON:
        return "button";
    case ECS_UI_NODE_TEXT:
        return "text";
    case ECS_UI_NODE_ICON:
        return "icon";
    case ECS_UI_NODE_CUSTOM:
        return "custom";
    case ECS_UI_NODE_PRESSABLE:
        return "pressable";
    case ECS_UI_NODE_NONE:
    default:
        return "none";
    }
}

static const char *EcsUiDebugHitTestName(EcsUiHitTestMode mode)
{
    switch (mode) {
    case ECS_UI_HIT_TEST_NONE:
        return "none";
    case ECS_UI_HIT_TEST_CHILDREN:
        return "children";
    case ECS_UI_HIT_TEST_CAPTURE:
        return "capture";
    case ECS_UI_HIT_TEST_AUTO:
    default:
        return "auto";
    }
}

static void EcsUiDebugIndexString(
    uint32_t index,
    char *out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (index == ECS_UI_TREE_INVALID_INDEX) {
        (void)snprintf(out, out_size, "none");
        return;
    }
    (void)snprintf(out, out_size, "%u", index);
}

static void EcsUiDebugChildRangeString(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node,
    char *out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (tree == NULL || node == NULL ||
        node->first_child == ECS_UI_TREE_INVALID_INDEX ||
        node->first_child >= tree->count) {
        (void)snprintf(out, out_size, "none");
        return;
    }

    uint32_t last_child = node->first_child;
    uint32_t child_count = 0u;
    for (uint32_t child = node->first_child;
            child != ECS_UI_TREE_INVALID_INDEX && child < tree->count &&
            child_count <= tree->count;
            child = tree->nodes[child].next_sibling) {
        last_child = child;
        child_count += 1u;
    }

    (void)snprintf(
        out,
        out_size,
        "%u..%u count=%u",
        node->first_child,
        last_child,
        child_count);
}

static void EcsUiDebugDumpAppend(
    EcsUiDebugDumpWriter *writer,
    const char *format,
    ...)
{
    if (writer == NULL || format == NULL) {
        return;
    }

    char line[512] = {0};
    va_list args;
    va_start(args, format);
    const int formatted = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (formatted <= 0) {
        return;
    }

    size_t length = (size_t)formatted;
    if (length >= sizeof(line)) {
        length = sizeof(line) - 1u;
        writer->truncated = true;
    }

    if (writer->buffer == NULL || writer->size == 0u) {
        writer->truncated = true;
        return;
    }

    if (writer->bytes_written >= writer->size - 1u) {
        writer->buffer[writer->size - 1u] = '\0';
        writer->truncated = true;
        return;
    }

    const size_t available = writer->size - 1u - writer->bytes_written;
    const size_t copy_length = length < available ? length : available;
    memcpy(writer->buffer + writer->bytes_written, line, copy_length);
    writer->bytes_written += copy_length;
    writer->buffer[writer->bytes_written] = '\0';
    if (copy_length < length) {
        writer->truncated = true;
    }
}

static void EcsUiDebugDumpNode(
    EcsUiDebugDumpWriter *writer,
    const EcsUiTreeSnapshot *tree,
    uint32_t index)
{
    if (writer == NULL || tree == NULL || index >= tree->count) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    char parent[16] = {0};
    char children[48] = {0};
    EcsUiDebugIndexString(node->parent_index, parent, sizeof(parent));
    EcsUiDebugChildRangeString(tree, node, children, sizeof(children));

    EcsUiDebugDumpAppend(
        writer,
        "[%u] parent=%s children=%s depth=%u entity=%llu id=\"%s\" "
        "kind=%s hit=%s visual(opacity=%.3g offset=%.3g,%.3g) "
        "placement=%s",
        index,
        parent,
        children,
        node->depth,
        (unsigned long long)node->entity,
        node->id,
        EcsUiDebugNodeKindName(node->kind),
        EcsUiDebugHitTestName(node->hit_test.mode),
        node->visual.opacity,
        node->visual.offset_x,
        node->visual.offset_y,
        node->has_placement ? "yes" : "no");

    if (node->has_text_field_view) {
        EcsUiDebugDumpAppend(
            writer,
            " text_field=%s",
            node->text_field_view.focused ? "focused" : "blurred");
    }

    if (node->has_layout) {
        EcsUiDebugDumpAppend(
            writer,
            " layout=(%.9g,%.9g %.9gx%.9g)",
            node->layout_x,
            node->layout_y,
            node->layout_width,
            node->layout_height);
    } else {
        EcsUiDebugDumpAppend(writer, " layout=none");
    }

    EcsUiDebugDumpAppend(writer, "\n");
}

EcsUiTreeDebugDumpResult EcsUiTreeDebugDumpSnapshot(
    const EcsUiTreeSnapshot *tree,
    char *buffer,
    size_t size)
{
    EcsUiDebugDumpWriter writer = {
        .buffer = buffer,
        .size = size,
    };
    if (buffer != NULL && size > 0u) {
        buffer[0] = '\0';
    }

    if (tree == NULL) {
        EcsUiDebugDumpAppend(&writer, "tree <null>\n");
        return (EcsUiTreeDebugDumpResult){
            .bytes_written = writer.bytes_written,
            .truncated = writer.truncated,
        };
    }

    EcsUiDebugDumpAppend(
        &writer,
        "tree root=%llu scale=%.3g count=%u truncated=%s\n",
        (unsigned long long)tree->root,
        tree->scale,
        tree->count,
        tree->truncated ? "true" : "false");
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        EcsUiDebugDumpNode(&writer, tree, i);
    }

    return (EcsUiTreeDebugDumpResult){
        .bytes_written = writer.bytes_written,
        .truncated = writer.truncated,
    };
}

EcsUiTreeDebugDumpResult EcsUiTreeDebugDump(
    const ecs_world_t *world,
    ecs_entity_t root,
    char *buffer,
    size_t size)
{
    EcsUiTreeSnapshot tree = {0};
    if (!EcsUiReadTree(world, root, &tree)) {
        EcsUiDebugDumpWriter writer = {
            .buffer = buffer,
            .size = size,
        };
        if (buffer != NULL && size > 0u) {
            buffer[0] = '\0';
        }
        EcsUiDebugDumpAppend(&writer, "tree <unreadable>\n");
        return (EcsUiTreeDebugDumpResult){
            .bytes_written = writer.bytes_written,
            .truncated = writer.truncated,
        };
    }
    return EcsUiTreeDebugDumpSnapshot(&tree, buffer, size);
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
