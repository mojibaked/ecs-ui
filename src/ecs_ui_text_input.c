#include "ecs_ui/ecs_ui_text_input.h"

#include <string.h>

typedef struct EcsUiTextFocusRequestSequence {
    uint64_t sequence;
} EcsUiTextFocusRequestSequence;

typedef struct EcsUiTextFocusRequestAccumulator {
    uint64_t next_sequence;
    uint64_t focus_sequence;
    uint64_t traversal_sequence;
    uint64_t blur_sequence;
    ecs_entity_t focus_field;
    int32_t traversal_direction;
    bool has_focus;
    bool has_traversal;
    bool has_blur;
} EcsUiTextFocusRequestAccumulator;

typedef struct EcsUiTextInputState {
    uint64_t revision;
    uint64_t projected_revision;
} EcsUiTextInputState;

ECS_COMPONENT_DECLARE(EcsUiTextField);
ECS_COMPONENT_DECLARE(EcsUiTextEditState);
ECS_COMPONENT_DECLARE(EcsUiTextInsertRequest);
ECS_COMPONENT_DECLARE(EcsUiTextPasteRequest);
ECS_COMPONENT_DECLARE(EcsUiTextClipboardWriteRequest);
ECS_COMPONENT_DECLARE(EcsUiTextFocusRequestSequence);
ECS_COMPONENT_DECLARE(EcsUiTextFocusRequestAccumulator);
ECS_COMPONENT_DECLARE(EcsUiTextInputState);
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
ECS_TAG_DECLARE(EcsUiTextCopyRequest);
ECS_TAG_DECLARE(EcsUiTextCutRequest);

