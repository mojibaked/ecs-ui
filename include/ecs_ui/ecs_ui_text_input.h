#ifndef ECS_UI_ECS_UI_TEXT_INPUT_H
#define ECS_UI_ECS_UI_TEXT_INPUT_H

#include "ecs_ui/ecs_ui.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EcsUiTextField {
    char value[ECS_UI_TEXT_MAX];
    char placeholder[ECS_UI_TEXT_MAX];
} EcsUiTextField;

typedef struct EcsUiTextEditState {
    uint32_t cursor;
    uint32_t selection_anchor;
    uint32_t selection_focus;
} EcsUiTextEditState;

typedef struct EcsUiTextInsertRequest {
    uint32_t codepoint;
} EcsUiTextInsertRequest;

typedef struct EcsUiTextPasteRequest {
    char text[ECS_UI_TEXT_MAX];
} EcsUiTextPasteRequest;

typedef struct EcsUiTextClipboardWriteRequest {
    char text[ECS_UI_TEXT_MAX];
} EcsUiTextClipboardWriteRequest;

typedef struct EcsUiTextFieldViewDesc {
    const char *field_id;
    const char *value_id;
    ecs_entity_t style_token;
} EcsUiTextFieldViewDesc;

extern ECS_COMPONENT_DECLARE(EcsUiTextField);
extern ECS_COMPONENT_DECLARE(EcsUiTextEditState);
extern ECS_COMPONENT_DECLARE(EcsUiTextInsertRequest);
extern ECS_COMPONENT_DECLARE(EcsUiTextPasteRequest);
extern ECS_COMPONENT_DECLARE(EcsUiTextClipboardWriteRequest);
extern ECS_TAG_DECLARE(EcsUiFocusedTextField);
extern ECS_TAG_DECLARE(EcsUiTextFieldUiNode);
extern ECS_TAG_DECLARE(EcsUiTextFieldValueUiNode);
extern ECS_TAG_DECLARE(EcsUiForTextField);
extern ECS_TAG_DECLARE(EcsUiFocusTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiFocusNextTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiFocusPreviousTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiBlurTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiTextDeleteRequest);
extern ECS_TAG_DECLARE(EcsUiTextCursorLeftRequest);
extern ECS_TAG_DECLARE(EcsUiTextCursorRightRequest);
extern ECS_TAG_DECLARE(EcsUiTextCursorStartRequest);
extern ECS_TAG_DECLARE(EcsUiTextCursorEndRequest);
extern ECS_TAG_DECLARE(EcsUiTextSelectLeftRequest);
extern ECS_TAG_DECLARE(EcsUiTextSelectRightRequest);
extern ECS_TAG_DECLARE(EcsUiTextSelectStartRequest);
extern ECS_TAG_DECLARE(EcsUiTextSelectEndRequest);
extern ECS_TAG_DECLARE(EcsUiTextCopyRequest);
extern ECS_TAG_DECLARE(EcsUiTextCutRequest);

void EcsUiTextInputImport(ecs_world_t *world);

ecs_entity_t EcsUiTextInputRoot(ecs_world_t *world);
ecs_entity_t EcsUiTextInputField(
    ecs_world_t *world,
    const char *name,
    const char *placeholder);
ecs_entity_t EcsUiTextInputFocusedField(const ecs_world_t *world);
bool EcsUiTextInputHasFocusedField(const ecs_world_t *world);
bool EcsUiTextInputIsFocused(
    const ecs_world_t *world,
    ecs_entity_t field);

/*
 * Focus, traversal, and blur requests are accumulated during a frame and
 * resolved once in the text-input update pipeline. Same-frame precedence is:
 * explicit field focus, then traversal focus-next/focus-previous, then blur.
 * Blur only wins when no focus or traversal request exists in that frame.
 * Within each class, the last request made by these APIs wins.
 */
ecs_entity_t EcsUiTextInputRequestFocusField(
    ecs_world_t *world,
    ecs_entity_t field);
