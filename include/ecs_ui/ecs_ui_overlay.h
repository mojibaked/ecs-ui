#ifndef ECS_UI_ECS_UI_OVERLAY_H
#define ECS_UI_ECS_UI_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_OVERLAY_LAYER_MAX 16u

typedef uint64_t EcsUiOverlayId;

typedef enum EcsUiOverlayKind {
    ECS_UI_OVERLAY_KIND_NONE = 0,
    ECS_UI_OVERLAY_KIND_MENU = 1,
    ECS_UI_OVERLAY_KIND_POPOVER = 2,
    ECS_UI_OVERLAY_KIND_MODAL = 3,
} EcsUiOverlayKind;

/* Overlay rectangles are logical, window-origin units. */
typedef struct EcsUiOverlayRect {
    float x;
    float y;
    float width;
    float height;
} EcsUiOverlayRect;

/* Overlay pointer input is logical, window-origin. */
typedef struct EcsUiOverlayInput {
    float x;
    float y;
    bool primary_pressed;
    bool secondary_pressed;
    bool cancel_pressed;
} EcsUiOverlayInput;

typedef struct EcsUiOverlayDesc {
    EcsUiOverlayId id;
    EcsUiOverlayId parent;
    EcsUiOverlayKind kind;
    EcsUiOverlayRect anchor;
    bool close_on_outside_pointer;
    bool close_on_cancel;
    bool blocks_pointer;
    bool blocks_keyboard;
} EcsUiOverlayDesc;

typedef struct EcsUiOverlayLayer {
    EcsUiOverlayId id;
    EcsUiOverlayId parent;
    EcsUiOverlayKind kind;
    EcsUiOverlayRect anchor;
    EcsUiOverlayRect bounds;
    ecs_entity_t root;
    uint32_t order;
    bool close_on_outside_pointer;
    bool close_on_cancel;
    bool blocks_pointer;
    bool blocks_keyboard;
    bool registered_this_frame;
} EcsUiOverlayLayer;

typedef struct EcsUiOverlayState {
    EcsUiOverlayLayer layers[ECS_UI_OVERLAY_LAYER_MAX];
    uint32_t layer_count;
    uint32_t next_order;
    bool pointer_blocked;
    bool keyboard_blocked;
} EcsUiOverlayState;

EcsUiOverlayId EcsUiOverlayHashId(const char *id);
void EcsUiOverlayInit(EcsUiOverlayState *state);
void EcsUiOverlayBeginFrame(
    EcsUiOverlayState *state,
    EcsUiOverlayInput input);
void EcsUiOverlayEndFrame(EcsUiOverlayState *state);

bool EcsUiOverlayOpen(EcsUiOverlayState *state, EcsUiOverlayDesc desc);
bool EcsUiOverlayOpenMenu(
    EcsUiOverlayState *state,
    EcsUiOverlayId id,
    EcsUiOverlayId parent,
    EcsUiOverlayRect anchor);
bool EcsUiOverlayToggleMenu(
    EcsUiOverlayState *state,
    EcsUiOverlayId id,
    EcsUiOverlayId parent,
    EcsUiOverlayRect anchor);
bool EcsUiOverlayClose(EcsUiOverlayState *state, EcsUiOverlayId id);
void EcsUiOverlayCloseAll(EcsUiOverlayState *state);
bool EcsUiOverlayCloseDescendants(
    EcsUiOverlayState *state,
    EcsUiOverlayId parent);
bool EcsUiOverlayIsOpen(
    const EcsUiOverlayState *state,
    EcsUiOverlayId id);

bool EcsUiOverlayRegisterLayerRoot(
    EcsUiOverlayState *state,
    EcsUiOverlayId id,
    ecs_entity_t root,
    EcsUiOverlayRect bounds);
bool EcsUiOverlayPruneUnregistered(EcsUiOverlayState *state);

uint32_t EcsUiOverlayLayerCount(const EcsUiOverlayState *state);
const EcsUiOverlayLayer *EcsUiOverlayLayerAt(
    const EcsUiOverlayState *state,
    uint32_t index);
const EcsUiOverlayLayer *EcsUiOverlayFindLayer(
    const EcsUiOverlayState *state,
    EcsUiOverlayId id);
bool EcsUiOverlayBlocksPointer(const EcsUiOverlayState *state);
bool EcsUiOverlayBlocksKeyboard(const EcsUiOverlayState *state);
bool EcsUiOverlayRectContains(EcsUiOverlayRect rect, float x, float y);

#ifdef __cplusplus
}
#endif

#endif
