#include "ecs_ui/ecs_ui_frame.h"
#include "ecs_ui/ecs_ui_paint.h"
#include "../src/ecs_ui_frame_internal.h"
#include "../src/ecs_ui_paint_internal.h"

#include <stdio.h>
#include <string.h>

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

typedef struct TestFrameErrors {
    uint32_t count;
    EcsUiFrameErrorKind last_kind;
    char last_message[256];
} TestFrameErrors;

static void CopyString(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0u) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, dst_size, "%s", src);
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

static int RequireColor(
    EcsUiColorF actual,
    EcsUiColorF expected,
    const char *message)
{
    int result = 0;
    result |= RequireNear(actual.r, expected.r, 0.001f, message);
    result |= RequireNear(actual.g, expected.g, 0.001f, message);
    result |= RequireNear(actual.b, expected.b, 0.001f, message);
    result |= RequireNear(actual.a, expected.a, 0.001f, message);
    return result;
}

static EcsUiColorF TestColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (EcsUiColorF){
        .r = (float)r,
        .g = (float)g,
        .b = (float)b,
        .a = (float)a,
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

static const EcsUiTreeNodeSnapshot *FindNode(
    const EcsUiTreeSnapshot *tree,
    const char *id)
{
    return EcsUiTreeSnapshotFindNodeById(tree, id);
}

static int BuildPaintTree(
    ecs_world_t *world,
    ecs_entity_t root,
    float scale)
{
    int result = 0;
    result |= Require(EcsUiSetScale(world, root, scale), "failed to set paint scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t fit = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintFit",
            .width_sizing = ECS_UI_SIZE_FIT,
            .height_sizing = ECS_UI_SIZE_FIT,
            .gap = 2.5f,
            .padding = 1.25f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PaintCustom",
            .kind = "paint.custom",
            .preferred_width = 20.5f,
            .preferred_height = 12.25f,
        });
    ecs_entity_t pressable = EcsUiBeginPressable(
        &builder,
        (EcsUiPressableDesc){
            .id = "PaintPress",
            .preferred_height = 18.5f,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    (void)EcsUiBeginButton(
        &builder,
        (EcsUiButtonDesc){
            .id = "PaintButton",
            .variant = ECS_UI_BUTTON_PRIMARY,
            .preferred_width = 19.5f,
            .preferred_height = 13.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintNoFill",
            .preferred_width = 8.5f,
            .preferred_height = 6.5f,
        });
    EcsUiEnd(&builder);
    ecs_entity_t transparent_group = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "PaintTransparentGroup",
            .preferred_width = 13.25f,
            .preferred_height = 7.25f,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PaintTransparentChild",
            .kind = "paint.transparent",
            .preferred_width = 9.5f,
            .preferred_height = 6.5f,
        });
    EcsUiEnd(&builder);
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "PaintAfter",
            .kind = "paint.after",
            .preferred_width = 11.5f,
            .preferred_height = 9.5f,
        });
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "paint builder failed");

    ecs_set(world, fit, EcsUiBoxStyle, {
        .background = {10u, 20u, 30u, 255u},
    });
    ecs_set(world, fit, EcsUiVisual, {
        .opacity = 0.75f,
    });
    ecs_set(world, pressable, EcsUiBoxStyle, {
        .background = {40u, 50u, 60u, 200u},
    });
    ecs_set(world, transparent_group, EcsUiBoxStyle, {
        .background = {70u, 80u, 90u, 255u},
    });
    ecs_set(world, transparent_group, EcsUiVisual, {
        .opacity = 0.005f,
    });
    return result;
}