static bool EcsUiTextInputReady(void)
{
    return ecs_id(EcsUiTextField) != 0 &&
        ecs_id(EcsUiTextFieldView) != 0 &&
        ecs_id(EcsUiTextEditState) != 0 &&
        ecs_id(EcsUiTextInsertRequest) != 0 &&
        ecs_id(EcsUiTextPasteRequest) != 0 &&
        ecs_id(EcsUiTextClipboardWriteRequest) != 0 &&
        ecs_id(EcsUiTextFocusRequestSequence) != 0 &&
        ecs_id(EcsUiTextFocusRequestAccumulator) != 0 &&
        ecs_id(EcsUiTextInputState) != 0 &&
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
        EcsUiTextSelectEndRequest != 0 &&
        EcsUiTextCopyRequest != 0 &&
        EcsUiTextCutRequest != 0;
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

static void EcsUiTextInputCopySubstring(
    char *out,
    size_t out_size,
    const char *value,
    uint32_t start,
    uint32_t end)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *source = value != NULL ? value : "";
    size_t length = strlen(source);
    size_t copy_start = start <= length ? (size_t)start : length;
    size_t copy_end = end <= length ? (size_t)end : length;
    if (copy_start > copy_end) {
        size_t swap = copy_start;
        copy_start = copy_end;
        copy_end = swap;
    }

    size_t out_index = 0u;
    for (size_t i = copy_start;
         i < copy_end && out_index + 1u < out_size;
         i += 1u) {
        out[out_index] = source[i];
        out_index += 1u;
    }
    out[out_index] = '\0';
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

static uint64_t EcsUiTextInputNextRevision(uint64_t revision)
{
    revision += 1u;
    return revision != 0u ? revision : 1u;
}

static void EcsUiTextInputBumpStateRevision(ecs_world_t *world)
{
    if (world == NULL || ecs_id(EcsUiTextInputState) == 0) {
        return;
    }

    ecs_entity_t root = EcsUiTextInputRoot(world);
    EcsUiTextInputState *state =
        root != 0 ? ecs_get_mut(world, root, EcsUiTextInputState) : NULL;
    if (state == NULL) {
        return;
    }
    state->revision = EcsUiTextInputNextRevision(state->revision);
    ecs_modified(world, root, EcsUiTextInputState);
}

ecs_entity_t EcsUiTextInputRoot(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }
    ecs_entity_t root = ecs_entity(world, {.name = "EcsUiTextInput"});
    if (root != 0) {
        ecs_add_id(world, root, EcsOrderedChildren);
        if (ecs_id(EcsUiTextInputState) != 0 &&
            !ecs_has(world, root, EcsUiTextInputState)) {
            ecs_set(
                world,
                root,
                EcsUiTextInputState,
                {
                    .revision = 0u,
                    .projected_revision = 0u,
                });
        }
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

uint64_t EcsUiTextInputStateRevision(const ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0u;
    }

    ecs_entity_t root = EcsUiTextInputRootEntity(world);
    const EcsUiTextInputState *state =
        root != 0 ? ecs_get(world, root, EcsUiTextInputState) : NULL;
    return state != NULL ? state->revision : 0u;
}

static bool EcsUiTextInputFocusAccumulator(
    ecs_world_t *world,
    ecs_entity_t *out_root,
    EcsUiTextFocusRequestAccumulator *out_accumulator)
{
    if (out_root != NULL) {
        *out_root = 0;
    }
    if (out_accumulator != NULL) {
        *out_accumulator = (EcsUiTextFocusRequestAccumulator){0};
    }
    if (world == NULL || out_accumulator == NULL) {
        return false;
    }
    ecs_entity_t root = EcsUiTextInputRoot(world);
    if (root == 0) {
        return false;
    }
    if (out_root != NULL) {
        *out_root = root;
    }
    const EcsUiTextFocusRequestAccumulator *existing =
        ecs_get(world, root, EcsUiTextFocusRequestAccumulator);
    if (existing != NULL) {
        *out_accumulator = *existing;
    }
    return true;
}

static void EcsUiTextInputStoreFocusAccumulator(
    ecs_world_t *world,
    ecs_entity_t root,
    const EcsUiTextFocusRequestAccumulator *accumulator)
{
    if (world == NULL || root == 0 || accumulator == NULL) {
        return;
    }
    ecs_set_ptr(world, root, EcsUiTextFocusRequestAccumulator, accumulator);
}

static uint64_t EcsUiTextInputNextFocusSequence(
    EcsUiTextFocusRequestAccumulator *accumulator)
{
    if (accumulator == NULL) {
        return 0u;
    }
    accumulator->next_sequence += 1u;
    if (accumulator->next_sequence == 0u) {
        accumulator->next_sequence = 1u;
    }
    return accumulator->next_sequence;
}

static void EcsUiTextInputStampFocusRequest(
    ecs_world_t *world,
    ecs_entity_t request,
    uint64_t sequence)
{
    if (world == NULL || request == 0 || sequence == 0u) {
        return;
    }
    ecs_set(
        world,
        request,
        EcsUiTextFocusRequestSequence,
        {
            .sequence = sequence,
        });
}

ecs_entity_t EcsUiTextInputRequestFocusField(
    ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return 0;
    }
    ecs_entity_t request =
        ecs_new_w_pair(world, EcsUiFocusTextFieldRequest, field);
    ecs_entity_t root = 0;
    EcsUiTextFocusRequestAccumulator accumulator = {0};
    bool has_accumulator =
        EcsUiTextInputFocusAccumulator(world, &root, &accumulator);
    uint64_t sequence = EcsUiTextInputNextFocusSequence(&accumulator);
    if (request == 0 || !has_accumulator || sequence == 0u) {
        if (request != 0) {
            ecs_delete(world, request);
        }
        return 0;
    }
    accumulator.has_focus = true;
    accumulator.focus_sequence = sequence;
    accumulator.focus_field = field;
    EcsUiTextInputStampFocusRequest(world, request, sequence);
    EcsUiTextInputStoreFocusAccumulator(world, root, &accumulator);
    return request;
}

ecs_entity_t EcsUiTextInputRequestFocusNext(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    ecs_entity_t request = ecs_new_w_id(world, EcsUiFocusNextTextFieldRequest);
    ecs_entity_t root = 0;
    EcsUiTextFocusRequestAccumulator accumulator = {0};
    bool has_accumulator =
        EcsUiTextInputFocusAccumulator(world, &root, &accumulator);
    uint64_t sequence = EcsUiTextInputNextFocusSequence(&accumulator);
    if (request == 0 || !has_accumulator || sequence == 0u) {
        if (request != 0) {
            ecs_delete(world, request);
        }
        return 0;
    }
    accumulator.has_traversal = true;
    accumulator.traversal_sequence = sequence;
    accumulator.traversal_direction = 1;
    EcsUiTextInputStampFocusRequest(world, request, sequence);
    EcsUiTextInputStoreFocusAccumulator(world, root, &accumulator);
    return request;
}

