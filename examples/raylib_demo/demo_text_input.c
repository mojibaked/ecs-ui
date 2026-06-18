#include "demo_text_input.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT_DECLARE(DemoTextField);
ECS_COMPONENT_DECLARE(DemoTextInsertRequest);
ECS_TAG_DECLARE(DemoFocusedField);
ECS_TAG_DECLARE(DemoTextFieldUiNode);
ECS_TAG_DECLARE(DemoTextFieldValueUiNode);
ECS_TAG_DECLARE(DemoUiForTextField);
ECS_TAG_DECLARE(DemoFocusTextFieldRequest);
ECS_TAG_DECLARE(DemoBlurTextFieldRequest);
ECS_TAG_DECLARE(DemoTextDeleteRequest);

static void DemoTextInputCopyString(
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

ecs_entity_t DemoTextInputRoot(ecs_world_t *world)
{
    return ecs_entity(world, {.name = "DemoTextInput"});
}

ecs_entity_t DemoTextInputAddItemNameField(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }

    ecs_entity_t root = DemoTextInputRoot(world);
    ecs_entity_t field = ecs_entity(world, {
        .parent = root,
        .name = "AddItemNameField",
        .sep = "",
    });
    if (field != 0 && !ecs_has(world, field, DemoTextField)) {
        DemoTextField text_field = {0};
        DemoTextInputCopyString(
            text_field.placeholder,
            sizeof(text_field.placeholder),
            "item name");
        ecs_set_ptr(world, field, DemoTextField, &text_field);
    }
    return field;
}

static ecs_entity_t DemoTextInputFocusedField(ecs_world_t *world)
{
    return ecs_get_target(
        world,
        DemoTextInputRoot(world),
        DemoFocusedField,
        0);
}

bool DemoTextInputHasFocusedField(ecs_world_t *world)
{
    return world != NULL && DemoTextInputFocusedField(world) != 0;
}

static bool DemoTextInputIsFocused(ecs_world_t *world, ecs_entity_t field)
{
    return field != 0 && DemoTextInputFocusedField(world) == field;
}

static void DemoTextInputUpdateFieldUi(
    ecs_world_t *world,
    ecs_entity_t field)
{
    const DemoTextField *field_data =
        field != 0 ? ecs_get(world, field, DemoTextField) : NULL;
    if (world == NULL || field_data == NULL) {
        return;
    }

    const bool focused = DemoTextInputIsFocused(world, field);
    char display[ECS_UI_TEXT_MAX] = {0};
    if (focused) {
        DemoTextInputCopyString(display, sizeof(display), field_data->value);
        size_t length = strlen(display);
        if (length + 1u < sizeof(display)) {
            display[length] = '|';
            display[length + 1u] = '\0';
        }
    } else if (field_data->value[0] != '\0') {
        DemoTextInputCopyString(display, sizeof(display), field_data->value);
    } else {
        DemoTextInputCopyString(
            display,
            sizeof(display),
            field_data->placeholder);
    }

    ecs_entity_t value_node =
        ecs_get_target(world, field, DemoTextFieldValueUiNode, 0);
    EcsUiText *text =
        value_node != 0 ? ecs_get_mut(world, value_node, EcsUiText) : NULL;
    if (text != NULL) {
        DemoTextInputCopyString(text->text, sizeof(text->text), display);
        text->role =
            field_data->value[0] != '\0' || focused ?
                ECS_UI_TEXT_BUTTON :
                ECS_UI_TEXT_CAPTION;
        ecs_modified(world, value_node, EcsUiText);
    }

    ecs_entity_t field_node =
        ecs_get_target(world, field, DemoTextFieldUiNode, 0);
    EcsUiButton *button =
        field_node != 0 ? ecs_get_mut(world, field_node, EcsUiButton) : NULL;
    if (button != NULL) {
        button->variant =
            focused ? ECS_UI_BUTTON_PRIMARY : ECS_UI_BUTTON_SUBTLE;
        ecs_modified(world, field_node, EcsUiButton);
    }
    if (field_node != 0) {
        ecs_set(
            world,
            field_node,
            EcsUiVisual,
            {
                .opacity = 1.0f,
                .highlight = focused ? 0.22f : 0.0f,
            });
    }
}

