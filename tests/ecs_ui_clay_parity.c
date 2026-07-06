#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#define CLAY_IMPLEMENTATION
#include "ecs_ui/ecs_ui_clay.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestClayErrors {
    uint32_t count;
    uint32_t duplicate_id_count;
    Clay_ErrorType last_type;
    char last_text[256];
} TestClayErrors;

static TestClayErrors g_clay_errors;

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

static int RequireClayColor(
    Clay_Color actual,
    EcsUiColor expected,
    const char *message)
{
    const float epsilon = 0.001f;
    float r_delta = actual.r - (float)expected.r;
    float g_delta = actual.g - (float)expected.g;
    float b_delta = actual.b - (float)expected.b;
    float a_delta = actual.a - (float)expected.a;
    if (r_delta < 0.0f) {
        r_delta = -r_delta;
    }
    if (g_delta < 0.0f) {
        g_delta = -g_delta;
    }
    if (b_delta < 0.0f) {
        b_delta = -b_delta;
    }
    if (a_delta < 0.0f) {
        a_delta = -a_delta;
    }

    if (r_delta > epsilon || g_delta > epsilon || b_delta > epsilon ||
        a_delta > epsilon) {
        (void)fprintf(
            stderr,
            "%s: actual={%f,%f,%f,%f} expected={%u,%u,%u,%u}\n",
            message,
            actual.r,
            actual.g,
            actual.b,
            actual.a,
            (unsigned int)expected.r,
            (unsigned int)expected.g,
            (unsigned int)expected.b,
            (unsigned int)expected.a);
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

static void CopyClayString(char *out, size_t out_size, Clay_String value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    int32_t length = value.length;
    if (length < 0) {
        length = 0;
    }

    size_t i = 0u;
    for (; i + 1u < out_size && i < (size_t)length; i += 1u) {
        out[i] = value.chars != NULL ? value.chars[i] : '\0';
    }
    out[i] = '\0';
}

static bool ClayStringSliceEquals(Clay_StringSlice value, const char *expected)
{
    if (expected == NULL || value.length < 0) {
        return false;
    }

    size_t expected_length = strlen(expected);
    if ((size_t)value.length != expected_length) {
        return false;
    }

    return expected_length == 0u ||
        (value.chars != NULL &&
         memcmp(value.chars, expected, expected_length) == 0);
}

static void TestClayHandleError(Clay_ErrorData error_data)
{
    TestClayErrors *errors = error_data.userData;
    if (errors == NULL) {
        return;
    }

    errors->count += 1u;
    errors->last_type = error_data.errorType;
    CopyClayString(errors->last_text, sizeof(errors->last_text), error_data.errorText);
    if (error_data.errorType == CLAY_ERROR_TYPE_DUPLICATE_ID) {
        errors->duplicate_id_count += 1u;
    }
}

static Clay_Dimensions TestMeasureText(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *user_data)
{
    (void)user_data;

    const float font_size =
        config != NULL && config->fontSize > 0u ? (float)config->fontSize : 16.0f;
    return (Clay_Dimensions){
        .width = (float)text.length * font_size * 0.5f,
        .height = font_size + 4.0f,
    };
}

static int InitializeClay(void **memory_out)
{
    if (memory_out == NULL) {
        return Require(false, "missing Clay memory output");
    }

    uint32_t memory_size = Clay_MinMemorySize();
    void *memory = malloc((size_t)memory_size);
    if (memory == NULL) {
        return Require(false, "failed to allocate Clay memory");
    }

    Clay_Arena arena =
        Clay_CreateArenaWithCapacityAndMemory((size_t)memory_size, memory);
    Clay_Context *context = Clay_Initialize(
        arena,
        (Clay_Dimensions){
            .width = 320.0f,
            .height = 240.0f,
        },
        (Clay_ErrorHandler){
            .errorHandlerFunction = TestClayHandleError,
            .userData = &g_clay_errors,
        });
    if (context == NULL) {
        free(memory);
        return Require(false, "failed to initialize Clay");
    }

    Clay_SetMeasureTextFunction(TestMeasureText, NULL);
    Clay_SetCullingEnabled(false);
    Clay_SetDebugModeEnabled(false);
    *memory_out = memory;
    return 0;
}

static int RequireClayTextColor(
    Clay_RenderCommandArray *commands,
    const char *text,
    EcsUiColor expected,
    const char *message)
{
    if (commands == NULL || text == NULL) {
        return Require(false, "missing Clay text command inputs");
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT) {
            continue;
        }

        Clay_TextRenderData *text_data = &command->renderData.text;
        if (ClayStringSliceEquals(text_data->stringContents, text)) {
            return RequireClayColor(text_data->textColor, expected, message);
        }
    }

    (void)fprintf(stderr, "%s: text command not found: %s\n", message, text);
    return 1;
}

static int RequireClayRectangleColor(
    Clay_RenderCommandArray *commands,
    EcsUiColor expected,
    const char *message)
{
    if (commands == NULL) {
        return Require(false, "missing Clay rectangle command inputs");
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
            continue;
        }

        Clay_Color color = command->renderData.rectangle.backgroundColor;
        if ((uint8_t)color.r == expected.r &&
            (uint8_t)color.g == expected.g &&
            (uint8_t)color.b == expected.b &&
            (uint8_t)color.a == expected.a) {
            return RequireClayColor(color, expected, message);
        }
    }

    (void)fprintf(stderr, "%s: rectangle color not found\n", message);
    return 1;
}

static int RequireOnlyTransparentOrClayRectangleColor(
    Clay_RenderCommandArray *commands,
    EcsUiColor allowed,
    const char *message)
{
    if (commands == NULL) {
        return Require(false, "missing Clay rectangle command inputs");
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
            continue;
        }

        Clay_Color color = command->renderData.rectangle.backgroundColor;
        if ((uint8_t)color.a == 0u) {
            continue;
        }
        if ((uint8_t)color.r != allowed.r ||
            (uint8_t)color.g != allowed.g ||
            (uint8_t)color.b != allowed.b ||
            (uint8_t)color.a != allowed.a) {
            (void)fprintf(
                stderr,
                "%s: unexpected opaque rectangle color {%u,%u,%u,%u}\n",
                message,
                (unsigned int)(uint8_t)color.r,
                (unsigned int)(uint8_t)color.g,
                (unsigned int)(uint8_t)color.b,
                (unsigned int)(uint8_t)color.a);
            return 1;
        }
    }

    return 0;
}

static int RequireClayBorderSides(
    Clay_RenderCommandArray *commands,
    EcsUiColor expected_color,
    uint16_t expected_left,
    uint16_t expected_top,
    uint16_t expected_right,
    uint16_t expected_bottom,
    const char *message)
{
    if (commands == NULL) {
        return Require(false, "missing Clay border command inputs");
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_BORDER) {
            continue;
        }

        Clay_BorderRenderData *border = &command->renderData.border;
        if (border->width.left == expected_left &&
            border->width.top == expected_top &&
            border->width.right == expected_right &&
            border->width.bottom == expected_bottom) {
            return RequireClayColor(border->color, expected_color, message);
        }
    }

    (void)fprintf(stderr, "%s: border command not found\n", message);
    return 1;
}

static int RequireClayBorder(
    Clay_RenderCommandArray *commands,
    EcsUiColor expected_color,
    uint16_t expected_width,
    const char *message)
{
    return RequireClayBorderSides(
        commands,
        expected_color,
        expected_width,
        expected_width,
        expected_width,
        expected_width,
        message);
}

static bool ClayColorEquals(Clay_Color actual, EcsUiColor expected)
{
    return (uint8_t)actual.r == expected.r &&
        (uint8_t)actual.g == expected.g &&
        (uint8_t)actual.b == expected.b &&
        (uint8_t)actual.a == expected.a;
}

typedef struct TestClayEdgeSummary {
    bool has_horizontal;
    bool has_vertical;
    float min_horizontal_y;
    float max_horizontal_y;
    float min_vertical_x;
    float max_vertical_x;
} TestClayEdgeSummary;

static void CollectClayEdgeRectangles(
    Clay_RenderCommandArray *commands,
    EcsUiColor color,
    TestClayEdgeSummary *out)
{
    if (commands == NULL || out == NULL) {
        return;
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE ||
            !ClayColorEquals(
                command->renderData.rectangle.backgroundColor,
                color)) {
            continue;
        }

        Clay_BoundingBox bounds = command->boundingBox;
        const bool horizontal =
            bounds.width > 4.0f && bounds.height > 0.0f &&
            bounds.height <= 1.01f;
        const bool vertical =
            bounds.height > 4.0f && bounds.width > 0.0f &&
            bounds.width <= 1.01f;
        if (horizontal) {
            if (!out->has_horizontal || bounds.y < out->min_horizontal_y) {
                out->min_horizontal_y = bounds.y;
            }
            if (!out->has_horizontal || bounds.y > out->max_horizontal_y) {
                out->max_horizontal_y = bounds.y;
            }
            out->has_horizontal = true;
        }
        if (vertical) {
            if (!out->has_vertical || bounds.x < out->min_vertical_x) {
                out->min_vertical_x = bounds.x;
            }
            if (!out->has_vertical || bounds.x > out->max_vertical_x) {
                out->max_vertical_x = bounds.x;
            }
            out->has_vertical = true;
        }
    }
}

static int RequireClayBevelEdges(
    Clay_RenderCommandArray *commands,
    EcsUiColor top_left_color,
    EcsUiColor bottom_right_color,
    const char *message)
{
    TestClayEdgeSummary top_left = {0};
    TestClayEdgeSummary bottom_right = {0};
    CollectClayEdgeRectangles(commands, top_left_color, &top_left);
    CollectClayEdgeRectangles(commands, bottom_right_color, &bottom_right);

    int result = 0;
    result |= Require(
        top_left.has_horizontal,
        "bevel top edge command not found");
    result |= Require(
        top_left.has_vertical,
        "bevel left edge command not found");
    result |= Require(
        bottom_right.has_horizontal,
        "bevel bottom edge command not found");
    result |= Require(
        bottom_right.has_vertical,
        "bevel right edge command not found");
    if (result != 0) {
        (void)fprintf(stderr, "%s\n", message);
        return result;
    }

    result |= Require(
        top_left.min_horizontal_y < bottom_right.max_horizontal_y,
        "bevel top edge should be above bottom edge");
    result |= Require(
        top_left.min_vertical_x < bottom_right.max_vertical_x,
        "bevel left edge should be left of right edge");
    if (result != 0) {
        (void)fprintf(stderr, "%s\n", message);
    }
    return result;
}

