#include "ecs_ui/ecs_ui.h"
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

typedef int (*BuildParityTreeFn)(ecs_world_t *world, ecs_entity_t root);

typedef struct ParityTreeCase {
    const char *name;
    BuildParityTreeFn build;
} ParityTreeCase;

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
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

static void ResetErrors(TestFrameErrors *errors)
{
    if (errors != NULL) {
        *errors = (TestFrameErrors){0};
    }
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
        .width = (float)safe_length * font_size * 0.5f + 3.0f,
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

static void AppendText(char *out, size_t out_size, const char *value)
{
    if (out == NULL || out_size == 0u || value == NULL) {
        return;
    }
    size_t length = strlen(out);
    if (length >= out_size) {
        return;
    }
    (void)snprintf(out + length, out_size - length, "%s", value);
}

static void BuildNodePath(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    char *out,
    size_t out_size)
{
    if (tree == NULL || index >= tree->count || out == NULL || out_size == 0u) {
        return;
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (node->parent_index != ECS_UI_TREE_INVALID_INDEX &&
        node->parent_index < tree->count) {
        BuildNodePath(tree, node->parent_index, out, out_size);
        AppendText(out, out_size, "/");
    }
    AppendText(out, out_size, node->id[0] != '\0' ? node->id : "<unnamed>");
}

static float AbsFloat(float value)
{
    return value < 0.0f ? -value : value;
}

static bool RectDiffers(
    const EcsUiTreeNodeSnapshot *reference,
    const EcsUiTreeNodeSnapshot *candidate,
    float epsilon)
{
    if (reference == NULL || candidate == NULL) {
        return true;
    }
    if (reference->has_layout != candidate->has_layout) {
        return true;
    }
    if (!reference->has_layout) {
        return false;
    }
    return AbsFloat(reference->layout_x - candidate->layout_x) > epsilon ||
        AbsFloat(reference->layout_y - candidate->layout_y) > epsilon ||
        AbsFloat(reference->layout_width - candidate->layout_width) > epsilon ||
        AbsFloat(reference->layout_height - candidate->layout_height) > epsilon;
}

static bool LayoutsDiverge(
    const EcsUiTreeSnapshot *reference,
    const EcsUiTreeSnapshot *candidate,
    const char *case_name,
    float scale,
    char *message,
    size_t message_size)
{
    if (message != NULL && message_size > 0u) {
        message[0] = '\0';
    }
    if (reference == NULL || candidate == NULL) {
        (void)snprintf(
            message,
            message_size,
            "%s scale %.1f: missing snapshot",
            case_name,
            scale);
        return true;
    }
    if (reference->count != candidate->count) {
        (void)snprintf(
            message,
            message_size,
            "%s scale %.1f: node count reference=%u candidate=%u",
            case_name,
            scale,
            (unsigned int)reference->count,
            (unsigned int)candidate->count);
        return true;
    }

    for (uint32_t i = 0u; i < reference->count; i += 1u) {
        const EcsUiTreeNodeSnapshot *reference_node = &reference->nodes[i];
        const EcsUiTreeNodeSnapshot *candidate_node = &candidate->nodes[i];
        if (reference_node->entity != candidate_node->entity ||
            strcmp(reference_node->id, candidate_node->id) != 0) {
            (void)snprintf(
                message,
                message_size,
                "%s scale %.1f: node identity mismatch at %u",
                case_name,
                scale,
                (unsigned int)i);
            return true;
        }
        if (!RectDiffers(reference_node, candidate_node, 0.001f)) {
            continue;
        }

        char path[256] = {0};
        BuildNodePath(reference, i, path, sizeof(path));
        (void)snprintf(
            message,
            message_size,
            "%s scale %.1f: divergent node path %s "
            "reference={%.3f %.3f %.3f %.3f} "
            "candidate={%.3f %.3f %.3f %.3f}",
            case_name,
            scale,
            path,
            reference_node->layout_x,
            reference_node->layout_y,
            reference_node->layout_width,
            reference_node->layout_height,
            candidate_node->layout_x,
            candidate_node->layout_y,
            candidate_node->layout_width,
            candidate_node->layout_height);
        return true;
    }
    return false;
}

static int RequireNoLayoutDiff(
    const EcsUiTreeSnapshot *reference,
    const EcsUiTreeSnapshot *candidate,
    const char *case_name,
    float scale)
{
    char message[512] = {0};
    if (!LayoutsDiverge(
            reference,
            candidate,
            case_name,
            scale,
            message,
            sizeof(message))) {
        return 0;
    }
    (void)fprintf(stderr, "%s\n", message);
    return 1;
}

static int RunFrameWithBackend(
    EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme,
    const EcsUiFrameLayoutOptions *options,
    EcsUiFrameInternalBackend backend,
    const char *message)
{
    EcsUiFrameInternalSelectBackend(backend);
    return Require(
        EcsUiFrameRun(tree, theme, options, NULL, NULL) != NULL,
        message);
}

static int BuildFixedVerticalPaddingGap(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 7.0f,
            .padding = 2.0f,
            .padding_left = 11.0f,
            .padding_top = 13.0f,
            .padding_right = 17.0f,
            .padding_bottom = 19.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FixedA",
            .kind = "parity.fixed",
            .preferred_width = 40.0f,
            .preferred_height = 20.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FixedB",
            .kind = "parity.fixed",
            .preferred_width = 64.0f,
            .preferred_height = 30.0f,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "fixed vertical builder failed");
}

static int BuildFractionalPaddingGap(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FractionalA",
            .kind = "parity.fractional",
            .preferred_width = 10.0f,
            .preferred_height = 10.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FractionalB",
            .kind = "parity.fractional",
            .preferred_width = 12.0f,
            .preferred_height = 10.0f,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "fractional builder failed");
}

