#include "ecs_ui/ecs_ui_clay.h"
#include "ecs_ui/ecs_ui_frame.h"
#include "../src/ecs_ui_frame_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestFrameErrors {
    uint32_t count;
    EcsUiFrameErrorKind last_kind;
    char last_message[256];
} TestFrameErrors;

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
            "%s: actual=%f expected=%f\n",
            message,
            actual,
            expected);
        return 1;
    }
    return 0;
}

static void CopyString(char *out, size_t out_size, const char *value)
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

static void TestFrameHandleError(
    EcsUiFrameErrorKind kind,
    const char *message,
    void *user_data)
{
    TestFrameErrors *errors = user_data;
    if (errors == NULL) {
        return;
    }

    errors->count += 1u;
    errors->last_kind = kind;
    CopyString(errors->last_message, sizeof(errors->last_message), message);
}

static EcsUiSize TestMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    (void)utf8;
    (void)user_data;

    const int32_t safe_length = length > 0 ? length : 0;
    const float font_size =
        spec != NULL && spec->font_size > 0.0f ? spec->font_size : 16.0f;
    return (EcsUiSize){
        .width = (float)safe_length * font_size * 0.5f,
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

static int BuildParityTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(
        EcsUiSetScale(world, root, scale),
        "failed to set frame containment scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FrameContainer",
            .gap = 5.0f,
            .padding = 7.0f,
            .preferred_width = 190.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FrameCanvas",
            .kind = "frame.canvas",
            .preferred_width = 64.0f,
            .preferred_height = 32.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "FrameText",
            .text = "contain",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "FrameScroll",
                .preferred_width = 96.0f,
                .preferred_height = 42.0f,
            },
        });
    for (int i = 0; i < 4; i += 1) {
        char id[ECS_UI_ID_MAX] = {0};
        char text[ECS_UI_TEXT_MAX] = {0};
        (void)snprintf(id, sizeof(id), "FrameRow%d", i);
        (void)snprintf(text, sizeof(text), "row %d", i);
        (void)EcsUiAddText(
            &builder,
            (EcsUiTextDesc){
                .id = id,
                .text = text,
                .role = ECS_UI_TEXT_BODY,
            });
    }
    EcsUiEnd(&builder);
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "FrameButton",
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "FrameButtonLabel",
            .text = "ok",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "frame parity builder failed");
    return result;
}

static EcsUiFrameLayoutOptions FrameOptions(float scale)
{
    return (EcsUiFrameLayoutOptions){
        .physical_bounds = {
            .x = 17.0f,
            .y = 23.0f,
            .width = 260.0f * scale,
            .height = 220.0f * scale,
        },
    };
}

static EcsUiClayLayoutOptions ClayOptions(
    const EcsUiFrameLayoutOptions *options)
{
    return (EcsUiClayLayoutOptions){
        .bounds = {
            .x = options->physical_bounds.x,
            .y = options->physical_bounds.y,
            .width = options->physical_bounds.width,
            .height = options->physical_bounds.height,
        },
    };
}

static Clay_FloatingAttachPointType TestClayAttachPoint(
    EcsUiAlign x,
    EcsUiAlign y)
{
    static const Clay_FloatingAttachPointType attach_points[3][3] = {
        {
            CLAY_ATTACH_POINT_LEFT_TOP,
            CLAY_ATTACH_POINT_LEFT_CENTER,
            CLAY_ATTACH_POINT_LEFT_BOTTOM,
        },
        {
            CLAY_ATTACH_POINT_CENTER_TOP,
            CLAY_ATTACH_POINT_CENTER_CENTER,
            CLAY_ATTACH_POINT_CENTER_BOTTOM,
        },
        {
            CLAY_ATTACH_POINT_RIGHT_TOP,
            CLAY_ATTACH_POINT_RIGHT_CENTER,
            CLAY_ATTACH_POINT_RIGHT_BOTTOM,
        },
    };
    const uint32_t ix =
        x == ECS_UI_ALIGN_CENTER ? 1u : (x == ECS_UI_ALIGN_END ? 2u : 0u);
    const uint32_t iy =
        y == ECS_UI_ALIGN_CENTER ? 1u : (y == ECS_UI_ALIGN_END ? 2u : 0u);
    return attach_points[ix][iy];
}

static EcsUiClayLayoutOptions ClayOptionsWithAttach(
    const EcsUiFrameLayoutOptions *options)
{
    EcsUiClayLayoutOptions out = ClayOptions(options);
    out.attach_points = (Clay_FloatingAttachPoints){
        .element = TestClayAttachPoint(
            options->attach_points.element_x,
            options->attach_points.element_y),
        .parent = TestClayAttachPoint(
            options->attach_points.parent_x,
            options->attach_points.parent_y),
    };
    out.z_index = options->z_index;
    out.capture_pointer = options->capture_pointer;
    return out;
}