static int RequireNoClayBevelEdges(
    Clay_RenderCommandArray *commands,
    EcsUiColor color,
    const char *message)
{
    TestClayEdgeSummary edges = {0};
    CollectClayEdgeRectangles(commands, color, &edges);
    if (edges.has_horizontal || edges.has_vertical) {
        (void)fprintf(stderr, "%s: unexpected bevel edge color found\n", message);
        return 1;
    }
    return 0;
}

static int RequireNoClayBorderColor(
    Clay_RenderCommandArray *commands,
    EcsUiColor color,
    const char *message)
{
    if (commands == NULL) {
        return Require(false, "missing Clay border command inputs");
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_BORDER) {
            continue;
        }
        if (ClayColorEquals(command->renderData.border.color, color)) {
            (void)fprintf(stderr, "%s: unexpected Clay border found\n", message);
            return 1;
        }
    }
    return 0;
}

static void ResetClayErrors(void)
{
    memset(&g_clay_errors, 0, sizeof(g_clay_errors));
}

static EcsUiClayLayoutOptions LayoutOptions(float width, float height)
{
    return (EcsUiClayLayoutOptions){
        .bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = width,
            .height = height,
        },
    };
}

static void CollectTreeFrameEvents(
    const EcsUiTreeSnapshot *tree,
    EcsUiClayPointerState pointer,
    const EcsUiClayLayoutOptions *options,
    EcsUiClayInteractionState *state,
    EcsUiEventList *events,
    EcsUiClayInteractionFrame *out_frame)
{
    EcsUiClayInteractionFrame frame = {0};
    EcsUiClayInteractionFrameBegin(&frame, state);
    if (options != NULL) {
        Clay_SetLayoutDimensions((Clay_Dimensions){
            .width = options->bounds.width,
            .height = options->bounds.height,
        });
    }
    Clay_BeginLayout();
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayEmitTreeEx(tree, &theme, options, &frame);
    (void)Clay_EndLayout();
    Clay_SetPointerState(
        (Clay_Vector2){
            .x = pointer.x,
            .y = pointer.y,
        },
        pointer.down);
    EcsUiClayCollectFrameEvents(&frame, pointer, events);
    if (out_frame != NULL) {
        *out_frame = frame;
    }
}

static ecs_world_t *CreateWorld(void)
{
    ecs_world_t *world = ecs_init();
    if (world != NULL) {
        EcsUiImport(world);
    }
    return world;
}

static ecs_entity_t CreateAction(ecs_world_t *world, const char *name)
{
    return ecs_entity(world, {
        .name = name,
        .sep = "",
    });
}

static void SetNodeId(
    ecs_world_t *world,
    ecs_entity_t entity,
    const char *id)
{
    EcsUiNodeId node_id = {0};
    CopyString(node_id.value, sizeof(node_id.value), id);
    ecs_set_ptr(world, entity, EcsUiNodeId, &node_id);
}

static uint32_t CountNodeId(const EcsUiTreeSnapshot *tree, const char *id)
{
    if (tree == NULL || id == NULL) {
        return 0u;
    }

    uint32_t count = 0u;
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (strcmp(tree->nodes[i].id, id) == 0) {
            count += 1u;
        }
    }
    return count;
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

static int32_t FindCustomCommandIndex(
    Clay_RenderCommandArray *commands,
    const char *node_id)
{
    if (commands == NULL || node_id == NULL) {
        return -1;
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
            return i;
        }
    }
    return -1;
}

static Clay_RenderCommand *FindTextCommand(
    Clay_RenderCommandArray *commands,
    const char *text)
{
    if (commands == NULL || text == NULL) {
        return NULL;
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT) {
            continue;
        }

        Clay_TextRenderData *text_data = &command->renderData.text;
        if (ClayStringSliceEquals(text_data->stringContents, text)) {
            return command;
        }
    }
    return NULL;
}

static int RequireEventCount(
    const EcsUiEventList *events,
    uint32_t expected,
    const char *message)
{
    if (events == NULL || events->count != expected) {
        (void)fprintf(
            stderr,
            "%s: actual=%u expected=%u\n",
            message,
            events != NULL ? events->count : 0u,
            expected);
        return 1;
    }
    return 0;
}

static int RequireEvent(
    const EcsUiEventList *events,
    uint32_t index,
    EcsUiEventType type,
    ecs_entity_t node,
    const char *node_id)
{
    if (events == NULL || index >= events->count) {
        return Require(false, "event index out of range");
    }

    const EcsUiEvent *event = &events->events[index];
    if (event->type != type ||
        (node != 0 && event->node != node) ||
        (node_id != NULL && strcmp(event->node_id, node_id) != 0)) {
        (void)fprintf(
            stderr,
            "unexpected event %u: type=%d node=%llu id=%s\n",
            index,
            (int)event->type,
            (unsigned long long)event->node,
            event->node_id);
        return 1;
    }
    return 0;
}

static int TestRootScaleAffectsClayLayoutOnly(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create scale world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "ScaleRoot");
    ecs_entity_t style = EcsUiStyleToken(world, "ScaleTextStyle");
    ecs_set(
        world,
        style,
        EcsUiTextStyle,
        {
            .body_size = 11.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "ScaleStack",
            .gap = 4.0f,
            .padding = 5.0f,
            .style_token = style,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ScaleSurface",
            .kind = "scale.surface",
            .preferred_width = 40.0f,
            .preferred_height = 20.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "ScaleText",
            .text = "Scale",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "scale builder failed");

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 220.0f);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "scale 1 tree read failed");
    result |= RequireNear(tree.scale, 1.0f, 0.001f, "default scale mismatch");
    const EcsUiTreeNodeSnapshot *stack = FindTreeNode(&tree, "ScaleStack");
    const EcsUiTreeNodeSnapshot *surface = FindTreeNode(&tree, "ScaleSurface");
    result |= Require(
        stack != NULL && surface != NULL,
        "scale snapshot nodes missing");
    if (stack != NULL && surface != NULL) {
        result |= RequireNear(
            stack->stack.padding,
            5.0f,
            0.001f,
            "logical padding should remain unscaled");
        result |= RequireNear(
            surface->custom.preferred_width,
            40.0f,
            0.001f,
            "logical custom width should remain unscaled");
    }

    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();
    Clay_RenderCommand *surface_command =
        FindCustomCommand(&commands, "ScaleSurface");
    Clay_RenderCommand *text_command = FindTextCommand(&commands, "Scale");
    result |= Require(surface_command != NULL, "scale 1 custom command missing");
    result |= Require(text_command != NULL, "scale 1 text command missing");
    if (surface_command != NULL) {
        result |= RequireNear(
            surface_command->boundingBox.x,
            5.0f,
            0.001f,
            "scale 1 custom x mismatch");
        result |= RequireNear(
            surface_command->boundingBox.width,
            40.0f,
            0.001f,
            "scale 1 custom width mismatch");
        result |= RequireNear(
            surface_command->boundingBox.height,
            20.0f,
            0.001f,
            "scale 1 custom height mismatch");
    }
    if (text_command != NULL) {
        result |= Require(
            text_command->renderData.text.fontSize == 11u,
            "scale 1 text font size should use style data");
    }

    result |= Require(EcsUiSetScale(world, root, 2.0f), "set scale failed");
    tree = (EcsUiTreeSnapshot){0};
    result |= Require(EcsUiReadTree(world, root, &tree), "scale 2 tree read failed");
    result |= RequireNear(tree.scale, 2.0f, 0.001f, "scale 2 snapshot mismatch");
    stack = FindTreeNode(&tree, "ScaleStack");
    surface = FindTreeNode(&tree, "ScaleSurface");
    if (stack != NULL && surface != NULL) {
        result |= RequireNear(
            stack->stack.padding,
            5.0f,
            0.001f,
            "scale 2 logical padding should remain unscaled");
        result |= RequireNear(
            surface->custom.preferred_width,
            40.0f,
            0.001f,
            "scale 2 logical custom width should remain unscaled");
    }

    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, NULL);
    commands = Clay_EndLayout();
    surface_command = FindCustomCommand(&commands, "ScaleSurface");
    text_command = FindTextCommand(&commands, "Scale");
    result |= Require(surface_command != NULL, "scale 2 custom command missing");
    result |= Require(text_command != NULL, "scale 2 text command missing");
    if (surface_command != NULL) {
        result |= RequireNear(
            surface_command->boundingBox.x,
            10.0f,
            0.001f,
            "scale 2 custom x mismatch");
        result |= RequireNear(
            surface_command->boundingBox.width,
            80.0f,
            0.001f,
            "scale 2 custom width mismatch");
        result |= RequireNear(
            surface_command->boundingBox.height,
            40.0f,
            0.001f,
            "scale 2 custom height mismatch");
    }
    if (text_command != NULL) {
        result |= Require(
            text_command->renderData.text.fontSize == 22u,
            "scale 2 text font size should be scaled");
    }

    ecs_fini(world);
    return result;
}

static int TestScaledPointerEventsAreLogical(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create scaled pointer world");
    }

    ecs_entity_t action = CreateAction(world, "ScaledPointerAction");
    ecs_entity_t root = EcsUiRootEntity(world, "ScaledPointerRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ScaledPointerTarget",
            .kind = "target",
            .preferred_width = 100.0f,
            .preferred_height = 40.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(
        EcsUiBuilderOk(&builder),
        "scaled pointer builder failed");
    result |= Require(
        EcsUiSetScale(world, root, 2.0f),
        "scaled pointer set scale failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "scaled pointer tree read failed");

    EcsUiClayLayoutOptions options = {
        .bounds = {20.0f, 30.0f, 240.0f, 120.0f},
    };
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiEventList events = {0};

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 60.0f,
            .y = 70.0f,
            .time = 1.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        3u,
        "scaled press should emit hover, pressed, and drag started");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        target,
        "ScaledPointerTarget");
    if (events.count > 1u) {
        result |= RequireNear(
            events.events[1u].x,
            30.0f,
            0.001f,
            "scaled pressed x should be logical");
        result |= RequireNear(
            events.events[1u].y,
            35.0f,
            0.001f,
            "scaled pressed y should be logical");
        result |= RequireNear(
            events.events[1u].start_x,
            30.0f,
            0.001f,
            "scaled pressed start_x should be logical");
        result |= RequireNear(
            events.events[1u].start_y,
            35.0f,
            0.001f,
            "scaled pressed start_y should be logical");
    }

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 86.0f,
            .y = 102.0f,
            .time = 1.25,
            .down = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        1u,
        "scaled held pointer should emit dragged");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_DRAGGED,
        target,
        "ScaledPointerTarget");
    if (events.count > 0u) {
        result |= RequireNear(
            events.events[0u].x,
            43.0f,
            0.001f,
            "scaled dragged x should be logical");
        result |= RequireNear(
            events.events[0u].y,
            51.0f,
            0.001f,
            "scaled dragged y should be logical");
        result |= RequireNear(
            events.events[0u].start_x,
            30.0f,
            0.001f,
            "scaled dragged start_x should be logical");
        result |= RequireNear(
            events.events[0u].start_y,
            35.0f,
            0.001f,
            "scaled dragged start_y should be logical");
        result |= RequireNear(
            events.events[0u].delta_x,
            13.0f,
            0.001f,
            "scaled dragged delta_x should be logical");
        result |= RequireNear(
            events.events[0u].delta_y,
            16.0f,
            0.001f,
            "scaled dragged delta_y should be logical");
        result |= RequireNear(
            events.events[0u].velocity_x,
            52.0f,
            0.01f,
            "scaled dragged velocity_x should be logical");
        result |= RequireNear(
            events.events[0u].velocity_y,
            64.0f,
            0.01f,
            "scaled dragged velocity_y should be logical");
    }

    ecs_fini(world);
    return result;
}

