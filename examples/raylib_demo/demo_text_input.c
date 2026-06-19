#include "demo_text_input.h"

#include "demo_theme.h"

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
            .style_token = DemoThemeTextInputFieldStyleToken(world),
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
}

void DemoTextInputRegister(ecs_world_t *world)
{
    EcsUiTextInputImport(world);
    (void)DemoTextInputAddItemNameField(world);
    (void)DemoTextInputAddItemNoteField(world);
    (void)DemoThemeTextInputFieldStyleToken(world);

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
