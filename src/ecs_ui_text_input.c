#include "ecs_ui/ecs_ui_text_input.h"

#include <string.h>

ECS_COMPONENT_DECLARE(EcsUiTextField);
ECS_COMPONENT_DECLARE(EcsUiTextEditState);
ECS_COMPONENT_DECLARE(EcsUiTextInsertRequest);
ECS_TAG_DECLARE(EcsUiFocusedTextField);
ECS_TAG_DECLARE(EcsUiTextFieldUiNode);
ECS_TAG_DECLARE(EcsUiTextFieldValueUiNode);
ECS_TAG_DECLARE(EcsUiForTextField);
ECS_TAG_DECLARE(EcsUiFocusTextFieldRequest);
ECS_TAG_DECLARE(EcsUiFocusNextTextFieldRequest);
ECS_TAG_DECLARE(EcsUiFocusPreviousTextFieldRequest);
ECS_TAG_DECLARE(EcsUiBlurTextFieldRequest);
ECS_TAG_DECLARE(EcsUiTextDeleteRequest);
ECS_TAG_DECLARE(EcsUiTextCursorLeftRequest);
ECS_TAG_DECLARE(EcsUiTextCursorRightRequest);
ECS_TAG_DECLARE(EcsUiTextCursorStartRequest);
ECS_TAG_DECLARE(EcsUiTextCursorEndRequest);
ECS_TAG_DECLARE(EcsUiTextSelectLeftRequest);
ECS_TAG_DECLARE(EcsUiTextSelectRightRequest);
ECS_TAG_DECLARE(EcsUiTextSelectStartRequest);
ECS_TAG_DECLARE(EcsUiTextSelectEndRequest);

static bool EcsUiTextInputReady(void)
{
    return ecs_id(EcsUiTextField) != 0 &&
        ecs_id(EcsUiTextEditState) != 0 &&
        ecs_id(EcsUiTextInsertRequest) != 0 &&
        EcsUiFocusedTextField != 0 && EcsUiTextFieldUiNode != 0 &&
        EcsUiTextFieldValueUiNode != 0 && EcsUiForTextField != 0 &&
        EcsUiFocusTextFieldRequest != 0 &&
        EcsUiFocusNextTextFieldRequest != 0 &&
        EcsUiFocusPreviousTextFieldRequest != 0 &&
        EcsUiBlurTextFieldRequest != 0 && EcsUiTextDeleteRequest != 0 &&
        EcsUiTextCursorLeftRequest != 0 &&
        EcsUiTextCursorRightRequest != 0 &&
        EcsUiTextCursorStartRequest != 0 &&
        EcsUiTextCursorEndRequest != 0 &&
        EcsUiTextSelectLeftRequest != 0 &&
        EcsUiTextSelectRightRequest != 0 &&
        EcsUiTextSelectStartRequest != 0 &&
        EcsUiTextSelectEndRequest != 0;
}

static void EcsUiTextInputCopyString(
    char *out,
    size_t out_size,
    const char *value)
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

static bool EcsUiTextInputPushChar(
    char *out,
    size_t out_size,
    size_t *out_index,
    char value)
{
    if (out == NULL || out_index == NULL || *out_index + 1u >= out_size) {
        return false;
    }
    out[*out_index] = value;
    *out_index += 1u;
    return true;
}

static void EcsUiTextInputCopyStringWithEditState(
    char *out,
    size_t out_size,
    const char *value,
    uint32_t cursor,
    uint32_t selection_start,
    uint32_t selection_end)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *source = value != NULL ? value : "";
    const size_t source_length = strlen(source);
    const bool has_selection = selection_start < selection_end;
    size_t out_index = 0u;
    for (size_t i = 0u; i <= source_length; i += 1u) {
        bool wrote_cursor = false;
        if (has_selection && i == (size_t)selection_start &&
            i == (size_t)cursor) {
            wrote_cursor =
                EcsUiTextInputPushChar(out, out_size, &out_index, '|');
        }
        if (has_selection && i == (size_t)selection_start) {
            (void)EcsUiTextInputPushChar(out, out_size, &out_index, '[');
        }
        if (has_selection && i == (size_t)selection_end) {
            (void)EcsUiTextInputPushChar(out, out_size, &out_index, ']');
        }
        if (!wrote_cursor && i == (size_t)cursor) {
            (void)EcsUiTextInputPushChar(out, out_size, &out_index, '|');
        }
        if (i < source_length) {
            (void)EcsUiTextInputPushChar(
                out,
                out_size,
                &out_index,
                source[i]);
        }
    }
    out[out_index] = '\0';
}

