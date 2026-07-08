#include "ecs_ui/ecs_ui_frame.h"
#include "../src/ecs_ui_frame_internal.h"
#include "../src/ecs_ui_paint_clay_adapter.h"

#include <stdint.h>
#include <stdio.h>

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
    (void)length;
    (void)spec;
    (void)user_data;
    return (EcsUiSize){.width = 0.0f, .height = 0.0f};
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
    if (node->kind == ECS_UI_NODE_TEXT) {
        return "bordered TEXT emits an inner CLAY_TEXT child";
    }
    if (node->kind == ECS_UI_NODE_PRESSABLE && node->has_text_field_view) {
        return "text-field pressable emits synthetic value/caret/selection children";
    }

    bool zstack_flow_child_seen = false;
    for (uint32_t child = node->first_child;
         child != ECS_UI_TREE_INVALID_INDEX && child < tree->count;
         child = tree->nodes[child].next_sibling) {
        const EcsUiTreeNodeSnapshot *child_node = &tree->nodes[child];
        if (child_node->visual.opacity <= 0.01f) {
            return "direct child is opacity-culled before Clay opens it";
        }
        if (child_node->has_placement) {
            return "direct child is emitted as a floating element";
        }
        if (node->kind == ECS_UI_NODE_ZSTACK) {
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

    const bool border_command_expected =
        item->payload.box.border.has_border &&
        DiffBorderHasPhysicalWidth(item->payload.box.border, tree->scale);
    if (border_command_expected) {
        const int scope_result = RequireBorderJoinScope(tree, node);
        if (scope_result != 0) {
            return result | scope_result;
        }
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

    const Clay_RenderCommand *adapter_border =
        NULL;
    const Clay_RenderCommand *bridge_border =
        NULL;
    if (border_command_expected) {
        Clay_ElementId border_id = {0};
        result |= Require(
            EcsUiPaintClayBorderCommandId(tree, node, &border_id),
            "diff item should have a computable Clay border command id");
        adapter_border =
            FindCommand(adapter, border_id.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
        bridge_border =
            FindCommand(bridge, border_id.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
        result |= Require(adapter_border != NULL, "adapter border missing");
        result |= Require(bridge_border != NULL, "bridge border missing");
        if (adapter_border != NULL && bridge_border != NULL) {
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
        }
    } else {
        if (item->payload.box.border.has_border) {
            Clay_ElementId border_id = {0};
            if (EcsUiPaintClayBorderCommandId(tree, node, &border_id)) {
                adapter_border =
                    FindCommand(adapter, border_id.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
                bridge_border =
                    FindCommand(bridge, border_id.id, CLAY_RENDER_COMMAND_TYPE_BORDER);
            }
        }
        result |= Require(adapter_border == NULL, "adapter unexpected border");
        result |= Require(bridge_border == NULL, "bridge unexpected border");
    }
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

    if (paint != NULL && bridge != NULL && adapter.internalArray != NULL) {
        for (uint32_t i = 0u; i < paint->count; i += 1u) {
            const EcsUiPaintItem *item = &paint->items[i];
            if (item->key.role == ECS_UI_PAINT_ROLE_BOX &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX) {
                result |= CompareBoxItem(&tree, item, bridge, &adapter);
                continue;
            }
            if (item->key.role == ECS_UI_PAINT_ROLE_BEVEL_EDGE &&
                    item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX) {
                result |= CompareBevelItem(&tree, item, bridge, &adapter);
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

    EcsUiFrameBackendShutdown();
    return result;
}
