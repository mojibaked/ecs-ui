#include "demo_text_input.h"

#include "demo_theme.h"
#include "demo_ui_action_button.h"

#include <string.h>

ECS_TAG_DECLARE(DemoAddItemFormCreateButtonNode);

static bool DemoTextInputIsAsciiSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
        c == '\f' || c == '\v';
}

static void DemoTextInputCopyTrimmed(
    char *out,
    size_t out_size,
    const char *value)
{
    if (out == NULL || out_size == 0u) {
        return;
    }

    const char *start = value != NULL ? value : "";
    while (DemoTextInputIsAsciiSpace(*start)) {
        start += 1;
    }

    const char *end = start + strlen(start);
    while (end > start && DemoTextInputIsAsciiSpace(*(end - 1))) {
        end -= 1;
    }

    size_t length = (size_t)(end - start);
    if (length + 1u > out_size) {
        length = out_size - 1u;
    }
    if (length > 0u) {
        (void)memcpy(out, start, length);
    }
    out[length] = '\0';
}

ecs_entity_t DemoTextInputRoot(ecs_world_t *world)
{
    return EcsUiTextInputRoot(world);
}

static ecs_entity_t DemoAddItemFormEntity(ecs_world_t *world)
{
    if (world == NULL) {
        return 0;
    }

    ecs_entity_t root = DemoTextInputRoot(world);
    if (root == 0) {
        return 0;
    }

    return ecs_entity(world, {
        .parent = root,
        .name = "DemoAddItemForm",
        .sep = "",
    });
}

ecs_entity_t DemoTextInputAddItemNameField(ecs_world_t *world)
{
    /*
     * The field entity is reusable text-input state. The add-item route decides
     * what the value means and when to clear it after successful submit.
     */
    return EcsUiTextInputField(world, "AddItemNameField", "item name");
}

ecs_entity_t DemoTextInputAddItemNoteField(ecs_world_t *world)
{
    return EcsUiTextInputField(world, "AddItemNoteField", "optional note");
}

static ecs_entity_t DemoTextInputBuildField(
    ecs_world_t *world,
    EcsUiBuilder *builder,
    ecs_entity_t field,
    const char *field_node_id,
    const char *value_node_id)
{
    if (world == NULL || builder == NULL) {
        return 0;
    }

    if (field == 0) {
        return 0;
    }

    return EcsUiTextInputBuildFieldView(
        builder,
        field,
        (EcsUiTextFieldViewDesc){
            .field_id = field_node_id,
            .value_id = value_node_id,
            .style_token = DemoThemeTextFieldStyleToken(world),
        });
}

ecs_entity_t DemoTextInputBuildAddItemNameField(
    ecs_world_t *world,
    EcsUiBuilder *builder)
{
    return DemoTextInputBuildField(
        world,
        builder,
        DemoTextInputAddItemNameField(world),
        "AddItemNameInput",
        "AddItemNameValue");
}

ecs_entity_t DemoTextInputBuildAddItemNoteField(
    ecs_world_t *world,
    EcsUiBuilder *builder)
{
    return DemoTextInputBuildField(
        world,
        builder,
        DemoTextInputAddItemNoteField(world),
        "AddItemNoteInput",
        "AddItemNoteValue");
}

const char *DemoTextInputAddItemNameValue(ecs_world_t *world)
{
    return EcsUiTextInputValue(world, DemoTextInputAddItemNameField(world));
}

bool DemoAddItemFormRead(ecs_world_t *world, DemoAddItemForm *out)
{
    if (world == NULL || out == NULL) {
        return false;
    }

    DemoAddItemForm form = {0};
    DemoTextInputCopyTrimmed(
        form.item_name,
        sizeof(form.item_name),
        DemoTextInputAddItemNameValue(world));
    form.can_submit = form.item_name[0] != '\0';
    *out = form;
    return true;
}