static int RequirePaintItem(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *node_id,
    EcsUiColorF expected_fill,
    float expected_opacity)
{
    int result = 0;
    result |= Require(paint != NULL, "paint list missing");
    result |= Require(tree != NULL, "paint tree missing");
    if (paint == NULL || tree == NULL || index >= paint->count) {
        return result | Require(false, "paint item index missing");
    }

    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    char message[256] = {0};
    (void)snprintf(
        message,
        sizeof(message),
        "paint node missing: %s",
        node_id);
    result |= Require(node != NULL, message);
    if (node == NULL) {
        return result;
    }

    const EcsUiPaintItem *item = &paint->items[index];
    (void)snprintf(
        message,
        sizeof(message),
        "paint source mismatch for %s",
        node_id);
    result |= Require(item->key.source == node->entity, message);
    result |= Require(
        item->key.role == ECS_UI_PAINT_ROLE_BOX,
        "paint role should be box");
    result |= Require(item->key.part == 0u, "paint part should be zero");
    result |= Require(
        item->key.generation == paint->generation,
        "paint key generation should match list");
    result |= Require(
        item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX,
        "paint primitive should be box");
    result |= Require(item->order == index, "paint order should match index");
    result |= Require(!item->clip.enabled, "7.2 paint clip should be disabled");
    result |= RequireNear(
        item->rect.x,
        node->layout_x,
        0.001f,
        "paint rect x should copy snapshot layout");
    result |= RequireNear(
        item->rect.y,
        node->layout_y,
        0.001f,
        "paint rect y should copy snapshot layout");
    result |= RequireNear(
        item->rect.width,
        node->layout_width,
        0.001f,
        "paint rect width should copy snapshot layout");
    result |= RequireNear(
        item->rect.height,
        node->layout_height,
        0.001f,
        "paint rect height should copy snapshot layout");
    result |= RequireColor(
        item->payload.box.fill,
        expected_fill,
        "paint box fill mismatch");
    result |= RequireNear(
        item->opacity,
        expected_opacity,
        0.001f,
        "paint opacity mismatch");
    return result;
}

static int RequireNoPaintSource(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const char *node_id)
{
    int result = 0;
    const EcsUiTreeNodeSnapshot *node = FindNode(tree, node_id);
    char message[256] = {0};
    (void)snprintf(
        message,
        sizeof(message),
        "paint node missing for absent check: %s",
        node_id);
    result |= Require(node != NULL, message);
    if (paint == NULL || node == NULL) {
        return result;
    }
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        if (paint->items[i].key.source == node->entity) {
            (void)snprintf(
                message,
                sizeof(message),
                "paint item should be absent for %s",
                node_id);
            result |= Require(false, message);
        }
    }
    return result;
}

static int RequirePaintList(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const EcsUiTheme *theme)
{
    int result = 0;
    result |= Require(paint != NULL, "paint list missing");
    result |= Require(tree != NULL, "paint source tree missing");
    result |= Require(theme != NULL, "paint theme missing");
    if (paint == NULL || tree == NULL || theme == NULL) {
        return result;
    }

    result |= Require(!paint->truncated, "paint list should not truncate");
    result |= Require(paint->tree == tree->root, "paint tree id mismatch");
    result |= Require(
        paint->generation == tree->generation,
        "paint generation should match snapshot");
    result |= Require(paint->count == 6u, "paint list should contain six boxes");
    result |= RequirePaintItem(
        paint,
        tree,
        0u,
        "PaintRoot",
        TestColor(
            theme->root_background.r,
            theme->root_background.g,
            theme->root_background.b,
            theme->root_background.a),
        1.0f);
    result |= RequirePaintItem(
        paint,
        tree,
        1u,
        "PaintFit",
        TestColor(10u, 20u, 30u, 255u),
        0.75f);
    result |= RequirePaintItem(
        paint,
        tree,
        2u,
        "PaintCustom",
        TestColor(
            theme->surface_subtle.r,
            theme->surface_subtle.g,
            theme->surface_subtle.b,
            theme->surface_subtle.a),
        0.75f);
    result |= RequirePaintItem(
        paint,
        tree,
        3u,
        "PaintPress",
        TestColor(40u, 50u, 60u, 200u),
        0.75f);
    result |= RequirePaintItem(
        paint,
        tree,
        4u,
        "PaintButton",
        TestColor(
            theme->button_primary.r,
            theme->button_primary.g,
            theme->button_primary.b,
            theme->button_primary.a),
        1.0f);
    result |= RequirePaintItem(
        paint,
        tree,
        5u,
        "PaintAfter",
        TestColor(
            theme->surface_subtle.r,
            theme->surface_subtle.g,
            theme->surface_subtle.b,
            theme->surface_subtle.a),
        1.0f);
    result |= RequireNoPaintSource(paint, tree, "PaintNoFill");
    result |= RequireNoPaintSource(paint, tree, "PaintTransparentGroup");
    result |= RequireNoPaintSource(paint, tree, "PaintTransparentChild");
    return result;
}