static int TestDuplicateAuthoredIdsDoNotCollide(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "DuplicateIdsRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(&builder, (EcsUiStackDesc){.id = "List"});

    ecs_entity_t row_a =
        EcsUiBeginHStack(&builder, (EcsUiStackDesc){.id = "RowA"});
    ecs_entity_t label_a = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "LabelA",
            .text = "alpha",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);

    ecs_entity_t row_b =
        EcsUiBeginHStack(&builder, (EcsUiStackDesc){.id = "RowB"});
    ecs_entity_t label_b = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "LabelB",
            .text = "bravo",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);

    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "duplicate ID builder failed");

    SetNodeId(world, row_a, "Row");
    SetNodeId(world, row_b, "Row");
    SetNodeId(world, label_a, "Label");
    SetNodeId(world, label_b, "Label");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "duplicate ID tree read failed");
    result |= Require(
        CountNodeId(&tree, "Row") == 2u,
        "test tree should contain duplicate authored row ids");
    result |= Require(
        CountNodeId(&tree, "Label") == 2u,
        "test tree should contain duplicate authored label ids");

    ResetClayErrors();
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 220.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= Require(commands.length > 0, "Clay layout should emit render commands");
    if (g_clay_errors.count != 0u || g_clay_errors.duplicate_id_count != 0u) {
        (void)fprintf(
            stderr,
            "Clay emitted errors: count=%u duplicate_ids=%u last_type=%d last_text=%s\n",
            g_clay_errors.count,
            g_clay_errors.duplicate_id_count,
            (int)g_clay_errors.last_type,
            g_clay_errors.last_text);
        result = 1;
    }

    ecs_fini(world);
    return result;
}

static int TestScrollViewEmitsScissorCommands(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create scroll view world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "ScrollClipRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "ScrollClip",
                .preferred_width = 120.0f,
                .preferred_height = 40.0f,
            },
        });
    for (int i = 0; i < 4; i += 1) {
        char id[ECS_UI_ID_MAX];
        char text[ECS_UI_TEXT_MAX];
        (void)snprintf(id, sizeof(id), "ScrollText%d", i);
        (void)snprintf(text, sizeof(text), "row %d", i);
        EcsUiAddText(
            &builder,
            (EcsUiTextDesc){
                .id = id,
                .text = text,
                .role = ECS_UI_TEXT_BODY,
            });
    }
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "scroll clip builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "scroll clip tree read failed");
    const EcsUiTreeNodeSnapshot *scroll = FindTreeNode(&tree, "ScrollClip");
    result |= Require(
        scroll != NULL &&
            scroll->has_scroll_view &&
            scroll->scroll_view.axes == ECS_UI_SCROLL_AXIS_Y,
        "scroll clip snapshot mismatch");

    ResetClayErrors();
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 220.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    uint32_t scissor_start_count = 0u;
    uint32_t scissor_end_count = 0u;
    bool found_vertical_scissor = false;
    for (int32_t i = 0; i < commands.length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
        if (command == NULL) {
            continue;
        }
        if (command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
            scissor_start_count += 1u;
            if (command->renderData.clip.vertical &&
                !command->renderData.clip.horizontal) {
                found_vertical_scissor = true;
            }
        } else if (command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
            scissor_end_count += 1u;
        }
    }

    result |= Require(
        found_vertical_scissor,
        "scroll view should emit vertical scissor start");
    result |= Require(
        scissor_start_count == scissor_end_count &&
            scissor_start_count > 0u,
        "scroll view scissor commands should be balanced");

    ecs_fini(world);
    return result;
}

static int TestTextStyleInheritanceEmitsClayForegroundColor(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t normal_style_token =
        EcsUiStyleToken(world, "ClayNormalTextStyle");
    ecs_entity_t disabled_style_token =
        EcsUiStyleToken(world, "ClayDisabledTextStyle");
    ecs_entity_t theme_entity = EcsUiThemeEntity(world, "ClayTextStyleTheme");
    EcsUiTextStyle normal_style = {
        .color = {31u, 47u, 83u, 255u},
        .muted_color = {71u, 87u, 123u, 255u},
        .disabled_color = {111u, 127u, 163u, 255u},
    };
    EcsUiTextStyle disabled_style = {
        .color = {29u, 67u, 103u, 255u},
        .muted_color = {69u, 107u, 143u, 255u},
        .disabled_color = {211u, 97u, 43u, 255u},
    };

    result |= Require(
        normal_style_token != 0 && disabled_style_token != 0 &&
            normal_style_token != disabled_style_token && theme_entity != 0,
        "text style inheritance setup failed");
    result |= Require(
        EcsUiThemeSetTextStyle(
            world,
            theme_entity,
            normal_style_token,
            normal_style),
        "normal text style token should store text style");
    result |= Require(
        EcsUiThemeSetTextStyle(
            world,
            theme_entity,
            disabled_style_token,
            disabled_style),
        "disabled text style token should store text style");
    result |= Require(
        EcsUiSetActiveTheme(world, theme_entity),
        "text style theme should become active");
    result |= Require(
        EcsUiThemeApply(world),
        "text style theme should apply to style tokens");

    ecs_entity_t root = EcsUiRootEntity(world, "TextStyleInheritanceRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(&builder, (EcsUiStackDesc){.id = "TextStyleStack"});

    (void)EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "NormalPressable",
            .style_token = normal_style_token,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "NormalText",
            .text = "inherited normal text",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "NormalIcon",
            .name = "normal-symbol",
        });
    EcsUiEnd(&builder);

    (void)EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "DisabledPressable",
            .disabled = true,
            .style_token = disabled_style_token,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DisabledText",
            .text = "disabled pressable text",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "DisabledIcon",
            .name = "disabled-symbol",
        });
    EcsUiEnd(&builder);

    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "text style builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "text style tree read failed");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(360.0f, 220.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= Require(
        commands.length > 0,
        "Clay text style layout should emit render commands");
    result |= Require(
        g_clay_errors.count == 0u,
        "Clay text style layout should not emit errors");
    result |= RequireClayTextColor(
        &commands,
        "inherited normal text",
        normal_style.color,
        "normal text child should inherit parent text style color");
    result |= RequireClayTextColor(
        &commands,
        "disabled pressable text",
        disabled_style.disabled_color,
        "disabled pressable text child should inherit disabled text color");

    ecs_fini(world);
    return result;
}

static int TestIconEmitsCustomCommand(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "IconCustomRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "ResolverIcon",
            .name = "slice-b-symbol",
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "icon custom builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "icon custom tree read failed");
    const EcsUiTreeNodeSnapshot *icon_node =
        FindTreeNode(&tree, "ResolverIcon");
    result |= Require(icon_node != NULL, "icon node should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(120.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    Clay_RenderCommand *icon_command =
        FindCustomCommand(&commands, "ResolverIcon");
    result |= Require(
        icon_command != NULL,
        "icon should emit a Clay custom command");
    result |= Require(
        FindTextCommand(&commands, "slice-b-symbol") == NULL,
        "icon should not emit its name as a Clay text command");
    if (icon_command != NULL && icon_node != NULL) {
        result |= Require(
            icon_command->renderData.custom.customData == icon_node,
            "icon custom command should carry the snapshot node");
        result |= RequireNear(
            icon_command->boundingBox.width,
            16.0f,
            0.001f,
            "icon custom command should be 16px wide");
        result |= RequireNear(
            icon_command->boundingBox.height,
            16.0f,
            0.001f,
            "icon custom command should be 16px tall");
    }
    result |= Require(
        g_clay_errors.count == 0u,
        "icon custom layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestVisualOpacitySkipsHitTesting(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "OpacityAction");
    ecs_entity_t root = EcsUiRootEntity(world, "OpacityRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "InvisibleTarget",
            .kind = "target",
            .preferred_width = 200.0f,
            .preferred_height = 60.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "opacity builder failed");

    ecs_set(
        world,
        target,
        EcsUiVisual,
        {
            .opacity = 0.01f,
        });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "opacity tree read failed");
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayLayoutOptions options = {
        .bounds = {20.0f, 30.0f, 200.0f, 100.0f},
    };
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 40.0f,
            .y = 50.0f,
            .time = 1.0,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        0u,
        "opacity <= 0.01 should not produce pointer events");

    ecs_set(
        world,
        target,
        EcsUiVisual,
        {
            .opacity = 0.02f,
        });
    result |= Require(EcsUiReadTree(world, root, &tree), "visible tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 40.0f,
            .y = 50.0f,
            .time = 1.1,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        1u,
        "visible custom target should produce a hover event");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "InvisibleTarget");

    ecs_fini(world);
    return result;
}