static const EcsUiTreeNodeSnapshot *FindTreeNode(
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

static Clay_String TestClayString(const char *text)
{
    return (Clay_String){
        .isStaticallyAllocated = false,
        .length = text != NULL ? (int32_t)strlen(text) : 0,
        .chars = text != NULL ? text : "",
    };
}

static Clay_ElementId TestClayNodeElementId(
    const EcsUiTreeNodeSnapshot *node)
{
    char id[ECS_UI_ID_MAX * 2u] = {0};
    const char *authored_id =
        node != NULL && node->id[0] != '\0' ? node->id : "Node";
    (void)snprintf(
        id,
        sizeof(id),
        "%s_%llu",
        authored_id,
        (unsigned long long)(node != NULL ? node->entity : 0));
    return Clay_GetElementId(TestClayString(id));
}

static Clay_RenderCommand *FindCustomCommand(
    Clay_RenderCommandArray *commands,
    const char *node_id)
{
    if (commands == NULL || node_id == NULL) {
        return NULL;
    }
    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) {
            continue;
        }
        const EcsUiTreeNodeSnapshot *node =
            command->renderData.custom.customData;
        if (node != NULL && strcmp(node->id, node_id) == 0) {
            return command;
        }
    }
    return NULL;
}

static Clay_RenderCommandArray RunRawFrame(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiClayLayoutOptions *options,
    float surface_width,
    float surface_height,
    EcsUiClayInteractionFrame *frame)
{
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = surface_width,
        .height = surface_height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(tree, theme, options, frame);
    Clay_RenderCommandArray commands = Clay_EndLayout();
    (void)EcsUiClayEnrichSnapshotLayout(tree, options);
    return commands;
}

static int RequireEventListsMatch(
    const EcsUiEventList *raw,
    const EcsUiEventList *neutral,
    const char *message)
{
    int result = 0;
    result |= Require(raw != NULL, "missing raw events");
    result |= Require(neutral != NULL, "missing neutral events");
    if (raw == NULL || neutral == NULL) {
        return result;
    }
    if (raw->count != neutral->count || raw->truncated != neutral->truncated) {
        (void)fprintf(
            stderr,
            "%s: count raw=%u neutral=%u truncated raw=%d neutral=%d\n",
            message,
            (unsigned int)raw->count,
            (unsigned int)neutral->count,
            raw->truncated,
            neutral->truncated);
        return 1;
    }

    for (uint32_t i = 0u; i < raw->count; i += 1u) {
        const EcsUiEvent *raw_event = &raw->events[i];
        const EcsUiEvent *neutral_event = &neutral->events[i];
        if (raw_event->type != neutral_event->type ||
            raw_event->tree != neutral_event->tree ||
            raw_event->node != neutral_event->node ||
            raw_event->action != neutral_event->action ||
            raw_event->payload != neutral_event->payload ||
            raw_event->button != neutral_event->button ||
            strcmp(raw_event->node_id, neutral_event->node_id) != 0) {
            (void)fprintf(
                stderr,
                "%s: event identity mismatch at %u\n",
                message,
                (unsigned int)i);
            result |= 1;
            continue;
        }
        result |= RequireNear(
            neutral_event->x,
            raw_event->x,
            0.001f,
            "event x mismatch");
        result |= RequireNear(
            neutral_event->y,
            raw_event->y,
            0.001f,
            "event y mismatch");
        result |= RequireNear(
            neutral_event->start_x,
            raw_event->start_x,
            0.001f,
            "event start x mismatch");
        result |= RequireNear(
            neutral_event->start_y,
            raw_event->start_y,
            0.001f,
            "event start y mismatch");
        result |= RequireNear(
            neutral_event->delta_x,
            raw_event->delta_x,
            0.001f,
            "event delta x mismatch");
        result |= RequireNear(
            neutral_event->delta_y,
            raw_event->delta_y,
            0.001f,
            "event delta y mismatch");
    }
    return result;
}

static int CompareLayouts(
    const EcsUiTreeSnapshot *raw,
    const EcsUiTreeSnapshot *neutral,
    float scale)
{
    int result = 0;
    result |= Require(raw != NULL, "raw tree missing");
    result |= Require(neutral != NULL, "neutral tree missing");
    if (raw == NULL || neutral == NULL) {
        return result;
    }

    result |= Require(raw->count == neutral->count, "snapshot counts differ");
    const uint32_t count =
        raw->count < neutral->count ? raw->count : neutral->count;
    for (uint32_t i = 0u; i < count; i += 1u) {
        const EcsUiTreeNodeSnapshot *raw_node = &raw->nodes[i];
        const EcsUiTreeNodeSnapshot *neutral_node = &neutral->nodes[i];
        if (raw_node->entity != neutral_node->entity ||
            strcmp(raw_node->id, neutral_node->id) != 0) {
            (void)fprintf(
                stderr,
                "snapshot node mismatch at %u: raw=%s neutral=%s\n",
                (unsigned int)i,
                raw_node->id,
                neutral_node->id);
            result |= 1;
            continue;
        }

        char message[128] = {0};
        (void)snprintf(
            message,
            sizeof(message),
            "layout presence mismatch for %s at scale %.1f",
            raw_node->id,
            scale);
        result |= Require(raw_node->has_layout == neutral_node->has_layout, message);
        if (!raw_node->has_layout || !neutral_node->has_layout) {
            continue;
        }

        (void)snprintf(
            message,
            sizeof(message),
            "layout x mismatch for %s at scale %.1f",
            raw_node->id,
            scale);
        result |= RequireNear(
            neutral_node->layout_x,
            raw_node->layout_x,
            0.001f,
            message);
        (void)snprintf(
            message,
            sizeof(message),
            "layout y mismatch for %s at scale %.1f",
            raw_node->id,
            scale);
        result |= RequireNear(
            neutral_node->layout_y,
            raw_node->layout_y,
            0.001f,
            message);
        (void)snprintf(
            message,
            sizeof(message),
            "layout width mismatch for %s at scale %.1f",
            raw_node->id,
            scale);
        result |= RequireNear(
            neutral_node->layout_width,
            raw_node->layout_width,
            0.001f,
            message);
        (void)snprintf(
            message,
            sizeof(message),
            "layout height mismatch for %s at scale %.1f",
            raw_node->id,
            scale);
        result |= RequireNear(
            neutral_node->layout_height,
            raw_node->layout_height,
            0.001f,
            message);
    }
    return result;
}