static uint32_t EcsUiTextInputClampCursor(
    const EcsUiTextField *field_data,
    uint32_t cursor)
{
    size_t length = field_data != NULL ? strlen(field_data->value) : 0u;
    if (length > UINT32_MAX) {
        length = UINT32_MAX;
    }
    return cursor > (uint32_t)length ? (uint32_t)length : cursor;
}

static uint32_t EcsUiTextInputMinU32(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static uint32_t EcsUiTextInputMaxU32(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

static void EcsUiTextInputClearEditSelection(EcsUiTextEditState *edit_state)
{
    if (edit_state == NULL) {
        return;
    }
    edit_state->selection_anchor = edit_state->cursor;
    edit_state->selection_focus = edit_state->cursor;
}

static bool EcsUiTextInputEditStateHasSelection(
    const EcsUiTextEditState *edit_state)
{
    return edit_state != NULL &&
        edit_state->selection_anchor != edit_state->selection_focus;
}

static uint32_t EcsUiTextInputSelectionMin(
    const EcsUiTextEditState *edit_state)
{
    return edit_state != NULL ?
        EcsUiTextInputMinU32(
            edit_state->selection_anchor,
            edit_state->selection_focus) :
        0u;
}

static uint32_t EcsUiTextInputSelectionMax(
    const EcsUiTextEditState *edit_state)
{
    return edit_state != NULL ?
        EcsUiTextInputMaxU32(
            edit_state->selection_anchor,
            edit_state->selection_focus) :
        0u;
}

static EcsUiTextEditState *EcsUiTextInputEnsureEditState(
    ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0) {
        return NULL;
    }

    const EcsUiTextField *field_data =
        ecs_get(world, field, EcsUiTextField);
    if (field_data == NULL) {
        return NULL;
    }

    uint32_t length = EcsUiTextInputClampCursor(field_data, UINT32_MAX);
    bool has_existing_state =
        ecs_get(world, field, EcsUiTextEditState) != NULL;
    if (!has_existing_state) {
        ecs_set(
            world,
            field,
            EcsUiTextEditState,
            {
                .cursor = length,
                .selection_anchor = length,
                .selection_focus = length,
            });
        return ecs_get_mut(world, field, EcsUiTextEditState);
    }

    EcsUiTextEditState *edit_state =
        ecs_get_mut(world, field, EcsUiTextEditState);
    if (edit_state == NULL) {
        return NULL;
    }

    uint32_t cursor =
        edit_state->cursor > length ? length : edit_state->cursor;
    uint32_t selection_anchor =
        edit_state->selection_anchor > length ?
            length :
            edit_state->selection_anchor;
    uint32_t selection_focus =
        edit_state->selection_focus > length ?
            length :
            edit_state->selection_focus;
    bool changed = edit_state->cursor != cursor ||
        edit_state->selection_anchor != selection_anchor ||
        edit_state->selection_focus != selection_focus;
    edit_state->cursor = cursor;
    edit_state->selection_anchor = selection_anchor;
    edit_state->selection_focus = selection_focus;
    if (changed) {
        ecs_modified(world, field, EcsUiTextEditState);
    }
    return edit_state;
}

static ecs_entity_t EcsUiTextInputRootEntity(const ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    return ecs_lookup(world, "EcsUiTextInput");
}

ecs_entity_t EcsUiTextInputRoot(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    ecs_entity_t root = ecs_entity(world, {.name = "EcsUiTextInput"});
    if (root != 0) {
        ecs_add_id(world, root, EcsOrderedChildren);
    }
    return root;
}

ecs_entity_t EcsUiTextInputField(
    ecs_world_t *world,
    const char *name,
    const char *placeholder)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }

    ecs_entity_t root = EcsUiTextInputRoot(world);
    ecs_entity_t field = ecs_entity(world, {
        .parent = root,
        .name = name != NULL && name[0] != '\0' ? name : "EcsUiTextField",
        .sep = "",
    });
    if (field == 0) {
        return 0;
    }

    if (!ecs_has(world, field, EcsUiTextField)) {
        EcsUiTextField text_field = {0};
        EcsUiTextInputCopyString(
            text_field.placeholder,
            sizeof(text_field.placeholder),
            placeholder);
        ecs_set_ptr(world, field, EcsUiTextField, &text_field);
    } else if (placeholder != NULL) {
        (void)EcsUiTextInputSetPlaceholder(world, field, placeholder);
    }
    (void)EcsUiTextInputEnsureEditState(world, field);
    return field;
}