static int TestRevealOnHoverRowActionsFallThrough(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create reveal world");
    }

    ecs_entity_t row_action = CreateAction(world, "RevealRowAction");
    ecs_entity_t duplicate_action =
        CreateAction(world, "RevealDuplicateAction");
    ecs_entity_t delete_action = CreateAction(world, "RevealDeleteAction");
    const EcsUiBoxStyle action_style = {
        .padding = 2.0f,
    };
    ecs_entity_t root = EcsUiRootEntity(world, "RevealRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t row = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "RevealRow",
            .preferred_width = 200.0f,
            .preferred_height = 20.0f,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    ecs_add_pair(world, row, EcsUiOnClick, row_action);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RevealLabel",
            .kind = "label",
            .preferred_height = 20.0f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "RevealActions",
            .gap = 2.0f,
            .preferred_width = 46.0f,
            .preferred_height = 20.0f,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DuplicateWrap",
            .preferred_width = 22.0f,
            .preferred_height = 20.0f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    ecs_entity_t duplicate_button = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "DuplicateButton",
            .on_click = duplicate_action,
            .preferred_height = 20.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DuplicateIcon",
            .text = "D",
            .role = ECS_UI_TEXT_CAPTION,
        });
    ecs_set_ptr(world, duplicate_button, EcsUiBoxStyle, &action_style);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DeleteWrap",
            .preferred_width = 22.0f,
            .preferred_height = 20.0f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    ecs_entity_t delete_button = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "DeleteButton",
            .on_click = delete_action,
            .preferred_height = 20.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DeleteIcon",
            .text = "X",
            .role = ECS_UI_TEXT_CAPTION,
        });
    ecs_set_ptr(world, delete_button, EcsUiBoxStyle, &action_style);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "reveal row builder failed");
    result |= Require(
        EcsUiSetRevealOnHover(world, duplicate_button, row),
        "duplicate reveal relationship should be set");
    result |= Require(
        EcsUiSetRevealOnHover(world, delete_button, row),
        "delete reveal relationship should be set");

    const EcsUiVisual *duplicate_visual =
        ecs_get(world, duplicate_button, EcsUiVisual);
    result |= Require(
        duplicate_visual != NULL && duplicate_visual->opacity == 0.0f,
        "duplicate should start hidden");

    EcsUiTreeSnapshot tree = {0};
    EcsUiClayLayoutOptions options = LayoutOptions(220.0f, 40.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayInteractionFrame frame = {0};

    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "reveal row hidden tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 10.0f,
            .time = 30.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        &frame);
    result |= RequireEventCount(
        &events,
        3u,
        "hidden row action press should go to row");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        row,
        "RevealRow");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        row,
        "RevealRow");
    result |= Require(
        events.count > 1u && events.events[1u].action == row_action,
        "row press should carry row action");
    result |= Require(
        EcsUiClayApplyInteractionFrame(world, &frame),
        "reveal row hover frame should apply");
    duplicate_visual = ecs_get(world, duplicate_button, EcsUiVisual);
    result |= Require(
        duplicate_visual != NULL && duplicate_visual->opacity == 1.0f,
        "duplicate should reveal when row is hovered");

    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "reveal row visible tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 10.0f,
            .time = 30.1,
            .released = true,
        },
        &options,
        &state,
        &events,
        &frame);
    result |= RequireEventCount(
        &events,
        2u,
        "row release should click captured row after reveal");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_CLICKED,
        row,
        "RevealRow");
    result |= Require(
        events.count > 1u && events.events[1u].action == row_action,
        "row click should carry row action");

    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "reveal button visible tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 162.0f,
            .y = 10.0f,
            .time = 31.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        &frame);
    result |= RequireEventCount(
        &events,
        3u,
        "revealed duplicate press should hit duplicate button");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        duplicate_button,
        "DuplicateButton");
    result |= Require(
        events.count > 1u && events.events[1u].action == duplicate_action,
        "duplicate press should carry duplicate action");
    result |= Require(
        EcsUiClayApplyInteractionFrame(world, &frame),
        "duplicate hover frame should keep row reveal active");

    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "reveal duplicate release tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 162.0f,
            .y = 10.0f,
            .time = 31.1,
            .released = true,
        },
        &options,
        &state,
        &events,
        &frame);
    result |= RequireEventCount(
        &events,
        2u,
        "duplicate release should click duplicate button");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_CLICKED,
        duplicate_button,
        "DuplicateButton");

    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "reveal gap tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 177.0f,
            .y = 10.0f,
            .time = 32.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        &frame);
    result |= RequireEventCount(
        &events,
        3u,
        "gap between revealed actions should fall through to row");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        row,
        "RevealRow");
    result |= Require(
        events.count > 1u && events.events[1u].action == row_action,
        "gap press should carry row action");

    ecs_fini(world);
    return result;
}

static int TestVisualOffsetAffectsHitTesting(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "OffsetAction");
    ecs_entity_t root = EcsUiRootEntity(world, "OffsetRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "OffsetTarget",
            .kind = "target",
            .preferred_width = 200.0f,
            .preferred_height = 40.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "offset builder failed");

    ecs_set(
        world,
        target,
        EcsUiVisual,
        {
            .opacity = 1.0f,
            .offset_x = 30.0f,
            .offset_y = 60.0f,
        });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "offset tree read failed");
    EcsUiClayLayoutOptions options = LayoutOptions(200.0f, 140.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 10.0f,
            .y = 70.0f,
            .time = 2.0,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        0u,
        "offset target should not hit at its original x position");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 40.0f,
            .y = 70.0f,
            .time = 2.1,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        1u,
        "offset target should hit at its shifted position");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "OffsetTarget");

    ecs_fini(world);
    return result;
}

static int TestActionStackEmitsPointerEvents(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "ChromeAction");
    ecs_entity_t root = EcsUiRootEntity(world, "ChromeRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "ChromeTarget",
            .preferred_width = 120.0f,
            .preferred_height = 44.0f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "ChromeLabel",
            .text = "+",
            .role = ECS_UI_TEXT_TITLE,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "action stack builder failed");
    ecs_add_pair(world, target, EcsUiOnClick, action);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "action stack tree read failed");
    EcsUiClayLayoutOptions options = LayoutOptions(160.0f, 80.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 4.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);

    result |= RequireEventCount(
        &events,
        3u,
        "action stack pressed frame should emit hover, pressed, and drag started");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "ChromeTarget");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        target,
        "ChromeTarget");
    result |= RequireEvent(
        &events,
        2u,
        ECS_UI_EVENT_DRAG_STARTED,
        target,
        "ChromeTarget");

    ecs_fini(world);
    return result;
}

static int TestActionPayloadFlowsToPointerEvents(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    const uint64_t payload = 0xC0FFEEu;
    ecs_entity_t action = CreateAction(world, "PayloadAction");
    ecs_entity_t root = EcsUiRootEntity(world, "PayloadRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PayloadTarget",
            .on_click = action,
            .payload = payload,
            .preferred_height = 44.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PayloadLabel",
            .text = "payload",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "payload builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "payload tree read failed");
    const EcsUiTreeNodeSnapshot *target_node =
        FindTreeNode(&tree, "PayloadTarget");
    result |= Require(
        target_node != NULL && target_node->payload == payload,
        "payload should be copied to tree snapshot");

    EcsUiClayLayoutOptions options = LayoutOptions(160.0f, 80.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 6.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);

    result |= RequireEventCount(
        &events,
        3u,
        "payload press should emit hover, pressed, and drag started");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "PayloadTarget");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        target,
        "PayloadTarget");
    result |= RequireEvent(
        &events,
        2u,
        ECS_UI_EVENT_DRAG_STARTED,
        target,
        "PayloadTarget");
    result |= Require(
        events.events[0u].payload == payload &&
            events.events[1u].payload == payload &&
            events.events[2u].payload == payload,
        "payload should be copied to pointer events");

    ecs_fini(world);
    return result;
}

static int TestInteractiveStackEmitsSecondaryPress(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action =
        CreateAction(world, "SecondaryStackAction");
    ecs_entity_t root = EcsUiRootEntity(world, "SecondaryStackRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "SecondaryTarget",
            .preferred_width = 120.0f,
            .preferred_height = 44.0f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    ecs_add_id(world, target, EcsUiInteractive);
    ecs_add_pair(world, target, EcsUiOnClick, action);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "SecondaryLabel",
            .text = "row",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "secondary stack builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "secondary stack tree read failed");
    EcsUiClayLayoutOptions options = LayoutOptions(160.0f, 80.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 4.5,
            .secondary_down = true,
            .secondary_pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);

    result |= RequireEventCount(
        &events,
        3u,
        "secondary stack press should emit hover, secondary press, and drag start");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "SecondaryTarget");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_SECONDARY_PRESSED,
        target,
        "SecondaryTarget");
    result |= Require(
        events.count > 1u && events.events[1u].action == action,
        "secondary press should carry click action");
    result |= Require(
        events.count > 1u && events.events[1u].button == ECS_UI_POINTER_BUTTON_SECONDARY,
        "secondary press should report secondary button");
    result |= RequireEvent(
        &events,
        2u,
        ECS_UI_EVENT_DRAG_STARTED,
        target,
        "SecondaryTarget");
    result |= Require(
        events.count > 2u && events.events[2u].button == ECS_UI_POINTER_BUTTON_SECONDARY,
        "secondary drag start should report secondary button");
    result |= Require(
        state.capture.active,
        "secondary press should start pointer capture");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 4.55,
            .secondary_released = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        2u,
        "secondary release should end drag and click captured target");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_CLICKED,
        target,
        "SecondaryTarget");
    result |= Require(
        events.count > 1u && events.events[1u].button == ECS_UI_POINTER_BUTTON_SECONDARY,
        "secondary click should report secondary button");
    result |= Require(
        !state.capture.active,
        "secondary release should clear pointer capture");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 4.6,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        3u,
        "interactive stack primary press should capture");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "SecondaryTarget");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        target,
        "SecondaryTarget");
    result |= RequireEvent(
        &events,
        2u,
        ECS_UI_EVENT_DRAG_STARTED,
        target,
        "SecondaryTarget");
    result |= Require(
        state.capture.active,
        "interactive stack primary press should start pointer capture");

    ecs_fini(world);
    return result;
}