bool DemoAddItemFormCanSubmit(ecs_world_t *world)
{
    DemoAddItemForm form = {0};
    return DemoAddItemFormRead(world, &form) && form.can_submit;
}

bool DemoAddItemFormHasFocusedField(ecs_world_t *world)
{
    ecs_entity_t focused = EcsUiTextInputFocusedField(world);
    return focused != 0 &&
        (focused == DemoTextInputAddItemNameField(world) ||
            focused == DemoTextInputAddItemNoteField(world));
}

bool DemoAddItemFormSetUiNodes(
    ecs_world_t *world,
    ecs_entity_t create_button)
{
    if (world == NULL || create_button == 0 ||
        DemoAddItemFormCreateButtonNode == 0) {
        return false;
    }

    ecs_entity_t form = DemoAddItemFormEntity(world);
    if (form == 0) {
        return false;
    }

    ecs_add_pair(
        world,
        form,
        DemoAddItemFormCreateButtonNode,
        create_button);
    return true;
}

static bool DemoAddItemFormProjectCreateButton(
    ecs_world_t *world,
    ecs_entity_t button,
    const DemoAddItemForm *form)
{
    if (world == NULL || button == 0 || !ecs_is_alive(world, button)) {
        return false;
    }

    if (form == NULL) {
        return false;
    }

    const bool disabled = !form->can_submit;
    const EcsUiPressable *pressable = ecs_get(world, button, EcsUiPressable);
    if (pressable == NULL) {
        return false;
    }
    if (pressable->disabled == disabled) {
        return true;
    }

    return DemoUiSetActionButtonDisabled(world, button, disabled);
}

bool DemoAddItemFormProject(ecs_world_t *world)
{
    if (world == NULL || DemoAddItemFormCreateButtonNode == 0) {
        return false;
    }

    DemoAddItemForm form = {0};
    if (!DemoAddItemFormRead(world, &form)) {
        return false;
    }

    ecs_entity_t form_entity = DemoAddItemFormEntity(world);
    ecs_entity_t create_button = form_entity != 0 ?
        ecs_get_target(
            world,
            form_entity,
            DemoAddItemFormCreateButtonNode,
            0) :
        0;
    return DemoAddItemFormProjectCreateButton(
        world,
        create_button,
        &form);
}

void DemoTextInputClearAddItemFields(ecs_world_t *world)
{
    (void)EcsUiTextInputClear(world, DemoTextInputAddItemNameField(world));
    (void)EcsUiTextInputClear(world, DemoTextInputAddItemNoteField(world));
}

void DemoTextInputClearAddItemName(ecs_world_t *world)
{
    DemoTextInputClearAddItemFields(world);
}

bool DemoTextInputPopClipboardWrite(
    ecs_world_t *world,
    char *out,
    size_t out_size)
{
    return EcsUiTextInputPopClipboardWrite(world, out, out_size);
}

static void DemoTextInputProjectFieldsSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        (void)EcsUiTextInputProjectFieldView(it->world, it->entities[i]);
    }
    (void)DemoAddItemFormProject(it->world);
}

void DemoTextInputRegister(ecs_world_t *world)
{
    EcsUiTextInputImport(world);
    ECS_TAG_DEFINE(world, DemoAddItemFormCreateButtonNode);
    ecs_add_id(world, DemoAddItemFormCreateButtonNode, EcsExclusive);

    (void)DemoAddItemFormEntity(world);
    (void)DemoTextInputAddItemNameField(world);
    (void)DemoTextInputAddItemNoteField(world);
    (void)DemoThemeTextFieldStyleToken(world);

    (void)ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "DemoTextInputProjectFieldsSystem",
            .add = ecs_ids(ecs_dependson(EcsOnUpdate)),
        }),
        .query.terms = {
            {.id = ecs_id(EcsUiTextField)},
        },
        .callback = DemoTextInputProjectFieldsSystem,
    });
}