ecs_entity_t EcsUiTextInputRequestFocusNext(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestFocusPrevious(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestBlur(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestInsert(
    ecs_world_t *world,
    uint32_t codepoint);
ecs_entity_t EcsUiTextInputRequestDelete(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestMoveCursorLeft(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestMoveCursorRight(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestMoveCursorStart(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestMoveCursorEnd(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestSelectLeft(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestSelectRight(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestSelectStart(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestSelectEnd(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestCopy(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestCut(ecs_world_t *world);
ecs_entity_t EcsUiTextInputRequestPaste(
    ecs_world_t *world,
    const char *text);

const char *EcsUiTextInputValue(
    const ecs_world_t *world,
    ecs_entity_t field);
const char *EcsUiTextInputPlaceholder(
    const ecs_world_t *world,
    ecs_entity_t field);
uint32_t EcsUiTextInputCursor(
    const ecs_world_t *world,
    ecs_entity_t field);
bool EcsUiTextInputHasSelection(
    const ecs_world_t *world,
    ecs_entity_t field);
uint32_t EcsUiTextInputSelectionStart(
    const ecs_world_t *world,
    ecs_entity_t field);
uint32_t EcsUiTextInputSelectionEnd(
    const ecs_world_t *world,
    ecs_entity_t field);
bool EcsUiTextInputSetValue(
    ecs_world_t *world,
    ecs_entity_t field,
    const char *value);
bool EcsUiTextInputSetPlaceholder(
    ecs_world_t *world,
    ecs_entity_t field,
    const char *placeholder);
bool EcsUiTextInputClear(
    ecs_world_t *world,
    ecs_entity_t field);
bool EcsUiTextInputSetCursor(
    ecs_world_t *world,
    ecs_entity_t field,
    uint32_t cursor);
bool EcsUiTextInputClearSelection(
    ecs_world_t *world,
    ecs_entity_t field);
bool EcsUiTextInputDisplayText(
    const ecs_world_t *world,
    ecs_entity_t field,
    bool include_caret,
    char *out,
    size_t out_size);

/*
 * Route renderer-neutral UI events into reusable text-input requests.
 *
 * Returns true when the event is fully consumed by text input. Clicking a node
 * linked with EcsUiForTextField focuses that field and is consumed. Clicking
 * outside a focused field enqueues blur but returns false so application action
 * handlers can still process the click. ECS_UI_EVENT_TEXT_SUBMIT is also left
 * to the application because submit policy is app-specific.
 */
bool EcsUiTextInputApplyEvent(
    ecs_world_t *world,
    const EcsUiEvent *event);
uint32_t EcsUiTextInputApplyEvents(
    ecs_world_t *world,
    const EcsUiEventList *events);
bool EcsUiTextInputPopClipboardWrite(
    ecs_world_t *world,
    char *out,
    size_t out_size);

/*
 * Build and maintain a basic pressable text-field view. The field node is
 * linked to the field with EcsUiForTextField so EcsUiTextInputApplyEvent can
 * focus it without an app-owned action token.
 */
ecs_entity_t EcsUiTextInputBuildFieldView(
    EcsUiBuilder *builder,
    ecs_entity_t field,
    EcsUiTextFieldViewDesc desc);
bool EcsUiTextInputProjectFieldView(
    ecs_world_t *world,
    ecs_entity_t field);

bool EcsUiTextInputSetFieldUiNodes(
    ecs_world_t *world,
    ecs_entity_t field,
    ecs_entity_t field_node,
    ecs_entity_t value_node);
bool EcsUiTextInputSetUiField(
    ecs_world_t *world,
    ecs_entity_t ui_node,
    ecs_entity_t field);
ecs_entity_t EcsUiTextInputFieldUiNode(
    const ecs_world_t *world,
    ecs_entity_t field);
ecs_entity_t EcsUiTextInputFieldValueUiNode(
    const ecs_world_t *world,
    ecs_entity_t field);
ecs_entity_t EcsUiTextInputUiField(
    const ecs_world_t *world,
    ecs_entity_t ui_node);

#ifdef __cplusplus
}
#endif

#endif