ecs_entity_t EcsUiTextInputFocusedField(const ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }

    ecs_entity_t root = EcsUiTextInputRootEntity(world);
    if (root == 0) {
        return 0;
    }
    return ecs_get_target(world, root, EcsUiFocusedTextField, 0);
}

bool EcsUiTextInputHasFocusedField(const ecs_world_t *world)
{
    return EcsUiTextInputFocusedField(world) != 0;
}

bool EcsUiTextInputIsFocused(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    return field != 0 && EcsUiTextInputFocusedField(world) == field;
}

ecs_entity_t EcsUiTextInputRequestFocusField(
    ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_pair(world, EcsUiFocusTextFieldRequest, field);
}

ecs_entity_t EcsUiTextInputRequestFocusNext(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiFocusNextTextFieldRequest);
}

ecs_entity_t EcsUiTextInputRequestFocusPrevious(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiFocusPreviousTextFieldRequest);
}

ecs_entity_t EcsUiTextInputRequestBlur(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiBlurTextFieldRequest);
}

ecs_entity_t EcsUiTextInputRequestInsert(
    ecs_world_t *world,
    uint32_t codepoint)
{
    if (world == NULL || codepoint == 0u || !EcsUiTextInputReady()) {
        return 0;
    }

    ecs_entity_t request = ecs_new(world);
    ecs_set(
        world,
        request,
        EcsUiTextInsertRequest,
        {
            .codepoint = codepoint,
        });
    return request;
}

ecs_entity_t EcsUiTextInputRequestDelete(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextDeleteRequest);
}

ecs_entity_t EcsUiTextInputRequestMoveCursorLeft(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextCursorLeftRequest);
}

ecs_entity_t EcsUiTextInputRequestMoveCursorRight(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextCursorRightRequest);
}

ecs_entity_t EcsUiTextInputRequestMoveCursorStart(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextCursorStartRequest);
}

ecs_entity_t EcsUiTextInputRequestMoveCursorEnd(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextCursorEndRequest);
}

ecs_entity_t EcsUiTextInputRequestSelectLeft(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextSelectLeftRequest);
}

ecs_entity_t EcsUiTextInputRequestSelectRight(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextSelectRightRequest);
}

ecs_entity_t EcsUiTextInputRequestSelectStart(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextSelectStartRequest);
}

ecs_entity_t EcsUiTextInputRequestSelectEnd(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextSelectEndRequest);
}

const char *EcsUiTextInputValue(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    return field_data != NULL ? field_data->value : "";
}

const char *EcsUiTextInputPlaceholder(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    return field_data != NULL ? field_data->placeholder : "";
}

uint32_t EcsUiTextInputCursor(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    if (field_data == NULL) {
        return 0u;
    }

    const EcsUiTextEditState *edit_state =
        ecs_get(world, field, EcsUiTextEditState);
    return EcsUiTextInputClampCursor(
        field_data,
        edit_state != NULL ?
            edit_state->cursor :
            (uint32_t)strlen(field_data->value));
}

bool EcsUiTextInputHasSelection(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    const EcsUiTextEditState *edit_state =
        field != 0 ? ecs_get(world, field, EcsUiTextEditState) : NULL;
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    uint32_t selection_anchor =
        EcsUiTextInputClampCursor(field_data, edit_state->selection_anchor);
    uint32_t selection_focus =
        EcsUiTextInputClampCursor(field_data, edit_state->selection_focus);
    return selection_anchor != selection_focus;
}

uint32_t EcsUiTextInputSelectionStart(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    const EcsUiTextEditState *edit_state =
        field != 0 ? ecs_get(world, field, EcsUiTextEditState) : NULL;
    if (field_data == NULL || edit_state == NULL) {
        return 0u;
    }

    return EcsUiTextInputMinU32(
        EcsUiTextInputClampCursor(field_data, edit_state->selection_anchor),
        EcsUiTextInputClampCursor(field_data, edit_state->selection_focus));
}