ecs_entity_t EcsUiTextInputRequestFocusPrevious(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    ecs_entity_t request =
        ecs_new_w_id(world, EcsUiFocusPreviousTextFieldRequest);
    ecs_entity_t root = 0;
    EcsUiTextFocusRequestAccumulator accumulator = {0};
    bool has_accumulator =
        EcsUiTextInputFocusAccumulator(world, &root, &accumulator);
    uint64_t sequence = EcsUiTextInputNextFocusSequence(&accumulator);
    if (request == 0 || !has_accumulator || sequence == 0u) {
        if (request != 0) {
            ecs_delete(world, request);
        }
        return 0;
    }
    accumulator.has_traversal = true;
    accumulator.traversal_sequence = sequence;
    accumulator.traversal_direction = -1;
    EcsUiTextInputStampFocusRequest(world, request, sequence);
    EcsUiTextInputStoreFocusAccumulator(world, root, &accumulator);
    return request;
}

ecs_entity_t EcsUiTextInputRequestBlur(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    ecs_entity_t request = ecs_new_w_id(world, EcsUiBlurTextFieldRequest);
    ecs_entity_t root = 0;
    EcsUiTextFocusRequestAccumulator accumulator = {0};
    bool has_accumulator =
        EcsUiTextInputFocusAccumulator(world, &root, &accumulator);
    uint64_t sequence = EcsUiTextInputNextFocusSequence(&accumulator);
    if (request == 0 || !has_accumulator || sequence == 0u) {
        if (request != 0) {
            ecs_delete(world, request);
        }
        return 0;
    }
    accumulator.has_blur = true;
    accumulator.blur_sequence = sequence;
    EcsUiTextInputStampFocusRequest(world, request, sequence);
    EcsUiTextInputStoreFocusAccumulator(world, root, &accumulator);
    return request;
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

ecs_entity_t EcsUiTextInputRequestCopy(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextCopyRequest);
}

ecs_entity_t EcsUiTextInputRequestCut(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return 0;
    }
    return ecs_new_w_id(world, EcsUiTextCutRequest);
}

