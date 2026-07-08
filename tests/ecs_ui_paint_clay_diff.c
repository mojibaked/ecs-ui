#include "ecs_ui/ecs_ui_frame.h"
#include "../src/ecs_ui_frame_internal.h"
#include "../src/ecs_ui_paint_clay_adapter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern Clay_ElementId Clay__HashNumber(uint32_t offset, uint32_t seed);

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static int RequireNear(
    float actual,
    float expected,
    float epsilon,
    const char *message)
{
    float delta = actual - expected;
    if (delta < 0.0f) {
        delta = -delta;
    }
    if (delta > epsilon) {
        (void)fprintf(
            stderr,
            "SEMANTIC %s: actual=%f expected=%f\n",
            message,
            actual,
            expected);
        return 1;
    }
    return 0;
}

static int RequireColor(
    Clay_Color actual,
    Clay_Color expected,
    const char *message)
{
    int result = 0;
    result |= RequireNear(actual.r, expected.r, 0.001f, message);
    result |= RequireNear(actual.g, expected.g, 0.001f, message);
    result |= RequireNear(actual.b, expected.b, 0.001f, message);
    result |= RequireNear(actual.a, expected.a, 0.001f, message);
    return result;
}

static Clay_Color DiffPaintColor(EcsUiColorF color, float opacity)
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

static EcsUiSize TestMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    (void)utf8;
    (void)user_data;
    const float font_size = spec != NULL ? spec->font_size : 0.0f;
    const float chars = length > 0 ? (float)length : 0.0f;
    return (EcsUiSize){
        .width = chars * font_size * 0.5f + 3.0f,
        .height = font_size + 4.0f,
    };
}

static ecs_world_t *CreateWorld(void)
{
    ecs_world_t *world = ecs_init();
    if (world != NULL) {
        EcsUiImport(world);
    }
    return world;
}