static int BuildNestedHorizontalStack(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.0f,
            .padding = 5.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NestedRow",
            .gap = 9.0f,
            .padding_left = 4.0f,
            .padding_top = 6.0f,
            .padding_right = 8.0f,
            .padding_bottom = 10.0f,
            .preferred_width = 130.0f,
            .preferred_height = 46.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "NestedA",
            .kind = "parity.nested",
            .preferred_width = 20.0f,
            .preferred_height = 12.0f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "NestedIcon",
            .name = "dot",
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "NestedB",
            .kind = "parity.nested",
            .preferred_width = 30.0f,
            .preferred_height = 14.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "nested horizontal builder failed");
}

static int BuildEmittedDefaults(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 4.0f,
            .padding = 6.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "DefaultCustom",
            .kind = "parity.default",
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "DefaultIcon",
            .name = "mark",
        });
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "DefaultButton",
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "DefaultPressable",
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "emitted defaults builder failed");
}

static int BuildNestedOpacitySkip(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 5.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t parent = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OpacityParent",
            .preferred_width = 80.0f,
            .preferred_height = 50.0f,
        });
    ecs_entity_t child = EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "OpacityChild",
            .kind = "parity.opacity",
            .preferred_width = 20.0f,
            .preferred_height = 20.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    ecs_set(world, parent, EcsUiVisual, {.opacity = 0.1f});
    ecs_set(world, child, EcsUiVisual, {.opacity = 0.1f});
    return Require(EcsUiBuilderOk(&builder), "opacity builder failed");
}

static int BuildStackPreferredGrow(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 6.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PreferredGrowStack",
            .preferred_width = 80.0f,
            .preferred_height = 40.0f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "preferred grow builder failed");
}

static int BuildRootPreferredBelowViewport(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .preferred_width = 100.0f,
            .preferred_height = 80.0f,
            .padding = 5.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RootPreferredChild",
            .kind = "parity.root",
            .preferred_width = 20.0f,
            .preferred_height = 10.0f,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "root preferred builder failed");
}

static int BuildUnsupportedTextFieldView(
    ecs_world_t *world,
    ecs_entity_t root)
{
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t pressable = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "UnsupportedTextField",
        });
    ecs_entity_t value = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "UnsupportedTextFieldValue",
            .text = "field value",
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    ecs_set(
        world,
        pressable,
        EcsUiTextFieldView,
        {
            .value_node = value,
            .focused = true,
        });
    return Require(
        EcsUiBuilderOk(&builder),
        "unsupported text field builder failed");
}

static int BuildUnsupportedZStack(
    ecs_world_t *world,
    ecs_entity_t root)
{
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginZStack(
        &builder,
        (EcsUiStackDesc){
            .id = "UnsupportedZStack",
            .preferred_width = 40.0f,
            .preferred_height = 30.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "UnsupportedZChild",
            .kind = "parity.unsupported",
            .preferred_width = 10.0f,
            .preferred_height = 10.0f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "unsupported zstack builder failed");
}

static int BuildFitStackSizing(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 120.0f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitRowA",
            .kind = "parity.fit",
            .preferred_width = 20.5f,
            .preferred_height = 10.5f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "FitRowIcon",
            .name = "fit",
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitColumn",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_height = 120.0f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitColumnA",
            .kind = "parity.fit",
            .preferred_width = 18.5f,
            .preferred_height = 11.5f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitColumnB",
            .kind = "parity.fit",
            .preferred_width = 22.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EmptyFitRow",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EmptyFitColumn",
            .padding = 2.25f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "fit stack builder failed");
}

static int BuildFitInGrowAxes(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 4.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitGrowRow",
            .gap = 4.25f,
            .padding = 2.25f,
            .preferred_width = 170.5f,
            .preferred_height = 38.5f,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "RowFitChild",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RowFitContent",
            .kind = "parity.fit",
            .preferred_width = 24.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RowGrowLeft",
            .kind = "parity.grow",
            .preferred_height = 12.5f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RowGrowRight",
            .kind = "parity.grow",
            .preferred_height = 12.5f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);

    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitGrowColumn",
            .gap = 4.25f,
            .padding = 2.25f,
            .preferred_width = 80.5f,
            .preferred_height = 130.5f,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "ColumnFitChild",
            .padding = 2.25f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ColumnFitContent",
            .kind = "parity.fit",
            .preferred_width = 14.5f,
            .preferred_height = 24.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ColumnGrowTop",
            .kind = "parity.grow",
            .preferred_width = 14.5f,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ColumnGrowBottom",
            .kind = "parity.grow",
            .preferred_width = 14.5f,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "fit-in-grow builder failed");
}

static int BuildGrowWaterFillContent(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "WaterFillRow",
            .gap = 4.25f,
            .padding = 2.25f,
            .preferred_width = 210.5f,
            .preferred_height = 50.5f,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "WaterGrowSmall",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "WaterSmallContent",
            .kind = "parity.grow",
            .preferred_width = 20.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "WaterGrowLarge",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "WaterLargeContent",
            .kind = "parity.grow",
            .preferred_width = 62.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "water-fill builder failed");
}

static int BuildEpsilonWaterFill(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 0.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EpsilonRow",
            .preferred_width = 100.5f,
            .preferred_height = 32.5f,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EpsilonGrowA",
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "EpsilonContentA",
            .kind = "parity.epsilon",
            .preferred_width = 30.0f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "EpsilonGrowB",
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "EpsilonContentB",
            .kind = "parity.epsilon",
            .preferred_width = 30.006f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "epsilon water-fill builder failed");
}

