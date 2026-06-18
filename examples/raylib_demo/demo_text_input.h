#ifndef ECS_UI_RAYLIB_DEMO_TEXT_INPUT_H
#define ECS_UI_RAYLIB_DEMO_TEXT_INPUT_H

#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_text_input.h"

void DemoTextInputRegister(ecs_world_t *world);
ecs_entity_t DemoTextInputRoot(ecs_world_t *world);
ecs_entity_t DemoTextInputAddItemNameField(ecs_world_t *world);
ecs_entity_t DemoTextInputBuildAddItemNameField(
    ecs_world_t *world,
    EcsUiBuilder *builder,
    ecs_entity_t focus_action);
void DemoTextInputRequestFocusField(ecs_world_t *world, ecs_entity_t field);
void DemoTextInputRequestBlur(ecs_world_t *world);
void DemoTextInputRequestInsert(ecs_world_t *world, uint32_t codepoint);
void DemoTextInputRequestDelete(ecs_world_t *world);
bool DemoTextInputHasFocusedField(ecs_world_t *world);
const char *DemoTextInputAddItemNameValue(ecs_world_t *world);
void DemoTextInputClearAddItemName(ecs_world_t *world);

#endif