static int RunContainmentCase(float scale)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create frame containment world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "FrameContainmentRoot");
    result |= BuildParityTree(world, root, scale);

    EcsUiTreeSnapshot raw_tree = {0};
    EcsUiTreeSnapshot neutral_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &raw_tree),
        "failed to read raw parity snapshot");
    result |= Require(
        EcsUiReadTree(world, root, &neutral_tree),
        "failed to read neutral parity snapshot");

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions frame_options = FrameOptions(scale);
    EcsUiClayLayoutOptions clay_options = ClayOptions(&frame_options);
    const float surface_width =
        frame_options.physical_bounds.x +
        frame_options.physical_bounds.width +
        40.0f;
    const float surface_height =
        frame_options.physical_bounds.y +
        frame_options.physical_bounds.height +
        40.0f;

    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = surface_width,
        .height = surface_height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&raw_tree, &theme, &clay_options, NULL);
    (void)Clay_EndLayout();
    (void)EcsUiClayEnrichSnapshotLayout(&raw_tree, &clay_options);

    EcsUiFrameBackendSetSurfaceSize(surface_width, surface_height);
    const EcsUiDrawList *draw_list = EcsUiFrameRun(
        &neutral_tree,
        &theme,
        &frame_options,
        NULL,
        NULL);
    result |= Require(draw_list != NULL, "neutral frame run returned no draw list");
    result |= CompareLayouts(&raw_tree, &neutral_tree, scale);

    ecs_fini(world);
    return result;
}

static int BuildAttachPointTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(
        EcsUiSetScale(world, root, scale),
        "failed to set attach-point scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t child = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "AttachPointChild",
            .kind = "attach.child",
            .preferred_width = 20.0f,
            .preferred_height = 12.0f,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "attach-point builder failed");
    ecs_set(
        world,
        child,
        EcsUiPlacement,
        {
            .mode = ECS_UI_PLACEMENT_POINT,
            .point_x = 90.0f,
            .point_y = 70.0f,
            .width = 20.0f,
            .height = 12.0f,
        });
    return result;
}

static int TestAttachPointMapping(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create attach-point world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "AttachPointRoot");
    result |= BuildAttachPointTree(world, root, 1.0f);
    EcsUiTheme theme = EcsUiThemeDefault();
    const EcsUiAlign aligns[3] = {
        ECS_UI_ALIGN_START,
        ECS_UI_ALIGN_CENTER,
        ECS_UI_ALIGN_END,
    };
    for (uint32_t ex = 0u; ex < 3u; ex += 1u) {
        for (uint32_t ey = 0u; ey < 3u; ey += 1u) {
            for (uint32_t px = 0u; px < 3u; px += 1u) {
                for (uint32_t py = 0u; py < 3u; py += 1u) {
                    EcsUiTreeSnapshot raw_tree = {0};
                    EcsUiTreeSnapshot neutral_tree = {0};
                    result |= Require(
                        EcsUiReadTree(world, root, &raw_tree),
                        "failed to read raw attach tree");
                    result |= Require(
                        EcsUiReadTree(world, root, &neutral_tree),
                        "failed to read neutral attach tree");

                    EcsUiFrameLayoutOptions frame_options = {
                        .physical_bounds = {
                            .x = 80.0f,
                            .y = 60.0f,
                            .width = 200.0f,
                            .height = 140.0f,
                        },
                        .attach_points = {
                            .element_x = aligns[ex],
                            .element_y = aligns[ey],
                            .parent_x = aligns[px],
                            .parent_y = aligns[py],
                        },
                    };
                    EcsUiClayLayoutOptions clay_options =
                        ClayOptionsWithAttach(&frame_options);
                    (void)RunRawFrame(
                        &raw_tree,
                        &theme,
                        &clay_options,
                        420.0f,
                        320.0f,
                        NULL);
                    EcsUiFrameBackendSetSurfaceSize(420.0f, 320.0f);
                    result |= Require(
                        EcsUiFrameRun(
                            &neutral_tree,
                            &theme,
                            &frame_options,
                            NULL,
                            NULL) != NULL,
                        "attach neutral frame run failed");

                    const EcsUiTreeNodeSnapshot *raw_child =
                        FindTreeNode(&raw_tree, "AttachPointChild");
                    const EcsUiTreeNodeSnapshot *neutral_child =
                        FindTreeNode(&neutral_tree, "AttachPointChild");
                    result |= Require(
                        raw_child != NULL && raw_child->has_layout,
                        "raw attach child missing layout");
                    result |= Require(
                        neutral_child != NULL && neutral_child->has_layout,
                        "neutral attach child missing layout");
                    if (raw_child == NULL || neutral_child == NULL ||
                        !raw_child->has_layout || !neutral_child->has_layout) {
                        continue;
                    }

                    result |= RequireNear(
                        neutral_child->layout_x,
                        raw_child->layout_x,
                        0.001f,
                        "attach child x mismatch");
                    result |= RequireNear(
                        neutral_child->layout_y,
                        raw_child->layout_y,
                        0.001f,
                        "attach child y mismatch");
                    result |= RequireNear(
                        neutral_child->layout_width,
                        raw_child->layout_width,
                        0.001f,
                        "attach child width mismatch");
                    result |= RequireNear(
                        neutral_child->layout_height,
                        raw_child->layout_height,
                        0.001f,
                        "attach child height mismatch");
                }
            }
        }
    }
    ecs_fini(world);
    return result;
}