static int BuildDescendingGrowWaterFill(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DescendingWaterRow",
            .gap = 4.25f,
            .padding = 2.25f,
            .preferred_width = 210.5f,
            .preferred_height = 50.5f,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DescendingGrowLarge",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "DescendingLargeContent",
            .kind = "parity.grow",
            .preferred_width = 62.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "DescendingGrowSmall",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "DescendingSmallContent",
            .kind = "parity.grow",
            .preferred_width = 20.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(
        EcsUiBuilderOk(&builder),
        "descending water-fill builder failed");
}

static int BuildDeepFitChains(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitOuter",
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitMiddle",
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitInner",
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitDeepLeafA",
            .kind = "parity.fit",
            .preferred_width = 17.5f,
            .preferred_height = 9.5f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitDeepLeafB",
            .kind = "parity.fit",
            .preferred_width = 21.5f,
            .preferred_height = 8.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "FitMiddleIcon",
            .name = "fit",
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "deep fit builder failed");
}

static int BuildFitInsideAutoHeight(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "AutoOuter",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 90.5f,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitInsideAuto",
            .gap = 3.25f,
            .padding = 2.25f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitInsideAutoA",
            .kind = "parity.fit",
            .preferred_width = 18.5f,
            .preferred_height = 11.5f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FitInsideAutoB",
            .kind = "parity.fit",
            .preferred_width = 16.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "fit inside auto builder failed");
}

static int BuildOverflowCompression(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CompressRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 70.5f,
            .preferred_height = 42.5f,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CompressFitLarge",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "CompressLargeContent",
            .kind = "parity.compress",
            .preferred_width = 58.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "CompressGrowSmall",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "CompressSmallContent",
            .kind = "parity.compress",
            .preferred_width = 28.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "compression builder failed");
}

static int BuildOffAxisFitClamp(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OffAxisParent",
            .padding = 2.25f,
            .preferred_width = 62.5f,
            .preferred_height = 60.5f,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OversizedFitChild",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "OversizedFitContent",
            .kind = "parity.fit",
            .preferred_width = 90.5f,
            .preferred_height = 12.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "off-axis fit builder failed");
}

static int BuildCustomFitNoop(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "CustomFitFixed",
            .kind = "parity.custom.fit",
            .preferred_width = 26.5f,
            .preferred_height = 13.5f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "CustomFitDefault",
            .kind = "parity.custom.fit",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "custom fit builder failed");
}

static int BuildRootFitContent(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RootFitA",
            .kind = "parity.root.fit",
            .preferred_width = 23.5f,
            .preferred_height = 12.5f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RootFitB",
            .kind = "parity.root.fit",
            .preferred_width = 31.5f,
            .preferred_height = 14.5f,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "root fit builder failed");
}

static int BuildRootContentExceedsViewport(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RootExceedsLarge",
            .kind = "parity.root.exceeds",
            .preferred_width = 320.5f,
            .preferred_height = 220.5f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "RootExceedsTail",
            .kind = "parity.root.exceeds",
            .preferred_width = 280.25f,
            .preferred_height = 40.25f,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "root exceeds builder failed");
}

static int BuildLargeArenaGrowth(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 1.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    for (uint32_t i = 0u; i < 150u; i += 1u) {
        char id[ECS_UI_ID_MAX] = {0};
        (void)snprintf(id, sizeof(id), "LargeArena%03u", (unsigned int)i);
        (void)EcsUiAddCustom(
            &builder,
            (EcsUiCustomDesc){
                .id = id,
                .kind = "parity.large",
                .preferred_width = 18.5f + (float)(i % 11u) * 0.25f,
                .preferred_height = 4.5f + (float)(i % 7u) * 0.25f,
            });
    }
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "large arena builder failed");
}

static void AddAlignCustom(
    EcsUiBuilder *builder,
    const char *id,
    float width,
    float height)
{
    (void)EcsUiAddCustom(
        builder,
        (EcsUiCustomDesc){
            .id = id,
            .kind = "parity.align",
            .preferred_width = width,
            .preferred_height = height,
        });
}

static int BuildVStackAlignmentMatrix(
    ecs_world_t *world,
    ecs_entity_t root)
{
    static const EcsUiAlign aligns[] = {
        ECS_UI_ALIGN_START,
        ECS_UI_ALIGN_CENTER,
        ECS_UI_ALIGN_END,
    };

    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 2.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    for (uint32_t y = 0u; y < 3u; y += 1u) {
        for (uint32_t x = 0u; x < 3u; x += 1u) {
            char id[ECS_UI_ID_MAX] = {0};
            char child_a[ECS_UI_ID_MAX] = {0};
            char child_b[ECS_UI_ID_MAX] = {0};
            (void)snprintf(
                id,
                sizeof(id),
                "VAlign%u%u",
                (unsigned int)x,
                (unsigned int)y);
            (void)snprintf(
                child_a,
                sizeof(child_a),
                "VAlign%u%uA",
                (unsigned int)x,
                (unsigned int)y);
            (void)snprintf(
                child_b,
                sizeof(child_b),
                "VAlign%u%uB",
                (unsigned int)x,
                (unsigned int)y);
            (void)EcsUiBeginVStack(
                &builder,
                (EcsUiStackDesc){
                    .id = id,
                    .gap = 3.25f,
                    .padding = 2.25f,
                    .preferred_width = 92.5f,
                    .preferred_height = 76.5f,
                    .align_x = aligns[x],
                    .align_y = aligns[y],
                });
            AddAlignCustom(&builder, child_a, 20.5f, 10.5f);
            AddAlignCustom(&builder, child_b, 24.5f, 12.5f);
            EcsUiEnd(&builder);
        }
    }
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "vstack alignment builder failed");
}

