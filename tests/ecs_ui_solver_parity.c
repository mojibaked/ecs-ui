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

static int BuildUnsupportedText(
    ecs_world_t *world,
    ecs_entity_t root)
{
    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "UnsupportedText",
            .text = "stage 5",
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "unsupported text builder failed");
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

static int BuildUnsupportedFit(
    ecs_world_t *world,
    ecs_entity_t root)
{
    ecs_set(
        world,
        root,
        EcsUiStack,
        {
            .axis = ECS_UI_AXIS_VERTICAL,
            .width_sizing = ECS_UI_SIZE_FIT,
        });

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "UnsupportedFitChild",
            .kind = "parity.unsupported",
            .preferred_width = 10.0f,
            .preferred_height = 10.0f,
        });
    EcsUiBuilderEnd(&builder);
    return Require(EcsUiBuilderOk(&builder), "unsupported fit builder failed");
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
        "unsupported_text",
        BuildUnsupportedText,
        "unsupported node kind 6 -- stage 5");
    result |= RunUnsupportedCase(
        errors,
        "unsupported_zstack",
        BuildUnsupportedZStack,
        "unsupported node kind 4 -- stage 6");
    result |= RunUnsupportedCase(
        errors,
        "unsupported_fit",
        BuildUnsupportedFit,
        "unsupported FIT sizing on node kind 1 -- stage 3");
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