static int BuildZIndexTree(ecs_world_t *world, ecs_entity_t root)
{
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ZIndexCustom",
            .kind = "z.custom",
            .preferred_width = 64.0f,
            .preferred_height = 24.0f,
        });
    EcsUiBuilderEnd(&builder);
    int result = Require(EcsUiBuilderOk(&builder), "z-index builder failed");
    ecs_set(
        world,
        target,
        EcsUiPlacement,
        {
            .parent_x = ECS_UI_ALIGN_START,
            .parent_y = ECS_UI_ALIGN_START,
            .child_x = ECS_UI_ALIGN_START,
            .child_y = ECS_UI_ALIGN_START,
            .offset_x = 8.0f,
            .offset_y = 6.0f,
            .width = 64.0f,
            .height = 24.0f,
        });
    return result;
}

static int TestZIndexTranslation(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create z-index world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "ZIndexRoot");
    result |= BuildZIndexTree(world, root);
    EcsUiTreeSnapshot raw_tree = {0};
    EcsUiTreeSnapshot neutral_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &raw_tree),
        "failed to read raw z-index tree");
    result |= Require(
        EcsUiReadTree(world, root, &neutral_tree),
        "failed to read neutral z-index tree");

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions frame_options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 180.0f,
            .height = 120.0f,
        },
        .z_index = 37,
    };
    EcsUiClayLayoutOptions converted_options =
        EcsUiFrameInternalClayLayoutOptions(&frame_options);
    result |= Require(
        converted_options.z_index == frame_options.z_index,
        "z-index adapter should copy z_index");
    EcsUiClayLayoutOptions clay_options = ClayOptionsWithAttach(&frame_options);
    Clay_RenderCommandArray raw_commands = RunRawFrame(
        &raw_tree,
        &theme,
        &clay_options,
        240.0f,
        180.0f,
        NULL);
    EcsUiFrameBackendSetSurfaceSize(240.0f, 180.0f);
    const EcsUiDrawList *draw_list = EcsUiFrameRun(
        &neutral_tree,
        &theme,
        &frame_options,
        NULL,
        NULL);
    result |= Require(draw_list != NULL, "z-index frame run failed");
    const Clay_RenderCommandArray *neutral_commands =
        EcsUiFrameDrawListClayCommands(draw_list);
    Clay_RenderCommandArray neutral_commands_copy =
        neutral_commands != NULL ? *neutral_commands : (Clay_RenderCommandArray){0};
    Clay_RenderCommand *raw_custom =
        FindCustomCommand(&raw_commands, "ZIndexCustom");
    Clay_RenderCommand *neutral_custom =
        FindCustomCommand(&neutral_commands_copy, "ZIndexCustom");
    result |= Require(raw_custom != NULL, "raw z-index custom missing");
    result |= Require(neutral_custom != NULL, "neutral z-index custom missing");
    if (raw_custom != NULL && neutral_custom != NULL) {
        result |= Require(
            neutral_custom->zIndex == raw_custom->zIndex,
            "neutral z-index should match raw clay z-index");
    }

    ecs_fini(world);
    return result;
}

static int BuildPressTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale,
    ecs_entity_t *out_target)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set press scale");
    ecs_entity_t action = ecs_entity(world, {.name = "FramePressAction", .sep = ""});
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FramePressTarget",
            .kind = "press.target",
            .preferred_width = 160.0f,
            .preferred_height = 80.0f,
            .on_click = action,
            .payload = 42u,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "press builder failed");
    if (out_target != NULL) {
        *out_target = target;
    }
    return result;
}

static EcsUiClayPointerState ClayPointer(EcsUiPointerState pointer)
{
    return (EcsUiClayPointerState){
        .x = pointer.x,
        .y = pointer.y,
        .time = pointer.time,
        .down = pointer.down,
        .pressed = pointer.pressed,
        .released = pointer.released,
        .secondary_down = pointer.secondary_down,
        .secondary_pressed = pointer.secondary_pressed,
        .secondary_released = pointer.secondary_released,
        .middle_down = pointer.middle_down,
        .middle_pressed = pointer.middle_pressed,
        .middle_released = pointer.middle_released,
        .scroll_x = pointer.scroll_x,
        .scroll_y = pointer.scroll_y,
    };
}

static void RunRawPointerFrame(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiClayLayoutOptions *options,
    EcsUiClayInteractionState *state,
    EcsUiPointerState pointer,
    EcsUiEventList *events,
    EcsUiClayInteractionFrame *out_frame)
{
    EcsUiClayInteractionFrame frame = {0};
    EcsUiClayInteractionFrameBegin(&frame, state);
    Clay_SetLayoutDimensions((Clay_Dimensions){.width = 240.0f, .height = 160.0f});
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(tree, theme, options, &frame);
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){.x = pointer.x, .y = pointer.y}, pointer.down);
    EcsUiClayCollectFrameEvents(&frame, ClayPointer(pointer), events);
    if (out_frame != NULL) {
        *out_frame = frame;
    }
}