static int BuildHStackAlignmentMatrix(
    ecs_world_t *world,
    ecs_entity_t root)
{
    static const EcsUiAlign aligns[] = {
        ECS_UI_ALIGN_START,
        ECS_UI_ALIGN_CENTER,
        ECS_UI_ALIGN_END,
    };

    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 2.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    for (uint32_t y = 0u; y < 3u; y += 1u) {
        for (uint32_t x = 0u; x < 3u; x += 1u) {
            char id[ECS_UI_ID_MAX] = {0};
            char child_a[ECS_UI_ID_MAX] = {0};
            char child_b[ECS_UI_ID_MAX] = {0};
            (void)snprintf(
                id,
                sizeof(id),
                "HAlign%u%u",
                (unsigned int)x,
                (unsigned int)y);
            (void)snprintf(
                child_a,
                sizeof(child_a),
                "HAlign%u%uA",
                (unsigned int)x,
                (unsigned int)y);
            (void)snprintf(
                child_b,
                sizeof(child_b),
                "HAlign%u%uB",
                (unsigned int)x,
                (unsigned int)y);
            (void)EcsUiBeginHStack(
                &builder,
                (EcsUiStackDesc){
                    .id = id,
                    .gap = 3.25f,
                    .padding = 2.25f,
                    .preferred_width = 94.5f,
                    .preferred_height = 72.5f,
                    .align_x = aligns[x],
                    .align_y = aligns[y],
                });
            AddAlignCustom(&builder, child_a, 20.5f, 10.5f);
            AddAlignCustom(&builder, child_b, 24.5f, 12.5f);
            EcsUiEnd(&builder);
        }
    }
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "hstack alignment builder failed");
}

static int BuildCenterOddFractionalAlignment(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OddCenterRow",
            .padding = 2.25f,
            .preferred_width = 101.5f,
            .preferred_height = 37.5f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    AddAlignCustom(&builder, "OddCenterRowChild", 20.5f, 11.5f);
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OddCenterColumn",
            .padding = 2.25f,
            .preferred_width = 79.5f,
            .preferred_height = 100.5f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    AddAlignCustom(&builder, "OddCenterColumnChild", 21.5f, 19.5f);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "odd center builder failed");
}

static int BuildAlignmentWithGrowSiblings(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "GrowAlignRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 168.5f,
            .preferred_height = 44.5f,
            .align_x = ECS_UI_ALIGN_END,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    AddAlignCustom(&builder, "GrowAlignRowFixed", 20.5f, 10.5f);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowAlignRowGrow",
            .kind = "parity.align",
            .preferred_height = 12.5f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "GrowAlignColumn",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 74.5f,
            .preferred_height = 136.5f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_END,
        });
    AddAlignCustom(&builder, "GrowAlignColumnFixed", 18.5f, 12.5f);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowAlignColumnGrow",
            .kind = "parity.align",
            .preferred_width = 16.5f,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "grow alignment builder failed");
}

static int BuildFitAlignmentInterplay(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FixedBoxForFitChild",
            .padding = 2.25f,
            .preferred_width = 92.5f,
            .preferred_height = 70.5f,
            .align_x = ECS_UI_ALIGN_END,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FitChildInFixedBox",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    AddAlignCustom(&builder, "FitChildInFixedBoxLeaf", 20.5f, 11.5f);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TightFitParent",
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .align_x = ECS_UI_ALIGN_END,
            .align_y = ECS_UI_ALIGN_END,
        });
    AddAlignCustom(&builder, "TightFitA", 18.5f, 9.5f);
    AddAlignCustom(&builder, "TightFitB", 16.5f, 12.5f);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "fit alignment builder failed");
}

static int BuildMainAxisOverflowAlignment(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OverflowCenterRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 52.5f,
            .preferred_height = 34.5f,
            .align_x = ECS_UI_ALIGN_CENTER,
        });
    AddAlignCustom(&builder, "OverflowCenterRowA", 40.5f, 10.5f);
    AddAlignCustom(&builder, "OverflowCenterRowB", 32.5f, 11.5f);
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OverflowEndRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 52.5f,
            .preferred_height = 34.5f,
            .align_x = ECS_UI_ALIGN_END,
        });
    AddAlignCustom(&builder, "OverflowEndRowA", 40.5f, 10.5f);
    AddAlignCustom(&builder, "OverflowEndRowB", 32.5f, 11.5f);
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OverflowCenterColumn",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 80.5f,
            .preferred_height = 42.5f,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    AddAlignCustom(&builder, "OverflowCenterColumnA", 20.5f, 30.5f);
    AddAlignCustom(&builder, "OverflowCenterColumnB", 22.5f, 24.5f);
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "OverflowEndColumn",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 80.5f,
            .preferred_height = 42.5f,
            .align_y = ECS_UI_ALIGN_END,
        });
    AddAlignCustom(&builder, "OverflowEndColumnA", 20.5f, 30.5f);
    AddAlignCustom(&builder, "OverflowEndColumnB", 22.5f, 24.5f);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "overflow alignment builder failed");
}