static int TestPointerCaptureLifecycle(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "DragAction");
    ecs_entity_t root = EcsUiRootEntity(world, "CaptureRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "DragTarget",
            .kind = "target",
            .preferred_width = 200.0f,
            .preferred_height = 60.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "capture builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "capture tree read failed");
    EcsUiClayLayoutOptions options = LayoutOptions(200.0f, 100.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 10.0f,
            .y = 10.0f,
            .time = 10.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        3u,
        "pressed frame should emit hover, pressed, and drag started");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_HOVERED, target, "DragTarget");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_PRESSED, target, "DragTarget");
    result |= RequireEvent(&events, 2u, ECS_UI_EVENT_DRAG_STARTED, target, "DragTarget");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 14.0f,
            .y = 13.0f,
            .time = 10.1,
            .down = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        1u,
        "held pointer should emit dragged while captured");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_DRAGGED, target, "DragTarget");
    if (events.count > 0u) {
        result |= RequireNear(
            events.events[0].start_x,
            10.0f,
            0.001f,
            "drag start_x mismatch");
        result |= RequireNear(
            events.events[0].start_y,
            10.0f,
            0.001f,
            "drag start_y mismatch");
        result |= RequireNear(
            events.events[0].delta_x,
            4.0f,
            0.001f,
            "drag delta_x mismatch");
        result |= RequireNear(
            events.events[0].delta_y,
            3.0f,
            0.001f,
            "drag delta_y mismatch");
    }

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 14.0f,
            .y = 13.0f,
            .time = 10.2,
            .released = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        2u,
        "small release should emit drag ended and clicked");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_DRAG_ENDED, target, "DragTarget");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_CLICKED, target, "DragTarget");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 180.0f,
            .y = 90.0f,
            .time = 10.3,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        0u,
        "pointer capture should clear after release");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 10.4,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        3u,
        "second press should start capture before missed-release regression");

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 10.5,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        2u,
        "button-up frame without release edge should end capture and click");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_DRAG_ENDED, target, "DragTarget");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_CLICKED, target, "DragTarget");
    result |= Require(!state.capture.active, "missed release should clear pointer capture");

    ecs_fini(world);
    return result;
}

static int TestPointerCaptureMissingTargetFallsThrough(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t stale_action = CreateAction(world, "StaleAction");
    ecs_entity_t fresh_action = CreateAction(world, "FreshAction");
    ecs_entity_t root = EcsUiRootEntity(world, "CaptureRebuildRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stale_target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "StaleTarget",
            .kind = "target",
            .preferred_width = 200.0f,
            .preferred_height = 60.0f,
            .on_click = stale_action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "stale capture builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "stale capture tree read failed");
    EcsUiClayLayoutOptions options = LayoutOptions(200.0f, 100.0f);
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 12.0f,
            .y = 12.0f,
            .time = 30.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        3u,
        "initial stale target press should start capture");
    result |= Require(state.capture.active, "initial stale target capture should be active");

    ecs_delete(world, stale_target);
    builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t fresh_target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FreshTarget",
            .kind = "target",
            .preferred_width = 200.0f,
            .preferred_height = 60.0f,
            .on_click = fresh_action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "fresh target builder failed");

    tree = (EcsUiTreeSnapshot){0};
    result |= Require(EcsUiReadTree(world, root, &tree), "fresh capture tree read failed");
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 12.0f,
            .y = 12.0f,
            .time = 30.1,
            .down = true,
            .pressed = true,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        3u,
        "missing capture target should not eat fresh target press");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_HOVERED, fresh_target, "FreshTarget");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_PRESSED, fresh_target, "FreshTarget");
    result |= RequireEvent(&events, 2u, ECS_UI_EVENT_DRAG_STARTED, fresh_target, "FreshTarget");
    result |= Require(state.capture.active && state.capture.node == fresh_target,
                      "fresh target should own capture after stale capture clears");

    ecs_fini(world);
    return result;
}

static int TestZStackCapturePreventsBackgroundFallthrough(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t background_action = CreateAction(world, "BackgroundAction");
    ecs_entity_t root = EcsUiRootEntity(world, "ZStackRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginZStack(&builder, (EcsUiStackDesc){.id = "Stack"});
    ecs_entity_t background = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "Background",
            .kind = "background",
            .preferred_width = 200.0f,
            .preferred_height = 100.0f,
            .on_click = background_action,
        });
    ecs_entity_t overlay = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "Overlay",
            .kind = "overlay",
            .preferred_width = 200.0f,
            .preferred_height = 100.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "zstack builder failed");

    ecs_set(
        world,
        overlay,
        EcsUiHitTest,
        {
            .mode = ECS_UI_HIT_TEST_CAPTURE,
        });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "zstack tree read failed");
    EcsUiEventList events = {0};
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayLayoutOptions options = {
        .bounds = {0.0f, 0.0f, 200.0f, 160.0f},
    };
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 20.0,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        0u,
        "capturing overlay should consume zstack hit test");
    if (events.count > 0u && events.events[0].node == background) {
        result |= Require(false, "background should not receive zstack hit through overlay");
    }

    ecs_fini(world);
    return result;
}

static int TestZStackBoxStyleEmitsBackgroundColor(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create zstack color world");
    }

    const EcsUiColor key_color = {
        .r = 38u,
        .g = 48u,
        .b = 54u,
        .a = 255u,
    };
    ecs_entity_t root = EcsUiRootEntity(world, "ZStackColorRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stack = EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "ZStackColorTarget",
            .preferred_width = 80.0f,
            .preferred_height = 40.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "zstack color builder failed");

    ecs_set(world, stack, EcsUiBoxStyle, {
        .background = key_color,
        .radius = 6.0f,
    });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "zstack color tree read failed");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(100.0f, 60.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayRectangleColor(
        &commands,
        key_color,
        "zstack should emit its box style background");
    result |= Require(
        g_clay_errors.count == 0u,
        "zstack color layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestStackHoverBackgroundUsesCoreHoverState(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create stack hover world");
    }

    const EcsUiColor base_color = {
        .r = 24u,
        .g = 34u,
        .b = 44u,
        .a = 255u,
    };
    const EcsUiColor hover_color = {
        .r = 88u,
        .g = 98u,
        .b = 108u,
        .a = 255u,
    };
    ecs_entity_t root = EcsUiRootEntity(world, "StackHoverRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stack = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "StackHoverTarget",
            .preferred_width = 100.0f,
            .preferred_height = 40.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "stack hover builder failed");

    ecs_set(world, stack, EcsUiBoxStyle, {
        .background = base_color,
        .hover_background = hover_color,
        .radius = 3.0f,
    });
    result |= Require(
        EcsUiApplyHoverState(world, stack),
        "stack hover state should apply");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "stack hover tree read failed");
    result |= Require(
        tree.count >= 2u && tree.nodes[1u].hovered &&
            tree.nodes[1u].hover_within,
        "stack hover state should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 72.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayRectangleColor(
        &commands,
        hover_color,
        "stack hover should emit hover background");
    result |= Require(
        g_clay_errors.count == 0u,
        "stack hover layout should not emit Clay errors");

    result |= Require(
        EcsUiApplyHoverState(world, 0),
        "stack empty hover state should apply");
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "stack unhover tree read failed");
    ResetClayErrors();
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    commands = Clay_EndLayout();
    result |= RequireClayRectangleColor(
        &commands,
        base_color,
        "unhovered stack should emit base background");

    ecs_fini(world);
    return result;
}

static int TestPressableHoverBackgroundUsesCoreHoverState(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create pressable hover world");
    }

    const EcsUiColor base_color = {
        .r = 39u,
        .g = 49u,
        .b = 59u,
        .a = 255u,
    };
    const EcsUiColor hover_color = {
        .r = 121u,
        .g = 131u,
        .b = 141u,
        .a = 255u,
    };
    ecs_entity_t action = CreateAction(world, "PressableHoverAction");
    ecs_entity_t root = EcsUiRootEntity(world, "PressableHoverRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t pressable = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PressableHoverTarget",
            .on_click = action,
            .preferred_height = 32.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PressableHoverLabel",
            .text = "hover",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(
        EcsUiBuilderOk(&builder),
        "pressable hover builder failed");

    ecs_set(world, pressable, EcsUiBoxStyle, {
        .background = base_color,
        .hover_background = hover_color,
        .padding = 4.0f,
    });
    result |= Require(
        EcsUiApplyHoverState(world, pressable),
        "pressable hover state should apply");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "pressable hover tree read failed");
    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(160.0f, 64.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayRectangleColor(
        &commands,
        hover_color,
        "pressable hover should emit hover background");
    result |= Require(
        g_clay_errors.count == 0u,
        "pressable hover layout should not emit Clay errors");

    result |= Require(
        EcsUiApplyHoverState(world, 0),
        "pressable empty hover state should apply");
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "pressable unhover tree read failed");
    ResetClayErrors();
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    commands = Clay_EndLayout();
    result |= RequireClayRectangleColor(
        &commands,
        base_color,
        "unhovered pressable should emit base background");

    ecs_fini(world);
    return result;
}

