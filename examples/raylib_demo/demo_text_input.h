#ifndef ECS_UI_RAYLIB_DEMO_TEXT_INPUT_H
#define ECS_UI_RAYLIB_DEMO_TEXT_INPUT_H

#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_text_input.h"

typedef struct DemoAddItemForm {
    char item_name[ECS_UI_TEXT_MAX];
    bool can_submit;
} DemoAddItemForm;

void DemoTextInputRegister(ecs_world_t *world);
ecs_entity_t DemoTextInputRoot(ecs_world_t *world);
ecs_entity_t DemoTextInputAddItemNameField(ecs_world_t *world);
ecs_entity_t DemoTextInputAddItemNoteField(ecs_world_t *world);
ecs_entity_t DemoTextInputBuildAddItemNameField(
    ecs_world_t *world,
    EcsUiBuilder *builder);
ecs_entity_t DemoTextInputBuildAddItemNoteField(
    ecs_world_t *world,
    EcsUiBuilder *builder);
const char *DemoTextInputAddItemNameValue(ecs_world_t *world);
bool DemoAddItemFormRead(ecs_world_t *world, DemoAddItemForm *out);
bool DemoAddItemFormCanSubmit(ecs_world_t *world);
bool DemoAddItemFormHasFocusedField(ecs_world_t *world);
bool DemoAddItemFormSetUiNodes(
    ecs_world_t *world,
    ecs_entity_t create_button);
bool DemoAddItemFormProject(ecs_world_t *world);
void DemoTextInputClearAddItemFields(ecs_world_t *world);
void DemoTextInputClearAddItemName(ecs_world_t *world);
bool DemoTextInputPopClipboardWrite(
    ecs_world_t *world,
    char *out,
    size_t out_size);

#endif
