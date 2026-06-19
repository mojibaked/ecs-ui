#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_animation.h"
#include "ecs_ui/ecs_ui_navigation.h"
#include "ecs_ui/ecs_ui_projection.h"
#include "ecs_ui/ecs_ui_text_input.h"

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

static int RequireNode(
    const EcsUiTreeSnapshot *tree,
    uint32_t index,
    const char *id,
    EcsUiNodeKind kind)
{
    if (tree == NULL || index >= tree->count) {
        return Require(false, "node index out of range");
    }

    const EcsUiTreeNodeSnapshot *node = &tree->nodes[index];
    if (strcmp(node->id, id) != 0 || node->kind != kind) {
        (void)fprintf(
            stderr,
            "unexpected node at %u: id=%s kind=%d\n",
            index,
            node->id,
            (int)node->kind);
        return 1;
    }
    return 0;
}

typedef struct TestProjectionItem {
    uint64_t key;
    char label[ECS_UI_TEXT_MAX];
} TestProjectionItem;

typedef struct TestProjectionContext {
    ecs_entity_t ui_parent;
    ecs_entity_t label_slot;
    uint32_t build_count;
    uint32_t update_count;
} TestProjectionContext;

ECS_COMPONENT_DECLARE(TestProjectionItem);

static void TestProjectionSyncSource(
    ecs_world_t *world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx)
{
    (void)ctx;

    const TestProjectionItem *data =
        item != NULL ? item->data : NULL;
    if (data != NULL) {
        ecs_set_ptr(world, source, TestProjectionItem, data);
    }
}

static ecs_entity_t TestProjectionBuildRoot(
    ecs_world_t *world,
    ecs_entity_t source,
    const EcsUiProjectionCollectionSource *item,
    void *ctx)
{
    TestProjectionContext *projection = ctx;
    const TestProjectionItem *data =
        item != NULL ? item->data : NULL;
    if (projection == NULL || data == NULL) {
        return 0;
    }

    char row_id[ECS_UI_ID_MAX] = {0};
    char label_id[ECS_UI_ID_MAX] = {0};
    (void)snprintf(
        row_id,
        sizeof(row_id),
        "CollectionRow%llu",
        (unsigned long long)data->key);
    (void)snprintf(
        label_id,
        sizeof(label_id),
        "CollectionLabel%llu",
        (unsigned long long)data->key);

    EcsUiBuilder builder =
        EcsUiBuilderBegin(world, projection->ui_parent);
    ecs_entity_t row = EcsUiBeginHStack(
        &builder,
        (EcsUiStackDesc){
            .id = row_id,
        });
    ecs_entity_t label = EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = label_id,
            .text = data->label,
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);

    if (EcsUiBuilderOk(&builder) && row != 0 && label != 0) {
        (void)EcsUiProjectionSetNode(
            world,
            source,
            projection->label_slot,
            label);
        projection->build_count += 1u;
        return row;
    }
    return 0;
}

static void TestProjectionUpdateRoot(
    ecs_world_t *world,
    ecs_entity_t source,
    ecs_entity_t ui_root,
    const EcsUiProjectionCollectionSource *item,
    uint32_t position,
    uint32_t count,
    void *ctx)
{
    (void)ui_root;
    (void)count;

    TestProjectionContext *projection = ctx;
    const TestProjectionItem *data =
        item != NULL ? item->data : NULL;
    if (projection == NULL || data == NULL) {
        return;
    }

    ecs_entity_t label =
        EcsUiProjectionGetNode(world, source, projection->label_slot);
    EcsUiText *text =
        label != 0 ? ecs_get_mut(world, label, EcsUiText) : NULL;
    if (text != NULL) {
        (void)snprintf(
            text->text,
            sizeof(text->text),
            "%u:%.200s",
            position,
            data->label);
        text->role = ECS_UI_TEXT_BODY;
        ecs_modified(world, label, EcsUiText);
    }
    projection->update_count += 1u;
}