static void RunNeutralPointerFrame(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options,
    EcsUiInteractionState *state,
    EcsUiPointerState pointer,
    EcsUiEventList *events,
    EcsUiInteractionFrame *out_frame)
{
    EcsUiInteractionFrame frame = {
        .state = state,
    };
    EcsUiFrameBackendSetSurfaceSize(240.0f, 160.0f);
    (void)EcsUiFrameRun(tree, theme, options, &pointer, &frame);
    EcsUiFrameCollectEvents(&frame, pointer, events);
    if (out_frame != NULL) {
        *out_frame = frame;
    }
}

static int TestPointerCaptureRoundTrip(float scale)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create pointer capture world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "FramePointerRoot");
    ecs_entity_t target = 0;
    result |= BuildPressTree(world, root, scale, &target);
    EcsUiTreeSnapshot raw_tree = {0};
    EcsUiTreeSnapshot neutral_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &raw_tree),
        "failed to read raw pointer tree");
    result |= Require(
        EcsUiReadTree(world, root, &neutral_tree),
        "failed to read neutral pointer tree");

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions frame_options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 220.0f,
            .height = 140.0f,
        },
    };
    EcsUiClayLayoutOptions clay_options = ClayOptionsWithAttach(&frame_options);
    EcsUiPointerState pointers[3] = {
        {
            .x = 40.0f,
            .y = 36.0f,
            .time = 10.0,
            .down = true,
            .pressed = true,
        },
        {
            .x = 72.0f,
            .y = 68.0f,
            .time = 10.5,
            .down = true,
        },
        {
            .x = 72.0f,
            .y = 68.0f,
            .time = 11.0,
            .released = true,
        },
    };

    EcsUiClayInteractionState raw_state = {0};
    EcsUiInteractionState neutral_state = {0};
    EcsUiClayInteractionStateInit(&raw_state);
    EcsUiFrameInteractionStateInit(&neutral_state);
    EcsUiClayInteractionFrame raw_frame = {0};
    EcsUiInteractionFrame neutral_frame = {0};
    EcsUiEventList raw_events = {0};
    EcsUiEventList neutral_events = {0};

    RunRawPointerFrame(
        &raw_tree,
        &theme,
        &clay_options,
        &raw_state,
        pointers[0],
        &raw_events,
        &raw_frame);
    RunNeutralPointerFrame(
        &neutral_tree,
        &theme,
        &frame_options,
        &neutral_state,
        pointers[0],
        &neutral_events,
        &neutral_frame);
    result |= RequireEventListsMatch(&raw_events, &neutral_events, "press events");
    result |= Require(raw_state.capture.active, "raw capture should start on press");
    result |= Require(
        neutral_state.capture.active,
        "neutral capture should start on press");
    result |= Require(
        strcmp(neutral_state.capture.node_id, "FramePressTarget") == 0,
        "neutral capture node id mismatch after press");
    result |= Require(
        neutral_state.capture.node == target,
        "neutral capture node mismatch after press");

    RunRawPointerFrame(
        &raw_tree,
        &theme,
        &clay_options,
        &raw_state,
        pointers[1],
        &raw_events,
        &raw_frame);
    RunNeutralPointerFrame(
        &neutral_tree,
        &theme,
        &frame_options,
        &neutral_state,
        pointers[1],
        &neutral_events,
        &neutral_frame);
    result |= RequireEventListsMatch(&raw_events, &neutral_events, "drag events");
    result |= Require(raw_state.capture.active, "raw capture should persist on drag");
    result |= Require(
        neutral_state.capture.active,
        "neutral capture should persist on drag");

    RunRawPointerFrame(
        &raw_tree,
        &theme,
        &clay_options,
        &raw_state,
        pointers[2],
        &raw_events,
        &raw_frame);
    RunNeutralPointerFrame(
        &neutral_tree,
        &theme,
        &frame_options,
        &neutral_state,
        pointers[2],
        &neutral_events,
        &neutral_frame);
    result |= RequireEventListsMatch(&raw_events, &neutral_events, "release events");
    result |= Require(!raw_state.capture.active, "raw capture should end on release");
    result |= Require(
        !neutral_state.capture.active,
        "neutral capture should end on release");
    result |= Require(
        neutral_events.count > 0u &&
            strcmp(neutral_events.events[0].node_id, "FramePressTarget") == 0,
        "neutral release event node id mismatch");
    if (neutral_events.count > 0u) {
        result |= RequireNear(
            neutral_events.events[0].x,
            pointers[2].x / scale,
            0.001f,
            "neutral release x should be logical");
        result |= RequireNear(
            neutral_events.events[0].start_x,
            pointers[0].x / scale,
            0.001f,
            "neutral release start x should be logical");
    }

    ecs_fini(world);
    return result;
}