static int RunPaintCase(
    EcsUiFrameInternalBackend backend,
    float scale,
    TestFrameErrors *errors)
{
    int result = 0;
    const uint32_t start_error_count = errors != NULL ? errors->count : 0u;
    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create paint world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintRoot");
    result |= Require(root != 0, "paint root missing");
    result |= BuildPaintTree(world, root, scale);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "paint tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f * scale,
            .height = 160.0f * scale,
        },
    };

    EcsUiFrameBackendSetSurfaceSize(
        options.physical_bounds.width,
        options.physical_bounds.height);
    EcsUiFrameInternalSelectBackend(backend);
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "paint frame run failed");
    const EcsUiPaintList *paint = EcsUiFrameInternalPaintList();
    const uint32_t first_generation = paint != NULL ? paint->generation : 0u;
    result |= RequirePaintList(paint, &tree, &theme);
    result |= Require(
        EcsUiFrameApply(world, NULL),
        "paint artifact apply failed");
    const EcsUiFrameArtifacts *artifacts =
        ecs_singleton_get(world, EcsUiFrameArtifacts);
    result |= Require(artifacts != NULL, "paint artifacts singleton missing");
    if (artifacts != NULL) {
        result |= Require(
            artifacts->paint == paint,
            "paint artifacts should point at backend list");
        result |= Require(
            artifacts->generation == first_generation,
            "paint artifacts generation mismatch");
    }

    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "second paint frame run failed");
    paint = EcsUiFrameInternalPaintList();
    result |= Require(
        paint != NULL && paint->generation == first_generation + 1u,
        "paint generation should increment per frame run");
    result |= Require(
        tree.generation == first_generation + 1u,
        "snapshot generation should increment per frame run");
    result |= RequirePaintList(paint, &tree, &theme);
    result |= Require(
        errors == NULL || errors->count == start_error_count,
        "paint case emitted unexpected frame errors");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int TestSourceTruncationPropagates(void)
{
    EcsUiTreeSnapshot tree = {
        .root = 101u,
        .scale = 1.0f,
        .generation = 77u,
        .count = 1u,
        .truncated = true,
    };
    tree.nodes[0] = (EcsUiTreeNodeSnapshot){
        .entity = 101u,
        .parent = 0u,
        .kind = ECS_UI_NODE_ROOT,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .visual = {.opacity = 1.0f},
        .layout_width = 10.0f,
        .layout_height = 10.0f,
        .has_layout = true,
    };

    static EcsUiPaintList paint;
    memset(&paint, 0, sizeof(paint));
    EcsUiTheme theme = EcsUiThemeDefault();
    int result = 0;
    result |= Require(
        EcsUiPaintListBuild(&paint, &tree, &theme),
        "source-truncated paint build should fit paint capacity");
    result |= Require(
        paint.truncated,
        "source tree truncation should propagate to paint list");
    result |= Require(paint.count == 1u, "source-truncated paint count mismatch");
    return result;
}

static int TestOpacityCullDirect(void)
{
    EcsUiTreeSnapshot tree = {
        .root = 202u,
        .scale = 1.0f,
        .generation = 88u,
        .count = 1u,
    };
    tree.nodes[0] = (EcsUiTreeNodeSnapshot){
        .entity = 202u,
        .parent = 0u,
        .kind = ECS_UI_NODE_VSTACK,
        .first_child = ECS_UI_TREE_INVALID_INDEX,
        .next_sibling = ECS_UI_TREE_INVALID_INDEX,
        .box_style = {
            .background = {10u, 20u, 30u, 255u},
        },
        .visual = {.opacity = 0.005f},
        .layout_width = 10.0f,
        .layout_height = 10.0f,
        .has_box_style = true,
        .has_layout = true,
    };

    static EcsUiPaintList paint;
    memset(&paint, 0, sizeof(paint));
    EcsUiTheme theme = EcsUiThemeDefault();
    int result = 0;
    result |= Require(
        EcsUiPaintListBuild(&paint, &tree, &theme),
        "direct opacity-cull paint build should fit paint capacity");
    result |= Require(
        paint.count == 0u,
        "direct opacity-cull paint build should skip low-opacity item");
    result |= Require(
        !paint.truncated,
        "direct opacity-cull paint build should not truncate");
    return result;
}