static int BuildOffAxisNegativeWhitespaceAlignment(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeXCenter",
            .padding = 2.25f,
            .preferred_width = 62.5f,
            .preferred_height = 50.5f,
            .align_x = ECS_UI_ALIGN_CENTER,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeXCenterChild",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    AddAlignCustom(&builder, "NegativeXCenterContent", 90.5f, 12.5f);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeXEnd",
            .padding = 2.25f,
            .preferred_width = 62.5f,
            .preferred_height = 50.5f,
            .align_x = ECS_UI_ALIGN_END,
        });
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeXEndChild",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });
    AddAlignCustom(&builder, "NegativeXEndContent", 90.5f, 12.5f);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeYCenter",
            .padding = 2.25f,
            .preferred_width = 110.5f,
            .preferred_height = 40.5f,
            .align_y = ECS_UI_ALIGN_CENTER,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeYCenterChild",
            .padding = 2.25f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    AddAlignCustom(&builder, "NegativeYCenterContent", 20.5f, 60.5f);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeYEnd",
            .padding = 2.25f,
            .preferred_width = 110.5f,
            .preferred_height = 40.5f,
            .align_y = ECS_UI_ALIGN_END,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "NegativeYEndChild",
            .padding = 2.25f,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    AddAlignCustom(&builder, "NegativeYEndContent", 20.5f, 60.5f);
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(
        EcsUiBuilderOk(&builder),
        "negative whitespace alignment builder failed");
}

static int BuildButtonPressableIconAlignment(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "CenteredButtonIcon",
            .preferred_width = 82.5f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "CenteredButtonIconGlyph",
            .name = "button-icon",
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "CenteredPressableIcon",
            .preferred_height = 50.5f,
        });
    (void)EcsUiAddIcon(
        &builder,
        (EcsUiIconDesc){
            .id = "CenteredPressableIconGlyph",
            .name = "pressable-icon",
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "button pressable icon builder failed");
}

static int BuildHorizontalRootAlignmentGrow(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_HORIZONTAL,
            .gap = 3.25f,
            .padding = 2.25f,
            .align_x = ECS_UI_ALIGN_END,
            .align_y = ECS_UI_ALIGN_CENTER,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    AddAlignCustom(&builder, "HorizontalRootFixed", 28.5f, 14.5f);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "HorizontalRootGrow",
            .kind = "parity.align",
            .preferred_height = 20.5f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "horizontal root builder failed");
}

static int BuildDepthRootMapsVertical(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_DEPTH,
            .gap = 3.25f,
            .padding = 2.25f,
            .align_x = ECS_UI_ALIGN_CENTER,
            .align_y = ECS_UI_ALIGN_END,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    AddAlignCustom(&builder, "DepthRootA", 24.5f, 12.5f);
    AddAlignCustom(&builder, "DepthRootB", 18.5f, 10.5f);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "depth root builder failed");
}

static int BuildTextVStackBasic(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextBasicColumn",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 180.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextSingleWord",
            .text = "single",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextMultiWord",
            .text = "two words here",
            .role = ECS_UI_TEXT_LABEL,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "text basic builder failed");
}

static int BuildTextFitWidthChain(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextFitOuter",
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextFitMiddle",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextFitMeasuredWords",
            .text = "alpha beta gamma",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "TextFitSidecar",
            .kind = "parity.text.fit",
            .preferred_width = 11.5f,
            .preferred_height = 9.5f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "text fit builder failed");
}

static int BuildTextFractionalFontSize(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FractionalFontFitOuter",
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "FractionalFontFitInner",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    ecs_entity_t text = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "FractionalFontText",
            .text = "fractional type",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "FractionalFontSidecar",
            .kind = "parity.text.fractional",
            .preferred_width = 8.5f,
            .preferred_height = 8.5f,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    ecs_set(
        world,
        text,
        EcsUiTextStyle,
        {
            .body_size = 13.3f,
        });
    return Require(EcsUiBuilderOk(&builder), "fractional text builder failed");
}

static int BuildTextStyleInheritanceRoles(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t styled = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "StyledTextAncestor",
            .gap = 2.25f,
            .padding = 2.25f,
            .preferred_width = 210.5f,
            .preferred_height = 74.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "InheritedBodyText",
            .text = "inherited body size",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DefaultTitleText",
            .text = "title",
            .role = ECS_UI_TEXT_TITLE,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DefaultBodyText",
            .text = "body",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DefaultLabelText",
            .text = "label",
            .role = ECS_UI_TEXT_LABEL,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DefaultButtonText",
            .text = "button",
            .role = ECS_UI_TEXT_BUTTON,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "DefaultCaptionText",
            .text = "caption",
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiBuilderEnd(&builder);
    ecs_set(
        world,
        styled,
        EcsUiTextStyle,
        {
            .body_size = 22.5f,
        });
    return Require(EcsUiBuilderOk(&builder), "text style builder failed");
}

static int BuildTextLayoutAlignY(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextLayoutBox",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 190.5f,
            .preferred_height = 92.5f,
        });
    ecs_entity_t start = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextAlignYStart",
            .text = "top aligned inner text",
            .role = ECS_UI_TEXT_BODY,
        });
    ecs_entity_t end = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextAlignYEnd",
            .text = "bottom aligned inner text",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    ecs_set(
        world,
        start,
        EcsUiTextLayout,
        {
            .align_y = ECS_UI_ALIGN_START,
        });
    ecs_set(
        world,
        end,
        EcsUiTextLayout,
        {
            .align_y = ECS_UI_ALIGN_END,
        });
    return Require(EcsUiBuilderOk(&builder), "text layout builder failed");
}