static int TestCapturePointerTranslation(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create capture-pointer world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "FrameCapturePointerRoot");
    ecs_entity_t target = 0;
    result |= BuildPressTree(world, root, 1.0f, &target);
    EcsUiTreeSnapshot passthrough_tree = {0};
    EcsUiTreeSnapshot capture_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &passthrough_tree),
        "failed to read passthrough tree");
    result |= Require(
        EcsUiReadTree(world, root, &capture_tree),
        "failed to read capture tree");

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiPointerState pointer = {
        .x = 20.0f,
        .y = 20.0f,
        .time = 20.0,
        .down = true,
        .pressed = true,
    };
    EcsUiEventList passthrough_events = {0};
    EcsUiEventList capture_events = {0};
    EcsUiInteractionState passthrough_state = {0};
    EcsUiInteractionState capture_state = {0};
    EcsUiFrameInteractionStateInit(&passthrough_state);
    EcsUiFrameInteractionStateInit(&capture_state);
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 220.0f,
            .height = 140.0f,
        },
    };
    RunNeutralPointerFrame(
        &passthrough_tree,
        &theme,
        &options,
        &passthrough_state,
        pointer,
        &passthrough_events,
        NULL);
    options.capture_pointer = true;
    EcsUiClayLayoutOptions converted_options =
        EcsUiFrameInternalClayLayoutOptions(&options);
    result |= Require(
        converted_options.capture_pointer,
        "capture-pointer adapter should copy capture_pointer");
    RunNeutralPointerFrame(
        &capture_tree,
        &theme,
        &options,
        &capture_state,
        pointer,
        &capture_events,
        NULL);
    result |= Require(
        passthrough_events.count == capture_events.count,
        "single-tree capture_pointer should preserve descendant event count");
    result |= Require(
        capture_events.count > 0u &&
            capture_events.events[1u].node == target,
        "capture_pointer should still route to descendants in its own tree");

    ecs_fini(world);
    return result;
}

static int BuildScrollStateTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale,
    ecs_entity_t *out_scroll)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set scroll scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FrameScrollFit",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .padding = 1.25f,
        });
    ecs_entity_t scroll = EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "FrameScrollState",
                .preferred_width = 96.5f,
                .preferred_height = 42.5f,
                .gap = 2.5f,
                .padding = 1.25f,
            },
        });
    for (int i = 0; i < 6; i += 1) {
        char id[ECS_UI_ID_MAX] = {0};
        (void)snprintf(id, sizeof(id), "FrameScrollStateRow%d", i);
        (void)EcsUiAddCustom(
            &builder,
            (EcsUiCustomDesc){
                .id = id,
                .kind = "frame.scroll",
                .preferred_width = 88.5f,
                .preferred_height = 30.5f,
            });
    }
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "frame scroll-state builder failed");
    if (out_scroll != NULL) {
        *out_scroll = scroll;
    }
    return result;
}

static int RunScrollStateFrame(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options,
    EcsUiInteractionState *state,
    EcsUiPointerState pointer,
    EcsUiInteractionFrame *out_frame)
{
    EcsUiInteractionFrame frame = {
        .state = state,
    };
    EcsUiFrameBackendSetSurfaceSize(
        options->physical_bounds.width,
        options->physical_bounds.height);
    const EcsUiDrawList *draw_list =
        EcsUiFrameRun(tree, theme, options, &pointer, &frame);
    int result = Require(draw_list != NULL, "scroll-state frame run failed");
    EcsUiEventList events = {0};
    EcsUiFrameCollectEvents(&frame, pointer, &events);
    if (out_frame != NULL) {
        *out_frame = frame;
    }
    return result;
}

static int RunScrollLayoutFrame(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options)
{
    EcsUiFrameBackendSetSurfaceSize(
        options->physical_bounds.width,
        options->physical_bounds.height);
    const EcsUiDrawList *draw_list =
        EcsUiFrameRun(tree, theme, options, NULL, NULL);
    return Require(draw_list != NULL, "scroll layout frame run failed");
}

static float ExpectedScrollClamp(float content, float viewport)
{
    float max_scroll = content - viewport;
    if (max_scroll < 0.0f) {
        max_scroll = 0.0f;
    }
    return -max_scroll;
}