uint32_t EcsUiTextInputSelectionEnd(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    const EcsUiTextEditState *edit_state =
        field != 0 ? ecs_get(world, field, EcsUiTextEditState) : NULL;
    if (field_data == NULL || edit_state == NULL) {
        return 0u;
    }

    return EcsUiTextInputMaxU32(
        EcsUiTextInputClampCursor(field_data, edit_state->selection_anchor),
        EcsUiTextInputClampCursor(field_data, edit_state->selection_focus));
}

bool EcsUiTextInputSetValue(
    ecs_world_t *world,
    ecs_entity_t field,
    const char *value)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return false;
    }

    EcsUiTextField *field_data =
        ecs_get_mut(world, field, EcsUiTextField);
    if (field_data == NULL) {
        return false;
    }

    EcsUiTextInputCopyString(
        field_data->value,
        sizeof(field_data->value),
        value);
    uint32_t length = EcsUiTextInputClampCursor(field_data, UINT32_MAX);
    ecs_modified(world, field, EcsUiTextField);

    EcsUiTextEditState *edit_state =
        EcsUiTextInputEnsureEditState(world, field);
    if (edit_state != NULL) {
        edit_state->cursor =
            edit_state->cursor > length ? length : edit_state->cursor;
        EcsUiTextInputClearEditSelection(edit_state);
        ecs_modified(world, field, EcsUiTextEditState);
    }
    return true;
}

bool EcsUiTextInputSetPlaceholder(
    ecs_world_t *world,
    ecs_entity_t field,
    const char *placeholder)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return false;
    }

    EcsUiTextField *field_data =
        ecs_get_mut(world, field, EcsUiTextField);
    if (field_data == NULL) {
        return false;
    }

    EcsUiTextInputCopyString(
        field_data->placeholder,
        sizeof(field_data->placeholder),
        placeholder);
    ecs_modified(world, field, EcsUiTextField);
    return true;
}

bool EcsUiTextInputClear(
    ecs_world_t *world,
    ecs_entity_t field)
{
    return EcsUiTextInputSetValue(world, field, "");
}

bool EcsUiTextInputSetCursor(
    ecs_world_t *world,
    ecs_entity_t field,
    uint32_t cursor)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return false;
    }

    EcsUiTextEditState *edit_state =
        EcsUiTextInputEnsureEditState(world, field);
    const EcsUiTextField *field_data =
        ecs_get(world, field, EcsUiTextField);
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    uint32_t clamped_cursor =
        EcsUiTextInputClampCursor(field_data, cursor);
    if (edit_state->cursor != clamped_cursor) {
        edit_state->cursor = clamped_cursor;
    }
    EcsUiTextInputClearEditSelection(edit_state);
    ecs_modified(world, field, EcsUiTextEditState);
    return true;
}

bool EcsUiTextInputClearSelection(
    ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return false;
    }

    EcsUiTextEditState *edit_state =
        EcsUiTextInputEnsureEditState(world, field);
    if (edit_state == NULL) {
        return false;
    }

    if (EcsUiTextInputEditStateHasSelection(edit_state)) {
        EcsUiTextInputClearEditSelection(edit_state);
        ecs_modified(world, field, EcsUiTextEditState);
    }
    return true;
}

bool EcsUiTextInputDisplayText(
    const ecs_world_t *world,
    ecs_entity_t field,
    bool include_caret,
    char *out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return false;
    }

    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    if (field_data == NULL) {
        out[0] = '\0';
        return false;
    }

    const bool focused = EcsUiTextInputIsFocused(world, field);
    if (!focused && field_data->value[0] == '\0') {
        EcsUiTextInputCopyString(out, out_size, field_data->placeholder);
    } else if (include_caret && focused) {
        const EcsUiTextEditState *edit_state =
            ecs_get(world, field, EcsUiTextEditState);
        uint32_t cursor = EcsUiTextInputCursor(world, field);
        uint32_t selection_start = edit_state != NULL ?
            EcsUiTextInputSelectionStart(world, field) :
            cursor;
        uint32_t selection_end = edit_state != NULL ?
            EcsUiTextInputSelectionEnd(world, field) :
            cursor;
        EcsUiTextInputCopyStringWithEditState(
            out,
            out_size,
            field_data->value,
            cursor,
            selection_start,
            selection_end);
    } else {
        EcsUiTextInputCopyString(out, out_size, field_data->value);
    }
    return true;
}