int main(void)
{
    ecs_world_t *world = ecs_init();
    if (world == NULL) {
        return 1;
    }

    EcsUiImport(world);
    EcsUiAnimationImport(world);
    EcsUiNavigationImport(world);
    EcsUiProjectionImport(world);
    EcsUiTextInputImport(world);
    ECS_COMPONENT_DEFINE(world, TestProjectionItem);
    int result = 0;
    result |= Require(
        ecs_has_id(world, EcsUiOnClick, EcsExclusive),
        "EcsUiOnClick should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiUsesStyle, EcsExclusive),
        "EcsUiUsesStyle should be exclusive");
    result |= Require(EcsUiTheme != 0, "EcsUiTheme should be registered");
    result |= Require(
        ecs_has_id(world, EcsUiActiveTheme, EcsExclusive),
        "EcsUiActiveTheme should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiThemeStyle, EcsExclusive),
        "EcsUiThemeStyle should be exclusive");
    result |= Require(
        ecs_id(EcsUiPressable) != 0,
        "EcsUiPressable should be registered");
    result |= Require(
        ecs_id(EcsUiBoxStyle) != 0,
        "EcsUiBoxStyle should be registered");
    result |= Require(
        ecs_id(EcsUiTextStyle) != 0,
        "EcsUiTextStyle should be registered");
    ecs_entity_t style_token_root = EcsUiStyleTokenRoot(world);
    ecs_entity_t text_field_style_token =
        EcsUiStyleToken(world, "TextField");
    ecs_entity_t primary_action_style_token =
        EcsUiStyleToken(world, "PrimaryAction");
    result |= Require(
        style_token_root != 0,
        "style token root should be created");
    result |= Require(
        EcsUiStyleTokenRoot(world) == style_token_root,
        "style token root should be stable");
    result |= Require(
        text_field_style_token != 0 &&
            EcsUiStyleToken(world, "TextField") == text_field_style_token,
        "style token identity should be stable");
    result |= Require(
        primary_action_style_token != 0 &&
            primary_action_style_token != text_field_style_token,
        "style token names should create distinct entities");
    result |= Require(
        ecs_has_pair(
            world,
            text_field_style_token,
            EcsChildOf,
            style_token_root),
        "style token should live under style token root");
    ecs_entity_t styled_entity =
        ecs_entity(world, {.name = "StyleTokenSetterTarget"});
    result |= Require(
        EcsUiSetStyleToken(world, styled_entity, text_field_style_token),
        "style token setter should attach token");
    result |= Require(
        ecs_get_target(world, styled_entity, EcsUiUsesStyle, 0) ==
            text_field_style_token,
        "style token setter should use EcsUiUsesStyle");
    result |= Require(
        ecs_id(EcsUiHitTest) != 0,
        "EcsUiHitTest should be registered");
    result |= Require(
        ecs_has_id(world, EcsUiProjectionRoot, EcsExclusive),
        "EcsUiProjectionRoot should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiProjectionSource, EcsExclusive),
        "EcsUiProjectionSource should be exclusive");
    result |= Require(
        ecs_id(EcsUiProjectionKey) != 0,
        "EcsUiProjectionKey should be registered");
    result |= Require(
        ecs_id(EcsUiAnimatedFloat) != 0,
        "EcsUiAnimatedFloat should be registered");
    result |= Require(
        ecs_id(EcsUiLinear1f) != 0,
        "EcsUiLinear1f should be registered");
    result |= Require(
        ecs_has_id(world, EcsUiPresentationRoute, EcsExclusive),
        "EcsUiPresentationRoute should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiPresentationUiNode, EcsExclusive),
        "EcsUiPresentationUiNode should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiActivePresentation, EcsExclusive),
        "EcsUiActivePresentation should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiPresentRouteRequest, EcsExclusive),
        "EcsUiPresentRouteRequest should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiFocusedTextField, EcsExclusive),
        "EcsUiFocusedTextField should be exclusive");
    result |= Require(
        ecs_id(EcsUiTextEditState) != 0,
        "EcsUiTextEditState should be registered");
    result |= Require(
        ecs_id(EcsUiTextPasteRequest) != 0 &&
            ecs_id(EcsUiTextClipboardWriteRequest) != 0,
        "text clipboard request components should be registered");
    result |= Require(
        ecs_has_id(world, EcsUiTextFieldUiNode, EcsExclusive),
        "EcsUiTextFieldUiNode should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiTextFieldValueUiNode, EcsExclusive),
        "EcsUiTextFieldValueUiNode should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiForTextField, EcsExclusive),
        "EcsUiForTextField should be exclusive");
    result |= Require(
        ecs_has_id(world, EcsUiFocusTextFieldRequest, EcsExclusive),
        "EcsUiFocusTextFieldRequest should be exclusive");
    result |= Require(
        EcsUiFocusNextTextFieldRequest != 0 &&
            EcsUiFocusPreviousTextFieldRequest != 0,
        "text traversal request tags should be registered");
    result |= Require(
        EcsUiTextCursorLeftRequest != 0 &&
            EcsUiTextCursorRightRequest != 0 &&
            EcsUiTextCursorStartRequest != 0 &&
            EcsUiTextCursorEndRequest != 0,
        "text cursor request tags should be registered");
    result |= Require(
        EcsUiTextSelectLeftRequest != 0 &&
            EcsUiTextSelectRightRequest != 0 &&
            EcsUiTextSelectStartRequest != 0 &&
            EcsUiTextSelectEndRequest != 0,
        "text selection request tags should be registered");
    result |= Require(
        EcsUiTextCopyRequest != 0 && EcsUiTextCutRequest != 0,
        "text clipboard request tags should be registered");

    ecs_entity_t animation_target =
        ecs_entity(world, {.name = "AnimationTarget"});
    EcsUiAnimationStartLinear1f(world, animation_target, 0.0f, 1.0f, 1.0f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        0.0f,
        0.0001f,
        "animation start value mismatch");
    (void)ecs_progress(world, 0.25f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        0.25f,
        0.0001f,
        "animation quarter value mismatch");
    result |= Require(
        ecs_get(world, animation_target, EcsUiLinear1f) != NULL,
        "animation should still have linear component");
    result |= Require(
        !ecs_has_id(world, animation_target, EcsUiAnimationComplete),
        "animation should not complete early");
    (void)ecs_progress(world, 1.0f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        1.0f,
        0.0001f,
        "animation final value mismatch");
    result |= Require(
        ecs_get(world, animation_target, EcsUiLinear1f) == NULL,
        "animation should remove linear component on completion");
    result |= Require(
        ecs_has_id(world, animation_target, EcsUiAnimationComplete),
        "animation should add completion tag");
    EcsUiAnimationSetValue(world, animation_target, 1.5f);
    result |= RequireNear(
        EcsUiAnimationValue(world, animation_target, -1.0f),
        1.0f,
        0.0001f,
        "manual animation value should clamp");
    result |= Require(
        !ecs_has_id(world, animation_target, EcsUiAnimationComplete),
        "manual animation value should clear completion tag");

    ecs_entity_t visual_target =
        ecs_entity(world, {.name = "AnimationVisualTarget"});
    EcsUiAnimationApplyFadeSlideY(world, visual_target, 0.25f, 80.0f);
    const EcsUiVisual *visual =
        ecs_get(world, visual_target, EcsUiVisual);
    result |= Require(
        visual != NULL,
        "fade slide visual should be set");
    if (visual != NULL) {
        result |= RequireNear(
            visual->opacity,
            0.25f,
            0.0001f,
            "fade slide opacity mismatch");
        result |= RequireNear(
            visual->offset_y,
            60.0f,
            0.0001f,
            "fade slide offset mismatch");
    }
    EcsUiAnimationApplyHighlight(world, visual_target, 0.5f);
    visual = ecs_get(world, visual_target, EcsUiVisual);
    result |= Require(
        visual != NULL,
        "highlight visual should be set");
    if (visual != NULL) {
        result |= RequireNear(
            visual->opacity,
            1.0f,
            0.0001f,
            "highlight opacity mismatch");
        result |= RequireNear(
            visual->highlight,
            0.5f,
            0.0001f,
            "highlight value mismatch");
    }

    ecs_entity_t nav_root = EcsUiNavRoot(world);
    ecs_entity_t route = EcsUiNavRoute(world, "TestRoute");
    result |= Require(nav_root != 0, "navigation root should be created");
    result |= Require(route != 0, "navigation route should be created");
    result |= Require(
        EcsUiNavIsRoute(world, route),
        "navigation route should be tagged");
    result |= Require(
        EcsUiNavRequestPresentRoute(world, 0) == 0,
        "invalid route should not create present request");
    ecs_entity_t present_request =
        EcsUiNavRequestPresentRoute(world, route);
    result |= Require(
        present_request != 0 &&
            ecs_get_target(
                world,
                present_request,
                EcsUiPresentRouteRequest,
                0) == route,
        "present request should target route");
    ecs_entity_t dismiss_request =
        EcsUiNavRequestDismissPresentation(world);
    result |= Require(
        dismiss_request != 0 &&
            ecs_has_id(world, dismiss_request, EcsUiDismissPresentationRequest),
        "dismiss request should be tagged");

    ecs_entity_t presentation =
        EcsUiNavCreatePresentation(world, route);
    result |= Require(
        presentation != 0 &&
            ecs_get_parent(world, presentation) == nav_root,
        "presentation should be child of nav root");
    result |= Require(
        EcsUiNavPresentationRoute(world, presentation) == route,
        "presentation should target route");
    result |= Require(
        EcsUiNavActivePresentation(world) == presentation,
        "presentation should become active");
    ecs_entity_t presentation_node =
        ecs_entity(world, {.name = "PresentationNode"});
    result |= Require(
        EcsUiNavSetPresentationUiNode(
            world,
            presentation,
            presentation_node),
        "presentation UI node link should be set");
    result |= Require(
        EcsUiNavPresentationUiNode(world, presentation) == presentation_node,
        "presentation UI node should round trip");
    result |= Require(
        !EcsUiNavSetActivePresentation(world, presentation_node),
        "non-presentation entity should not become active");
    result |= Require(
        EcsUiNavActivePresentation(world) == presentation,
        "invalid active setter should not replace active presentation");
    result |= Require(
        !EcsUiNavClearActivePresentation(world, presentation_node),
        "clearing non-active presentation should fail");
    result |= Require(
        EcsUiNavActivePresentation(world) == presentation,
        "non-active clear should not clear active presentation");
    result |= Require(
        EcsUiNavClearActivePresentation(world, presentation),
        "active presentation should clear");
    result |= Require(
        EcsUiNavActivePresentation(world) == 0,
        "active presentation should be empty after clear");
    ecs_defer_begin(world);
    ecs_entity_t staged_presentation =
        EcsUiNavCreatePresentation(world, route);
    ecs_defer_end(world);
    result |= Require(
        staged_presentation != 0 &&
            EcsUiNavActivePresentation(world) == staged_presentation,
        "deferred presentation should become active after merge");
    result |= Require(
        EcsUiNavPresentationRoute(world, staged_presentation) == route,
        "deferred presentation should keep route after merge");
    result |= Require(
        EcsUiNavClearActivePresentation(world, staged_presentation),
        "deferred active presentation should clear");

    ecs_entity_t text_field_a =
        EcsUiTextInputField(world, "TextFieldA", "first");
    ecs_entity_t text_field_b =
        EcsUiTextInputField(world, "TextFieldB", "second");
    result |= Require(
        text_field_a != 0 && text_field_b != 0,
        "text fields should be created");
    result |= Require(
        ecs_get(world, text_field_a, EcsUiTextEditState) != NULL &&
            EcsUiTextInputCursor(world, text_field_a) == 0u &&
            !EcsUiTextInputHasSelection(world, text_field_a),
        "text field edit state should be created");
    result |= Require(
        strcmp(EcsUiTextInputPlaceholder(world, text_field_a), "first") == 0,
        "text field placeholder mismatch");
    result |= Require(
        EcsUiTextInputRequestFocusField(world, text_field_a) != 0,
        "focus request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_a &&
            EcsUiTextInputIsFocused(world, text_field_a),
        "text field should be focused");
    result |= Require(
        EcsUiTextInputRequestInsert(world, 'h') != 0,
        "insert request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputRequestInsert(world, 'i') != 0,
        "second insert request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "hi") == 0,
        "text field insert mismatch");
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 2u,
        "text field cursor should advance after insert");
    result |= Require(
        EcsUiTextInputRequestMoveCursorLeft(world) != 0,
        "cursor left request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 1u,
        "cursor should move left");
    char text_display[ECS_UI_TEXT_MAX] = {0};
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_a,
            true,
            text_display,
            sizeof(text_display)),
        "text field cursor display should be created");
    result |= Require(
        strcmp(text_display, "h|i") == 0,
        "text field cursor display mismatch");
    result |= Require(
        EcsUiTextInputRequestInsert(world, '!') != 0,
        "middle insert request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "h!i") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 2u,
        "middle insert should use cursor");
    result |= Require(
        EcsUiTextInputRequestDelete(world) != 0,
        "middle delete request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "hi") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 1u,
        "delete should remove before cursor");
    result |= Require(
        EcsUiTextInputRequestMoveCursorStart(world) != 0,
        "cursor start request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 0u,
        "cursor should move to start");
    result |= Require(
        EcsUiTextInputRequestMoveCursorRight(world) != 0,
        "cursor right request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 1u,
        "cursor should move right");
    result |= Require(
        EcsUiTextInputRequestMoveCursorEnd(world) != 0,
        "cursor end request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 2u,
        "cursor should move to end");
    result |= Require(
        EcsUiTextInputRequestSelectStart(world) != 0,
        "select start request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 0u &&
            EcsUiTextInputHasSelection(world, text_field_a) &&
            EcsUiTextInputSelectionStart(world, text_field_a) == 0u &&
            EcsUiTextInputSelectionEnd(world, text_field_a) == 2u,
        "select start should extend selection to start");
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_a,
            true,
            text_display,
            sizeof(text_display)),
        "backward selected text display should be created");
    result |= Require(
        strcmp(text_display, "|[hi]") == 0,
        "backward selected text display mismatch");
    result |= Require(
        EcsUiTextInputRequestMoveCursorEnd(world) != 0,
        "cursor end collapse request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 2u &&
            !EcsUiTextInputHasSelection(world, text_field_a),
        "cursor movement should collapse selection");
    result |= Require(
        EcsUiTextInputRequestSelectLeft(world) != 0,
        "select left request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 1u &&
            EcsUiTextInputHasSelection(world, text_field_a) &&
            EcsUiTextInputSelectionStart(world, text_field_a) == 1u &&
            EcsUiTextInputSelectionEnd(world, text_field_a) == 2u,
        "select left should select previous character");
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_a,
            true,
            text_display,
            sizeof(text_display)),
        "selected text field display should be created");
    result |= Require(
        strcmp(text_display, "h|[i]") == 0,
        "selected text field display mismatch");
    result |= Require(
        EcsUiTextInputRequestDelete(world) != 0,
        "selection delete request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "h") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 1u &&
            !EcsUiTextInputHasSelection(world, text_field_a),
        "delete should remove selected text");
    result |= Require(
        EcsUiTextInputSetValue(world, text_field_a, "hi") &&
            EcsUiTextInputSetCursor(world, text_field_a, 0u),
        "text field should reset for selection replacement");
    result |= Require(
        EcsUiTextInputRequestSelectRight(world) != 0,
        "select right request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 1u &&
            EcsUiTextInputHasSelection(world, text_field_a) &&
            EcsUiTextInputSelectionStart(world, text_field_a) == 0u &&
            EcsUiTextInputSelectionEnd(world, text_field_a) == 1u,
        "select right should select next character");
    result |= Require(
        EcsUiTextInputRequestSelectEnd(world) != 0,
        "select end request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputCursor(world, text_field_a) == 2u &&
            EcsUiTextInputSelectionStart(world, text_field_a) == 0u &&
            EcsUiTextInputSelectionEnd(world, text_field_a) == 2u,
        "select end should extend selection to end");
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_a,
            true,
            text_display,
            sizeof(text_display)),
        "forward selected text display should be created");
    result |= Require(
        strcmp(text_display, "[hi]|") == 0,
        "forward selected text display mismatch");
    result |= Require(
        EcsUiTextInputRequestInsert(world, '!') != 0,
        "selection replacement insert request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "!") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 1u &&
            !EcsUiTextInputHasSelection(world, text_field_a),
        "insert should replace selected text");
    result |= Require(
        EcsUiTextInputSetValue(world, text_field_a, "clip") &&
            EcsUiTextInputSetCursor(world, text_field_a, 0u),
        "text field should reset before clipboard tests");
    result |= Require(
        EcsUiTextInputRequestSelectEnd(world) != 0,
        "clipboard select request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputRequestCopy(world) != 0,
        "copy request should be created");
    (void)ecs_progress(world, 0.0f);
    char clipboard_text[ECS_UI_TEXT_MAX] = {0};
    result |= Require(
        EcsUiTextInputPopClipboardWrite(
            world,
            clipboard_text,
            sizeof(clipboard_text)) &&
            strcmp(clipboard_text, "clip") == 0,
        "copy should publish selected text");
    result |= Require(
        !EcsUiTextInputPopClipboardWrite(
            world,
            clipboard_text,
            sizeof(clipboard_text)),
        "clipboard write queue should be empty after pop");
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "clip") == 0 &&
            EcsUiTextInputHasSelection(world, text_field_a),
        "copy should preserve field value and selection");
    result |= Require(
        EcsUiTextInputRequestCut(world) != 0,
        "cut request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputPopClipboardWrite(
            world,
            clipboard_text,
            sizeof(clipboard_text)) &&
            strcmp(clipboard_text, "clip") == 0,
        "cut should publish selected text");
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 0u &&
            !EcsUiTextInputHasSelection(world, text_field_a),
        "cut should delete selected text");
    result |= Require(
        EcsUiTextInputRequestPaste(world, "go") != 0,
        "paste request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "go") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 2u,
        "paste should insert clipboard text");
    result |= Require(
        EcsUiTextInputSetCursor(world, text_field_a, 0u) &&
            EcsUiTextInputRequestSelectRight(world) != 0,
        "paste replacement selection should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputRequestPaste(world, "X") != 0,
        "paste replacement request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "Xo") == 0 &&
            EcsUiTextInputCursor(world, text_field_a) == 1u &&
            !EcsUiTextInputHasSelection(world, text_field_a),
        "paste should replace selected text");
    result |= Require(
        EcsUiTextInputSetValue(world, text_field_a, "hi") &&
            EcsUiTextInputSetCursor(world, text_field_a, 2u),
        "text field should reset before final delete");
    result |= Require(
        EcsUiTextInputRequestDelete(world) != 0,
        "delete request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "h") == 0,
        "text field delete mismatch");
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_a,
            true,
            text_display,
            sizeof(text_display)),
        "text field display should be created");
    result |= Require(
        strcmp(text_display, "h|") == 0,
        "focused text field display mismatch");
    result |= Require(
        EcsUiTextInputRequestFocusField(world, text_field_b) != 0,
        "second focus request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_b &&
            !EcsUiTextInputIsFocused(world, text_field_a),
        "text focus should move to second field");
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_b,
            true,
            text_display,
            sizeof(text_display)),
        "second text field display should be created");
    result |= Require(
        strcmp(text_display, "|") == 0,
        "focused empty text field display mismatch");
    result |= Require(
        EcsUiTextInputRequestBlur(world) != 0,
        "blur request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        !EcsUiTextInputHasFocusedField(world),
        "text field should blur");
    result |= Require(
        EcsUiTextInputDisplayText(
            world,
            text_field_b,
            true,
            text_display,
            sizeof(text_display)),
        "blurred text field display should be created");
    result |= Require(
        strcmp(text_display, "second") == 0,
        "blurred empty text field display mismatch");
    result |= Require(
        EcsUiTextInputRequestFocusNext(world) != 0,
        "focus next request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_a,
        "focus next should focus first field when none is focused");
    result |= Require(
        EcsUiTextInputRequestFocusNext(world) != 0,
        "second focus next request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_b,
        "focus next should advance to second field");
    result |= Require(
        EcsUiTextInputRequestFocusNext(world) != 0,
        "third focus next request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_a,
        "focus next should wrap to first field");
    result |= Require(
        EcsUiTextInputRequestFocusPrevious(world) != 0,
        "focus previous request should be created");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_b,
        "focus previous should wrap to second field");
    (void)EcsUiTextInputRequestBlur(world);
    (void)ecs_progress(world, 0.0f);
    ecs_entity_t text_field_node =
        ecs_entity(world, {.name = "TextFieldNode"});
    ecs_entity_t text_value_node =
        ecs_entity(world, {.name = "TextFieldValueNode"});
    result |= Require(
        EcsUiTextInputSetFieldUiNodes(
            world,
            text_field_a,
            text_field_node,
            text_value_node),
        "text field UI links should be set");
    result |= Require(
        EcsUiTextInputSetUiField(world, text_field_node, text_field_a),
        "text UI reverse link should be set");
    result |= Require(
        EcsUiTextInputFieldUiNode(world, text_field_a) == text_field_node &&
            EcsUiTextInputFieldValueUiNode(world, text_field_a) ==
                text_value_node &&
            EcsUiTextInputUiField(world, text_field_node) == text_field_a,
        "text UI links should round trip");
    result |= Require(
        EcsUiTextInputClear(world, text_field_a) &&
            strcmp(EcsUiTextInputValue(world, text_field_a), "") == 0,
        "text field clear mismatch");
    EcsUiEvent focus_text_event = {
        .type = ECS_UI_EVENT_CLICKED,
        .node = text_field_node,
    };
    result |= Require(
        EcsUiTextInputApplyEvent(world, &focus_text_event),
        "text event router should consume field focus click");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_a,
        "text event router should focus linked field");
    EcsUiEvent text_key_event = {
        .type = ECS_UI_EVENT_TEXT_INPUT,
        .codepoint = 'z',
    };
    result |= Require(
        EcsUiTextInputApplyEvents(
            world,
            &(EcsUiEventList){
                .count = 1u,
                .events = {text_key_event},
            }) == 1u,
        "text event router should consume text input");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "z") == 0,
        "text event router should insert typed character");
    EcsUiEvent submit_text_event = {
        .type = ECS_UI_EVENT_TEXT_SUBMIT,
    };
    result |= Require(
        !EcsUiTextInputApplyEvent(world, &submit_text_event),
        "text submit should stay app-owned");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputFocusedField(world) == text_field_a,
        "text submit should not blur focused field");
    EcsUiEvent outside_click_event = {
        .type = ECS_UI_EVENT_CLICKED,
    };
    result |= Require(
        !EcsUiTextInputApplyEvent(world, &outside_click_event),
        "outside click blur should not consume app click");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        !EcsUiTextInputHasFocusedField(world),
        "outside click should blur focused text field");
    result |= Require(
        !EcsUiTextInputApplyEvent(world, &text_key_event),
        "text event router should ignore typing without focus");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        strcmp(EcsUiTextInputValue(world, text_field_a), "z") == 0,
        "ignored text input should not mutate blurred field");
    result |= Require(
        EcsUiTextInputApplyEvent(world, &focus_text_event),
        "text event router should refocus linked field");
    (void)ecs_progress(world, 0.0f);
    EcsUiEvent cancel_text_event = {
        .type = ECS_UI_EVENT_TEXT_CANCEL,
    };
    result |= Require(
        EcsUiTextInputApplyEvents(
            world,
            &(EcsUiEventList){
                .count = 1u,
                .events = {cancel_text_event},
            }) == 1u,
        "text event router should consume cancel while focused");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        !EcsUiTextInputHasFocusedField(world),
        "text cancel should blur focused field");

    ecs_entity_t text_view_root =
        EcsUiRootEntity(world, "TextFieldViewRoot");
    ecs_entity_t text_view_style =
        ecs_entity(world, {.name = "TextFieldViewStyle"});
    ecs_set(
        world,
        text_view_style,
        EcsUiBoxStyle,
        {
            .background = {10u, 20u, 30u, 255u},
            .highlight_background = {20u, 30u, 40u, 255u},
            .padding = 8.0f,
        });
    EcsUiBuilder text_view_builder =
        EcsUiBuilderBegin(world, text_view_root);
    ecs_entity_t text_view_node = EcsUiTextInputBuildFieldView(
        &text_view_builder,
        text_field_a,
        (EcsUiTextFieldViewDesc){
            .field_id = "TextFieldView",
            .value_id = "TextFieldViewValue",
            .style_token = text_view_style,
        });
    EcsUiBuilderEnd(&text_view_builder);
    ecs_entity_t text_view_value =
        EcsUiTextInputFieldValueUiNode(world, text_field_a);
    const EcsUiText *text_view_text =
        text_view_value != 0 ? ecs_get(world, text_view_value, EcsUiText) : NULL;
    result |= Require(
        EcsUiBuilderOk(&text_view_builder) && text_view_node != 0 &&
            text_view_value != 0,
        "text field view should build field and value nodes");
    result |= Require(
        EcsUiTextInputFieldUiNode(world, text_field_a) == text_view_node &&
            EcsUiTextInputUiField(world, text_view_node) == text_field_a,
        "text field view should wire field relationships");
    result |= Require(
        ecs_get_target(world, text_view_node, EcsUiUsesStyle, 0) ==
            text_view_style,
        "text field view should attach style token");
    result |= Require(
        text_view_text != NULL &&
            strcmp(text_view_text->text, "z") == 0 &&
            text_view_text->role == ECS_UI_TEXT_BUTTON,
        "text field view should project blurred value text");
    EcsUiEvent focus_view_event = {
        .type = ECS_UI_EVENT_CLICKED,
        .node = text_view_node,
    };
    result |= Require(
        EcsUiTextInputApplyEvent(world, &focus_view_event),
        "text field view should focus through router");
    (void)ecs_progress(world, 0.0f);
    result |= Require(
        EcsUiTextInputProjectFieldView(world, text_field_a),
        "text field view projection should update focused field");
    text_view_text =
        text_view_value != 0 ? ecs_get(world, text_view_value, EcsUiText) : NULL;
    const EcsUiVisual *text_view_visual =
        ecs_get(world, text_view_node, EcsUiVisual);
    result |= Require(
        text_view_text != NULL &&
            strcmp(text_view_text->text, "z|") == 0 &&
            text_view_text->role == ECS_UI_TEXT_BUTTON,
        "text field view should project focused caret text");
    result |= RequireNear(
        text_view_visual != NULL ? text_view_visual->opacity : 0.0f,
        1.0f,
        0.001f,
        "text field view opacity mismatch");
    result |= RequireNear(
        text_view_visual != NULL ? text_view_visual->highlight : 0.0f,
        0.22f,
        0.001f,
        "text field view focus highlight mismatch");
    (void)EcsUiTextInputRequestBlur(world);
    (void)ecs_progress(world, 0.0f);

    ecs_entity_t root = EcsUiRootEntity(world, "Home");
    if (root == 0) {
        ecs_fini(world);
        return 2;
    }

    ecs_entity_t present_add_machine_action =
        ecs_entity(world, {.name = "PresentAddMachineAction"});
    ecs_entity_t light_theme = EcsUiThemeEntity(world, "LightTheme");
    ecs_entity_t dark_theme = EcsUiThemeEntity(world, "DarkTheme");
    result |= Require(light_theme != 0, "light theme should be created");
    result |= Require(dark_theme != 0, "dark theme should be created");
    result |= Require(
        EcsUiThemeSetBoxStyle(
            world,
            light_theme,
            text_field_style_token,
            (EcsUiBoxStyle){
                .background = {10u, 20u, 30u, 255u},
                .hover_background = {20u, 30u, 40u, 255u},
                .highlight_background = {50u, 60u, 70u, 255u},
                .radius = 0.1f,
                .padding = 9.0f,
            }),
        "light theme should store token box style");
    result |= Require(
        EcsUiThemeSetBoxStyle(
            world,
            dark_theme,
            text_field_style_token,
            (EcsUiBoxStyle){
                .background = {210u, 220u, 230u, 255u},
                .hover_background = {200u, 210u, 220u, 255u},
                .highlight_background = {180u, 190u, 200u, 255u},
                .radius = 0.3f,
                .padding = 11.0f,
            }),
        "dark theme should store token box style");
    result |= Require(
        EcsUiThemeSetTextStyle(
            world,
            light_theme,
            text_field_style_token,
            (EcsUiTextStyle){
                .color = {1u, 2u, 3u, 255u},
                .muted_color = {4u, 5u, 6u, 255u},
                .disabled_color = {7u, 8u, 9u, 255u},
            }),
        "light theme should store token text style");
    result |= Require(
        EcsUiThemeSetTextStyle(
            world,
            dark_theme,
            text_field_style_token,
            (EcsUiTextStyle){
                .color = {101u, 102u, 103u, 255u},
                .muted_color = {104u, 105u, 106u, 255u},
                .disabled_color = {107u, 108u, 109u, 255u},
            }),
        "dark theme should store token text style");
    result |= Require(
        EcsUiThemeSetBoxStyle(
            world,
            light_theme,
            primary_action_style_token,
            (EcsUiBoxStyle){
                .background = {30u, 40u, 50u, 255u},
                .hover_background = {31u, 41u, 51u, 255u},
                .highlight_background = {32u, 42u, 52u, 255u},
                .radius = 0.4f,
                .padding = 13.0f,
            }),
        "light theme should store action token box style");
    result |= Require(
        EcsUiThemeSetBoxStyle(
            world,
            dark_theme,
            primary_action_style_token,
            (EcsUiBoxStyle){
                .background = {130u, 140u, 150u, 255u},
                .hover_background = {131u, 141u, 151u, 255u},
                .highlight_background = {132u, 142u, 152u, 255u},
                .radius = 0.5f,
                .padding = 15.0f,
            }),
        "dark theme should store action token box style");
    result |= Require(
        EcsUiThemeSetTextStyle(
            world,
            light_theme,
            primary_action_style_token,
            (EcsUiTextStyle){
                .color = {11u, 12u, 13u, 255u},
                .muted_color = {14u, 15u, 16u, 255u},
                .disabled_color = {17u, 18u, 19u, 255u},
            }),
        "light theme should store action token text style");
    result |= Require(
        EcsUiThemeSetTextStyle(
            world,
            dark_theme,
            primary_action_style_token,
            (EcsUiTextStyle){
                .color = {111u, 112u, 113u, 255u},
                .muted_color = {114u, 115u, 116u, 255u},
                .disabled_color = {117u, 118u, 119u, 255u},
            }),
        "dark theme should store action token text style");
    result |= Require(
        EcsUiSetActiveTheme(world, light_theme),
        "light theme should become active");
    result |= Require(
        EcsUiGetActiveTheme(world) == light_theme,
        "active theme should round trip");
    result |= Require(
        EcsUiThemeApply(world),
        "active theme should apply to style token");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    VStack(&builder, {.id = "HomeStack", .gap = 10.0f, .padding = 16.0f}) {
        Button(
            &builder,
            {
                .id = "AddMachine",
                .variant = ECS_UI_BUTTON_PRIMARY,
                .on_click = present_add_machine_action,
                .style_token = primary_action_style_token,
            }) {
            Text(
                &builder,
                {
                    .id = "AddLabel",
                    .text = "add machine",
                    .role = ECS_UI_TEXT_BUTTON,
                });
        }
        HStack(&builder, {.id = "Footer", .gap = 4.0f}) {
            Icon(&builder, {.id = "FooterIcon", .name = "plus"});
            Text(
                &builder,
                {
                    .id = "FooterLabel",
                    .text = "footer",
                    .role = ECS_UI_TEXT_CAPTION,
                });
        }
        EcsUiBeginPressable(
            &builder,
            (EcsUiPressableDesc){
                .id = "SearchField",
                .on_click = present_add_machine_action,
                .style_token = text_field_style_token,
            });
        Text(
            &builder,
            {
                .id = "SearchText",
                .text = "search",
                .role = ECS_UI_TEXT_BODY,
            });
        EcsUiEnd(&builder);
        ecs_entity_t direct_field = EcsUiBeginPressable(
            &builder,
            (EcsUiPressableDesc){
                .id = "DirectStyleField",
                .on_click = present_add_machine_action,
                .style_token = text_field_style_token,
            });
        Text(
            &builder,
            {
                .id = "DirectStyleText",
                .text = "direct",
                .role = ECS_UI_TEXT_BODY,
            });
        EcsUiEnd(&builder);
        ecs_set(
            world,
            direct_field,
            EcsUiBoxStyle,
            {
                .background = {90u, 80u, 70u, 255u},
                .hover_background = {91u, 81u, 71u, 255u},
                .highlight_background = {92u, 82u, 72u, 255u},
                .radius = 0.2f,
                .padding = 7.0f,
            });
        ecs_set(
            world,
            direct_field,
            EcsUiTextStyle,
            {
                .color = {201u, 202u, 203u, 255u},
                .muted_color = {204u, 205u, 206u, 255u},
                .disabled_color = {207u, 208u, 209u, 255u},
            });
        ecs_entity_t terminal_preview = Custom(
            &builder,
            {
                .id = "TerminalPreview",
                .kind = "terminal",
                .preferred_width = 320.0f,
                .preferred_height = 120.0f,
            });
        ecs_set(
            world,
            terminal_preview,
            EcsUiHitTest,
            {
                .mode = ECS_UI_HIT_TEST_CAPTURE,
            });
    }
    EcsUiBuilderEnd(&builder);

    result |= Require(EcsUiBuilderOk(&builder), "builder failed");

    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "read tree failed");
    result |= Require(tree.count == 12u, "unexpected tree count");
    result |= Require(!tree.truncated, "tree truncated");
    result |= RequireNode(&tree, 0u, "Home", ECS_UI_NODE_ROOT);
    result |= RequireNode(&tree, 1u, "HomeStack", ECS_UI_NODE_VSTACK);
    result |= RequireNode(&tree, 2u, "AddMachine", ECS_UI_NODE_BUTTON);
    result |= RequireNode(&tree, 3u, "AddLabel", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 4u, "Footer", ECS_UI_NODE_HSTACK);
    result |= RequireNode(&tree, 5u, "FooterIcon", ECS_UI_NODE_ICON);
    result |= RequireNode(&tree, 6u, "FooterLabel", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 7u, "SearchField", ECS_UI_NODE_PRESSABLE);
    result |= RequireNode(&tree, 8u, "SearchText", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 9u, "DirectStyleField", ECS_UI_NODE_PRESSABLE);
    result |= RequireNode(&tree, 10u, "DirectStyleText", ECS_UI_NODE_TEXT);
    result |= RequireNode(&tree, 11u, "TerminalPreview", ECS_UI_NODE_CUSTOM);

    result |= Require(
        tree.nodes[1u].first_child == 2u,
        "HomeStack first child should be AddMachine");
    result |= Require(
        tree.nodes[2u].next_sibling == 4u,
        "AddMachine next sibling should be Footer");
    result |= Require(
        tree.nodes[4u].next_sibling == 7u,
        "Footer next sibling should be SearchField");
    result |= Require(
        tree.nodes[7u].next_sibling == 9u,
        "SearchField next sibling should be DirectStyleField");
    result |= Require(
        tree.nodes[9u].next_sibling == 11u,
        "DirectStyleField next sibling should be TerminalPreview");
    result |= Require(
        strcmp(tree.nodes[3u].text.text, "add machine") == 0,
        "text payload not copied");
    result |= Require(
        strcmp(tree.nodes[8u].text.text, "search") == 0,
        "pressable text payload not copied");
    result |= Require(
        strcmp(tree.nodes[10u].text.text, "direct") == 0,
        "direct style text payload not copied");
    result |= Require(
        strcmp(tree.nodes[11u].custom.kind, "terminal") == 0,
        "custom kind not copied");
    result |= Require(
        tree.nodes[11u].custom.preferred_width == 320.0f &&
            tree.nodes[11u].custom.preferred_height == 120.0f,
        "custom preferred size not copied");
    result |= Require(
        tree.nodes[11u].hit_test.mode == ECS_UI_HIT_TEST_CAPTURE,
        "custom hit-test mode should be copied");
    result |= Require(
        tree.nodes[2u].visual.opacity == 1.0f,
        "visual opacity should default to 1");
    result |= Require(
        tree.nodes[2u].visual.offset_x == 0.0f &&
            tree.nodes[2u].visual.offset_y == 0.0f,
        "visual offset should default to 0");
    result |= Require(
        tree.nodes[2u].visual.highlight == 0.0f,
        "visual highlight should default to 0");

    ecs_entity_t home_stack = tree.nodes[1u].entity;
    result |= Require(
        ecs_has_id(world, home_stack, EcsOrderedChildren),
        "HomeStack should have EcsOrderedChildren");
    result |= Require(
        ecs_has_pair(
            world,
            tree.nodes[2u].entity,
            EcsUiOnClick,
            present_add_machine_action),
        "button should have OnClick action pair");
    result |= Require(
        tree.nodes[2u].on_click == present_add_machine_action,
        "button snapshot should expose OnClick action");
    result |= Require(
        ecs_get_target(world, tree.nodes[2u].entity, EcsUiUsesStyle, 0) ==
            primary_action_style_token,
        "button desc should attach style token");
    result |= Require(
        tree.nodes[2u].has_box_style &&
            tree.nodes[2u].box_style.background.r == 30u &&
            tree.nodes[2u].box_style.background.g == 40u &&
            tree.nodes[2u].box_style.background.b == 50u &&
            tree.nodes[2u].box_style.background.a == 255u,
        "button token box style should be copied");
    result |= Require(
        tree.nodes[2u].has_text_style &&
            tree.nodes[2u].text_style.color.r == 11u &&
            tree.nodes[2u].text_style.color.g == 12u &&
            tree.nodes[2u].text_style.color.b == 13u &&
            tree.nodes[2u].text_style.disabled_color.r == 17u,
        "button token text style should be copied");
    result |= Require(
        ecs_has_pair(
            world,
            tree.nodes[7u].entity,
            EcsUiOnClick,
            present_add_machine_action),
        "pressable should have OnClick action pair");
    result |= Require(
        tree.nodes[7u].on_click == present_add_machine_action,
        "pressable snapshot should expose OnClick action");
    result |= Require(
        ecs_get_target(world, tree.nodes[7u].entity, EcsUiUsesStyle, 0) ==
            text_field_style_token,
        "pressable desc should attach style token");
    result |= Require(
        tree.nodes[7u].has_box_style &&
            tree.nodes[7u].box_style.background.r == 10u &&
            tree.nodes[7u].box_style.background.g == 20u &&
            tree.nodes[7u].box_style.background.b == 30u &&
            tree.nodes[7u].box_style.background.a == 255u,
        "pressable box style color should be copied");
    result |= Require(
        tree.nodes[7u].has_text_style &&
            tree.nodes[7u].text_style.color.r == 1u &&
            tree.nodes[7u].text_style.color.g == 2u &&
            tree.nodes[7u].text_style.color.b == 3u &&
            tree.nodes[7u].text_style.disabled_color.r == 7u,
        "pressable token text style should be copied");
    result |= RequireNear(
        tree.nodes[7u].box_style.padding,
        9.0f,
        0.0001f,
        "pressable box style padding should be copied");
    result |= RequireNear(
        tree.nodes[7u].box_style.radius,
        0.1f,
        0.0001f,
        "pressable box style radius should be copied");
    result |= Require(
        tree.nodes[9u].has_box_style &&
            tree.nodes[9u].box_style.background.r == 90u &&
            tree.nodes[9u].box_style.background.g == 80u &&
            tree.nodes[9u].box_style.background.b == 70u &&
            tree.nodes[9u].box_style.background.a == 255u,
        "direct box style should override style token");
    result |= Require(
        tree.nodes[9u].has_text_style &&
            tree.nodes[9u].text_style.color.r == 201u &&
            tree.nodes[9u].text_style.color.g == 202u &&
            tree.nodes[9u].text_style.color.b == 203u &&
            tree.nodes[9u].text_style.disabled_color.r == 207u,
        "direct text style should override style token");
    result |= Require(
        EcsUiSetActiveTheme(world, dark_theme),
        "dark theme should become active");
    result |= Require(
        EcsUiGetActiveTheme(world) == dark_theme,
        "dark active theme should round trip");
    result |= Require(
        EcsUiThemeApply(world),
        "dark theme should apply to style token");
    result |= Require(EcsUiReadTree(world, root, &tree), "reread tree failed");
    result |= Require(
        tree.nodes[2u].has_box_style &&
            tree.nodes[2u].box_style.background.r == 130u &&
            tree.nodes[2u].box_style.background.g == 140u &&
            tree.nodes[2u].box_style.background.b == 150u &&
            tree.nodes[2u].box_style.background.a == 255u,
        "theme switch should update action token box style");
    result |= Require(
        tree.nodes[2u].has_text_style &&
            tree.nodes[2u].text_style.color.r == 111u &&
            tree.nodes[2u].text_style.color.g == 112u &&
            tree.nodes[2u].text_style.color.b == 113u &&
            tree.nodes[2u].text_style.disabled_color.r == 117u,
        "theme switch should update action token text style");
    result |= Require(
        tree.nodes[7u].has_box_style &&
            tree.nodes[7u].box_style.background.r == 210u &&
            tree.nodes[7u].box_style.background.g == 220u &&
            tree.nodes[7u].box_style.background.b == 230u &&
            tree.nodes[7u].box_style.background.a == 255u,
        "theme switch should update token box style");
    result |= Require(
        tree.nodes[7u].has_text_style &&
            tree.nodes[7u].text_style.color.r == 101u &&
            tree.nodes[7u].text_style.color.g == 102u &&
            tree.nodes[7u].text_style.color.b == 103u &&
            tree.nodes[7u].text_style.disabled_color.r == 107u,
        "theme switch should update token text style");
    result |= RequireNear(
        tree.nodes[7u].box_style.padding,
        11.0f,
        0.0001f,
        "theme switch should update token padding");
    result |= Require(
        tree.nodes[9u].has_box_style &&
            tree.nodes[9u].box_style.background.r == 90u &&
            tree.nodes[9u].box_style.background.g == 80u &&
            tree.nodes[9u].box_style.background.b == 70u &&
            tree.nodes[9u].box_style.background.a == 255u,
        "direct box style should still override switched style token");
    result |= Require(
        tree.nodes[9u].has_text_style &&
            tree.nodes[9u].text_style.color.r == 201u &&
            tree.nodes[9u].text_style.color.g == 202u &&
            tree.nodes[9u].text_style.color.b == 203u &&
            tree.nodes[9u].text_style.disabled_color.r == 207u,
        "direct text style should still override switched style token");

    ecs_entities_t ordered = ecs_get_ordered_children(world, home_stack);
    result |= Require(ordered.count == 5, "ordered child count mismatch");
    result |= Require(
        ordered.ids[0] == tree.nodes[2u].entity &&
            ordered.ids[1] == tree.nodes[4u].entity &&
            ordered.ids[2] == tree.nodes[7u].entity &&
            ordered.ids[3] == tree.nodes[9u].entity &&
            ordered.ids[4] == tree.nodes[11u].entity,
        "ordered children mismatch");

    ecs_entity_t source_tag =
        ecs_entity(world, {.name = "TestProjectionSource"});
    ecs_entity_t source_parent =
        ecs_entity(world, {.name = "TestProjectionSources"});
    ecs_add_id(world, source_parent, EcsOrderedChildren);
    ecs_entity_t source_a = ecs_entity(world, {
        .parent = source_parent,
        .name = "SourceA",
        .sep = "",
    });
    ecs_entity_t source_b = ecs_entity(world, {
        .parent = source_parent,
        .name = "SourceB",
        .sep = "",
    });
    ecs_add_id(world, source_a, source_tag);
    ecs_add_id(world, source_b, source_tag);

    ecs_entity_t projection_root =
        EcsUiRootEntity(world, "ProjectionRoot");
    EcsUiBuilder projection_builder =
        EcsUiBuilderBegin(world, projection_root);
    Text(
        &projection_builder,
        {
            .id = "ProjectionHeader",
            .text = "header",
            .role = ECS_UI_TEXT_LABEL,
        });
    ecs_entity_t row_a = EcsUiBeginHStack(
        &projection_builder,
        (EcsUiStackDesc){
            .id = "ProjectedRowA",
        });
    ecs_entity_t label_a = EcsUiAddText(
        &projection_builder,
        (EcsUiTextDesc){
            .id = "ProjectedLabelA",
            .text = "A",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&projection_builder);
    ecs_entity_t row_b = EcsUiBeginHStack(
        &projection_builder,
        (EcsUiStackDesc){
            .id = "ProjectedRowB",
        });
    ecs_entity_t label_b = EcsUiAddText(
        &projection_builder,
        (EcsUiTextDesc){
            .id = "ProjectedLabelB",
            .text = "B",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&projection_builder);
    EcsUiBuilderEnd(&projection_builder);
    result |= Require(
        EcsUiBuilderOk(&projection_builder),
        "projection builder failed");

    ecs_entity_t label_slot =
        ecs_entity(world, {.name = "TestProjectionLabelSlot"});
    result |= Require(
        EcsUiProjectionLink(world, source_a, row_a),
        "projection link A failed");
    result |= Require(
        EcsUiProjectionLink(world, source_b, row_b),
        "projection link B failed");
    result |= Require(
        EcsUiProjectionSetNode(world, source_a, label_slot, label_a),
        "projection slot A failed");
    result |= Require(
        EcsUiProjectionSetNode(world, source_b, label_slot, label_b),
        "projection slot B failed");
    result |= Require(
        EcsUiProjectionGetRoot(world, source_a) == row_a,
        "projection root lookup failed");
    result |= Require(
        ecs_has_id(world, row_a, EcsUiProjectionRootNode) &&
            ecs_has_id(world, row_b, EcsUiProjectionRootNode),
        "projected row roots should be marked");
    result |= Require(
        EcsUiProjectionGetSource(world, row_b) == source_b,
        "projection source lookup failed");
    result |= Require(
        EcsUiProjectionGetNode(world, source_a, label_slot) == label_a,
        "projection slot lookup failed");
    result |= Require(
        ecs_has_id(world, label_slot, EcsUiProjectionSlot) &&
            ecs_has_id(world, label_slot, EcsExclusive),
        "projection slot should be registered");

    ecs_entity_t source_order[2] = {source_b, source_a};
    ecs_set_child_order(world, source_parent, source_order, 2);
    result |= Require(
        EcsUiProjectionSyncOrderedChildren(
            world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = source_parent,
                .ui_parent = projection_root,
                .source_filter = source_tag,
                .preserve_unprojected_ui_children = true,
            }),
        "projection order sync failed");
    ecs_entities_t projected_order =
        ecs_get_ordered_children(world, projection_root);
    result |= Require(
        projected_order.count == 3,
        "projection ordered child count mismatch");
    result |= Require(
        projected_order.ids[0] != row_a &&
            projected_order.ids[0] != row_b &&
            projected_order.ids[1] == row_b &&
            projected_order.ids[2] == row_a,
        "projection order mismatch");

    ecs_delete(world, source_b);
    result |= Require(
        !EcsUiProjectionSyncOrderedChildren(
            world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = source_parent,
                .ui_parent = projection_root,
                .source_filter = source_tag,
                .preserve_unprojected_ui_children = true,
            }),
        "projection order sync should reject stale projected rows");

    ecs_delete(world, row_b);
    result |= Require(
        EcsUiProjectionSyncOrderedChildren(
            world,
            (EcsUiProjectionOrderSyncDesc){
                .source_parent = source_parent,
                .ui_parent = projection_root,
                .source_filter = source_tag,
                .preserve_unprojected_ui_children = true,
            }),
        "projection order sync after stale cleanup failed");
    projected_order = ecs_get_ordered_children(world, projection_root);
    result |= Require(
        projected_order.count == 2 &&
            projected_order.ids[0] != row_a &&
            projected_order.ids[1] == row_a,
        "projection order after stale cleanup mismatch");

    result |= Require(
        EcsUiProjectionDelete(world, source_a),
        "projection delete failed");
    result |= Require(
        EcsUiProjectionGetRoot(world, source_a) == 0 &&
            !ecs_is_alive(world, row_a),
        "projection delete cleanup failed");

    ecs_entity_t collection_source_parent =
        ecs_entity(world, {.name = "CollectionSources"});
    ecs_add_id(world, collection_source_parent, EcsOrderedChildren);
    ecs_entity_t collection_ui_parent =
        EcsUiRootEntity(world, "CollectionUi");
    EcsUiBuilder collection_builder =
        EcsUiBuilderBegin(world, collection_ui_parent);
    (void)EcsUiAddText(
        &collection_builder,
        (EcsUiTextDesc){
            .id = "CollectionHeader",
            .text = "collection",
            .role = ECS_UI_TEXT_LABEL,
        });
    EcsUiBuilderEnd(&collection_builder);
    result |= Require(
        EcsUiBuilderOk(&collection_builder),
        "collection builder failed");

    TestProjectionContext collection_ctx = {
        .ui_parent = collection_ui_parent,
        .label_slot = ecs_entity(world, {.name = "CollectionLabelSlot"}),
    };
    TestProjectionItem collection_data[2] = {
        {
            .key = 1u,
            .label = "alpha",
        },
        {
            .key = 2u,
            .label = "beta",
        },
    };
    EcsUiProjectionCollectionSource collection_items[2] = {
        {
            .key = collection_data[0].key,
            .data = &collection_data[0],
        },
        {
            .key = collection_data[1].key,
            .data = &collection_data[1],
        },
    };
    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = collection_items,
                .item_count = 2u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "collection sync failed");
    result |= Require(
        collection_ctx.build_count == 2u &&
            collection_ctx.update_count == 2u,
        "collection build/update counts mismatch");

    ecs_entities_t collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    result |= Require(
        collection_sources.count == 2,
        "collection source count mismatch");
    const EcsUiProjectionKey *first_key =
        ecs_get(world, collection_sources.ids[0], EcsUiProjectionKey);
    const EcsUiProjectionKey *second_key =
        ecs_get(world, collection_sources.ids[1], EcsUiProjectionKey);
    result |= Require(
        first_key != NULL && second_key != NULL &&
            first_key->value == 1u && second_key->value == 2u,
        "collection source key order mismatch");
    ecs_entity_t collection_row_1 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[0]);
    ecs_entity_t collection_row_2 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[1]);
    result |= Require(
        collection_row_1 != 0 && collection_row_2 != 0,
        "collection rows not linked");

    ecs_entities_t collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_order.count == 3 &&
            collection_order.ids[1] == collection_row_1 &&
            collection_order.ids[2] == collection_row_2,
        "collection row order mismatch");

    collection_data[0] = (TestProjectionItem){
        .key = 2u,
        .label = "beta updated",
    };
    collection_data[1] = (TestProjectionItem){
        .key = 1u,
        .label = "alpha",
    };
    collection_items[0] = (EcsUiProjectionCollectionSource){
        .key = collection_data[0].key,
        .data = &collection_data[0],
    };
    collection_items[1] = (EcsUiProjectionCollectionSource){
        .key = collection_data[1].key,
        .data = &collection_data[1],
    };
    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = collection_items,
                .item_count = 2u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "collection reorder sync failed");
    collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    collection_row_2 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[0]);
    collection_row_1 =
        EcsUiProjectionGetRoot(world, collection_sources.ids[1]);
    collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_order.count == 3 &&
            collection_order.ids[1] == collection_row_2 &&
            collection_order.ids[2] == collection_row_1,
        "collection reorder mismatch");
    ecs_entity_t updated_label =
        EcsUiProjectionGetNode(
            world,
            collection_sources.ids[0],
            collection_ctx.label_slot);
    const EcsUiText *updated_text =
        updated_label != 0 ? ecs_get(world, updated_label, EcsUiText) : NULL;
    result |= Require(
        updated_text != NULL &&
            strcmp(updated_text->text, "1:beta updated") == 0,
        "collection row update mismatch");

    EcsUiProjectionCollectionSource one_item[1] = {
        {
            .key = collection_data[0].key,
            .data = &collection_data[0],
        },
    };
    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = one_item,
                .item_count = 1u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "collection stale delete sync failed");
    collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_sources.count == 1 &&
            collection_order.count == 2,
        "collection stale delete mismatch");

    result |= Require(
        EcsUiProjectionSyncCollection(
            world,
            (EcsUiProjectionCollectionDesc){
                .source_parent = collection_source_parent,
                .ui_parent = collection_ui_parent,
                .source_filter = ecs_id(TestProjectionItem),
                .items = NULL,
                .item_count = 0u,
                .preserve_unprojected_ui_children = true,
                .source_name_prefix = "CollectionSource",
                .sync_source = TestProjectionSyncSource,
                .build_root = TestProjectionBuildRoot,
                .update_root = TestProjectionUpdateRoot,
                .ctx = &collection_ctx,
            }),
        "empty collection sync failed");
    collection_sources =
        ecs_get_ordered_children(world, collection_source_parent);
    collection_order =
        ecs_get_ordered_children(world, collection_ui_parent);
    result |= Require(
        collection_sources.count == 0 &&
            collection_order.count == 1,
        "empty collection cleanup mismatch");

    ecs_fini(world);
    return result;
}