static int TestScrollStateOwnership(float scale)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create scroll-state world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "FrameScrollStateRoot");
    ecs_entity_t scroll = 0;
    result |= BuildScrollStateTree(world, root, scale, &scroll);
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f * scale,
            .height = 160.0f * scale,
        },
    };

    ecs_set(world, scroll, EcsUiScrollState, {.offset_y = -8.25f});
    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read scroll sync tree");
    EcsUiInteractionState state = {0};
    EcsUiFrameInteractionStateInit(&state);
    EcsUiInteractionFrame frame = {0};
    result |= RunScrollStateFrame(
        &tree,
        &theme,
        &options,
        &state,
        (EcsUiPointerState){0},
        &frame);
    const EcsUiTreeNodeSnapshot *scroll_node =
        EcsUiTreeSnapshotFindNodeById(&tree, "FrameScrollState");
    result |= Require(scroll_node != NULL, "scroll sync node missing");
    if (scroll_node != NULL) {
        Clay_ScrollContainerData data =
            Clay_GetScrollContainerData(TestClayNodeElementId(scroll_node));
        result |= Require(data.found, "scroll sync clay data missing");
        if (data.found && data.scrollPosition != NULL) {
            result |= RequireNear(
                data.scrollPosition->y,
                -8.25f * scale,
                0.001f,
                "clay retained scroll should follow component");
        }
    }

    ecs_set(world, scroll, EcsUiScrollState, {0});
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read wheel timing tree");
    EcsUiFrameInteractionStateInit(&state);
    EcsUiPointerState wheel = {
        .x = 10.0f * scale,
        .y = 10.0f * scale,
        .scroll_y = -1.0f,
    };
    result |= RunScrollStateFrame(&tree, &theme, &options, &state, wheel, &frame);
    result |= Require(frame.scroll_consumed, "wheel should queue scroll update");
    result |= Require(
        frame.pending_scroll_count == 1u,
        "wheel should produce one pending scroll update");
    if (frame.pending_scroll_count > 0u) {
        result |= RequireNear(
            frame.pending_scrolls[0].offset_y,
            -10.0f / scale,
            0.001f,
            "pending scroll offset should be logical");
    }
    const EcsUiScrollState *before_apply =
        ecs_get(world, scroll, EcsUiScrollState);
    result |= RequireNear(
        before_apply != NULL ? before_apply->offset_y : 99.0f,
        0.0f,
        0.001f,
        "wheel should not mutate component before apply");
    result |= Require(EcsUiFrameApply(world, &frame), "scroll apply failed");
    const EcsUiScrollState *after_apply =
        ecs_get(world, scroll, EcsUiScrollState);
    result |= RequireNear(
        after_apply != NULL ? after_apply->offset_y : 0.0f,
        -10.0f / scale,
        0.001f,
        "apply should commit pending scroll");

    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read next-frame scroll tree");
    result |= RunScrollStateFrame(
        &tree,
        &theme,
        &options,
        &state,
        (EcsUiPointerState){0},
        &frame);
    scroll_node = EcsUiTreeSnapshotFindNodeById(&tree, "FrameScrollState");
    if (scroll_node != NULL) {
        Clay_ScrollContainerData data =
            Clay_GetScrollContainerData(TestClayNodeElementId(scroll_node));
        if (data.found && data.scrollPosition != NULL) {
            result |= RequireNear(
                data.scrollPosition->y,
                -10.0f,
                0.001f,
                "next frame should consume committed component offset");
        }
    }

    ecs_set(world, scroll, EcsUiScrollState, {.offset_y = -999.0f});
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read settle tree");
    result |= RunScrollStateFrame(
        &tree,
        &theme,
        &options,
        &state,
        (EcsUiPointerState){0},
        &frame);
    scroll_node = EcsUiTreeSnapshotFindNodeById(&tree, "FrameScrollState");
    float expected_clamped = 0.0f;
    if (scroll_node != NULL) {
        Clay_ScrollContainerData data =
            Clay_GetScrollContainerData(TestClayNodeElementId(scroll_node));
        result |= Require(data.found, "settle scroll data missing");
        if (data.found) {
            expected_clamped = ExpectedScrollClamp(
                data.contentDimensions.height / scale,
                data.scrollContainerDimensions.height / scale);
        }
    }
    EcsUiFrameSettleScroll(world, 0.0);
    const EcsUiScrollState *settled = ecs_get(world, scroll, EcsUiScrollState);
    result |= Require(settled != NULL, "settled scroll state missing");
    if (settled != NULL) {
        result |= RequireNear(
            settled->offset_y,
            expected_clamped,
            0.001f,
            "settle should clamp against reported content");
        result |= Require(
            settled->content_h > 0.0f,
            "settle should write content height");
    }

    ecs_set(world, scroll, EcsUiScrollState, {.offset_y = -4.0f});
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read accumulation tree");
    EcsUiFrameInteractionStateInit(&state);
    result |= RunScrollStateFrame(&tree, &theme, &options, &state, wheel, &frame);
    result |= Require(frame.scroll_consumed, "accumulated wheel should consume");
    result |= Require(
        frame.pending_scroll_count == 1u,
        "accumulated wheel should produce one pending update");
    if (frame.pending_scroll_count > 0u) {
        result |= RequireNear(
            frame.pending_scrolls[0].offset_y,
            -4.0f - (10.0f / scale),
            0.001f,
            "wheel should accumulate from scaled existing offset");
    }

    ecs_set(world, scroll, EcsUiScrollState, {.offset_y = -999.0f});
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read wheel clamp tree");
    EcsUiFrameInteractionStateInit(&state);
    result |= RunScrollStateFrame(&tree, &theme, &options, &state, wheel, &frame);
    result |= Require(frame.scroll_consumed, "clamped wheel should consume");
    result |= Require(
        frame.pending_scroll_count == 1u,
        "clamped wheel should produce one pending update");
    if (frame.pending_scroll_count > 0u) {
        result |= RequireNear(
            frame.pending_scrolls[0].offset_y,
            expected_clamped,
            0.001f,
            "wheel write should clamp pending scroll offset");
    }

    ecs_fini(world);
    return result;
}

static int BuildScrollSettleTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale,
    float child_height,
    bool empty,
    ecs_entity_t *out_scroll)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set settle scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t scroll = EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "SettleScroll",
                .preferred_width = 100.0f,
                .preferred_height = 40.0f,
            },
        });
    if (!empty) {
        (void)EcsUiAddCustom(
            &builder,
            (EcsUiCustomDesc){
                .id = "SettleScrollChild",
                .kind = "frame.settle",
                .preferred_width = 80.0f,
                .preferred_height = child_height,
            });
    }
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "scroll settle builder failed");
    if (out_scroll != NULL) {
        *out_scroll = scroll;
    }
    return result;
}

static int TestNativeScrollSettle(float scale, float child_height, float expected)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create native settle world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "NativeSettleRoot");
    ecs_entity_t scroll = 0;
    result |= BuildScrollSettleTree(
        world,
        root,
        scale,
        child_height,
        false,
        &scroll);
    ecs_set(world, scroll, EcsUiScrollState, {.offset_y = -999.0f});

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read native settle tree");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f * scale,
            .height = 160.0f * scale,
        },
    };
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    result |= RunScrollLayoutFrame(&tree, &theme, &options);
    EcsUiFrameSettleScroll(world, 0.0);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);

    const EcsUiScrollState *state = ecs_get(world, scroll, EcsUiScrollState);
    result |= Require(state != NULL, "native settle state missing");
    if (state != NULL) {
        result |= RequireNear(
            state->offset_y,
            expected,
            0.001f,
            "native settle should clamp scroll offset");
        result |= RequireNear(
            state->content_h,
            child_height,
            0.001f,
            "native settle should write content height");
    }

    ecs_fini(world);
    return result;
}