bool EcsUiTextInputSetFieldUiNodes(
    ecs_world_t *world,
    ecs_entity_t field,
    ecs_entity_t field_node,
    ecs_entity_t value_node)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return false;
    }

    bool linked = false;
    if (field_node != 0) {
        ecs_add_pair(world, field, EcsUiTextFieldUiNode, field_node);
        linked = true;
    }
    if (value_node != 0) {
        ecs_add_pair(world, field, EcsUiTextFieldValueUiNode, value_node);
        linked = true;
    }
    return linked;
}

bool EcsUiTextInputSetUiField(
    ecs_world_t *world,
    ecs_entity_t ui_node,
    ecs_entity_t field)
{
    if (world == NULL || ui_node == 0 || field == 0 ||
        !EcsUiTextInputReady()) {
        return false;
    }

    ecs_add_pair(world, ui_node, EcsUiForTextField, field);
    return true;
}

ecs_entity_t EcsUiTextInputFieldUiNode(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_get_target(world, field, EcsUiTextFieldUiNode, 0);
}

ecs_entity_t EcsUiTextInputFieldValueUiNode(
    const ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_get_target(world, field, EcsUiTextFieldValueUiNode, 0);
}

ecs_entity_t EcsUiTextInputUiField(
    const ecs_world_t *world,
    ecs_entity_t ui_node)
{
    if (world == NULL || ui_node == 0 || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_get_target(world, ui_node, EcsUiForTextField, 0);
}

static bool EcsUiTextInputDeleteSelection(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state)
{
    if (field_data == NULL || edit_state == NULL ||
        !EcsUiTextInputEditStateHasSelection(edit_state)) {
        return false;
    }

    size_t length = strlen(field_data->value);
    uint32_t selection_start =
        EcsUiTextInputClampCursor(
            field_data,
            EcsUiTextInputSelectionMin(edit_state));
    uint32_t selection_end =
        EcsUiTextInputClampCursor(
            field_data,
            EcsUiTextInputSelectionMax(edit_state));
    if (selection_start >= selection_end) {
        edit_state->cursor = selection_start;
        EcsUiTextInputClearEditSelection(edit_state);
        return false;
    }

    memmove(
        &field_data->value[selection_start],
        &field_data->value[selection_end],
        length - (size_t)selection_end + 1u);
    edit_state->cursor = selection_start;
    EcsUiTextInputClearEditSelection(edit_state);
    return true;
}

static bool EcsUiTextInputInsertCodepoint(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state,
    uint32_t codepoint)
{
    if (field_data == NULL || edit_state == NULL || codepoint < 32u ||
        codepoint > 126u) {
        return false;
    }

    if (EcsUiTextInputEditStateHasSelection(edit_state)) {
        (void)EcsUiTextInputDeleteSelection(field_data, edit_state);
    }

    size_t length = strlen(field_data->value);
    if (length + 1u >= sizeof(field_data->value)) {
        return false;
    }

    uint32_t cursor =
        EcsUiTextInputClampCursor(field_data, edit_state->cursor);
    memmove(
        &field_data->value[cursor + 1u],
        &field_data->value[cursor],
        length - (size_t)cursor + 1u);
    field_data->value[cursor] = (char)codepoint;
    edit_state->cursor = cursor + 1u;
    EcsUiTextInputClearEditSelection(edit_state);
    return true;
}

static bool EcsUiTextInputDeleteBeforeCursor(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state)
{
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    if (EcsUiTextInputEditStateHasSelection(edit_state)) {
        return EcsUiTextInputDeleteSelection(field_data, edit_state);
    }

    size_t length = strlen(field_data->value);
    uint32_t cursor =
        EcsUiTextInputClampCursor(field_data, edit_state->cursor);
    if (length == 0u || cursor == 0u) {
        edit_state->cursor = cursor;
        EcsUiTextInputClearEditSelection(edit_state);
        return false;
    }

    memmove(
        &field_data->value[cursor - 1u],
        &field_data->value[cursor],
        length - (size_t)cursor + 1u);
    edit_state->cursor = cursor - 1u;
    EcsUiTextInputClearEditSelection(edit_state);
    return true;
}

static bool EcsUiTextInputMoveCursor(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state,
    int32_t direction)
{
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    uint32_t cursor =
        EcsUiTextInputClampCursor(field_data, edit_state->cursor);
    uint32_t next_cursor = cursor;
    if (EcsUiTextInputEditStateHasSelection(edit_state)) {
        next_cursor = direction < 0 ?
            EcsUiTextInputSelectionMin(edit_state) :
            EcsUiTextInputSelectionMax(edit_state);
    } else if (direction < 0 && cursor > 0u) {
        next_cursor = cursor - 1u;
    } else if (direction > 0) {
        uint32_t length = EcsUiTextInputClampCursor(field_data, UINT32_MAX);
        if (cursor < length) {
            next_cursor = cursor + 1u;
        }
    }

    bool changed = edit_state->cursor != next_cursor ||
        EcsUiTextInputEditStateHasSelection(edit_state);
    if (!changed) {
        return false;
    }

    edit_state->cursor = next_cursor;
    EcsUiTextInputClearEditSelection(edit_state);
    return true;
}

static bool EcsUiTextInputMoveCursorToEdge(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state,
    bool to_end)
{
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    uint32_t next_cursor =
        to_end ? EcsUiTextInputClampCursor(field_data, UINT32_MAX) : 0u;
    bool changed = edit_state->cursor != next_cursor ||
        EcsUiTextInputEditStateHasSelection(edit_state);
    if (!changed) {
        return false;
    }

    edit_state->cursor = next_cursor;
    EcsUiTextInputClearEditSelection(edit_state);
    return true;
}

static bool EcsUiTextInputExtendSelection(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state,
    int32_t direction)
{
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    uint32_t focus =
        EcsUiTextInputClampCursor(field_data, edit_state->cursor);
    uint32_t next_focus = focus;
    if (direction < 0 && focus > 0u) {
        next_focus = focus - 1u;
    } else if (direction > 0) {
        uint32_t length = EcsUiTextInputClampCursor(field_data, UINT32_MAX);
        if (focus < length) {
            next_focus = focus + 1u;
        }
    }

    if (!EcsUiTextInputEditStateHasSelection(edit_state)) {
        edit_state->selection_anchor = focus;
    }
    bool changed = edit_state->cursor != next_focus ||
        edit_state->selection_focus != next_focus ||
        edit_state->selection_anchor != edit_state->selection_focus;
    edit_state->cursor = next_focus;
    edit_state->selection_focus = next_focus;
    if (edit_state->selection_anchor == edit_state->selection_focus) {
        EcsUiTextInputClearEditSelection(edit_state);
    }
    return changed;
}

static bool EcsUiTextInputExtendSelectionToEdge(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state,
    bool to_end)
{
    if (field_data == NULL || edit_state == NULL) {
        return false;
    }

    uint32_t focus =
        EcsUiTextInputClampCursor(field_data, edit_state->cursor);
    uint32_t next_focus =
        to_end ? EcsUiTextInputClampCursor(field_data, UINT32_MAX) : 0u;
    if (!EcsUiTextInputEditStateHasSelection(edit_state)) {
        edit_state->selection_anchor = focus;
    }
    bool changed = edit_state->cursor != next_focus ||
        edit_state->selection_focus != next_focus ||
        edit_state->selection_anchor != edit_state->selection_focus;
    edit_state->cursor = next_focus;
    edit_state->selection_focus = next_focus;
    if (edit_state->selection_anchor == edit_state->selection_focus) {
        EcsUiTextInputClearEditSelection(edit_state);
    }
    return changed;
}

static void EcsUiTextInputFocusFieldSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field =
            ecs_get_target(
                it->world,
                it->entities[i],
                EcsUiFocusTextFieldRequest,
                0);
        if (field != 0 && ecs_has(it->world, field, EcsUiTextField)) {
            (void)EcsUiTextInputEnsureEditState(it->world, field);
            ecs_add_pair(
                it->world,
                EcsUiTextInputRoot(it->world),
                EcsUiFocusedTextField,
                field);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static ecs_entity_t EcsUiTextInputFieldForTraversal(
    ecs_world_t *world,
    int32_t direction)
{
    if (world == NULL) {
        return 0;
    }

    ecs_entity_t root = EcsUiTextInputRoot(world);
    ecs_entity_t focused = EcsUiTextInputFocusedField(world);
    ecs_entity_t fields[ECS_UI_TREE_NODE_MAX] = {0};
    int32_t field_count = 0;
    int32_t focused_index = -1;
    ecs_entities_t children = ecs_get_ordered_children(world, root);
    for (int32_t i = 0; i < children.count; i += 1) {
        ecs_entity_t child = children.ids[i];
        if (!ecs_has(world, child, EcsUiTextField)) {
            continue;
        }
        if (field_count >= (int32_t)ECS_UI_TREE_NODE_MAX) {
            break;
        }
        if (child == focused) {
            focused_index = field_count;
        }
        fields[field_count] = child;
        field_count += 1;
    }

    if (field_count == 0) {
        return 0;
    }
    if (focused_index < 0) {
        return direction >= 0 ? fields[0] : fields[field_count - 1];
    }

    int32_t next_index =
        direction >= 0 ? focused_index + 1 : focused_index - 1;
    if (next_index >= field_count) {
        next_index = 0;
    } else if (next_index < 0) {
        next_index = field_count - 1;
    }
    return fields[next_index];
}

static void EcsUiTextInputFocusTraversalSystem(ecs_iter_t *it, int32_t direction)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field =
            EcsUiTextInputFieldForTraversal(it->world, direction);
        if (field != 0) {
            (void)EcsUiTextInputEnsureEditState(it->world, field);
            ecs_add_pair(
                it->world,
                EcsUiTextInputRoot(it->world),
                EcsUiFocusedTextField,
                field);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputFocusNextSystem(ecs_iter_t *it)
{
    EcsUiTextInputFocusTraversalSystem(it, 1);
}

static void EcsUiTextInputFocusPreviousSystem(ecs_iter_t *it)
{
    EcsUiTextInputFocusTraversalSystem(it, -1);
}

static void EcsUiTextInputBlurFieldSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_remove_pair(
            it->world,
            EcsUiTextInputRoot(it->world),
            EcsUiFocusedTextField,
            EcsWildcard);
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputInsertSystem(ecs_iter_t *it)
{
    const EcsUiTextInsertRequest *requests =
        ecs_field(it, EcsUiTextInsertRequest, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputInsertCodepoint(
                field_data,
                edit_state,
                requests[i].codepoint)) {
            ecs_modified(it->world, field, EcsUiTextField);
            ecs_modified(it->world, field, EcsUiTextEditState);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputDeleteSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputDeleteBeforeCursor(field_data, edit_state)) {
            ecs_modified(it->world, field, EcsUiTextField);
            ecs_modified(it->world, field, EcsUiTextEditState);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputMoveCursorSystem(ecs_iter_t *it, int32_t direction)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputMoveCursor(field_data, edit_state, direction)) {
            ecs_modified(it->world, field, EcsUiTextEditState);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputCursorLeftSystem(ecs_iter_t *it)
{
    EcsUiTextInputMoveCursorSystem(it, -1);
}

static void EcsUiTextInputCursorRightSystem(ecs_iter_t *it)
{
    EcsUiTextInputMoveCursorSystem(it, 1);
}

static void EcsUiTextInputCursorToEdgeSystem(
    ecs_iter_t *it,
    bool to_end)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputMoveCursorToEdge(
                field_data,
                edit_state,
                to_end)) {
            ecs_modified(it->world, field, EcsUiTextEditState);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputCursorStartSystem(ecs_iter_t *it)
{
    EcsUiTextInputCursorToEdgeSystem(it, false);
}

static void EcsUiTextInputCursorEndSystem(ecs_iter_t *it)
{
    EcsUiTextInputCursorToEdgeSystem(it, true);
}

static void EcsUiTextInputSelectSystem(ecs_iter_t *it, int32_t direction)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputExtendSelection(field_data, edit_state, direction)) {
            ecs_modified(it->world, field, EcsUiTextEditState);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputSelectLeftSystem(ecs_iter_t *it)
{
    EcsUiTextInputSelectSystem(it, -1);
}

static void EcsUiTextInputSelectRightSystem(ecs_iter_t *it)
{
    EcsUiTextInputSelectSystem(it, 1);
}

static void EcsUiTextInputSelectToEdgeSystem(
    ecs_iter_t *it,
    bool to_end)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputExtendSelectionToEdge(
                field_data,
                edit_state,
                to_end)) {
            ecs_modified(it->world, field, EcsUiTextEditState);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputSelectStartSystem(ecs_iter_t *it)
{
    EcsUiTextInputSelectToEdgeSystem(it, false);
}

static void EcsUiTextInputSelectEndSystem(ecs_iter_t *it)
{
    EcsUiTextInputSelectToEdgeSystem(it, true);
}

void EcsUiTextInputImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiTextField);
    ECS_COMPONENT_DEFINE(world, EcsUiTextEditState);
    ECS_COMPONENT_DEFINE(world, EcsUiTextInsertRequest);
    ECS_TAG_DEFINE(world, EcsUiFocusedTextField);
    ECS_TAG_DEFINE(world, EcsUiTextFieldUiNode);
    ECS_TAG_DEFINE(world, EcsUiTextFieldValueUiNode);
    ECS_TAG_DEFINE(world, EcsUiForTextField);
    ECS_TAG_DEFINE(world, EcsUiFocusTextFieldRequest);
    ECS_TAG_DEFINE(world, EcsUiFocusNextTextFieldRequest);
    ECS_TAG_DEFINE(world, EcsUiFocusPreviousTextFieldRequest);
    ECS_TAG_DEFINE(world, EcsUiBlurTextFieldRequest);
    ECS_TAG_DEFINE(world, EcsUiTextDeleteRequest);
    ECS_TAG_DEFINE(world, EcsUiTextCursorLeftRequest);
    ECS_TAG_DEFINE(world, EcsUiTextCursorRightRequest);
    ECS_TAG_DEFINE(world, EcsUiTextCursorStartRequest);
    ECS_TAG_DEFINE(world, EcsUiTextCursorEndRequest);
    ECS_TAG_DEFINE(world, EcsUiTextSelectLeftRequest);
    ECS_TAG_DEFINE(world, EcsUiTextSelectRightRequest);
    ECS_TAG_DEFINE(world, EcsUiTextSelectStartRequest);
    ECS_TAG_DEFINE(world, EcsUiTextSelectEndRequest);

    ecs_add_id(world, EcsUiFocusedTextField, EcsExclusive);
    ecs_add_id(world, EcsUiTextFieldUiNode, EcsExclusive);
    ecs_add_id(world, EcsUiTextFieldValueUiNode, EcsExclusive);
    ecs_add_id(world, EcsUiForTextField, EcsExclusive);
    ecs_add_id(world, EcsUiFocusTextFieldRequest, EcsExclusive);

    (void)EcsUiTextInputRoot(world);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputFocusFieldSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(EcsUiFocusTextFieldRequest, EcsWildcard)},
        },
        .callback = EcsUiTextInputFocusFieldSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputBlurFieldSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiBlurTextFieldRequest},
        },
        .callback = EcsUiTextInputBlurFieldSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputFocusNextSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiFocusNextTextFieldRequest},
        },
        .callback = EcsUiTextInputFocusNextSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputFocusPreviousSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiFocusPreviousTextFieldRequest},
        },
        .callback = EcsUiTextInputFocusPreviousSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputInsertSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(EcsUiTextInsertRequest)},
        },
        .callback = EcsUiTextInputInsertSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputDeleteSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextDeleteRequest},
        },
        .callback = EcsUiTextInputDeleteSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputCursorLeftSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextCursorLeftRequest},
        },
        .callback = EcsUiTextInputCursorLeftSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputCursorRightSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextCursorRightRequest},
        },
        .callback = EcsUiTextInputCursorRightSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputCursorStartSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextCursorStartRequest},
        },
        .callback = EcsUiTextInputCursorStartSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputCursorEndSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextCursorEndRequest},
        },
        .callback = EcsUiTextInputCursorEndSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputSelectLeftSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextSelectLeftRequest},
        },
        .callback = EcsUiTextInputSelectLeftSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputSelectRightSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextSelectRightRequest},
        },
        .callback = EcsUiTextInputSelectRightSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputSelectStartSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextSelectStartRequest},
        },
        .callback = EcsUiTextInputSelectStartSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputSelectEndSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextSelectEndRequest},
        },
        .callback = EcsUiTextInputSelectEndSystem,
    });
}