static int BuildTextCompressionDropout(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextCompressDropoutRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 220.5f,
            .preferred_height = 48.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextCompressLong",
            .text = "longestword aa aa aa",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextCompressMedium",
            .text = "mediumword bb",
            .role = ECS_UI_TEXT_BODY,
        });
    /*
     * This FIT stack starts at its minDimensions floor. Keeping it in Clay's
     * resizable buffer changes the free/active_count STEP SIZE before any
     * dropout, but in Stage 5 scope that is endpoint-invariant (at-min
     * children absorb nothing, and compression conserves total shrinkage
     * among the movable children), so this golden does NOT pin buffer
     * membership or divisor semantics. Likewise an order-independent
     * widthToAdd recompute has not produced a distinct endpoint because
     * every non-text resizable sibling already starts at its min floor;
     * later stages with additional min<size node kinds should add a
     * dedicated order-sensitive golden.
     */
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextCompressAtMinStack",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "TextCompressAtMinLeaf",
            .kind = "parity.text.compress",
            .preferred_width = 36.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextCompressFitStack",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "TextCompressFitLeaf",
            .kind = "parity.text.compress",
            .preferred_width = 21.5f,
            .preferred_height = 10.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "text compression builder failed");
}

static int BuildTextCompressionBetween(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextCompressBetweenRow",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 245.5f,
            .preferred_height = 48.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextCompressBetweenLargest",
            .text = "largestword aa aa",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextCompressBetweenSecond",
            .text = "secondword bb",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "text between builder failed");
}

static int BuildTextVerticalCompressionFixedHeight(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextVerticalCompressColumn",
            .gap = 3.25f,
            .padding = 2.25f,
            .preferred_width = 180.5f,
            .preferred_height = 42.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextVerticalCompressA",
            .text = "fixed height one",
            .role = ECS_UI_TEXT_TITLE,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextVerticalCompressB",
            .text = "fixed height two",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "text vertical builder failed");
}

static int BuildTextNewlineWrapper(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "NewlineText",
            .text = "line one\nline two",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "newline text builder failed");
}

static int BuildButtonTextInterplay(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "TextButton",
            .preferred_width = 132.5f,
            .preferred_height = 50.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextButtonLabel",
            .text = "button words",
            .role = ECS_UI_TEXT_BUTTON,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "button text builder failed");
}

static int BuildRootGrowTextOverflowViewport(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "RootGrowWideTitle",
            .text = "wide words that exceed viewport width easily",
            .role = ECS_UI_TEXT_TITLE,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "root grow text builder failed");
}

static int BuildRootFitTextOverflowViewport(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "RootFitWideTitle",
            .text = "wide words that exceed viewport width easily",
            .role = ECS_UI_TEXT_TITLE,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "root fit text builder failed");
}

static int BuildRootFitTextNoOverflow(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "RootFitShortText",
            .text = "short",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "root fit short text builder failed");
}

static int BuildTextPreferredWalkLocalStyle(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t styled = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PreferredWalkStyledAncestor",
            .padding = 2.25f,
            .preferred_width = 160.5f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "PreferredWalkInheritedText",
            .text = "inherits only when emitted",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    ecs_set(
        world,
        styled,
        EcsUiTextStyle,
        {
            .body_size = 34.5f,
        });
    return Require(
        EcsUiBuilderOk(&builder),
        "preferred walk local style builder failed");
}

static int BuildTextMeasurementEdgeCases(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 3.25f,
            .padding = 2.25f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextConsecutiveSpacesRow",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextConsecutiveSpaces",
            .text = "alpha  beta   gamma",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextLeadingTrailingSpacesRow",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextLeadingTrailingSpaces",
            .text = "  padded text  ",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextSingleSpaceRow",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextSingleSpace",
            .text = " ",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "TextEmptyRow",
            .padding = 2.25f,
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "TextEmpty",
            .text = "",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "text edge builder failed");
}

static int BuildVerticalGrowDistribution(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .gap = 5.0f,
            .padding = 10.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowTop",
            .kind = "parity.grow",
            .preferred_width = 30.0f,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowBottom",
            .kind = "parity.grow",
            .preferred_width = 40.0f,
            .height_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "vertical grow builder failed");
}

static int BuildHorizontalMixedGrow(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 5.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "GrowRow",
            .gap = 5.0f,
            .padding = 10.0f,
            .preferred_width = 180.0f,
            .preferred_height = 50.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowFixed",
            .kind = "parity.grow",
            .preferred_width = 40.0f,
            .preferred_height = 20.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowLeft",
            .kind = "parity.grow",
            .preferred_height = 20.0f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "GrowRight",
            .kind = "parity.grow",
            .preferred_height = 20.0f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "horizontal grow builder failed");
}

static int BuildGrowZeroMinOverflow(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .padding = 4.0f,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "ZeroMinRow",
            .gap = 5.0f,
            .padding = 10.0f,
            .preferred_width = 70.0f,
            .preferred_height = 40.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ZeroMinFixed",
            .kind = "parity.grow",
            .preferred_width = 60.0f,
            .preferred_height = 20.0f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "ZeroMinGrow",
            .kind = "parity.grow",
            .preferred_height = 20.0f,
            .width_sizing = ECS_UI_SIZE_GROW,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "zero-min grow builder failed");
}

static EcsUiFrameLayoutOptions LayoutOptions(float scale)
{
    return (EcsUiFrameLayoutOptions){
        .physical_bounds = {
            .x = 17.0f,
            .y = 23.0f,
            .width = 240.0f * scale,
            .height = 180.0f * scale,
        },
    };
}

