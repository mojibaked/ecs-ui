#include "demo_text_input.h"

#include <stdio.h>
#include <string.h>

ecs_entity_t DemoTextInputRoot(ecs_world_t *world)
{
    return EcsUiTextInputRoot(world);
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

static void DemoTextInputUpdateFieldUi(
    ecs_world_t *world,
    ecs_entity_t field)
{
    const EcsUiTextField *field_data =
        field != 0 ? ecs_get(world, field, EcsUiTextField) : NULL;
    if (world == NULL || field_data == NULL) {
        return;
    }

    /*
     * The reusable text-input layer owns focus and value. The demo owns this
     * particular projection: focused fields show a simple caret marker and use
     * the primary button style, while empty unfocused fields show placeholder
     * text.
     */
    const bool focused = EcsUiTextInputIsFocused(world, field);
    char display[ECS_UI_TEXT_MAX] = {0};
    (void)EcsUiTextInputDisplayText(
        world,
        field,
        true,
        display,
        sizeof(display));

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
            (void)snprintf(text->text, sizeof(text->text), "%s", display);
            text->role = role;
            ecs_modified(world, value_node, EcsUiText);
        }
    }

    ecs_entity_t field_node =
        EcsUiTextInputFieldUiNode(world, field);
    EcsUiButton *button =
        field_node != 0 ? ecs_get_mut(world, field_node, EcsUiButton) : NULL;
    if (button != NULL) {
        EcsUiButtonVariant variant =
            focused ? ECS_UI_BUTTON_PRIMARY : ECS_UI_BUTTON_SUBTLE;
        if (button->variant != variant) {
            button->variant = variant;
            ecs_modified(world, field_node, EcsUiButton);
        }
    }
    if (field_node != 0) {
        const float highlight = focused ? 0.22f : 0.0f;
        const EcsUiVisual *visual = ecs_get(world, field_node, EcsUiVisual);
        if (visual == NULL || visual->opacity != 1.0f ||
            visual->highlight != highlight) {
            ecs_set(
                world,
                field_node,
                EcsUiVisual,
                {
                    .opacity = 1.0f,
                    .highlight = highlight,
                });
        }
    }
}

static ecs_entity_t DemoTextInputBuildField(
    ecs_world_t *world,
    EcsUiBuilder *builder,
    ecs_entity_t focus_action,
    ecs_entity_t field,
    const char *field_node_id,
    const char *value_node_id)
{
    if (world == NULL || builder == NULL || focus_action == 0) {
        return 0;
    }

    if (field == 0) {
        return 0;
    }

    ecs_entity_t field_node = EcsUiBeginButton(
        builder,
        (EcsUiButtonDesc){
            .id = field_node_id,
            .variant = EcsUiTextInputIsFocused(world, field) ?
                ECS_UI_BUTTON_PRIMARY :
                ECS_UI_BUTTON_SUBTLE,
            .on_click = focus_action,
        });
    ecs_entity_t value_node = EcsUiAddText(
        builder,
        (EcsUiTextDesc){
            .id = value_node_id,
            .text = "",
            .role = ECS_UI_TEXT_CAPTION,
        });
    EcsUiEnd(builder);

    if (EcsUiBuilderOk(builder) && field_node != 0) {
        (void)EcsUiTextInputSetFieldUiNodes(
            world,
            field,
            field_node,
            value_node);
        (void)EcsUiTextInputSetUiField(world, field_node, field);
        DemoTextInputUpdateFieldUi(world, field);
    }
    return field_node;
}

ecs_entity_t DemoTextInputBuildAddItemNameField(
    ecs_world_t *world,
    EcsUiBuilder *builder,
    ecs_entity_t focus_action)
{
    return DemoTextInputBuildField(
        world,
        builder,
        focus_action,
        DemoTextInputAddItemNameField(world),
        "AddItemNameInput",
        "AddItemNameValue");
}

ecs_entity_t DemoTextInputBuildAddItemNoteField(
    ecs_world_t *world,
    EcsUiBuilder *builder,
    ecs_entity_t focus_action)
{
    return DemoTextInputBuildField(
        world,
        builder,
        focus_action,
        DemoTextInputAddItemNoteField(world),
        "AddItemNoteInput",
        "AddItemNoteValue");
}

void DemoTextInputRequestFocusField(ecs_world_t *world, ecs_entity_t field)
{
    (void)EcsUiTextInputRequestFocusField(world, field);
}

void DemoTextInputRequestFocusNext(ecs_world_t *world)
{
    (void)EcsUiTextInputRequestFocusNext(world);
}

void DemoTextInputRequestFocusPrevious(ecs_world_t *world)
{
    (void)EcsUiTextInputRequestFocusPrevious(world);
}

void DemoTextInputRequestBlur(ecs_world_t *world)
{
    (void)EcsUiTextInputRequestBlur(world);
}

void DemoTextInputRequestInsert(ecs_world_t *world, uint32_t codepoint)
{
    (void)EcsUiTextInputRequestInsert(world, codepoint);
}

void DemoTextInputRequestDelete(ecs_world_t *world)
{
    (void)EcsUiTextInputRequestDelete(world);
}

bool DemoTextInputHasFocusedField(ecs_world_t *world)
{
    return EcsUiTextInputHasFocusedField(world);
}

const char *DemoTextInputAddItemNameValue(ecs_world_t *world)
{
    return EcsUiTextInputValue(world, DemoTextInputAddItemNameField(world));
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

static void DemoTextInputProjectFieldsSystem(ecs_iter_t *it)
{
    for (int32_t i = 0; i < it->count; i += 1) {
        /*
         * Value and focus mutations happen in the reusable text-input systems,
         * while this demo projection keeps the retained field UI in sync.
         */
        DemoTextInputUpdateFieldUi(it->world, it->entities[i]);
    }
}

void DemoTextInputRegister(ecs_world_t *world)
{
    EcsUiTextInputImport(world);
    (void)DemoTextInputAddItemNameField(world);
    (void)DemoTextInputAddItemNoteField(world);

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