static int TestEmptyScrollSettle(
    EcsUiFrameInternalBackend backend,
    float scale)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create empty settle world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "EmptySettleRoot");
    ecs_entity_t scroll = 0;
    result |= BuildScrollSettleTree(
        world,
        root,
        scale,
        0.0f,
        true,
        &scroll);
    ecs_set(
        world,
        scroll,
        EcsUiScrollState,
        {.offset_y = -25.0f, .content_h = 200.0f});

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read empty settle tree");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f * scale,
            .height = 160.0f * scale,
        },
    };
    EcsUiFrameInternalSelectBackend(backend);
    if (backend == ECS_UI_FRAME_INTERNAL_BACKEND_CLAY) {
        EcsUiInteractionState state = {0};
        EcsUiFrameInteractionStateInit(&state);
        EcsUiInteractionFrame frame = {0};
        result |= RunScrollStateFrame(
            &tree,
            &theme,
            &options,
            &state,
            (EcsUiPointerState){0},
            &frame);
    } else {
        result |= RunScrollLayoutFrame(&tree, &theme, &options);
    }
    EcsUiFrameSettleScroll(world, 0.0);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);

    const EcsUiScrollState *state = ecs_get(world, scroll, EcsUiScrollState);
    result |= Require(state != NULL, "empty settle state missing");
    if (state != NULL) {
        result |= RequireNear(
            state->offset_y,
            0.0f,
            0.001f,
            "empty settle should clamp offset to zero");
        result |= RequireNear(
            state->content_h,
            0.0f,
            0.001f,
            "empty settle should clear content height");
    }

    ecs_fini(world);
    return result;
}

static int TestFrameErrorCases(TestFrameErrors *errors)
{
    int result = 0;
    if (errors == NULL) {
        return Require(false, "missing error recorder");
    }

    *errors = (TestFrameErrors){0};
    EcsUiTheme theme = EcsUiThemeDefault();
    result |= Require(
        EcsUiFrameRun(NULL, &theme, NULL, NULL, NULL) == NULL,
        "null tree frame run should fail");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_INVALID_ARGUMENT,
        "null tree should report invalid argument");

    *errors = (TestFrameErrors){0};
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return result | Require(false, "failed to create stale-frame world");
    }
    ecs_entity_t root = EcsUiRootEntity(world, "FrameStaleRoot");
    result |= BuildPressTree(world, root, 1.0f, NULL);
    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read stale-frame tree");
    EcsUiInteractionState state = {0};
    EcsUiFrameInteractionStateInit(&state);
    EcsUiInteractionFrame frame = {
        .state = &state,
    };
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 220.0f,
            .height = 140.0f,
        },
    };
    EcsUiPointerState pointer = {
        .x = 20.0f,
        .y = 20.0f,
        .down = true,
        .pressed = true,
    };
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, &pointer, &frame) != NULL,
        "stale setup frame run failed");
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "headless stale-invalidating frame run failed");
    EcsUiEventList events = {0};
    EcsUiFrameCollectEvents(&frame, pointer, &events);
    result |= Require(
        events.count == 0u,
        "stale collect should not emit events");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_STALE_INTERACTION_FRAME,
        "stale collect should report stale interaction frame");

    ecs_fini(world);
    return result;
}

int main(void)
{
    int result = 0;
    TestFrameErrors errors = {0};
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 320.0f,
                .surface_height = 240.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = &errors,
            }),
        "failed to initialize frame backend");
    EcsUiFrameBackendSetCullingEnabled(false);
    result |= RunContainmentCase(1.0f);
    result |= RunContainmentCase(2.0f);
    result |= TestAttachPointMapping();
    result |= TestZIndexTranslation();
    result |= TestCapturePointerTranslation();
    result |= TestPointerCaptureRoundTrip(1.0f);
    result |= TestPointerCaptureRoundTrip(2.0f);
    result |= TestScrollStateOwnership(1.0f);
    result |= TestScrollStateOwnership(2.0f);
    result |= TestNativeScrollSettle(1.0f, 90.0f, -50.0f);
    result |= TestNativeScrollSettle(2.0f, 90.0f, -50.0f);
    result |= TestNativeScrollSettle(1.0f, 30.0f, 0.0f);
    result |= TestNativeScrollSettle(2.0f, 30.0f, 0.0f);
    result |= TestEmptyScrollSettle(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY, 1.0f);
    result |= TestEmptyScrollSettle(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY, 2.0f);
    result |= TestEmptyScrollSettle(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE, 1.0f);
    result |= TestEmptyScrollSettle(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE, 2.0f);
    result |= Require(errors.count == 0u, "frame backend emitted errors");
    if (errors.count != 0u) {
        (void)fprintf(
            stderr,
            "last frame error kind=%d message=%s\n",
            (int)errors.last_kind,
            errors.last_message);
    }
    result |= TestFrameErrorCases(&errors);
    EcsUiFrameBackendShutdown();
    return result;
}
