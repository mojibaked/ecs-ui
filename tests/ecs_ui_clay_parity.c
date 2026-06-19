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
    EcsUiClayTheme theme = EcsUiClayThemeDefault();
    EcsUiClayLayoutOptions options = LayoutOptions(320.0f, 220.0f);
    Clay_SetLayoutDimensions((Clay_Dimensions){
        .width = options.bounds.width,
        .height = options.bounds.height,
    });
    Clay_BeginLayout();
    EcsUiClayEmitTreeEx(&tree, &theme, &options);
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
    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 1.0,
        },
        &(EcsUiClayLayoutOptions){.bounds = {0.0f, 0.0f, 200.0f, 100.0f}},
        &events);
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
    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 1.1,
        },
        &(EcsUiClayLayoutOptions){.bounds = {0.0f, 0.0f, 200.0f, 100.0f}},
        &events);
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

    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 10.0f,
            .y = 70.0f,
            .time = 2.0,
        },
        &options,
        &events);
    result |= RequireEventCount(
        &events,
        0u,
        "offset target should not hit at its original x position");

    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 40.0f,
            .y = 70.0f,
            .time = 2.1,
        },
        &options,
        &events);
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

    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 10.0f,
            .y = 10.0f,
            .time = 10.0,
            .down = true,
            .pressed = true,
        },
        &options,
        &events);
    result |= RequireEventCount(
        &events,
        3u,
        "pressed frame should emit hover, pressed, and drag started");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_HOVERED, target, "DragTarget");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_PRESSED, target, "DragTarget");
    result |= RequireEvent(&events, 2u, ECS_UI_EVENT_DRAG_STARTED, target, "DragTarget");

    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 14.0f,
            .y = 13.0f,
            .time = 10.1,
            .down = true,
        },
        &options,
        &events);
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

    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 14.0f,
            .y = 13.0f,
            .time = 10.2,
            .released = true,
        },
        &options,
        &events);
    result |= RequireEventCount(
        &events,
        2u,
        "small release should emit drag ended and clicked");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_DRAG_ENDED, target, "DragTarget");
    result |= RequireEvent(&events, 1u, ECS_UI_EVENT_CLICKED, target, "DragTarget");

    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 180.0f,
            .y = 90.0f,
            .time = 10.3,
        },
        &options,
        &events);
    result |= RequireEventCount(
        &events,
        0u,
        "pointer capture should clear after release");

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
    EcsUiClayCollectEventsEx(
        &tree,
        (EcsUiClayPointerState){
            .x = 20.0f,
            .y = 20.0f,
            .time = 20.0,
        },
        &(EcsUiClayLayoutOptions){.bounds = {0.0f, 0.0f, 200.0f, 160.0f}},
        &events);
    result |= RequireEventCount(
        &events,
        1u,
        "capturing overlay should consume zstack hit test");
    result |= RequireEvent(&events, 0u, ECS_UI_EVENT_HOVERED, overlay, "Overlay");
    if (events.count > 0u && events.events[0].node == background) {
        result |= Require(false, "background should not receive zstack hit through overlay");
    }

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

    result |= TestDuplicateAuthoredIdsDoNotCollide();
    result |= TestVisualOpacitySkipsHitTesting();
    result |= TestVisualOffsetAffectsHitTesting();
    result |= TestPointerCaptureLifecycle();
    result |= TestZStackCapturePreventsBackgroundFallthrough();

    free(clay_memory);
    return result;
}