ecs_entity_t EcsUiTextInputRequestPaste(
    ecs_world_t *world,
    const char *text)
{
    if (world == NULL || text == NULL || !EcsUiTextInputReady()) {
        return 0;
    }

    ecs_entity_t request = ecs_new(world);
    EcsUiTextPasteRequest paste = {0};
    EcsUiTextInputCopyString(paste.text, sizeof(paste.text), text);
    ecs_set_ptr(world, request, EcsUiTextPasteRequest, &paste);
    return request;
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

    char copied_value[ECS_UI_TEXT_MAX] = {0};
    EcsUiTextInputCopyString(
        copied_value,
        sizeof(copied_value),
        value);
    bool changed = strcmp(field_data->value, copied_value) != 0;
    if (changed) {
        EcsUiTextInputCopyString(
            field_data->value,
            sizeof(field_data->value),
            copied_value);
        ecs_modified(world, field, EcsUiTextField);
    }
    uint32_t length = EcsUiTextInputClampCursor(field_data, UINT32_MAX);

    EcsUiTextEditState *edit_state =
        EcsUiTextInputEnsureEditState(world, field);
    if (edit_state != NULL) {
        uint32_t next_cursor =
            edit_state->cursor > length ? length : edit_state->cursor;
        bool edit_changed = edit_state->cursor != next_cursor ||
            edit_state->selection_anchor != next_cursor ||
            edit_state->selection_focus != next_cursor;
        if (edit_changed) {
            edit_state->cursor = next_cursor;
            EcsUiTextInputClearEditSelection(edit_state);
            ecs_modified(world, field, EcsUiTextEditState);
            changed = true;
        }
    }
    if (changed) {
        EcsUiTextInputBumpStateRevision(world);
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

    char copied_placeholder[ECS_UI_TEXT_MAX] = {0};
    EcsUiTextInputCopyString(
        copied_placeholder,
        sizeof(copied_placeholder),
        placeholder);
    if (strcmp(field_data->placeholder, copied_placeholder) == 0) {
        return true;
    }
    EcsUiTextInputCopyString(
        field_data->placeholder,
        sizeof(field_data->placeholder),
        copied_placeholder);
    ecs_modified(world, field, EcsUiTextField);
    EcsUiTextInputBumpStateRevision(world);
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
    bool changed = edit_state->cursor != clamped_cursor ||
        EcsUiTextInputEditStateHasSelection(edit_state);
    if (changed) {
        edit_state->cursor = clamped_cursor;
        EcsUiTextInputClearEditSelection(edit_state);
        ecs_modified(world, field, EcsUiTextEditState);
        EcsUiTextInputBumpStateRevision(world);
    }
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
        EcsUiTextInputBumpStateRevision(world);
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

bool EcsUiTextInputApplyEvent(
    ecs_world_t *world,
    const EcsUiEvent *event)
{
    if (world == NULL || event == NULL || !EcsUiTextInputReady()) {
        return false;
    }

    const bool has_focus = EcsUiTextInputHasFocusedField(world);
    switch (event->type) {
    case ECS_UI_EVENT_TEXT_INPUT:
        return has_focus &&
            EcsUiTextInputRequestInsert(world, event->codepoint) != 0;
    case ECS_UI_EVENT_TEXT_DELETE:
        return has_focus && EcsUiTextInputRequestDelete(world) != 0;
    case ECS_UI_EVENT_TEXT_CANCEL:
        return has_focus && EcsUiTextInputRequestBlur(world) != 0;
    case ECS_UI_EVENT_TEXT_FOCUS_NEXT:
        return has_focus && EcsUiTextInputRequestFocusNext(world) != 0;
    case ECS_UI_EVENT_TEXT_FOCUS_PREVIOUS:
        return has_focus && EcsUiTextInputRequestFocusPrevious(world) != 0;
    case ECS_UI_EVENT_TEXT_CURSOR_LEFT:
        return has_focus &&
            EcsUiTextInputRequestMoveCursorLeft(world) != 0;
    case ECS_UI_EVENT_TEXT_CURSOR_RIGHT:
        return has_focus &&
            EcsUiTextInputRequestMoveCursorRight(world) != 0;
    case ECS_UI_EVENT_TEXT_CURSOR_START:
        return has_focus &&
            EcsUiTextInputRequestMoveCursorStart(world) != 0;
    case ECS_UI_EVENT_TEXT_CURSOR_END:
        return has_focus &&
            EcsUiTextInputRequestMoveCursorEnd(world) != 0;
    case ECS_UI_EVENT_TEXT_SELECT_LEFT:
        return has_focus && EcsUiTextInputRequestSelectLeft(world) != 0;
    case ECS_UI_EVENT_TEXT_SELECT_RIGHT:
        return has_focus && EcsUiTextInputRequestSelectRight(world) != 0;
    case ECS_UI_EVENT_TEXT_SELECT_START:
        return has_focus && EcsUiTextInputRequestSelectStart(world) != 0;
    case ECS_UI_EVENT_TEXT_SELECT_END:
        return has_focus && EcsUiTextInputRequestSelectEnd(world) != 0;
    case ECS_UI_EVENT_TEXT_COPY:
        return has_focus && EcsUiTextInputRequestCopy(world) != 0;
    case ECS_UI_EVENT_TEXT_CUT:
        return has_focus && EcsUiTextInputRequestCut(world) != 0;
    case ECS_UI_EVENT_TEXT_PASTE:
        return has_focus &&
            EcsUiTextInputRequestPaste(world, event->text) != 0;
    case ECS_UI_EVENT_CLICKED: {
        ecs_entity_t field = EcsUiTextInputUiField(world, event->node);
        if (field != 0) {
            return EcsUiTextInputRequestFocusField(world, field) != 0;
        }
        if (has_focus) {
            (void)EcsUiTextInputRequestBlur(world);
        }
        return false;
    }
    case ECS_UI_EVENT_TEXT_SUBMIT:
    case ECS_UI_EVENT_NONE:
    case ECS_UI_EVENT_HOVERED:
    case ECS_UI_EVENT_PRESSED:
    case ECS_UI_EVENT_DRAG_STARTED:
    case ECS_UI_EVENT_DRAGGED:
    case ECS_UI_EVENT_DRAG_ENDED:
    default:
        return false;
    }
}

uint32_t EcsUiTextInputApplyEvents(
    ecs_world_t *world,
    const EcsUiEventList *events)
{
    if (world == NULL || events == NULL) {
        return 0u;
    }

    uint32_t consumed = 0u;
    for (uint32_t i = 0u; i < events->count; i += 1u) {
        if (EcsUiTextInputApplyEvent(world, &events->events[i])) {
            consumed += 1u;
        }
    }
    return consumed;
}

static bool EcsUiApplyFrameEventShouldConsumePolicyEvent(
    const EcsUiEvent *event,
    uint32_t flags)
{
    if (event == NULL) {
        return false;
    }
    switch (event->type) {
    case ECS_UI_EVENT_TEXT_SUBMIT:
        return (flags & ECS_UI_APPLY_FRAME_EVENTS_CONSUME_TEXT_SUBMIT) != 0u;
    case ECS_UI_EVENT_TEXT_CANCEL:
        return (flags & ECS_UI_APPLY_FRAME_EVENTS_CONSUME_TEXT_CANCEL) != 0u;
    default:
        return false;
    }
}

static bool EcsUiApplyFrameEventPreservesPolicyEvent(
    const EcsUiEvent *event,
    uint32_t flags)
{
    if (event == NULL) {
        return false;
    }
    switch (event->type) {
    case ECS_UI_EVENT_TEXT_SUBMIT:
    case ECS_UI_EVENT_TEXT_CANCEL:
        return !EcsUiApplyFrameEventShouldConsumePolicyEvent(event, flags);
    default:
        return false;
    }
}

EcsUiApplyFrameEventsResult EcsUiApplyFrameEvents(
    ecs_world_t *world,
    const EcsUiEventList *in_events,
    EcsUiEventList *out_remaining,
    uint32_t flags)
{
    EcsUiApplyFrameEventsResult result = {0};
    if (out_remaining != NULL) {
        EcsUiEventListClear(out_remaining);
    }
    if (in_events == NULL) {
        return result;
    }

    result.input_count = in_events->count;
    result.input_truncated =
        in_events->truncated || in_events->count > ECS_UI_EVENT_MAX;
    const uint32_t input_count =
        in_events->count > ECS_UI_EVENT_MAX
            ? ECS_UI_EVENT_MAX
            : in_events->count;
    for (uint32_t i = 0u; i < input_count; i += 1u) {
        const EcsUiEvent *event = &in_events->events[i];
        bool consumed = false;
        if (EcsUiApplyFrameEventPreservesPolicyEvent(event, flags)) {
            consumed = false;
        } else if (EcsUiApplyFrameEventShouldConsumePolicyEvent(event, flags)) {
            if (event->type == ECS_UI_EVENT_TEXT_CANCEL) {
                (void)EcsUiTextInputApplyEvent(world, event);
            }
            consumed = true;
        } else {
            consumed = EcsUiTextInputApplyEvent(world, event);
        }

        if (consumed) {
            result.consumed_count += 1u;
            continue;
        }

        if (!EcsUiEventListPush(out_remaining, event)) {
            result.output_truncated = true;
        }
    }

    if (out_remaining != NULL) {
        if (result.input_truncated) {
            out_remaining->truncated = true;
        }
        result.remaining_count = out_remaining->count;
        result.output_truncated =
            result.output_truncated || out_remaining->truncated;
    }
    return result;
}

bool EcsUiTextInputPopClipboardWrite(
    ecs_world_t *world,
    char *out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return false;
    }
    out[0] = '\0';
    if (world == NULL || !EcsUiTextInputReady()) {
        return false;
    }

    ecs_query_t *query = ecs_query(world, {
        .terms = {
            {.id = ecs_id(EcsUiTextClipboardWriteRequest)},
        },
    });
    if (query == NULL) {
        return false;
    }

    bool found = false;
    ecs_entity_t request = 0;
    ecs_iter_t it = ecs_query_iter(world, query);
    while (!found && ecs_query_next(&it)) {
        const EcsUiTextClipboardWriteRequest *writes =
            ecs_field(&it, EcsUiTextClipboardWriteRequest, 0);
        for (int32_t i = 0; i < it.count; i += 1) {
            EcsUiTextInputCopyString(out, out_size, writes[i].text);
            request = it.entities[i];
            found = true;
            break;
        }
    }
    if (found) {
        ecs_iter_fini(&it);
    }
    ecs_query_fini(query);

    if (request != 0) {
        ecs_delete(world, request);
    }
    return found;
}

bool EcsUiTextInputProjectFieldView(
    ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !EcsUiTextInputReady()) {
        return false;
    }

    const EcsUiTextField *field_data = ecs_get(world, field, EcsUiTextField);
    if (field_data == NULL) {
        return false;
    }

    const bool focused = EcsUiTextInputIsFocused(world, field);
    char display[ECS_UI_TEXT_MAX] = {0};
    if (!EcsUiTextInputDisplayText(
            world,
            field,
            false,
            display,
            sizeof(display))) {
        return false;
    }

    bool projected = false;
    ecs_entity_t value_node =
        EcsUiTextInputFieldValueUiNode(world, field);
    EcsUiText *text =
        value_node != 0 ? ecs_get_mut(world, value_node, EcsUiText) : NULL;
    if (text != NULL) {
        EcsUiTextRole role =
            field_data->value[0] != '\0' || focused ?
                ECS_UI_TEXT_BUTTON :
                ECS_UI_TEXT_CAPTION;
        if (text->role != role || strcmp(text->text, display) != 0) {
            EcsUiTextInputCopyString(text->text, sizeof(text->text), display);
            text->role = role;
            ecs_modified(world, value_node, EcsUiText);
        }
        projected = true;
    }

    ecs_entity_t field_node =
        EcsUiTextInputFieldUiNode(world, field);
    if (field_node != 0) {
        const EcsUiTextEditState *edit_state =
            ecs_get(world, field, EcsUiTextEditState);
        const uint32_t cursor = EcsUiTextInputCursor(world, field);
        EcsUiTextFieldView view;
        memset(&view, 0, sizeof(view));
        view.value_node = value_node;
        view.cursor = cursor;
        view.selection_anchor = edit_state != NULL ?
            edit_state->selection_anchor :
            cursor;
        view.selection_focus = edit_state != NULL ?
            edit_state->selection_focus :
            cursor;
        view.caret_width = 2.0f;
        view.focused = focused;
        const EcsUiTextFieldView *existing_view =
            ecs_get(world, field_node, EcsUiTextFieldView);
        if (existing_view == NULL ||
            memcmp(existing_view, &view, sizeof(view)) != 0) {
            ecs_set_ptr(world, field_node, EcsUiTextFieldView, &view);
        }

        const float highlight = focused ? 0.22f : 0.0f;
        const EcsUiVisual *existing =
            ecs_get(world, field_node, EcsUiVisual);
        EcsUiVisual visual = existing != NULL ? *existing : (EcsUiVisual){0};
        visual.opacity = 1.0f;
        visual.highlight = highlight;
        if (existing == NULL ||
            memcmp(existing, &visual, sizeof(visual)) != 0) {
            ecs_set_ptr(world, field_node, EcsUiVisual, &visual);
        }
        projected = true;
    }

    return projected;
}