static int TestPlainStacksEmitNoDefaultBackground(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create transparent stack world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "TransparentStackRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PlainVStack",
            .preferred_width = 80.0f,
            .preferred_height = 24.0f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PlainHStack",
            .preferred_width = 80.0f,
            .preferred_height = 24.0f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PlainZStack",
            .preferred_width = 80.0f,
            .preferred_height = 24.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(
        EcsUiBuilderOk(&builder),
        "transparent stack builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "transparent stack tree read failed");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(160.0f, 120.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireOnlyTransparentOrClayRectangleColor(
        &commands,
        clay_theme.root_background,
        "plain stacks should not emit opaque stack rectangles");
    result |= Require(
        g_clay_errors.count == 0u,
        "transparent stack layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestStackStyleTokenEmitsBackgroundColor(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create stack token color world");
    }

    const EcsUiColor key_color = {
        .r = 76u,
        .g = 61u,
        .b = 139u,
        .a = 255u,
    };
    ecs_entity_t stack_style = EcsUiStyleToken(world, "StackBackground");
    ecs_set(world, stack_style, EcsUiBoxStyle, {
        .background = key_color,
        .radius = 4.0f,
    });

    ecs_entity_t root = EcsUiRootEntity(world, "StackTokenRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stack = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "StackTokenTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
            .style_token = stack_style,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "stack token builder failed");
    result |= Require(
        ecs_get_target(world, stack, EcsUiUsesStyle, 0) == stack_style,
        "stack desc should attach style token");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "stack token tree read failed");
    result |= Require(
        tree.count >= 2u &&
            tree.nodes[1u].has_box_style &&
            tree.nodes[1u].box_style.background.r == key_color.r &&
            tree.nodes[1u].box_style.background.g == key_color.g &&
            tree.nodes[1u].box_style.background.b == key_color.b &&
            tree.nodes[1u].box_style.background.a == key_color.a,
        "stack token box style should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayRectangleColor(
        &commands,
        key_color,
        "stack style token should emit its box style background");
    result |= Require(
        g_clay_errors.count == 0u,
        "stack token color layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestStackDirectStyleEmitsBackgroundAndBorder(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create stack direct style world");
    }

    const EcsUiColor token_color = {
        .r = 18u,
        .g = 88u,
        .b = 112u,
        .a = 255u,
    };
    const EcsUiColor direct_color = {
        .r = 142u,
        .g = 73u,
        .b = 44u,
        .a = 255u,
    };
    const EcsUiColor border_color = {
        .r = 13u,
        .g = 201u,
        .b = 154u,
        .a = 255u,
    };
    ecs_entity_t token_style = EcsUiStyleToken(world, "StackDirectFallback");
    ecs_set(world, token_style, EcsUiBoxStyle, {
        .background = token_color,
    });

    ecs_entity_t root = EcsUiRootEntity(world, "StackDirectRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stack = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "StackDirectTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
            .style_token = token_style,
            .style = &(EcsUiBoxStyle){
                .background = direct_color,
                .border_color = border_color,
                .border_width = 2.0f,
            },
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "stack direct builder failed");
    result |= Require(
        ecs_get_target(world, stack, EcsUiUsesStyle, 0) == token_style,
        "stack direct style should still attach style token");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "stack direct style tree read failed");
    result |= Require(
        tree.count >= 2u &&
            tree.nodes[1u].has_box_style &&
            tree.nodes[1u].box_style.background.r == direct_color.r &&
            tree.nodes[1u].box_style.background.g == direct_color.g &&
            tree.nodes[1u].box_style.background.b == direct_color.b &&
            tree.nodes[1u].box_style.background.a == direct_color.a,
        "direct stack style should win over token style");
    result |= Require(
        tree.count >= 2u &&
            tree.nodes[1u].box_style.border_color.r == border_color.r &&
            tree.nodes[1u].box_style.border_color.g == border_color.g &&
            tree.nodes[1u].box_style.border_color.b == border_color.b &&
            tree.nodes[1u].box_style.border_color.a == border_color.a,
        "direct stack border color should be snapshotted");
    result |= RequireNear(
        tree.count >= 2u ? tree.nodes[1u].box_style.border_width : 0.0f,
        2.0f,
        0.0001f,
        "direct stack border width should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayRectangleColor(
        &commands,
        direct_color,
        "direct stack style should emit its background");
    result |= RequireClayBorder(
        &commands,
        border_color,
        2u,
        "direct stack style should emit its border");
    result |= Require(
        g_clay_errors.count == 0u,
        "stack direct style layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestNineSliceStyleEmitsCustomBackground(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create nine-slice Clay world");
    }

    const EcsUiColor background_color = {93u, 17u, 41u, 255u};
    const EcsUiColor border_color = {31u, 211u, 174u, 255u};
    const EcsUiColor bevel_light = {250u, 240u, 100u, 255u};
    const EcsUiColor bevel_dark = {12u, 18u, 30u, 255u};
    const EcsUiNineSliceStyle nine_slice_style = {
        .image = "frame.pixel-button",
        .slice_left = 3u,
        .slice_top = 3u,
        .slice_right = 3u,
        .slice_bottom = 3u,
        .scale = 2.0f,
        .tint = {180u, 190u, 200u, 255u},
    };
    const EcsUiBoxStyle box_style = {
        .background = background_color,
        .border_color = border_color,
        .border_width = 2.0f,
        .bevel = ECS_UI_BEVEL_RAISED,
        .bevel_light = bevel_light,
        .bevel_dark = bevel_dark,
    };

    ecs_entity_t root = EcsUiRootEntity(world, "NineSliceClayRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NineSliceFrame",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
            .style = &box_style,
            .nine_slice_style = &nine_slice_style,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "NineSliceIcon",
            .name = "slice-b-symbol",
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "nine-slice Clay builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "nine-slice Clay tree read failed");
    const EcsUiTreeNodeSnapshot *frame_node =
        FindTreeNode(&tree, "NineSliceFrame");
    result |= Require(
        frame_node != NULL && frame_node->has_nine_slice_style,
        "nine-slice style should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    Clay_RenderCommand *nine_slice_command =
        FindCustomCommand(&commands, "NineSliceFrame");
    Clay_RenderCommand *icon_command =
        FindCustomCommand(&commands, "NineSliceIcon");
    result |= Require(
        nine_slice_command != NULL,
        "nine-slice style should emit a custom Clay command");
    result |= Require(
        icon_command != NULL,
        "nine-slice child icon should emit a custom Clay command");
    if (nine_slice_command != NULL && frame_node != NULL) {
        result |= Require(
            nine_slice_command->renderData.custom.customData == frame_node,
            "nine-slice custom command should carry the snapshot node");
        result |= Require(
            nine_slice_command->zIndex >= options.z_index,
            "nine-slice custom command should not render below ancestor backgrounds");
        result |= RequireClayColor(
            nine_slice_command->renderData.custom.backgroundColor,
            nine_slice_style.tint,
            "nine-slice custom command should carry style tint");
        result |= Require(
            nine_slice_command->boundingBox.width > 0.0f &&
                nine_slice_command->boundingBox.height > 0.0f,
            "nine-slice custom command should have nonzero bounds");
    }
    const int32_t nine_slice_command_index =
        FindCustomCommandIndex(&commands, "NineSliceFrame");
    const int32_t icon_command_index =
        FindCustomCommandIndex(&commands, "NineSliceIcon");
    result |= Require(
        nine_slice_command_index >= 0 &&
            icon_command_index >= 0 &&
            nine_slice_command_index < icon_command_index,
        "nine-slice background should render before child custom content");
    result |= RequireOnlyTransparentOrClayRectangleColor(
        &commands,
        clay_theme.root_background,
        "nine-slice should suppress flat box background rectangles");
    result |= RequireNoClayBorderColor(
        &commands,
        border_color,
        "nine-slice should suppress Clay border command");
    result |= RequireNoClayBevelEdges(
        &commands,
        bevel_light,
        "nine-slice should suppress light bevel edges");
    result |= RequireNoClayBevelEdges(
        &commands,
        bevel_dark,
        "nine-slice should suppress dark bevel edges");
    result |= Require(
        g_clay_errors.count == 0u,
        "nine-slice Clay layout should not emit errors");

    ecs_fini(world);
    return result;
}

static int TestBoxStyleBorderEmitsBorderCommand(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create border command world");
    }

    const EcsUiColor border_color = {
        .r = 221u,
        .g = 35u,
        .b = 90u,
        .a = 255u,
    };
    const EcsUiColor side_border_color = {
        .r = 67u,
        .g = 145u,
        .b = 232u,
        .a = 255u,
    };
    const float border_width = 3.0f;
    ecs_entity_t root = EcsUiRootEntity(world, "BorderRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t stack = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "BorderTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t side_stack = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "BorderSideTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "border builder failed");

    ecs_set(world, stack, EcsUiBoxStyle, {
        .border_color = border_color,
        .border_width = border_width,
    });
    ecs_set(world, side_stack, EcsUiBoxStyle, {
        .border_color = side_border_color,
        .border_bottom_width = 2.0f,
    });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "border tree read failed");
    const EcsUiTreeNodeSnapshot *side_node =
        FindTreeNode(&tree, "BorderSideTarget");
    result |= Require(
        tree.count >= 2u &&
            tree.nodes[1u].has_box_style &&
            tree.nodes[1u].box_style.border_color.r == border_color.r &&
            tree.nodes[1u].box_style.border_color.g == border_color.g &&
            tree.nodes[1u].box_style.border_color.b == border_color.b &&
            tree.nodes[1u].box_style.border_color.a == border_color.a,
        "border color should be snapshotted");
    result |= RequireNear(
        tree.count >= 2u ? tree.nodes[1u].box_style.border_width : 0.0f,
        border_width,
        0.0001f,
        "border width should be snapshotted");
    result |= Require(
        side_node != NULL &&
            side_node->has_box_style &&
            side_node->box_style.border_color.r == side_border_color.r &&
            side_node->box_style.border_color.g == side_border_color.g &&
            side_node->box_style.border_color.b == side_border_color.b &&
            side_node->box_style.border_color.a == side_border_color.a,
        "side-specific border color should be snapshotted");
    result |= RequireNear(
        side_node != NULL ? side_node->box_style.border_bottom_width : 0.0f,
        2.0f,
        0.0001f,
        "side-specific border width should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 120.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayBorder(
        &commands,
        border_color,
        3u,
        "box style border should emit uniform Clay border command");
    result |= RequireClayBorderSides(
        &commands,
        side_border_color,
        0u,
        0u,
        0u,
        2u,
        "box style should emit side-specific Clay border command");
    result |= Require(
        g_clay_errors.count == 0u,
        "border layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestStackSidePaddingAffectsClayLayout(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create stack side padding world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "SidePaddingRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "SidePaddingRow",
            .padding_left = 4.0f,
            .padding_top = 2.0f,
            .padding_right = 5.0f,
            .padding_bottom = 3.0f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "SidePaddingChild",
            .kind = "sample",
            .preferred_width = 10.0f,
            .preferred_height = 10.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(
        EcsUiBuilderOk(&builder),
        "stack side padding builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "stack side padding tree read failed");
    const EcsUiTreeNodeSnapshot *row =
        FindTreeNode(&tree, "SidePaddingRow");
    result |= Require(
        row != NULL &&
            row->stack.padding == 0.0f &&
            row->stack.padding_left == 4.0f &&
            row->stack.padding_top == 2.0f &&
            row->stack.padding_right == 5.0f &&
            row->stack.padding_bottom == 3.0f,
        "stack side padding should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(64.0f, 32.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    Clay_RenderCommand *child_command =
        FindCustomCommand(&commands, "SidePaddingChild");
    result |= Require(
        child_command != NULL,
        "stack side padding custom child command missing");
    if (child_command != NULL) {
        result |= RequireNear(
            child_command->boundingBox.x,
            4.0f,
            0.001f,
            "stack side padding should offset child x");
        result |= RequireNear(
            child_command->boundingBox.y,
            2.0f,
            0.001f,
            "stack side padding should offset child y");
        result |= RequireNear(
            child_command->boundingBox.height,
            10.0f,
            0.001f,
            "stack side padding should preserve child height");
    }
    result |= Require(
        g_clay_errors.count == 0u,
        "stack side padding layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestRaisedBevelEmitsLightTopLeftDarkBottomRightEdges(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create raised bevel world");
    }

    const EcsUiColor background = {
        .r = 32u,
        .g = 38u,
        .b = 44u,
        .a = 255u,
    };
    const EcsUiColor light = {
        .r = 236u,
        .g = 224u,
        .b = 196u,
        .a = 255u,
    };
    const EcsUiColor dark = {
        .r = 22u,
        .g = 20u,
        .b = 28u,
        .a = 255u,
    };
    const EcsUiColor border = {
        .r = 201u,
        .g = 31u,
        .b = 91u,
        .a = 255u,
    };

    ecs_entity_t root = EcsUiRootEntity(world, "RaisedBevelRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "RaisedBevelTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
            .style = &(EcsUiBoxStyle){
                .background = background,
                .border_color = border,
                .border_width = 4.0f,
                .bevel = ECS_UI_BEVEL_RAISED,
                .bevel_light = light,
                .bevel_dark = dark,
            },
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "raised bevel builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "raised bevel tree read failed");
    result |= Require(
        tree.count >= 2u &&
            tree.nodes[1u].has_box_style &&
            tree.nodes[1u].box_style.bevel == ECS_UI_BEVEL_RAISED,
        "raised bevel should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayRectangleColor(
        &commands,
        background,
        "raised bevel should still emit its background");
    result |= RequireClayBevelEdges(
        &commands,
        light,
        dark,
        "raised bevel should emit light top/left and dark bottom/right");
    result |= RequireNoClayBorderColor(
        &commands,
        border,
        "raised bevel should suppress uniform border");
    result |= Require(
        g_clay_errors.count == 0u,
        "raised bevel layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestSunkenBevelEmitsDarkTopLeftLightBottomRightEdges(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create sunken bevel world");
    }

    const EcsUiColor light = {
        .r = 214u,
        .g = 238u,
        .b = 241u,
        .a = 255u,
    };
    const EcsUiColor dark = {
        .r = 18u,
        .g = 33u,
        .b = 41u,
        .a = 255u,
    };

    ecs_entity_t root = EcsUiRootEntity(world, "SunkenBevelRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "SunkenBevelTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
            .style = &(EcsUiBoxStyle){
                .background = {
                    .r = 41u,
                    .g = 47u,
                    .b = 54u,
                    .a = 255u,
                },
                .bevel = ECS_UI_BEVEL_SUNKEN,
                .bevel_light = light,
                .bevel_dark = dark,
            },
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "sunken bevel builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "sunken bevel tree read failed");
    result |= Require(
        tree.count >= 2u &&
            tree.nodes[1u].has_box_style &&
            tree.nodes[1u].box_style.bevel == ECS_UI_BEVEL_SUNKEN,
        "sunken bevel should be snapshotted");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireClayBevelEdges(
        &commands,
        dark,
        light,
        "sunken bevel should emit dark top/left and light bottom/right");
    result |= Require(
        g_clay_errors.count == 0u,
        "sunken bevel layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestNoBevelEmitsNoBevelEdges(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create no bevel world");
    }

    const EcsUiColor light = {
        .r = 245u,
        .g = 211u,
        .b = 141u,
        .a = 255u,
    };
    const EcsUiColor dark = {
        .r = 13u,
        .g = 17u,
        .b = 23u,
        .a = 255u,
    };

    ecs_entity_t root = EcsUiRootEntity(world, "NoBevelRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NoBevelTarget",
            .preferred_width = 96.0f,
            .preferred_height = 48.0f,
            .style = &(EcsUiBoxStyle){
                .background = {
                    .r = 49u,
                    .g = 56u,
                    .b = 64u,
                    .a = 255u,
                },
                .bevel = ECS_UI_BEVEL_NONE,
                .bevel_light = light,
                .bevel_dark = dark,
            },
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "no bevel builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "no bevel tree read failed");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(128.0f, 80.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    result |= RequireNoClayBevelEdges(
        &commands,
        light,
        "no bevel should not emit light bevel edges");
    result |= RequireNoClayBevelEdges(
        &commands,
        dark,
        "no bevel should not emit dark bevel edges");
    result |= Require(
        g_clay_errors.count == 0u,
        "no bevel layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestGrowSizingFillsWardrobeShell(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create grow sizing world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "GrowSizingRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "TopBar",
            .kind = "bar",
            .preferred_height = 20.0f,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "Body",
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CanvasHost",
            .width_sizing = ECS_UI_SIZE_GROW,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "Canvas",
            .kind = "canvas",
            .width_sizing = ECS_UI_SIZE_GROW,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "SidePanel",
            .preferred_width = 80.0f,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "BottomBar",
            .kind = "bar",
            .preferred_height = 30.0f,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "grow sizing builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "grow sizing tree read failed");

    ResetClayErrors();
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(300.0f, 200.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    Clay_RenderCommand *canvas = FindCustomCommand(&commands, "Canvas");
    result |= Require(canvas != NULL, "canvas custom command should be emitted");
    if (canvas != NULL) {
        result |= RequireNear(
            canvas->boundingBox.x,
            0.0f,
            0.001f,
            "canvas should start at shell left edge");
        result |= RequireNear(
            canvas->boundingBox.y,
            20.0f,
            0.001f,
            "canvas should start below top bar");
        result |= RequireNear(
            canvas->boundingBox.width,
            220.0f,
            0.001f,
            "canvas should fill remaining width beside side panel");
        result |= RequireNear(
            canvas->boundingBox.height,
            150.0f,
            0.001f,
            "canvas should fill height between fixed bars");
    }
    result |= Require(
        g_clay_errors.count == 0u,
        "grow sizing layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestZStackPlacementAnchorsHitBounds(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create placement world");
    }

    ecs_entity_t overlay_action = CreateAction(world, "PlacedOverlayAction");
    ecs_entity_t root = EcsUiRootEntity(world, "PlacedZStackRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PlacedStack",
            .preferred_width = 200.0f,
            .preferred_height = 100.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PlacedBackground",
            .kind = "background",
            .preferred_width = 200.0f,
            .preferred_height = 100.0f,
        });
    ecs_entity_t overlay = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PlacedOverlay",
            .kind = "overlay",
            .preferred_width = 40.0f,
            .preferred_height = 30.0f,
            .on_click = overlay_action,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "placement zstack builder failed");

    ecs_set(world, overlay, EcsUiPlacement, {
        .parent_x = ECS_UI_ALIGN_END,
        .parent_y = ECS_UI_ALIGN_START,
        .child_x = ECS_UI_ALIGN_END,
        .child_y = ECS_UI_ALIGN_START,
        .offset_x = -10.0f,
        .offset_y = 5.0f,
        .width = 40.0f,
        .height = 30.0f,
    });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "placement zstack tree read failed");
    EcsUiClayLayoutOptions options = {
        .bounds = {20.0f, 30.0f, 200.0f, 100.0f},
    };
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiEventList events = {0};

    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 205.0f,
            .y = 50.0f,
            .time = 20.0,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        1u,
        "placed overlay should receive pointer inside anchored bounds");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        overlay,
        "PlacedOverlay");

    EcsUiEventListClear(&events);
    CollectTreeFrameEvents(
        &tree,
        (EcsUiClayPointerState){
            .x = 165.0f,
            .y = 50.0f,
            .time = 21.0,
        },
        &options,
        &state,
        &events,
        NULL);
    result |= RequireEventCount(
        &events,
        0u,
        "placed overlay should not receive pointer outside anchored bounds");

    ecs_fini(world);
    return result;
}

