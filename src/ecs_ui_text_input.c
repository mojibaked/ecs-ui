#include "ecs_ui/ecs_ui_text_input.h"

#include <string.h>

ECS_COMPONENT_DECLARE(EcsUiTextField);
ECS_COMPONENT_DECLARE(EcsUiTextInsertRequest);
ECS_TAG_DECLARE(EcsUiFocusedTextField);
ECS_TAG_DECLARE(EcsUiTextFieldUiNode);
ECS_TAG_DECLARE(EcsUiTextFieldValueUiNode);
ECS_TAG_DECLARE(EcsUiForTextField);
ECS_TAG_DECLARE(EcsUiFocusTextFieldRequest);
ECS_TAG_DECLARE(EcsUiBlurTextFieldRequest);
ECS_TAG_DECLARE(EcsUiTextDeleteRequest);

static bool EcsUiTextInputReady(void)
{
    return ecs_id(EcsUiTextField) != 0 &&
        ecs_id(EcsUiTextInsertRequest) != 0 &&
        EcsUiFocusedTextField != 0 && EcsUiTextFieldUiNode != 0 &&
        EcsUiTextFieldValueUiNode != 0 && EcsUiForTextField != 0 &&
        EcsUiFocusTextFieldRequest != 0 && EcsUiBlurTextFieldRequest != 0 &&
        EcsUiTextDeleteRequest != 0;
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
    return ecs_entity(world, {.name = "EcsUiTextInput"});
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
    ecs_modified(world, field, EcsUiTextField);
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
    if (focused || field_data->value[0] != '\0') {
        EcsUiTextInputCopyString(out, out_size, field_data->value);
    } else {
        EcsUiTextInputCopyString(out, out_size, field_data->placeholder);
    }

    if (include_caret && focused) {
        size_t length = strlen(out);
        if (length + 1u < out_size) {
            out[length] = '|';
            out[length + 1u] = '\0';
        }
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

static bool EcsUiTextInputAppendCodepoint(
    EcsUiTextField *field_data,
    uint32_t codepoint)
{
    if (field_data == NULL || codepoint < 32u || codepoint > 126u) {
        return false;
    }

    size_t length = strlen(field_data->value);
    if (length + 1u >= sizeof(field_data->value)) {
        return false;
    }

    field_data->value[length] = (char)codepoint;
    field_data->value[length + 1u] = '\0';
    return true;
}

static bool EcsUiTextInputDeleteLast(EcsUiTextField *field_data)
{
    if (field_data == NULL) {
        return false;
    }

    size_t length = strlen(field_data->value);
    if (length == 0u) {
        return false;
    }

    field_data->value[length - 1u] = '\0';
    return true;
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
            ecs_add_pair(
                it->world,
                EcsUiTextInputRoot(it->world),
                EcsUiFocusedTextField,
                field);
        }
        ecs_delete(it->world, it->entities[i]);
    }
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
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputAppendCodepoint(field_data, requests[i].codepoint)) {
            ecs_modified(it->world, field, EcsUiTextField);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void EcsUiTextInputDeleteSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = EcsUiTextInputFocusedField(it->world);
        EcsUiTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, EcsUiTextField) : NULL;
        if (EcsUiTextInputDeleteLast(field_data)) {
            ecs_modified(it->world, field, EcsUiTextField);
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

void EcsUiTextInputImport(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    ECS_COMPONENT_DEFINE(world, EcsUiTextField);
    ECS_COMPONENT_DEFINE(world, EcsUiTextInsertRequest);
    ECS_TAG_DEFINE(world, EcsUiFocusedTextField);
    ECS_TAG_DEFINE(world, EcsUiTextFieldUiNode);
    ECS_TAG_DEFINE(world, EcsUiTextFieldValueUiNode);
    ECS_TAG_DEFINE(world, EcsUiForTextField);
    ECS_TAG_DEFINE(world, EcsUiFocusTextFieldRequest);
    ECS_TAG_DEFINE(world, EcsUiBlurTextFieldRequest);
    ECS_TAG_DEFINE(world, EcsUiTextDeleteRequest);

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
}