ecs_entity_t EcsUiTextInputBuildFieldView(
    EcsUiBuilder *builder,
    ecs_entity_t field,
    EcsUiTextFieldViewDesc desc)
{
    if (builder == NULL || builder->world == NULL || field == 0 ||
        !EcsUiTextInputReady()) {
        return 0;
    }

    ecs_entity_t field_node = EcsUiBeginPressable(
        builder,
        (EcsUiPressableDesc){
            .id = desc.field_id,
        });
    ecs_entity_t value_node = EcsUiAddText(
        builder,
        (EcsUiTextDesc){
            .id = desc.value_id,
            .text = "",
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiEnd(builder);

    if (!EcsUiBuilderOk(builder) || field_node == 0 || value_node == 0) {
        return field_node;
    }

    (void)EcsUiTextInputSetFieldUiNodes(
        builder->world,
        field,
        field_node,
        value_node);
    (void)EcsUiTextInputSetUiField(builder->world, field_node, field);
    if (desc.style_token != 0 && EcsUiUsesStyle != 0) {
        ecs_add_pair(
            builder->world,
            field_node,
            EcsUiUsesStyle,
            desc.style_token);
    }
    (void)EcsUiTextInputProjectFieldView(builder->world, field);
    return field_node;
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

static bool EcsUiTextInputInsertString(
    EcsUiTextField *field_data,
    EcsUiTextEditState *edit_state,
    const char *text)
{
    if (field_data == NULL || edit_state == NULL || text == NULL) {
        return false;
    }

    bool changed = false;
    if (EcsUiTextInputEditStateHasSelection(edit_state)) {
        changed = EcsUiTextInputDeleteSelection(field_data, edit_state);
    }
    for (size_t i = 0u; text[i] != '\0'; i += 1u) {
        if (EcsUiTextInputInsertCodepoint(
                field_data,
                edit_state,
                (uint32_t)(unsigned char)text[i])) {
            changed = true;
        }
    }
    return changed;
}

static bool EcsUiTextInputCopySelectedText(
    const EcsUiTextField *field_data,
    const EcsUiTextEditState *edit_state,
    char *out,
    size_t out_size)
{
    if (field_data == NULL || edit_state == NULL || out == NULL ||
        out_size == 0u ||
        !EcsUiTextInputEditStateHasSelection(edit_state)) {
        return false;
    }

    EcsUiTextInputCopySubstring(
        out,
        out_size,
        field_data->value,
        EcsUiTextInputSelectionMin(edit_state),
        EcsUiTextInputSelectionMax(edit_state));
    return true;
}

static bool EcsUiTextInputCreateClipboardWrite(
    ecs_world_t *world,
    const char *text)
{
    if (world == NULL || text == NULL) {
        return false;
    }

    EcsUiTextClipboardWriteRequest write = {0};
    EcsUiTextInputCopyString(write.text, sizeof(write.text), text);
    ecs_entity_t request = ecs_new(world);
    ecs_set_ptr(world, request, EcsUiTextClipboardWriteRequest, &write);
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

static bool EcsUiTextInputFocusField(
    ecs_world_t *world,
    ecs_entity_t field)
{
    if (world == NULL || field == 0 || !ecs_has(world, field, EcsUiTextField)) {
        return false;
    }
    if (EcsUiTextInputFocusedField(world) == field) {
        (void)EcsUiTextInputEnsureEditState(world, field);
        return false;
    }
    (void)EcsUiTextInputEnsureEditState(world, field);
    ecs_add_pair(
        world,
        EcsUiTextInputRoot(world),
        EcsUiFocusedTextField,
        field);
    EcsUiTextInputBumpStateRevision(world);
    return true;
}

static bool EcsUiTextInputBlurFocusedField(ecs_world_t *world)
{
    if (world == NULL) {
        return false;
    }
    if (!EcsUiTextInputHasFocusedField(world)) {
        return false;
    }
    ecs_remove_pair(
        world,
        EcsUiTextInputRoot(world),
        EcsUiFocusedTextField,
        EcsWildcard);
    EcsUiTextInputBumpStateRevision(world);
    return true;
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

static void EcsUiTextInputDeleteFocusRequestEntities(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }
    ecs_delete_with(world, ecs_pair(EcsUiFocusTextFieldRequest, EcsWildcard));
    ecs_delete_with(world, EcsUiFocusNextTextFieldRequest);
    ecs_delete_with(world, EcsUiFocusPreviousTextFieldRequest);
    ecs_delete_with(world, EcsUiBlurTextFieldRequest);
}

static void EcsUiTextInputResolveFocusRequestsSystem(ecs_iter_t *it)
{
    EcsUiTextFocusRequestAccumulator *requests =
        ecs_field(it, EcsUiTextFocusRequestAccumulator, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        EcsUiTextFocusRequestAccumulator request = requests[i];
        if (request.has_focus) {
            EcsUiTextInputFocusField(it->world, request.focus_field);
        } else if (request.has_traversal) {
            EcsUiTextInputFocusField(
                it->world,
                EcsUiTextInputFieldForTraversal(
                    it->world,
                    request.traversal_direction));
        } else if (request.has_blur) {
            EcsUiTextInputBlurFocusedField(it->world);
        }

        EcsUiTextInputDeleteFocusRequestEntities(it->world);
        ecs_remove(
            it->world,
            it->entities[i],
            EcsUiTextFocusRequestAccumulator);
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
            EcsUiTextInputBumpStateRevision(it->world);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputPasteSystem(ecs_iter_t *it)
{
    const EcsUiTextPasteRequest *requests =
        ecs_field(it, EcsUiTextPasteRequest, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputInsertString(
                field_data,
                edit_state,
                requests[i].text)) {
            ecs_modified(it->world, field, EcsUiTextField);
            ecs_modified(it->world, field, EcsUiTextEditState);
            EcsUiTextInputBumpStateRevision(it->world);
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
            EcsUiTextInputBumpStateRevision(it->world);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputClipboardSystem(ecs_iter_t *it, bool cut)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextEditState *edit_state =
            EcsUiTextInputEnsureEditState(it->world, field);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        char clipboard_text[ECS_UI_TEXT_MAX] = {0};
        if (EcsUiTextInputCopySelectedText(
                field_data,
                edit_state,
                clipboard_text,
                sizeof(clipboard_text))) {
            if (cut && EcsUiTextInputDeleteSelection(field_data, edit_state)) {
                ecs_modified(it->world, field, EcsUiTextField);
                ecs_modified(it->world, field, EcsUiTextEditState);
                EcsUiTextInputBumpStateRevision(it->world);
            }
            (void)EcsUiTextInputCreateClipboardWrite(
                it->world,
                clipboard_text);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputCopySystem(ecs_iter_t *it)
{
    EcsUiTextInputClipboardSystem(it, false);
}

static void EcsUiTextInputCutSystem(ecs_iter_t *it)
{
    EcsUiTextInputClipboardSystem(it, true);
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
            EcsUiTextInputBumpStateRevision(it->world);
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
            EcsUiTextInputBumpStateRevision(it->world);
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
            EcsUiTextInputBumpStateRevision(it->world);
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
            EcsUiTextInputBumpStateRevision(it->world);
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

static bool EcsUiTextInputProjectLiveFieldViews(ecs_world_t *world)
{
    if (world == NULL || !EcsUiTextInputReady()) {
        return false;
    }

    ecs_entity_t root = EcsUiTextInputRootEntity(world);
    if (root == 0) {
        return false;
    }

    ecs_entities_t children = ecs_get_ordered_children(world, root);
    for (int32_t i = 0; i < children.count; i += 1) {
        ecs_entity_t field = children.ids[i];
        if (ecs_has(world, field, EcsUiTextField) &&
            EcsUiTextInputFieldUiNode(world, field) != 0) {
            (void)EcsUiTextInputProjectFieldView(world, field);
        }
    }
    return true;
}

static void EcsUiTextInputProjectFieldViewsSystem(ecs_iter_t *it)
{
    EcsUiTextInputState *states =
        ecs_field(it, EcsUiTextInputState, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        uint64_t revision = states[i].revision;
        if (revision == states[i].projected_revision) {
            continue;
        }
        if (!EcsUiTextInputProjectLiveFieldViews(it->world)) {
            continue;
        }

        EcsUiTextInputState *state =
            ecs_get_mut(it->world, it->entities[i], EcsUiTextInputState);
        if (state != NULL) {
            state->projected_revision = revision;
            ecs_modified(it->world, it->entities[i], EcsUiTextInputState);
        }
    }
}

void EcsUiTextInputImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiTextField);
    ECS_COMPONENT_DEFINE(world, EcsUiTextEditState);
    ECS_COMPONENT_DEFINE(world, EcsUiTextInsertRequest);
    ECS_COMPONENT_DEFINE(world, EcsUiTextPasteRequest);
    ECS_COMPONENT_DEFINE(world, EcsUiTextClipboardWriteRequest);
    ECS_COMPONENT_DEFINE(world, EcsUiTextFocusRequestSequence);
    ECS_COMPONENT_DEFINE(world, EcsUiTextFocusRequestAccumulator);
    ECS_COMPONENT_DEFINE(world, EcsUiTextInputState);
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
    ECS_TAG_DEFINE(world, EcsUiTextCopyRequest);
    ECS_TAG_DEFINE(world, EcsUiTextCutRequest);

    ecs_add_id(world, EcsUiFocusedTextField, EcsExclusive);
    ecs_add_id(world, EcsUiTextFieldUiNode, EcsExclusive);
    ecs_add_id(world, EcsUiTextFieldValueUiNode, EcsExclusive);
    ecs_add_id(world, EcsUiForTextField, EcsExclusive);
    ecs_add_id(world, EcsUiFocusTextFieldRequest, EcsExclusive);

    (void)EcsUiTextInputRoot(world);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputResolveFocusRequestsSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(EcsUiTextFocusRequestAccumulator)},
        },
        .callback = EcsUiTextInputResolveFocusRequestsSystem,
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
            .name = "EcsUiTextInputPasteSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(EcsUiTextPasteRequest)},
        },
        .callback = EcsUiTextInputPasteSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputCopySystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextCopyRequest},
        },
        .callback = EcsUiTextInputCopySystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputCutSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = EcsUiTextCutRequest},
        },
        .callback = EcsUiTextInputCutSystem,
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

    /* Keep this registration last and immediate in the next built-in phase:
       Flecs merges deferred OnUpdate focus/edit commands before immediate
       systems, so field views see resolved state in the same progress. */
    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "EcsUiTextInputProjectFieldViewsSystem",
        }),
        .phase = EcsOnValidate,
        .query.terms = {
            {.id = ecs_id(EcsUiTextInputState)},
        },
        .callback = EcsUiTextInputProjectFieldViewsSystem,
        .immediate = true,
    });
}