static int TestZStackPlacedTextRendersInRetainedBounds(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create placed text world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PlacedTextRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PlacedTextStack",
            .preferred_width = 200.0f,
            .preferred_height = 100.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PlacedTextBackground",
            .kind = "background",
            .preferred_width = 200.0f,
            .preferred_height = 100.0f,
        });
    ecs_entity_t label = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PlacedTextLabel",
            .text = "q",
            .role = ECS_UI_TEXT_LABEL,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "placed text builder failed");

    ecs_set(world, label, EcsUiPlacement, {
        .parent_x = ECS_UI_ALIGN_CENTER,
        .parent_y = ECS_UI_ALIGN_CENTER,
        .child_x = ECS_UI_ALIGN_CENTER,
        .child_y = ECS_UI_ALIGN_CENTER,
        .width = 60.0f,
        .height = 30.0f,
    });
    ecs_set(world, label, EcsUiTextLayout, {
        .align_x = ECS_UI_ALIGN_CENTER,
        .align_y = ECS_UI_ALIGN_CENTER,
    });

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "placed text tree read failed");

    ResetClayErrors();
    EcsUiTheme clay_theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = {
        .bounds = {20.0f, 30.0f, 200.0f, 100.0f},
        .z_index = 23,
    };
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = 240.0f,
        .height = 160.0f,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &clay_theme, &options, NULL);
    Clay_RenderCommandArray commands = Clay_EndLayout();

    bool found = false;
    for (int32_t i = 0; i < commands.length; i += 1) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
        if (command == NULL ||
            command->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT ||
            !ClayStringSliceEquals(
                command->renderData.text.stringContents,
                "q")) {
            continue;
        }

        found = true;
        result |= RequireNear(
            command->boundingBox.x,
            115.5f,
            0.001f,
            "placed text x should include retained viewport origin");
        result |= RequireNear(
            command->boundingBox.y,
            69.0f,
            0.001f,
            "placed text y should include retained viewport origin");
        result |= Require(
            command->zIndex > options.z_index,
            "placed text should render after retained viewport root");
    }

    result |= Require(found, "placed text render command not found");
    result |= Require(
        g_clay_errors.count == 0u,
        "placed text layout should not emit Clay errors");

    ecs_fini(world);
    return result;
}