static void DemoTextInputRefreshFieldUi(ecs_world_t *world)
{
    DemoTextInputUpdateFieldUi(world, DemoTextInputAddItemNameField(world));
}

ecs_entity_t DemoTextInputBuildAddItemNameField(
    ecs_world_t *world,
    EcsUiBuilder *builder,
    ecs_entity_t focus_action)
{
    if (world == NULL || builder == NULL || focus_action == 0) {
        return 0;
    }

    ecs_entity_t field = DemoTextInputAddItemNameField(world);
    if (field == 0) {
        return 0;
    }

    ecs_entity_t field_node = EcsUiBeginButton(
        builder,
        (EcsUiButtonDesc){
            .id = "AddItemNameInput",
            .variant = DemoTextInputIsFocused(world, field) ?
                ECS_UI_BUTTON_PRIMARY :
                ECS_UI_BUTTON_SUBTLE,
            .on_click = focus_action,
        });
    ecs_entity_t value_node = EcsUiAddText(
        builder,
        (EcsUiTextDesc){
            .id = "AddItemNameValue",
            .text = "",
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiEnd(builder);

    if (EcsUiBuilderOk(builder) && field_node != 0) {
        ecs_add_pair(world, field, DemoTextFieldUiNode, field_node);
        ecs_add_pair(world, field_node, DemoUiForTextField, field);
        if (value_node != 0) {
            ecs_add_pair(world, field, DemoTextFieldValueUiNode, value_node);
        }
        DemoTextInputUpdateFieldUi(world, field);
    }
    return field_node;
}

void DemoTextInputRequestFocusField(ecs_world_t *world, ecs_entity_t field)
{
    if (world == NULL || field == 0 || !ecs_has(world, field, DemoTextField)) {
        return;
    }

    (void)ecs_new_w_pair(world, DemoFocusTextFieldRequest, field);
}

void DemoTextInputRequestBlur(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    (void)ecs_new_w_id(world, DemoBlurTextFieldRequest);
}

void DemoTextInputRequestInsert(ecs_world_t *world, uint32_t codepoint)
{
    if (world == NULL || codepoint == 0u) {
        return;
    }

    ecs_entity_t request = ecs_new(world);
    ecs_set(
        world,
        request,
        DemoTextInsertRequest,
        {
            .codepoint = codepoint,
        });
}

void DemoTextInputRequestDelete(ecs_world_t *world)
{
    if (world == NULL) {
        return;
    }

    (void)ecs_new_w_id(world, DemoTextDeleteRequest);
}

const char *DemoTextInputAddItemNameValue(ecs_world_t *world)
{
    ecs_entity_t field = DemoTextInputAddItemNameField(world);
    const DemoTextField *field_data =
        field != 0 ? ecs_get(world, field, DemoTextField) : NULL;
    return field_data != NULL ? field_data->value : "";
}

void DemoTextInputClearAddItemName(ecs_world_t *world)
{
    ecs_entity_t field = DemoTextInputAddItemNameField(world);
    DemoTextField *field_data =
        field != 0 ? ecs_get_mut(world, field, DemoTextField) : NULL;
    if (field_data == NULL) {
        return;
    }

    field_data->value[0] = '\0';
    ecs_modified(world, field, DemoTextField);
}

static bool DemoTextInputAppendCodepoint(
    DemoTextField *field_data,
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

static bool DemoTextInputDeleteLast(DemoTextField *field_data)
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

static void DemoTextInputFocusFieldSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field =
            ecs_get_target(it->world, it->entities[i], DemoFocusTextFieldRequest, 0);
        if (field != 0 && ecs_has(it->world, field, DemoTextField)) {
            ecs_add_pair(
                it->world,
                DemoTextInputRoot(it->world),
                DemoFocusedField,
                field);
            TraceLog(LOG_INFO, "DEMO: focused text field");
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoTextInputBlurFieldSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_remove_pair(
            it->world,
            DemoTextInputRoot(it->world),
            DemoFocusedField,
            EcsWildcard);
        TraceLog(LOG_INFO, "DEMO: blurred text field");
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoTextInputInsertSystem(ecs_iter_t *it)
{
    const DemoTextInsertRequest *requests =
        ecs_field(it, DemoTextInsertRequest, 0);

    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = DemoTextInputFocusedField(it->world);
        DemoTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, DemoTextField) : NULL;
        if (DemoTextInputAppendCodepoint(field_data, requests[i].codepoint)) {
            ecs_modified(it->world, field, DemoTextField);
            TraceLog(LOG_INFO, "DEMO: inserted text character");
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoTextInputDeleteSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        ecs_entity_t field = DemoTextInputFocusedField(it->world);
        DemoTextField *field_data =
            field != 0 ? ecs_get_mut(it->world, field, DemoTextField) : NULL;
        if (DemoTextInputDeleteLast(field_data)) {
            ecs_modified(it->world, field, DemoTextField);
            TraceLog(LOG_INFO, "DEMO: deleted text character");
        }
        ecs_delete(it->world, it->entities[i]);
    }
}

static void DemoTextInputFieldChangedObserver(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        DemoTextInputUpdateFieldUi(it->world, it->entities[i]);
    }
}

static void DemoTextInputFocusChangedObserver(ecs_iter_t *it)
{
    DemoTextInputRefreshFieldUi(it->world);
}

void DemoTextInputRegister(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, DemoTextField);
    ECS_COMPONENT_DEFINE(world, DemoTextInsertRequest);
    ECS_TAG_DEFINE(world, DemoFocusedField);
    ECS_TAG_DEFINE(world, DemoTextFieldUiNode);
    ECS_TAG_DEFINE(world, DemoTextFieldValueUiNode);
    ECS_TAG_DEFINE(world, DemoUiForTextField);
    ECS_TAG_DEFINE(world, DemoFocusTextFieldRequest);
    ECS_TAG_DEFINE(world, DemoBlurTextFieldRequest);
    ECS_TAG_DEFINE(world, DemoTextDeleteRequest);

    ecs_add_id(world, DemoFocusedField, EcsExclusive);
    ecs_add_id(world, DemoTextFieldUiNode, EcsExclusive);
    ecs_add_id(world, DemoTextFieldValueUiNode, EcsExclusive);
    ecs_add_id(world, DemoUiForTextField, EcsExclusive);
    ecs_add_id(world, DemoFocusTextFieldRequest, EcsExclusive);

    (void)DemoTextInputAddItemNameField(world);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputFocusFieldSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_pair(DemoFocusTextFieldRequest, EcsWildcard)},
        },
        .callback = DemoTextInputFocusFieldSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputBlurFieldSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoBlurTextFieldRequest},
        },
        .callback = DemoTextInputBlurFieldSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputInsertSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(DemoTextInsertRequest)},
        },
        .callback = DemoTextInputInsertSystem,
    });

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputDeleteSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = DemoTextDeleteRequest},
        },
        .callback = DemoTextInputDeleteSystem,
    });

    (void)ecs_observer(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputFieldChangedObserver",
        }),
        .query.terms = {
            {.id = ecs_id(DemoTextField)},
        },
        .events = {EcsOnSet},
        .callback = DemoTextInputFieldChangedObserver,
    });

    (void)ecs_observer(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputFocusChangedObserver",
        }),
        .query.terms = {
            {.id = ecs_pair(DemoFocusedField, EcsWildcard)},
        },
        .events = {EcsOnAdd, EcsOnRemove},
        .callback = DemoTextInputFocusChangedObserver,
    });
}