static int BuildDiffTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "diff scale failed");

    const EcsUiNineSliceStyle nine_slice_style = {
        .image = "diff.frame",
        .slice_left = 3u,
        .slice_top = 4u,
        .slice_right = 5u,
        .slice_bottom = 6u,
        .scale = 1.0f,
        .tint = {180u, 190u, 200u, 210u},
    };
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t panel = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffPanel",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .padding = 1.25f,
            .gap = 2.5f,
        });
    ecs_entity_t inner = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffInner",
            .preferred_width = 32.5f,
            .preferred_height = 18.25f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t diff_text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DiffText",
            .text = "x",
            .role = ECS_UI_TEXT_CAPTION,
        });
    ecs_entity_t field = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "DiffTextField",
            .preferred_height = 28.5f,
        });
    ecs_entity_t field_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DiffTextFieldValue",
            .text = "aa bb",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    ecs_entity_t repeated_field = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "DiffRepeatedTextField",
            .preferred_height = 28.5f,
        });
    ecs_entity_t repeated_value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DiffRepeatedTextFieldValue",
            .text = "aa",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "DiffButton",
            .variant = ECS_UI_BUTTON_PRIMARY,
            .preferred_width = 28.5f,
            .preferred_height = 16.25f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t subpixel = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffSubpixel",
            .preferred_width = 12.75f,
            .preferred_height = 9.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t custom = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "DiffCustom",
            .kind = "diff.custom",
            .preferred_width = 18.5f,
            .preferred_height = 12.5f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "DiffIcon",
            .name = "diff-icon",
        });
    ecs_entity_t nine_slice = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffNine",
            .preferred_width = 19.25f,
            .preferred_height = 11.75f,
            .nine_slice_style = &nine_slice_style,
        });
    EcsUiEnd(&builder);
    ecs_entity_t bevel = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffBevel",
            .preferred_width = 21.5f,
            .preferred_height = 13.25f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t clip = EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "DiffClip",
                .preferred_width = 54.5f,
                .preferred_height = 28.5f,
            },
            .axes = ECS_UI_SCROLL_AXIS_Y,
        });
    ecs_entity_t clip_child = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffClipChild",
            .preferred_width = 48.5f,
            .preferred_height = 64.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    ecs_entity_t z = EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffZOrder",
            .preferred_width = 46.5f,
            .preferred_height = 26.5f,
        });
    ecs_entity_t z_flow = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffZFlow",
            .preferred_width = 22.5f,
            .preferred_height = 14.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t z_float_a = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffZFloatA",
            .preferred_width = 20.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t z_float_b = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DiffZFloatB",
            .preferred_width = 18.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "diff builder failed");

    ecs_set(world, panel, EcsUiBoxStyle, {
        .background = {22u, 33u, 44u, 210u},
        .radius = 0.25f,
        .border_width = 1.25f,
        .border_left_width = 2.5f,
        .border_bottom_width = 3.5f,
        .border_color = {80u, 90u, 100u, 220u},
    });
    ecs_set(world, panel, EcsUiVisual, {
        .opacity = 0.8f,
    });
    ecs_set(world, inner, EcsUiBoxStyle, {
        .background = {120u, 130u, 140u, 240u},
        .radius = 5.75f,
        .border_width = 2.25f,
        .border_top_width = 3.25f,
        .border_color = {150u, 160u, 170u, 230u},
    });
    ecs_set(world, field, EcsUiTextFieldView, {
        .value_node = field_value,
        .focused = true,
        .cursor = 5u,
        .selection_anchor = 3u,
        .selection_focus = 5u,
        .caret_width = 2.5f,
    });
    ecs_set(world, field, EcsUiBoxStyle, {
        .background = {55u, 65u, 75u, 210u},
        .border_width = 2.0f,
        .border_color = {145u, 155u, 165u, 230u},
    });
    ecs_set(world, repeated_field, EcsUiTextFieldView, {
        .value_node = repeated_value,
        .focused = true,
        .cursor = 1u,
    });
    ecs_set(world, diff_text, EcsUiBoxStyle, {
        .border_width = 2.0f,
        .border_color = {135u, 145u, 155u, 230u},
    });
    ecs_set(world, subpixel, EcsUiBoxStyle, {
        .background = {44u, 55u, 66u, 200u},
        .radius = 3.5f,
        .border_width = 0.3f,
        .border_color = {210u, 160u, 90u, 240u},
    });
    ecs_set(world, custom, EcsUiBoxStyle, {
        .border_width = 2.0f,
        .border_color = {70u, 80u, 90u, 230u},
    });
    ecs_set(world, nine_slice, EcsUiBoxStyle, {
        .background = {77u, 88u, 99u, 255u},
    });
    ecs_set(world, bevel, EcsUiBoxStyle, {
        .background = {30u, 40u, 50u, 220u},
        .bevel = ECS_UI_BEVEL_RAISED,
        .bevel_light = {240u, 245u, 250u, 230u},
        .bevel_dark = {20u, 25u, 30u, 210u},
    });
    ecs_set(world, clip, EcsUiBoxStyle, {
        .background = {88u, 98u, 108u, 220u},
    });
    ecs_set(world, clip_child, EcsUiBoxStyle, {
        .background = {98u, 108u, 118u, 220u},
    });
    ecs_set(world, z, EcsUiBoxStyle, {
        .background = {108u, 118u, 128u, 220u},
    });
    ecs_set(world, z_flow, EcsUiBoxStyle, {
        .background = {118u, 128u, 138u, 220u},
    });
    ecs_set(world, z_float_a, EcsUiBoxStyle, {
        .background = {128u, 138u, 148u, 220u},
    });
    ecs_set(world, z_float_b, EcsUiBoxStyle, {
        .background = {138u, 148u, 158u, 220u},
    });
    ecs_set(world, z_float_a, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_START,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_START,
        .child_y = ECS_UI_ALIGN_START,
        .width = 20.5f,
        .height = 12.5f,
    });
    ecs_set(world, z_float_b, EcsUiPlacement, {
        .mode = ECS_UI_PLACEMENT_PARENT,
        .parent_x = ECS_UI_ALIGN_END,
        .parent_y = ECS_UI_ALIGN_END,
        .child_x = ECS_UI_ALIGN_END,
        .child_y = ECS_UI_ALIGN_END,
        .width = 18.5f,
        .height = 10.5f,
    });
    return result;
}