static int TestGenerationHeldOnSolverFailure(TestFrameErrors *errors)
{
    int result = 0;
    if (errors == NULL) {
        return Require(false, "missing paint error recorder");
    }

    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create generation world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintRoot");
    result |= Require(root != 0, "paint failure root missing");
    result |= BuildPaintTree(world, root, 1.0f);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "paint failure tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f,
            .height = 160.0f,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(240.0f, 160.0f);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "last-good native paint run failed");
    result |= RequirePaintList(EcsUiFrameInternalPaintList(), &tree, &theme);
    result |= Require(EcsUiFrameApply(world, NULL), "last-good apply failed");

    const EcsUiPaintList *last_good = EcsUiFrameInternalPaintList();
    const uint32_t last_generation =
        last_good != NULL ? last_good->generation : 0u;
    const EcsUiFrameArtifacts *artifacts =
        ecs_singleton_get(world, EcsUiFrameArtifacts);
    result |= Require(
        artifacts != NULL && artifacts->paint == last_good,
        "last-good artifact missing before failure");

    EcsUiTreeSnapshot failed_tree = tree;
    failed_tree.count = 0u;
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&failed_tree, &theme, &options, NULL, NULL) == NULL,
        "native solver failure should abort frame run");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_INTERNAL,
        "native solver failure should report an internal frame error");
    result |= Require(
        failed_tree.generation == last_generation,
        "failed run should keep snapshot generation at last good value");
    result |= Require(
        EcsUiFrameInternalPaintList() == last_good,
        "failed run should keep last-good paint list active");
    result |= Require(
        EcsUiFrameApply(world, NULL),
        "failed-run artifact apply should keep last-good handle");
    artifacts = ecs_singleton_get(world, EcsUiFrameArtifacts);
    result |= Require(
        artifacts != NULL && artifacts->paint == last_good,
        "artifact should still point at last-good paint list after failure");
    result |= Require(
        artifacts != NULL && artifacts->generation == last_generation &&
            last_good->generation == last_generation,
        "artifact generation should stay at last good value after failure");

    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    ecs_fini(world);
    return result;
}

static int TestPaintCapacityFailure(TestFrameErrors *errors)
{
    int result = 0;
    if (errors == NULL) {
        return Require(false, "missing paint capacity error recorder");
    }

    ecs_world_t *world = CreateWorld();
    if (world == NULL) {
        return Require(false, "failed to create paint capacity world");
    }

    ecs_entity_t root = EcsUiRootEntity(world, "PaintRoot");
    result |= Require(root != 0, "paint capacity root missing");
    result |= BuildPaintTree(world, root, 1.0f);

    EcsUiTreeSnapshot tree = {0};
    result |= Require(
        EcsUiReadTree(world, root, &tree),
        "paint capacity tree read failed");
    EcsUiTheme theme = EcsUiThemeDefault();
    EcsUiFrameLayoutOptions options = {
        .physical_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 240.0f,
            .height = 160.0f,
        },
    };
    EcsUiFrameBackendSetSurfaceSize(240.0f, 160.0f);
    EcsUiFrameInternalSelectBackend(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY);
    EcsUiFrameInternalSetPaintItemCapacity(0u);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "paint capacity last-good run failed");
    const EcsUiPaintList *last_good = EcsUiFrameInternalPaintList();
    const uint32_t last_generation =
        last_good != NULL ? last_good->generation : 0u;
    result |= RequirePaintList(last_good, &tree, &theme);

    static EcsUiPaintList local;
    memset(&local, 0, sizeof(local));
    result |= Require(
        !EcsUiPaintListBuildWithCapacity(&local, &tree, &theme, 2u),
        "direct paint build should fail a tiny item capacity");
    result |= Require(
        local.truncated,
        "direct paint build should mark list truncated on item overflow");

    EcsUiFrameInternalSetPaintItemCapacity(2u);
    *errors = (TestFrameErrors){0};
    result |= Require(
        EcsUiFrameRun(&tree, &theme, &options, NULL, NULL) != NULL,
        "paint capacity overflow should keep frame draw list alive");
    result |= Require(
        errors->count == 1u &&
            errors->last_kind == ECS_UI_FRAME_ERROR_ELEMENT_CAPACITY,
        "paint capacity overflow should report element capacity error");
    result |= Require(
        EcsUiFrameInternalPaintList() == last_good,
        "paint capacity overflow should keep last-good paint list active");
    result |= Require(
        tree.generation == last_generation,
        "paint capacity overflow should restore last-good snapshot generation");
    result |= Require(
        last_good != NULL && !last_good->truncated,
        "active last-good paint list should not expose failed overflow scratch");

    EcsUiFrameInternalSetPaintItemCapacity(0u);
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
        "failed to initialize paint frame backend");
    EcsUiFrameBackendSetCullingEnabled(false);

    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY, 1.0f, &errors);
    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_CLAY, 2.0f, &errors);
    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE, 1.0f, &errors);
    result |= RunPaintCase(ECS_UI_FRAME_INTERNAL_BACKEND_NATIVE, 2.0f, &errors);
    result |= TestSourceTruncationPropagates();
    result |= TestOpacityCullDirect();
    result |= TestGenerationHeldOnSolverFailure(&errors);
    result |= TestPaintCapacityFailure(&errors);

    EcsUiFrameBackendShutdown();
    return result;
}
