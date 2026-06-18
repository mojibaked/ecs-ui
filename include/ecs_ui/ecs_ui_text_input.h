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

typedef struct EcsUiTextInsertRequest {
    uint32_t codepoint;
} EcsUiTextInsertRequest;

extern ECS_COMPONENT_DECLARE(EcsUiTextField);
extern ECS_COMPONENT_DECLARE(EcsUiTextInsertRequest);
extern ECS_TAG_DECLARE(EcsUiFocusedTextField);
extern ECS_TAG_DECLARE(EcsUiTextFieldUiNode);
extern ECS_TAG_DECLARE(EcsUiTextFieldValueUiNode);
extern ECS_TAG_DECLARE(EcsUiForTextField);
extern ECS_TAG_DECLARE(EcsUiFocusTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiFocusNextTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiFocusPreviousTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiBlurTextFieldRequest);
extern ECS_TAG_DECLARE(EcsUiTextDeleteRequest);

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

const char *EcsUiTextInputValue(
    const ecs_world_t *world,
    ecs_entity_t field);
const char *EcsUiTextInputPlaceholder(
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
bool EcsUiTextInputDisplayText(
    const ecs_world_t *world,
    ecs_entity_t field,
    bool include_caret,
    char *out,
    size_t out_size);

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