static int RunParityCase(const ParityTreeCase *parity_case, float scale)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create solver parity world");
    }

    char root_id[ECS_UI_ID_MAX] = {0};
    (void)snprintf(root_id, sizeof(root_id), "%sRoot", parity_case->name);
    ecs_entity_t root = EcsUiRootEntity(world, root_id);
    result |= Require(root != 0, "failed to create solver parity root");
    result |= Require(
        EcsUiSetScale(world, root, scale),
        "failed to set solver parity scale");
    result |= parity_case->build(world, root);

    EcsUiTreeSnapshot clay_tree = {0};
    EcsUiTreeSnapshot native_tree = {0};
    EcsUiTreeSnapshot diverge_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &clay_tree),
        "failed to read clay parity snapshot");
    result |= Require(
        EcsUiReadTree(world, root, &native_tree),
        "failed to read native parity snapshot");
    result |= Require(
        EcsUiReadTree(world, root, &diverge_tree),
        "failed to read divergent parity snapshot");
    if (result != 0) {
        EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
        ecs_fini(world);
        return result;
    }

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = LayoutOptions(scale);
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.x + options.physical_bounds.width + 40.0f,
        options.physical_bounds.y + options.physical_bounds.height + 40.0f);

    result |= RunFrameWithBackend(
        &clay_tree,
        &theme,
        &options,
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        "clay parity frame failed");
    result |= RunFrameWithBackend(
        &diverge_tree,
        &theme,
        &options,
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DIVERGE,
        "divergent solver frame failed");

    char divergence[512] = {0};
    const bool detected = LayoutsDiverge(
        &clay_tree,
        &diverge_tree,
        parity_case->name,
        scale,
        divergence,
        sizeof(divergence));
    result |= Require(detected, "solver scoreboard did not detect stub divergence");
    result |= Require(
        strstr(divergence, "divergent node path") != NULL,
        "solver scoreboard did not report a divergent node path");

    result |= RunFrameWithBackend(
        &native_tree,
        &theme,
        &options,
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE,
        "native solver frame failed");
    result |= RequireNoLayoutDiff(
        &clay_tree,
        &native_tree,
        parity_case->name,
        scale);

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int TestDeepDivergenceProof(void)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create deep divergence world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "DeepDivergenceRoot");
    result |= Require(root != 0, "failed to create deep divergence root");
    result |= Require(
        EcsUiSetScale(world, root, 1.0f),
        "failed to set deep divergence scale");
    result |= BuildNestedHorizontalStack(world, root);

    EcsUiTreeSnapshot clay_tree = {0};
    EcsUiTreeSnapshot deep_tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &clay_tree),
        "failed to read deep clay snapshot");
    result |= Require(
        EcsUiReadTree(world, root, &deep_tree),
        "failed to read deep divergent snapshot");
    if (result != 0) {
        EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
        ecs_fini(world);
        return result;
    }

    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = LayoutOptions(1.0f);
    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.x + options.physical_bounds.width + 40.0f,
        options.physical_bounds.y + options.physical_bounds.height + 40.0f);

    result |= RunFrameWithBackend(
        &clay_tree,
        &theme,
        &options,
        ECS_UI_FRAME_INTERNAL_BACKEND_CLAY,
        "deep clay frame failed");
    result |= RunFrameWithBackend(
        &deep_tree,
        &theme,
        &options,
        ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE_DEEP_DIVERGE,
        "deep divergent frame failed");

    char divergence[512] = {0};
    const bool detected = LayoutsDiverge(
        &clay_tree,
        &deep_tree,
        "deep_divergence",
        1.0f,
        divergence,
        sizeof(divergence));
    result |= Require(detected, "deep divergence was not detected");
    result |= Require(
        strstr(divergence, "NestedRow/NestedA") != NULL,
        "deep divergence did not name the nested node path");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int RunUnsupportedCase(
    TestFrameErrors *errors,
    const char *name,
    BuildParityTreeFn build,
    const char *expected_message)
{
    int result = 0;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create unsupported world");
    }

    char root_id[ECS_UI_ID_MAX] = {0};
    (void)snprintf(root_id, sizeof(root_id), "%sRoot", name);
    ecs_entity_t root = EcsUiRootEntity(world, root_id);
    result |= Require(root != 0, "failed to create unsupported root");
    result |= build(world, root);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "failed to read unsupported snapshot");
    if (result != 0) {
        EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
        ecs_fini(world);
        return result;
    }

    ResetErrors(errors);
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = LayoutOptions(1.0f);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    const EcsUiDrawList *draw_list =
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL);
    result |= Require(draw_list == NULL, "unsupported native run should fail");
    result |= Require(errors != NULL && errors->count > 0u, "unsupported error missing");
    result |= Require(
        errors != NULL && errors->last_kind == ECS_UI_FRAME_ERROR_INTERNAL,
        "unsupported error kind mismatch");
    result |= Require(
        errors != NULL && strstr(errors->last_message, expected_message) != NULL,
        "unsupported error message mismatch");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int TestUnsupportedStageFailures(TestFrameErrors *errors)
{
    int result = 0;
    result |= RunUnsupportedCase(
        errors,
        "unsupported_text_field_view",
        BuildUnsupportedTextFieldView,
        "unsupported text-field view on node kind 9 -- stage 6");
    result |= RunUnsupportedCase(
        errors,
        "unsupported_zstack",
        BuildUnsupportedZStack,
        "unsupported node kind 4 -- stage 6");
    return result;
}