static int TestOverlappingRetainedTreesRouteTopmost(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action_a = CreateAction(world, "ActionA");
    ecs_entity_t action_b = CreateAction(world, "ActionB");
    ecs_entity_t root_a = EcsUiRootEntity(world, "TopmostRootA");
    EcsUiBuilder builder_a = EcsUiBuilderBegin(world, root_a);
    ecs_entity_t target_a = EcsUiAddCustom(
        &builder_a,
        (EcsUiCustomDesc){
            .id = "TargetA",
            .kind = "target",
            .preferred_width = 120.0f,
            .preferred_height = 60.0f,
            .on_click = action_a,
        });
    EcsUiBuilderEnd(&builder_a);
    result |= Require(EcsUiBuilderOk(&builder_a), "tree A builder failed");

    ecs_entity_t root_b = EcsUiRootEntity(world, "TopmostRootB");
    EcsUiBuilder builder_b = EcsUiBuilderBegin(world, root_b);
    ecs_entity_t target_b = EcsUiAddCustom(
        &builder_b,
        (EcsUiCustomDesc){
            .id = "TargetB",
            .kind = "target",
            .preferred_width = 120.0f,
            .preferred_height = 60.0f,
            .on_click = action_b,
        });
    EcsUiBuilderEnd(&builder_b);
    result |= Require(EcsUiBuilderOk(&builder_b), "tree B builder failed");

    EcsUiTreeSnapshot tree_a = {0};
    EcsUiTreeSnapshot tree_b = {0};
    result |= Require(EcsUiReadTree(world, root_a, &tree_a), "tree A read failed");
    result |= Require(EcsUiReadTree(world, root_b, &tree_b), "tree B read failed");

    EcsUiClayLayoutOptions options = {
        .bounds = {0.0f, 0.0f, 120.0f, 60.0f},
    };
    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayInteractionFrame frame = {0};
    EcsUiEventList events = {0};
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayPointerState press = {
        .x = 20.0f,
        .y = 20.0f,
        .time = 30.0,
        .down = true,
        .pressed = true,
    };

    EcsUiClayInteractionFrameBegin(&frame, &state);
    Clay_SetLayoutDimensions((Clay_Dimensions){.width = 120.0f, .height = 60.0f});
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree_a, &theme, &options, &frame);
    EcsUiClayEmitTreeEx(&tree_b, &theme, &options, &frame);
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){press.x, press.y}, press.down);
    EcsUiClayCollectFrameEvents(&frame, press, &events);

    result |= RequireEventCount(
        &events,
        3u,
        "topmost tree press should emit hover, pressed, and drag started");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_PRESSED, target_b, "TargetB");
    if (events.count > 1u && events.events[1].node == target_a) {
        result |= Require(false, "underlying tree should not receive press");
    }

    EcsUiClayPointerState release = {
        .x = 20.0f,
        .y = 20.0f,
        .time = 30.1,
        .released = true,
    };
    EcsUiClayInteractionFrameBegin(&frame, &state);
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree_a, &theme, &options, &frame);
    EcsUiClayEmitTreeEx(&tree_b, &theme, &options, &frame);
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){release.x, release.y}, release.down);
    EcsUiClayCollectFrameEvents(&frame, release, &events);

    result |= RequireEventCount(&events, 2u, "topmost tree release should click captured target");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_CLICKED, target_b, "TargetB");

    ecs_fini(world);
    return result;
}

static int TestLayoutOptionsCapturePointerBlocksEarlierTree(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "CaptureOptionAction");
    ecs_entity_t target_root = EcsUiRootEntity(world, "CaptureOptionTargetRoot");
    EcsUiBuilder target_builder = EcsUiBuilderBegin(world, target_root);
    ecs_entity_t target = EcsUiAddCustom(
        &target_builder,
        (EcsUiCustomDesc){
            .id = "CaptureOptionTarget",
            .kind = "target",
            .preferred_width = 160.0f,
            .preferred_height = 80.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&target_builder);
    result |= Require(
        EcsUiBuilderOk(&target_builder),
        "capture option target builder failed");

    ecs_entity_t blocker_root = EcsUiRootEntity(world, "CaptureOptionBlockerRoot");
    EcsUiTreeSnapshot target_tree = {0};
    EcsUiTreeSnapshot blocker_tree = {0};
    result |= Require(
        EcsUiReadTree(world, target_root, &target_tree),
        "capture option target tree read failed");
    result |= Require(
        EcsUiReadTree(world, blocker_root, &blocker_tree),
        "capture option blocker tree read failed");

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 240.0f);
    EcsUiClayLayoutOptions blocker_options = LayoutOptions(320.0f, 240.0f);
    blocker_options.z_index = 10;
    EcsUiClayPointerState press = {
        .x = 20.0f,
        .y = 20.0f,
        .time = 50.0,
        .down = true,
        .pressed = true,
    };

    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayInteractionFrame frame = {0};
    EcsUiEventList events = {0};
    EcsUiClayInteractionFrameBegin(&frame, &state);
    Clay_SetLayoutDimensions((Clay_Dimensions){.width = 320.0f, .height = 240.0f});
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&target_tree, &theme, &options, &frame);
    EcsUiClayEmitTreeEx(&blocker_tree, &theme, &blocker_options, &frame);
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){press.x, press.y}, press.down);
    EcsUiClayCollectFrameEvents(&frame, press, &events);
    result |= RequireEventCount(
        &events,
        3u,
        "default bounded emit should pass through pointer capture");
    result |= RequireEvent(
        &events,
        1u,
        ECS_UI_EVENT_PRESSED,
        target,
        "CaptureOptionTarget");

    state = (EcsUiClayInteractionState){0};
    EcsUiClayInteractionStateInit(&state);
    press.time = 51.0;
    blocker_options.capture_pointer = true;
    EcsUiClayInteractionFrameBegin(&frame, &state);
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&target_tree, &theme, &options, &frame);
    EcsUiClayEmitTreeEx(&blocker_tree, &theme, &blocker_options, &frame);
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){press.x, press.y}, press.down);
    EcsUiClayCollectFrameEvents(&frame, press, &events);
    result |= RequireEventCount(
        &events,
        0u,
        "capturing bounded emit should block earlier retained tree");

    ecs_fini(world);
    return result;
}

static int TestFloatingCaptureBlocksRetainedTree(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "CaptureBlockedAction");
    ecs_entity_t root = EcsUiRootEntity(world, "CaptureBlockedRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "BlockedTarget",
            .kind = "target",
            .preferred_width = 160.0f,
            .preferred_height = 80.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "capture blocked builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "capture blocked tree read failed");

    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayInteractionFrame frame = {0};
    EcsUiEventList events = {0};
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 240.0f);
    EcsUiClayPointerState pointer = {
        .x = 20.0f,
        .y = 20.0f,
        .time = 40.0,
        .down = true,
        .pressed = true,
    };

    EcsUiClayInteractionFrameBegin(&frame, &state);
    Clay_SetLayoutDimensions((Clay_Dimensions){.width = 320.0f, .height = 240.0f});
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, &frame);
    CLAY(CLAY_ID("CaptureLayer"), {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(320.0f),
                .height = CLAY_SIZING_FIXED(240.0f),
            },
        },
        .floating = {
            .zIndex = 10,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
    }) {}
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){pointer.x, pointer.y}, pointer.down);
    EcsUiClayCollectFrameEvents(&frame, pointer, &events);

    result |= RequireEventCount(
        &events,
        0u,
        "capturing floating root should block retained events underneath");
    result |= Require(
        !EcsUiClayInteractionFrameTreePointerInside(&frame, tree.root),
        "capturing floating root should block retained tree inside state");
    if (events.count > 0u && events.events[0].node == target) {
        result |= Require(false, "blocked target should not receive event");
    }

    ecs_fini(world);
    return result;
}

static int TestFloatingPassthroughAllowsRetainedTree(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create world");
    }

    ecs_entity_t action = CreateAction(world, "PassthroughAction");
    ecs_entity_t root = EcsUiRootEntity(world, "PassthroughRoot");
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t target = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PassthroughTarget",
            .kind = "target",
            .preferred_width = 160.0f,
            .preferred_height = 80.0f,
            .on_click = action,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "passthrough builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "passthrough tree read failed");

    EcsUiClayInteractionState state = {0};
    EcsUiClayInteractionStateInit(&state);
    EcsUiClayInteractionFrame frame = {0};
    EcsUiEventList events = {0};
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 240.0f);
    EcsUiClayPointerState pointer = {
        .x = 20.0f,
        .y = 20.0f,
        .time = 41.0,
    };

    EcsUiClayInteractionFrameBegin(&frame, &state);
    Clay_SetLayoutDimensions((Clay_Dimensions){.width = 320.0f, .height = 240.0f});
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options, &frame);
    CLAY(CLAY_ID("PassthroughLayer"), {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(320.0f),
                .height = CLAY_SIZING_FIXED(240.0f),
            },
        },
        .floating = {
            .zIndex = 10,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
    }) {}
    (void)Clay_EndLayout();
    Clay_SetPointerState((Clay_Vector2){pointer.x, pointer.y}, pointer.down);
    EcsUiClayCollectFrameEvents(&frame, pointer, &events);

    result |= RequireEventCount(
        &events,
        1u,
        "passthrough floating root should allow retained hover underneath");
    result |= RequireEvent(
        &events,
        0u,
        ECS_UI_EVENT_HOVERED,
        target,
        "PassthroughTarget");
    result |= Require(
        EcsUiClayInteractionFrameTreePointerInside(&frame, tree.root),
        "passthrough floating root should allow retained tree inside state");

    ecs_fini(world);
    return result;
}

int main(void)
{
    void *clay_memory = NULL;
    int result = InitializeClay(&clay_memory);
    if (result != 0) {
        return result;
    }

    result |= TestRootScaleAffectsClayLayoutOnly();
    result |= TestScaledPointerEventsAreLogical();
    result |= TestDuplicateAuthoredIdsDoNotCollide();
    result |= TestScrollViewEmitsScissorCommands();
    result |= TestTextStyleInheritanceEmitsClayForegroundColor();
    result |= TestIconEmitsCustomCommand();
    result |= TestVisualOpacitySkipsHitTesting();
    result |= TestRevealOnHoverRowActionsFallThrough();
    result |= TestVisualOffsetAffectsHitTesting();
    result |= TestActionStackEmitsPointerEvents();
    result |= TestActionPayloadFlowsToPointerEvents();
    result |= TestInteractiveStackEmitsSecondaryPress();
    result |= TestPointerCaptureLifecycle();
    result |= TestPointerCaptureMissingTargetFallsThrough();
    result |= TestOverlappingRetainedTreesRouteTopmost();
    result |= TestLayoutOptionsCapturePointerBlocksEarlierTree();
    result |= TestZStackCapturePreventsBackgroundFallthrough();
    result |= TestZStackBoxStyleEmitsBackgroundColor();
    result |= TestStackHoverBackgroundUsesCoreHoverState();
    result |= TestPressableHoverBackgroundUsesCoreHoverState();
    result |= TestPlainStacksEmitNoDefaultBackground();
    result |= TestStackStyleTokenEmitsBackgroundColor();
    result |= TestStackDirectStyleEmitsBackgroundAndBorder();
    result |= TestNineSliceStyleEmitsCustomBackground();
    result |= TestBoxStyleBorderEmitsBorderCommand();
    result |= TestStackSidePaddingAffectsClayLayout();
    result |= TestRaisedBevelEmitsLightTopLeftDarkBottomRightEdges();
    result |= TestSunkenBevelEmitsDarkTopLeftLightBottomRightEdges();
    result |= TestNoBevelEmitsNoBevelEdges();
    result |= TestGrowSizingFillsWardrobeShell();
    result |= TestZStackPlacementAnchorsHitBounds();
    result |= TestZStackPlacedTextRendersInRetainedBounds();
    result |= TestFloatingCaptureBlocksRetainedTree();
    result |= TestFloatingPassthroughAllowsRetainedTree();

    free(clay_memory);
    return result;
}