static const EcsUiTreeNodeSnapshot *FindNodeByEntity(
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

static const Clay_RenderCommand *FindCommand(
    const Clay_RenderCommandArray *commands,
    uint32_t id,
    Clay_RenderCommandType type)
{
    if (commands == NULL || commands->internalArray == NULL || id == 0u) {
        return NULL;
    }
    for (int32_t i = 0; i < commands->length; i += 1) {
        const Clay_RenderCommand *command = &commands->internalArray[i];
        if (command->id == id && command->commandType == type) {
            return command;
        }
    }
    return NULL;
}

static int32_t FindCommandIndex(
    const Clay_RenderCommandArray *commands,
    uint32_t id,
    Clay_RenderCommandType type)
{
    if (commands == NULL || commands->internalArray == NULL || id == 0u) {
        return -1;
    }
    for (int32_t i = 0; i < commands->length; i += 1) {
        const Clay_RenderCommand *command = &commands->internalArray[i];
        if (command->id == id && command->commandType == type) {
            return i;
        }
    }
    return -1;
}

static const Clay_RenderCommand *FindTextCommandByRun(
    const Clay_RenderCommandArray *commands,
    const EcsUiPaintTextRun *run,
    bool *claimed,
    int32_t *out_index)
{
    if (out_index != NULL) {
        *out_index = -1;
    }
    if (commands == NULL || commands->internalArray == NULL ||
            run == NULL || run->text == NULL) {
        return NULL;
    }
    for (int32_t i = 0; i < commands->length; i += 1) {
        if (claimed != NULL && claimed[i]) {
            continue;
        }
        const Clay_RenderCommand *command = &commands->internalArray[i];
        if (command->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT) {
            continue;
        }
        const Clay_StringSlice slice =
            command->renderData.text.stringContents;
        const int32_t length =
            (int32_t)(run->byte_end - run->byte_start);
        if (slice.length != length) {
            continue;
        }
        if ((slice.baseChars == run->text &&
                slice.chars == &run->text[run->byte_start]) ||
                (slice.chars != NULL &&
                    strncmp(
                        slice.chars,
                        &run->text[run->byte_start],
                        (size_t)length) == 0)) {
            if (out_index != NULL) {
                *out_index = i;
            }
            return command;
        }
    }
    return NULL;
}

static int RequireBoundsEqual(
    Clay_BoundingBox actual,
    Clay_BoundingBox expected,
    const char *message)
{
    int result = 0;
    result |= RequireNear(actual.x, expected.x, 0.001f, message);
    result |= RequireNear(actual.y, expected.y, 0.001f, message);
    result |= RequireNear(actual.width, expected.width, 0.001f, message);
    result |= RequireNear(actual.height, expected.height, 0.001f, message);
    return result;
}

static int CompareScissorSequence(
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter)
{
    int result = 0;
    int32_t bridge_seen = 0;
    int32_t adapter_seen = 0;
    int32_t adapter_cursor = 0;
    for (int32_t i = 0; bridge != NULL && i < bridge->length; i += 1) {
        const Clay_RenderCommand *bridge_command = &bridge->internalArray[i];
        if (bridge_command->commandType != CLAY_RENDER_COMMAND_TYPE_SCISSOR_START &&
                bridge_command->commandType != CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
            continue;
        }
        bridge_seen += 1;
        const Clay_RenderCommand *adapter_command = NULL;
        while (adapter != NULL && adapter_cursor < adapter->length) {
            const Clay_RenderCommand *candidate =
                &adapter->internalArray[adapter_cursor];
            adapter_cursor += 1;
            if (candidate->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START ||
                    candidate->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
                adapter_command = candidate;
                adapter_seen += 1;
                break;
            }
        }
        result |= Require(
            adapter_command != NULL,
            "adapter scissor command missing");
        if (adapter_command == NULL) {
            continue;
        }
        result |= Require(
            adapter_command->commandType == bridge_command->commandType,
            "scissor command type/order mismatch");
        if (bridge_command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
            result |= RequireBoundsEqual(
                adapter_command->boundingBox,
                bridge_command->boundingBox,
                "scissor start bounds mismatch");
        }
    }
    while (adapter != NULL && adapter_cursor < adapter->length) {
        const Clay_RenderCommand *candidate =
            &adapter->internalArray[adapter_cursor];
        adapter_cursor += 1;
        if (candidate->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START ||
                candidate->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
            adapter_seen += 1;
        }
    }
    result |= Require(
        bridge_seen > 0,
        "scissor sequence should be non-vacuous");
    result |= Require(
        adapter_seen == bridge_seen,
        "scissor command count mismatch");
    return result;
}

static int CompareCommandOrder(
    const EcsUiTreeSnapshot *tree,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter,
    const char *before_id,
    const char *after_id,
    const char *message)
{
    const EcsUiTreeNodeSnapshot *before = EcsUiTreeSnapshotFindNodeById(
        tree,
        before_id);
    const EcsUiTreeNodeSnapshot *after = EcsUiTreeSnapshotFindNodeById(
        tree,
        after_id);
    Clay_ElementId before_clay = {0};
    Clay_ElementId after_clay = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayElementId(before, NULL, &before_clay),
        "before command id missing");
    result |= Require(
        EcsUiPaintClayElementId(after, NULL, &after_clay),
        "after command id missing");
    const int32_t bridge_before = FindCommandIndex(
        bridge,
        before_clay.id,
        CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const int32_t bridge_after = FindCommandIndex(
        bridge,
        after_clay.id,
        CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const int32_t adapter_before = FindCommandIndex(
        adapter,
        before_clay.id,
        CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const int32_t adapter_after = FindCommandIndex(
        adapter,
        after_clay.id,
        CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    result |= Require(bridge_before >= 0, "bridge before command missing");
    result |= Require(bridge_after >= 0, "bridge after command missing");
    result |= Require(adapter_before >= 0, "adapter before command missing");
    result |= Require(adapter_after >= 0, "adapter after command missing");
    result |= Require(bridge_before < bridge_after, message);
    result |= Require(adapter_before < adapter_after, message);
    return result;
}

static int CompareCommandBeforeBorder(
    const EcsUiTreeSnapshot *tree,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter,
    const char *before_id,
    const char *border_owner_id,
    const char *message)
{
    const EcsUiTreeNodeSnapshot *before = EcsUiTreeSnapshotFindNodeById(
        tree,
        before_id);
    const EcsUiTreeNodeSnapshot *border_owner = EcsUiTreeSnapshotFindNodeById(
        tree,
        border_owner_id);
    Clay_ElementId before_clay = {0};
    Clay_ElementId border_clay = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayElementId(before, NULL, &before_clay),
        "border-order child command id missing");
    result |= Require(
        EcsUiPaintClayBorderCommandId(tree, border_owner, &border_clay),
        "border-order border command id missing");
    const int32_t bridge_before = FindCommandIndex(
        bridge,
        before_clay.id,
        CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const int32_t bridge_border = FindCommandIndex(
        bridge,
        border_clay.id,
        CLAY_RENDER_COMMAND_TYPE_BORDER);
    const int32_t adapter_before = FindCommandIndex(
        adapter,
        before_clay.id,
        CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const int32_t adapter_border = FindCommandIndex(
        adapter,
        border_clay.id,
        CLAY_RENDER_COMMAND_TYPE_BORDER);
    result |= Require(bridge_before >= 0, "bridge border-order child missing");
    result |= Require(bridge_border >= 0, "bridge border-order border missing");
    result |= Require(adapter_before >= 0, "adapter border-order child missing");
    result |= Require(adapter_border >= 0, "adapter border-order border missing");
    result |= Require(bridge_before < bridge_border, message);
    result |= Require(adapter_before < adapter_border, message);
    return result;
}

static int RequireRadiusEqual(
    Clay_CornerRadius actual,
    Clay_CornerRadius expected,
    const char *message)
{
    int result = 0;
    result |= RequireNear(actual.topLeft, expected.topLeft, 0.001f, message);
    result |= RequireNear(actual.topRight, expected.topRight, 0.001f, message);
    result |= RequireNear(actual.bottomLeft, expected.bottomLeft, 0.001f, message);
    result |= RequireNear(actual.bottomRight, expected.bottomRight, 0.001f, message);
    return result;
}

static int RequireBorderWidthEqual(
    Clay_BorderWidth actual,
    Clay_BorderWidth expected,
    const char *message)
{
    int result = 0;
    result |= Require(actual.left == expected.left, message);
    result |= Require(actual.top == expected.top, message);
    result |= Require(actual.right == expected.right, message);
    result |= Require(actual.bottom == expected.bottom, message);
    return result;
}

static const char *DiffBevelSuffix(uint16_t part)
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

static uint16_t DiffScaledU16(float value, float scale)
{
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    const float scaled = value * scale;
    if (scaled <= 0.0f) {
        return 0u;
    }
    if (scaled >= 65535.0f) {
        return UINT16_MAX;
    }
    return (uint16_t)scaled;
}

static bool DiffBorderHasPhysicalWidth(
    EcsUiPaintBorder border,
    float scale)
{
    return DiffScaledU16(border.left, scale) != 0u ||
        DiffScaledU16(border.top, scale) != 0u ||
        DiffScaledU16(border.right, scale) != 0u ||
        DiffScaledU16(border.bottom, scale) != 0u;
}

static const char *DiffBorderJoinScopeReason(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    if (tree == NULL || node == NULL) {
        return "missing snapshot node";
    }

    bool zstack_flow_child_seen = false;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        const bool text_field_value =
            node->kind == ECS_UI_NODE_PRESSABLE &&
            node->has_text_field_view &&
            node->text_field_view.value_node == child_node->entity &&
            child_node->kind == ECS_UI_NODE_TEXT;
        if (!text_field_value && child_node->visual.opacity <= 0.01f) {
            return "direct child is opacity-culled before Clay opens it";
        }
        if (node->kind == ECS_UI_NODE_ZSTACK) {
            if (child_node->has_placement) {
                return "direct child is emitted as a floating element";
            }
            if (zstack_flow_child_seen) {
                return "ZStack children after the first flow child are floating";
            }
            zstack_flow_child_seen = true;
        }
    }
    return NULL;
}

static int RequireBorderJoinScope(
    const EcsUiTreeSnapshot *tree,
    const EcsUiTreeNodeSnapshot *node)
{
    const char *reason = DiffBorderJoinScopeReason(tree, node);
    if (reason == NULL) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "DIFF-JOIN SCOPE unsupported border join for node '%s' (%llu): %s; "
        "generalize Clay children.length modeling before adding this case\n",
        node != NULL && node->id[0] != '\0' ? node->id : "Node",
        node != NULL ? (unsigned long long)node->entity : 0ull,
        reason);
    return 1;
}

static int CompareBoxItem(
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintItem *item,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter)
{
    if (tree == NULL || item == NULL) {
        return Require(false, "missing diff item inputs");
    }
    const EcsUiTreeNodeSnapshot *node =
        FindNodeByEntity(tree, item->key.source);
    Clay_ElementId element_id = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayElementId(node, NULL, &element_id),
        "diff item should have a computable Clay id");
    if (element_id.id == 0u) {
        return result;
    }

    const bool rectangle_expected =
        DiffPaintColor(item->payload.box.fill, item->opacity).a > 0.0f;
    const Clay_RenderCommand *adapter_rect =
        FindCommand(adapter, element_id.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const Clay_RenderCommand *bridge_rect =
        FindCommand(bridge, element_id.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    if (rectangle_expected) {
        result |= Require(adapter_rect != NULL, "adapter rectangle missing");
        result |= Require(bridge_rect != NULL, "bridge rectangle missing");
    } else {
        result |= Require(adapter_rect == NULL, "adapter unexpected rectangle");
        result |= Require(bridge_rect == NULL, "bridge unexpected rectangle");
    }
    if (rectangle_expected && adapter_rect != NULL && bridge_rect != NULL) {
        result |= RequireBoundsEqual(
            adapter_rect->boundingBox,
            bridge_rect->boundingBox,
            "rectangle bounds mismatch");
        result |= RequireColor(
            adapter_rect->renderData.rectangle.backgroundColor,
            bridge_rect->renderData.rectangle.backgroundColor,
            "rectangle color mismatch");
        result |= RequireRadiusEqual(
            adapter_rect->renderData.rectangle.cornerRadius,
            bridge_rect->renderData.rectangle.cornerRadius,
            "rectangle radius mismatch");
    }

    return result;
}

static int CompareBorderItem(
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintItem *item,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter)
{
    if (tree == NULL || item == NULL) {
        return Require(false, "missing border diff item inputs");
    }
    const EcsUiTreeNodeSnapshot *node =
        FindNodeByEntity(tree, item->key.source);
    int result = 0;
    const bool border_command_expected =
        item->payload.border.has_border &&
        DiffBorderHasPhysicalWidth(item->payload.border, tree->scale);
    if (border_command_expected) {
        const int scope_result = RequireBorderJoinScope(tree, node);
        if (scope_result != 0) {
            return result | scope_result;
        }
    }

    Clay_ElementId border_id = {0};
    if (!EcsUiPaintClayBorderCommandId(tree, node, &border_id)) {
        return result | Require(false, "border item should have a computable Clay id");
    }
    const Clay_RenderCommand *adapter_border =
        FindCommand(adapter, border_id.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
    const Clay_RenderCommand *bridge_border =
        FindCommand(bridge, border_id.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
    if (!border_command_expected) {
        result |= Require(adapter_border == NULL, "adapter unexpected border");
        result |= Require(bridge_border == NULL, "bridge unexpected border");
        return result;
    }

    result |= Require(adapter_border != NULL, "adapter border missing");
    result |= Require(bridge_border != NULL, "bridge border missing");
    if (adapter_border == NULL || bridge_border == NULL) {
        return result;
    }
    result |= RequireBoundsEqual(
        adapter_border->boundingBox,
        bridge_border->boundingBox,
        "border bounds mismatch");
    result |= RequireColor(
        adapter_border->renderData.border.color,
        bridge_border->renderData.border.color,
        "border color mismatch");
    result |= RequireRadiusEqual(
        adapter_border->renderData.border.cornerRadius,
        bridge_border->renderData.border.cornerRadius,
        "border radius mismatch");
    result |= RequireBorderWidthEqual(
        adapter_border->renderData.border.width,
        bridge_border->renderData.border.width,
        "border width mismatch");
    return result;
}

static int CompareBevelItem(
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintItem *item,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter)
{
    if (tree == NULL || item == NULL) {
        return Require(false, "missing bevel diff item inputs");
    }
    const EcsUiTreeNodeSnapshot *node =
        FindNodeByEntity(tree, item->key.source);
    Clay_ElementId bevel_id = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayElementId(node, DiffBevelSuffix(item->key.part), &bevel_id),
        "bevel item should have a computable Clay id");
    if (bevel_id.id == 0u) {
        return result;
    }

    const Clay_RenderCommand *adapter_rect =
        FindCommand(adapter, bevel_id.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const Clay_RenderCommand *bridge_rect =
        FindCommand(bridge, bevel_id.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    result |= Require(adapter_rect != NULL, "adapter bevel edge missing");
    result |= Require(bridge_rect != NULL, "bridge bevel edge missing");
    if (adapter_rect != NULL && bridge_rect != NULL) {
        result |= RequireBoundsEqual(
            adapter_rect->boundingBox,
            bridge_rect->boundingBox,
            "bevel edge bounds mismatch");
        result |= RequireColor(
            adapter_rect->renderData.rectangle.backgroundColor,
            bridge_rect->renderData.rectangle.backgroundColor,
            "bevel edge color mismatch");
        result |= RequireColor(
            adapter_rect->renderData.rectangle.backgroundColor,
            DiffPaintColor(item->payload.bevel_edge.color, item->opacity),
            "bevel edge paint color mismatch");
    }
    return result;
}

static int CompareCustomItem(
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintItem *item,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter)
{
    if (tree == NULL || item == NULL) {
        return Require(false, "missing custom diff item inputs");
    }
    const EcsUiTreeNodeSnapshot *node =
        FindNodeByEntity(tree, item->key.source);
    Clay_ElementId element_id = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayElementId(node, NULL, &element_id),
        "custom item should have a computable Clay id");
    if (element_id.id == 0u) {
        return result;
    }

    const Clay_RenderCommand *adapter_custom =
        FindCommand(adapter, element_id.id, CLAY_RENDER_COMMAND_TYPE_CUSTOM);
    const Clay_RenderCommand *bridge_custom =
        FindCommand(bridge, element_id.id, CLAY_RENDER_COMMAND_TYPE_CUSTOM);
    result |= Require(adapter_custom != NULL, "adapter custom command missing");
    result |= Require(bridge_custom != NULL, "bridge custom command missing");
    if (adapter_custom != NULL && bridge_custom != NULL) {
        result |= RequireBoundsEqual(
            adapter_custom->boundingBox,
            bridge_custom->boundingBox,
            "custom bounds mismatch");
        result |= RequireColor(
            adapter_custom->renderData.custom.backgroundColor,
            bridge_custom->renderData.custom.backgroundColor,
            "custom background color mismatch");
        result |= RequireColor(
            adapter_custom->renderData.custom.backgroundColor,
            DiffPaintColor(item->payload.custom.color, item->opacity),
            "custom paint color mismatch");
        result |= Require(
            adapter_custom->renderData.custom.customData ==
                bridge_custom->renderData.custom.customData,
            "custom data identity mismatch");
        result |= Require(
            adapter_custom->renderData.custom.customData == node,
            "custom data should point at source snapshot node");
    }
    return result;
}

static int CompareTextItem(
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintItem *item,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter,
    bool *bridge_claimed,
    bool *adapter_claimed)
{
    if (tree == NULL || item == NULL) {
        return Require(false, "missing text diff item inputs");
    }
    const EcsUiTreeNodeSnapshot *node =
        FindNodeByEntity(tree, item->key.source);
    Clay_ElementId element_id = {0};
    int result = 0;
    (void)EcsUiPaintClayElementId(node, NULL, &element_id);
    const uint32_t line_index = item->key.part & 0xffu;
    const Clay_ElementId text_id =
        Clay__HashNumber(line_index, element_id.id);
    const bool text_field_value =
        node != NULL &&
        node->parent_index != ECS_UI_TREE_INVALID_INDEX &&
        node->parent_index < tree->count &&
        tree->nodes[node->parent_index].kind == ECS_UI_NODE_PRESSABLE &&
        tree->nodes[node->parent_index].has_text_field_view &&
        tree->nodes[node->parent_index].text_field_view.value_node ==
            node->entity;

    const Clay_RenderCommand *adapter_text = text_field_value ?
        NULL :
        FindCommand(adapter, text_id.id, CLAY_RENDER_COMMAND_TYPE_TEXT);
    const Clay_RenderCommand *bridge_text = text_field_value ?
        NULL :
        FindCommand(bridge, text_id.id, CLAY_RENDER_COMMAND_TYPE_TEXT);
    int32_t adapter_index = -1;
    int32_t bridge_index = -1;
    if (adapter_text != NULL && adapter->internalArray != NULL) {
        adapter_index = (int32_t)(adapter_text - adapter->internalArray);
    }
    if (bridge_text != NULL && bridge->internalArray != NULL) {
        bridge_index = (int32_t)(bridge_text - bridge->internalArray);
    }
    if (adapter_text == NULL) {
        adapter_text =
            FindTextCommandByRun(
                adapter,
                &item->payload.text_run,
                adapter_claimed,
                &adapter_index);
    }
    if (bridge_text == NULL) {
        bridge_text =
            FindTextCommandByRun(
                bridge,
                &item->payload.text_run,
                bridge_claimed,
                &bridge_index);
    }
    if (adapter_text == NULL || bridge_text == NULL) {
        (void)fprintf(
            stderr,
            "text diff missing command node=%s range=%u..%u part=%u\n",
            node != NULL && node->id[0] != '\0' ? node->id : "Node",
            item->payload.text_run.byte_start,
            item->payload.text_run.byte_end,
            item->key.part);
    }
    result |= Require(adapter_text != NULL, "adapter text command missing");
    result |= Require(bridge_text != NULL, "bridge text command missing");
    if (adapter_text != NULL && bridge_text != NULL) {
        if (adapter_claimed != NULL && adapter_index >= 0) {
            adapter_claimed[adapter_index] = true;
        }
        if (bridge_claimed != NULL && bridge_index >= 0) {
            bridge_claimed[bridge_index] = true;
        }
        result |= RequireBoundsEqual(
            adapter_text->boundingBox,
            bridge_text->boundingBox,
            "text bounds mismatch");
        result |= RequireColor(
            adapter_text->renderData.text.textColor,
            bridge_text->renderData.text.textColor,
            "text color mismatch");
        result |= Require(
            adapter_text->renderData.text.fontSize ==
                bridge_text->renderData.text.fontSize,
            "text font size mismatch");
        result |= Require(
            adapter_text->renderData.text.fontId ==
                item->payload.text_run.font_id,
            "adapter text font id mismatch");
        result |= Require(
            adapter_text->renderData.text.letterSpacing ==
                item->payload.text_run.letter_spacing,
            "adapter text letter spacing mismatch");
        result |= Require(
            adapter_text->renderData.text.letterSpacing ==
                bridge_text->renderData.text.letterSpacing,
            "text letter spacing mismatch");
        result |= Require(
            adapter_text->renderData.text.stringContents.chars ==
                bridge_text->renderData.text.stringContents.chars &&
                adapter_text->renderData.text.stringContents.length ==
                    bridge_text->renderData.text.stringContents.length,
            "text slice mismatch");
    }
    return result;
}

static int CompareRoleBoxItem(
    const EcsUiTreeSnapshot *tree,
    const EcsUiPaintItem *item,
    const Clay_RenderCommandArray *bridge,
    const Clay_RenderCommandArray *adapter)
{
    if (tree == NULL || item == NULL) {
        return Require(false, "missing role-box diff item inputs");
    }
    const EcsUiTreeNodeSnapshot *node =
        FindNodeByEntity(tree, item->key.source);
    Clay_ElementId role_id = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayElementId(
            node,
            item->key.role == ECS_UI_PAINT_ROLE_CARET ?
                "_Caret" :
                "_Selection",
            &role_id),
        "role-box item should have a computable Clay id");
    if (role_id.id == 0u) {
        return result;
    }

    const Clay_RenderCommand *adapter_rect =
        FindCommand(adapter, role_id.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    const Clay_RenderCommand *bridge_rect =
        FindCommand(bridge, role_id.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    result |= Require(adapter_rect != NULL, "adapter role-box command missing");
    result |= Require(bridge_rect != NULL, "bridge role-box command missing");
    if (adapter_rect != NULL && bridge_rect != NULL) {
        result |= RequireBoundsEqual(
            adapter_rect->boundingBox,
            bridge_rect->boundingBox,
            "role-box bounds mismatch");
        result |= RequireColor(
            adapter_rect->renderData.rectangle.backgroundColor,
            bridge_rect->renderData.rectangle.backgroundColor,
            "role-box color mismatch");
    }
    return result;
}

static int RunDiffCase(float scale)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create diff world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "DiffRoot");
    result |= Require(root != 0, "diff root missing");
    result |= BuildDiffTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "diff tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 5.0f,
            .y = 7.0f,
            .width = 240.0f * scale,
            .height = 160.0f * scale,
        },
    };

    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width + options.physical_bounds.x,
        options.physical_bounds.height + options.physical_bounds.y);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    const EcsUiDrawList *draw_list =
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL);
    result |= Require(draw_list != NULL, "diff frame run failed");
    const Clay_RenderCommandArray *bridge =
        EcsUiFrameDrawListClayCommands(draw_list);
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    result |= Require(bridge != NULL, "diff bridge command list missing");
    result |= Require(paint != NULL, "diff paint list missing");

    static Clay_RenderCommand adapter_storage[256];
    Clay_RenderCommandArray adapter = {0};
    result |= Require(
        EcsUiPaintClayAdapterBuild(
            paint,
            &tree,
            &(EcsUiPaintClayAdapterOptions){
                .scale = scale,
                .physical_x = options.physical_bounds.x,
                .physical_y = options.physical_bounds.y,
            },
            adapter_storage,
            (int32_t)(sizeof(adapter_storage) / sizeof(adapter_storage[0])),
            &adapter),
        "paint clay adapter build failed");
    result |= CompareScissorSequence(bridge, &adapter);
    result |= CompareCommandOrder(
        &tree,
        bridge,
        &adapter,
        "DiffZFlow",
        "DiffZFloatA",
        "z-order flow should draw before first floater");
    result |= CompareCommandOrder(
        &tree,
        bridge,
        &adapter,
        "DiffZFloatA",
        "DiffZFloatB",
        "z-order floaters should preserve z ordinal order");
    result |= CompareCommandBeforeBorder(
        &tree,
        bridge,
        &adapter,
        "DiffInner",
        "DiffPanel",
        "border command should draw after child content");

    bool bridge_text_claimed[256] = {0};
    bool adapter_text_claimed[256] = {0};
    if (paint != NULL && bridge != NULL && adapter.internalArray != NULL) {
        for (uint32_t i = 0u; i < paint->count; i += 1u) {
            const EcsUiPaintItem *item = &paint->items[i];
            if (item->key.role == ECS_UI_PAINT_ROLE_BOX &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX) {
                result |= CompareBoxItem(&tree, item, bridge, &adapter);
                continue;
            }
            if (item->key.role == ECS_UI_PAINT_ROLE_BORDER &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_BORDER) {
                result |= CompareBorderItem(&tree, item, bridge, &adapter);
                continue;
            }
            if (item->key.role == ECS_UI_PAINT_ROLE_BEVEL_EDGE &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX) {
                result |= CompareBevelItem(&tree, item, bridge, &adapter);
                continue;
            }
            if (item->key.role == ECS_UI_PAINT_ROLE_TEXT_RUN &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_TEXT_RUN) {
                result |= CompareTextItem(
                    &tree,
                    item,
                    bridge,
                    &adapter,
                    bridge_text_claimed,
                    adapter_text_claimed);
                continue;
            }
            if ((item->key.role == ECS_UI_PAINT_ROLE_CARET ||
                    item->key.role == ECS_UI_PAINT_ROLE_SELECTION) &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX) {
                result |= CompareRoleBoxItem(&tree, item, bridge, &adapter);
                continue;
            }
            if ((item->key.role == ECS_UI_PAINT_ROLE_NINE_SLICE ||
                    item->key.role == ECS_UI_PAINT_ROLE_CUSTOM ||
                    item->key.role == ECS_UI_PAINT_ROLE_ICON) &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_CUSTOM) {
                result |= CompareCustomItem(&tree, item, bridge, &adapter);
                continue;
            }
        }
    }

    ecs_fini(world);
    return result;
}

static int RunFontIdAdapterCase(void)
{
    static const char text[] = "font";
    EcsUiTreeSnapshot tree = {
        .root = 1001u,
        .count = 1u,
        .scale = 1.0f,
    };
    tree.nodes[0] = (EcsUiTreeNodeSnapshot){
        .entity = 1002u,
        .kind = ECS_UI_NODE_TEXT,
        .has_layout = true,
    };
    (void)snprintf(tree.nodes[0].id, sizeof(tree.nodes[0].id), "%s", "FontNode");

    EcsUiPaintList paint = {
        .tree = tree.root,
        .generation = 17u,
        .count = 1u,
    };
    paint.items[0] = (EcsUiPaintItem){
        .key = {
            .source = tree.nodes[0].entity,
            .role = ECS_UI_PAINT_ROLE_TEXT_RUN,
            .part = 0u,
            .generation = paint.generation,
        },
        .primitive = ECS_UI_PAINT_PRIMITIVE_TEXT_RUN,
        .rect = {
            .x = 1.0f,
            .y = 2.0f,
            .width = 30.0f,
            .height = 12.0f,
        },
        .opacity = 1.0f,
        .payload = {
            .text_run = {
                .text = text,
                .byte_start = 0u,
                .byte_end = 4u,
                .font_id = 7u,
                .font_size = 18u,
                .color = {10.0f, 20.0f, 30.0f, 255.0f},
            },
        },
    };

    Clay_RenderCommand storage[4] = {0};
    Clay_RenderCommandArray commands = {0};
    int result = 0;
    result |= Require(
        EcsUiPaintClayAdapterBuild(
            &paint,
            &tree,
            &(EcsUiPaintClayAdapterOptions){.scale = 1.0f},
            storage,
            (int32_t)(sizeof(storage) / sizeof(storage[0])),
            &commands),
        "font-id adapter build failed");
    result |= Require(commands.length == 1, "font-id adapter command count mismatch");
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, 0);
    result |= Require(command != NULL, "font-id adapter command missing");
    if (command != NULL) {
        result |= Require(
            command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT,
            "font-id adapter command type mismatch");
        result |= Require(
            command->renderData.text.fontId == 7u,
            "font-id adapter did not preserve paint font id");
    }
    return result;
}

int main(void)
{
    int result = 0;
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 320.0f,
                .surface_height = 240.0f,
                .measure_text = TestMeasureText,
            }),
        "failed to initialize diff frame backend");
    EcsUiFrameBackendSetCullingEnabled(false);

    result |= RunDiffCase(1.0f);
    result |= RunDiffCase(2.0f);
    result |= RunFontIdAdapterCase();

    EcsUiFrameBackendShutdown();
    return result;
}