int main(void)
{
    int result = 0;
    TestFrameErrors errors = {0};
    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 640.0f,
                .surface_height = 480.0f,
                .measure_text = TestMeasureText,
                .error = TestFrameHandleError,
                .error_user_data = &errors,
            }),
        "failed to initialize solver parity backend");
    if (result != 0) {
        return result;
    }

    EcsUiFrameBackendSetCullingEnabled(false);
    const ParityTreeCase cases[] = {
        {
            .name = "fixed_vertical_padding_gap",
            .build = BuildFixedVerticalPaddingGap,
        },
        {
            .name = "fractional_padding_gap_u16",
            .build = BuildFractionalPaddingGap,
        },
        {
            .name = "nested_horizontal_stack",
            .build = BuildNestedHorizontalStack,
        },
        {
            .name = "emitted_defaults",
            .build = BuildEmittedDefaults,
        },
        {
            .name = "nested_opacity_skip",
            .build = BuildNestedOpacitySkip,
        },
        {
            .name = "stack_preferred_grow",
            .build = BuildStackPreferredGrow,
        },
        {
            .name = "root_preferred_below_viewport",
            .build = BuildRootPreferredBelowViewport,
        },
        {
            .name = "fit_stack_sizing",
            .build = BuildFitStackSizing,
        },
        {
            .name = "fit_in_grow_axes",
            .build = BuildFitInGrowAxes,
        },
        {
            .name = "grow_water_fill_content",
            .build = BuildGrowWaterFillContent,
        },
        {
            .name = "epsilon_water_fill",
            .build = BuildEpsilonWaterFill,
        },
        {
            .name = "descending_grow_water_fill",
            .build = BuildDescendingGrowWaterFill,
        },
        {
            .name = "deep_fit_chains",
            .build = BuildDeepFitChains,
        },
        {
            .name = "fit_inside_auto_height",
            .build = BuildFitInsideAutoHeight,
        },
        {
            .name = "overflow_compression",
            .build = BuildOverflowCompression,
        },
        {
            .name = "off_axis_fit_clamp",
            .build = BuildOffAxisFitClamp,
        },
        {
            .name = "custom_fit_noop",
            .build = BuildCustomFitNoop,
        },
        {
            .name = "root_fit_content",
            .build = BuildRootFitContent,
        },
        {
            .name = "root_content_exceeds_viewport",
            .build = BuildRootContentExceedsViewport,
        },
        {
            .name = "large_arena_growth",
            .build = BuildLargeArenaGrowth,
        },
        {
            .name = "vstack_alignment_matrix",
            .build = BuildVStackAlignmentMatrix,
        },
        {
            .name = "hstack_alignment_matrix",
            .build = BuildHStackAlignmentMatrix,
        },
        {
            .name = "center_odd_fractional_alignment",
            .build = BuildCenterOddFractionalAlignment,
        },
        {
            .name = "alignment_with_grow_siblings",
            .build = BuildAlignmentWithGrowSiblings,
        },
        {
            .name = "fit_alignment_interplay",
            .build = BuildFitAlignmentInterplay,
        },
        {
            .name = "main_axis_overflow_alignment",
            .build = BuildMainAxisOverflowAlignment,
        },
        {
            .name = "off_axis_negative_whitespace_alignment",
            .build = BuildOffAxisNegativeWhitespaceAlignment,
        },
        {
            .name = "button_pressable_icon_alignment",
            .build = BuildButtonPressableIconAlignment,
        },
        {
            .name = "horizontal_root_alignment_grow",
            .build = BuildHorizontalRootAlignmentGrow,
        },
        {
            .name = "depth_root_maps_vertical",
            .build = BuildDepthRootMapsVertical,
        },
        {
            .name = "text_vstack_basic",
            .build = BuildTextVStackBasic,
        },
        {
            .name = "text_fit_width_chain",
            .build = BuildTextFitWidthChain,
        },
        {
            .name = "text_fractional_font_size",
            .build = BuildTextFractionalFontSize,
        },
        {
            .name = "text_style_inheritance_roles",
            .build = BuildTextStyleInheritanceRoles,
        },
        {
            .name = "text_layout_align_y",
            .build = BuildTextLayoutAlignY,
        },
        {
            .name = "text_compression_dropout",
            .build = BuildTextCompressionDropout,
        },
        {
            .name = "text_compression_between",
            .build = BuildTextCompressionBetween,
        },
        {
            .name = "text_vertical_compression_fixed_height",
            .build = BuildTextVerticalCompressionFixedHeight,
        },
        {
            .name = "text_newline_wrapper",
            .build = BuildTextNewlineWrapper,
        },
        {
            .name = "button_text_interplay",
            .build = BuildButtonTextInterplay,
        },
        {
            .name = "root_grow_text_overflow_viewport",
            .build = BuildRootGrowTextOverflowViewport,
        },
        {
            .name = "root_fit_text_overflow_viewport",
            .build = BuildRootFitTextOverflowViewport,
        },
        {
            .name = "root_fit_text_no_overflow",
            .build = BuildRootFitTextNoOverflow,
        },
        {
            .name = "text_preferred_walk_local_style",
            .build = BuildTextPreferredWalkLocalStyle,
        },
        {
            .name = "text_measurement_edge_cases",
            .build = BuildTextMeasurementEdgeCases,
        },
        {
            .name = "vertical_grow_distribution",
            .build = BuildVerticalGrowDistribution,
        },
        {
            .name = "horizontal_mixed_fixed_grow",
            .build = BuildHorizontalMixedGrow,
        },
        {
            .name = "grow_zero_min_overflow",
            .build = BuildGrowZeroMinOverflow,
        },
    };
    const float scales[] = {1.0f, 2.0f};

    for (uint32_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i += 1u) {
        for (uint32_t scale_index = 0u;
             scale_index < sizeof(scales) / sizeof(scales[0]);
             scale_index += 1u) {
            result |= RunParityCase(&cases[i], scales[scale_index]);
        }
    }
    result |= TestDeepDivergenceProof();
    result |= TestUnsupportedStageFailures(&errors);

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    EcsUiFrameBackendShutdown();
    return result;
}
