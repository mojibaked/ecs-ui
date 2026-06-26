#include "ecs_ui/ecs_ui_overlay.h"

#include <string.h>

static int32_t EcsUiOverlayFindIndex(
    const EcsUiOverlayState *state,
    EcsUiOverlayId id)
{
    if (state == NULL || id == 0u) {
        return -1;
    }
    for (uint32_t i = 0u; i < state->layer_count; i += 1u) {
        if (state->layers[i].id == id) {
            return (int32_t)i;
        }
    }
    return -1;
}

static bool EcsUiOverlayHasPointerBlocker(const EcsUiOverlayState *state)
{
    if (state == NULL) {
        return false;
    }
    for (uint32_t i = 0u; i < state->layer_count; i += 1u) {
        if (state->layers[i].blocks_pointer) {
            return true;
        }
    }
    return false;
}

static bool EcsUiOverlayHasKeyboardBlocker(const EcsUiOverlayState *state)
{
    if (state == NULL) {
        return false;
    }
    for (uint32_t i = 0u; i < state->layer_count; i += 1u) {
        if (state->layers[i].blocks_keyboard) {
            return true;
        }
    }
    return false;
}

static void EcsUiOverlayRefreshBlocks(EcsUiOverlayState *state)
{
    if (state == NULL) {
        return;
    }
    state->pointer_blocked =
        state->pointer_blocked || EcsUiOverlayHasPointerBlocker(state);
    state->keyboard_blocked =
        state->keyboard_blocked || EcsUiOverlayHasKeyboardBlocker(state);
}

static bool EcsUiOverlayIsDescendant(
    const EcsUiOverlayState *state,
    EcsUiOverlayId id,
    EcsUiOverlayId parent)
{
    if (state == NULL || id == 0u || parent == 0u || id == parent) {
        return false;
    }

    EcsUiOverlayId cursor = id;
    for (uint32_t guard = 0u; guard < state->layer_count; guard += 1u) {
        int32_t index = EcsUiOverlayFindIndex(state, cursor);
        if (index < 0) {
            return false;
        }
        EcsUiOverlayId current_parent =
            state->layers[(uint32_t)index].parent;
        if (current_parent == parent) {
            return true;
        }
        if (current_parent == 0u) {
            return false;
        }
        cursor = current_parent;
    }
    return false;
}

static void EcsUiOverlayRemoveAt(EcsUiOverlayState *state, uint32_t index)
{
    if (state == NULL || index >= state->layer_count) {
        return;
    }
    for (uint32_t i = index + 1u; i < state->layer_count; i += 1u) {
        state->layers[i - 1u] = state->layers[i];
    }
    state->layer_count -= 1u;
    if (state->layer_count < ECS_UI_OVERLAY_LAYER_MAX) {
        state->layers[state->layer_count] = (EcsUiOverlayLayer){0};
    }
}

static bool EcsUiOverlayCloseByPredicate(
    EcsUiOverlayState *state,
    bool (*predicate)(
        const EcsUiOverlayState *state,
        const EcsUiOverlayLayer *layer,
        void *ctx),
    void *ctx)
{
    if (state == NULL || predicate == NULL) {
        return false;
    }

    bool closed = false;
    for (uint32_t i = state->layer_count; i > 0u; i -= 1u) {
        const uint32_t index = i - 1u;
        if (predicate(state, &state->layers[index], ctx)) {
            EcsUiOverlayRemoveAt(state, index);
            closed = true;
        }
    }
    return closed;
}

static bool EcsUiOverlayCloseIdPredicate(
    const EcsUiOverlayState *state,
    const EcsUiOverlayLayer *layer,
    void *ctx)
{
    const EcsUiOverlayId id = ctx != NULL ? *(const EcsUiOverlayId *)ctx : 0u;
    return layer != NULL &&
        (layer->id == id || EcsUiOverlayIsDescendant(state, layer->id, id));
}

static bool EcsUiOverlayCloseDescendantPredicate(
    const EcsUiOverlayState *state,
    const EcsUiOverlayLayer *layer,
    void *ctx)
{
    const EcsUiOverlayId id = ctx != NULL ? *(const EcsUiOverlayId *)ctx : 0u;
    return layer != NULL && EcsUiOverlayIsDescendant(state, layer->id, id);
}

static bool EcsUiOverlayCloseOutsidePointerPredicate(
    const EcsUiOverlayState *state,
    const EcsUiOverlayLayer *layer,
    void *ctx)
{
    (void)state;
    (void)ctx;
    return layer != NULL && layer->close_on_outside_pointer;
}

static bool EcsUiOverlayPrunePredicate(
    const EcsUiOverlayState *state,
    const EcsUiOverlayLayer *layer,
    void *ctx)
{
    (void)state;
    (void)ctx;
    return layer != NULL && !layer->registered_this_frame;
}

static int32_t EcsUiOverlayTopmostIndexContaining(
    const EcsUiOverlayState *state,
    float x,
    float y)
{
    if (state == NULL) {
        return -1;
    }
    int32_t found = -1;
    uint32_t found_order = 0u;
    for (uint32_t i = 0u; i < state->layer_count; i += 1u) {
        const EcsUiOverlayLayer *layer = &state->layers[i];
        if (EcsUiOverlayRectContains(layer->bounds, x, y) &&
            (found < 0 || layer->order >= found_order)) {
            found = (int32_t)i;
            found_order = layer->order;
        }
    }
    return found;
}

static int32_t EcsUiOverlayTopmostCancelableIndex(
    const EcsUiOverlayState *state)
{
    if (state == NULL) {
        return -1;
    }
    int32_t found = -1;
    uint32_t found_order = 0u;
    for (uint32_t i = 0u; i < state->layer_count; i += 1u) {
        const EcsUiOverlayLayer *layer = &state->layers[i];
        if (layer->close_on_cancel &&
            (found < 0 || layer->order >= found_order)) {
            found = (int32_t)i;
            found_order = layer->order;
        }
    }
    return found;
}

EcsUiOverlayId EcsUiOverlayHashId(const char *id)
{
    const unsigned char *bytes = (const unsigned char *)(id != NULL ? id : "");
    uint64_t hash = 1469598103934665603ull;
    while (*bytes != '\0') {
        hash ^= (uint64_t)(*bytes);
        hash *= 1099511628211ull;
        bytes += 1;
    }
    return hash != 0u ? hash : 1469598103934665603ull;
}

void EcsUiOverlayInit(EcsUiOverlayState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->next_order = 1u;
}

void EcsUiOverlayBeginFrame(
    EcsUiOverlayState *state,
    EcsUiOverlayInput input)
{
    if (state == NULL) {
        return;
    }

    const bool had_pointer_blocker = EcsUiOverlayHasPointerBlocker(state);
    const bool had_keyboard_blocker = EcsUiOverlayHasKeyboardBlocker(state);
    state->pointer_blocked = false;
    state->keyboard_blocked = false;
    for (uint32_t i = 0u; i < state->layer_count; i += 1u) {
        state->layers[i].registered_this_frame = false;
    }

    if (input.cancel_pressed) {
        const int32_t index = EcsUiOverlayTopmostCancelableIndex(state);
        if (index >= 0) {
            const EcsUiOverlayId id = state->layers[(uint32_t)index].id;
            (void)EcsUiOverlayClose(state, id);
        }
        state->keyboard_blocked = had_keyboard_blocker;
    }

    if (input.primary_pressed || input.secondary_pressed) {
        const int32_t inside =
            EcsUiOverlayTopmostIndexContaining(state, input.x, input.y);
        if (inside < 0) {
            (void)EcsUiOverlayCloseByPredicate(
                state,
                EcsUiOverlayCloseOutsidePointerPredicate,
                NULL);
        }
        state->pointer_blocked = had_pointer_blocker;
    }

    EcsUiOverlayRefreshBlocks(state);
}

void EcsUiOverlayEndFrame(EcsUiOverlayState *state)
{
    EcsUiOverlayRefreshBlocks(state);
}

bool EcsUiOverlayOpen(EcsUiOverlayState *state, EcsUiOverlayDesc desc)
{
    if (state == NULL || desc.id == 0u ||
        desc.kind == ECS_UI_OVERLAY_KIND_NONE) {
        return false;
    }

    int32_t index = EcsUiOverlayFindIndex(state, desc.id);
    if (index < 0) {
        if (state->layer_count >= ECS_UI_OVERLAY_LAYER_MAX) {
            return false;
        }
        index = (int32_t)state->layer_count;
        state->layer_count += 1u;
        state->layers[(uint32_t)index] = (EcsUiOverlayLayer){
            .id = desc.id,
            .order = state->next_order,
            .registered_this_frame = true,
        };
        state->next_order += 1u;
        if (state->next_order == 0u) {
            state->next_order = 1u;
        }
    }

    EcsUiOverlayLayer *layer = &state->layers[(uint32_t)index];
    layer->parent = desc.parent;
    layer->kind = desc.kind;
    layer->anchor = desc.anchor;
    layer->close_on_outside_pointer = desc.close_on_outside_pointer;
    layer->close_on_cancel = desc.close_on_cancel;
    layer->blocks_pointer = desc.blocks_pointer;
    layer->blocks_keyboard = desc.blocks_keyboard;
    return true;
}

bool EcsUiOverlayOpenMenu(
    EcsUiOverlayState *state,
    EcsUiOverlayId id,
    EcsUiOverlayId parent,
    EcsUiOverlayRect anchor)
{
    return EcsUiOverlayOpen(
        state,
        (EcsUiOverlayDesc){
            .id = id,
            .parent = parent,
            .kind = ECS_UI_OVERLAY_KIND_MENU,
            .anchor = anchor,
            .close_on_outside_pointer = true,
            .close_on_cancel = true,
            .blocks_pointer = true,
            .blocks_keyboard = true,
        });
}

bool EcsUiOverlayToggleMenu(
    EcsUiOverlayState *state,
    EcsUiOverlayId id,
    EcsUiOverlayId parent,
    EcsUiOverlayRect anchor)
{
    if (EcsUiOverlayIsOpen(state, id)) {
        return EcsUiOverlayClose(state, id);
    }
    return EcsUiOverlayOpenMenu(state, id, parent, anchor);
}

bool EcsUiOverlayClose(EcsUiOverlayState *state, EcsUiOverlayId id)
{
    return EcsUiOverlayCloseByPredicate(
        state,
        EcsUiOverlayCloseIdPredicate,
        &id);
}

void EcsUiOverlayCloseAll(EcsUiOverlayState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state->layers, 0, sizeof(state->layers));
    state->layer_count = 0u;
    state->pointer_blocked = false;
    state->keyboard_blocked = false;
}

bool EcsUiOverlayCloseDescendants(
    EcsUiOverlayState *state,
    EcsUiOverlayId parent)
{
    return EcsUiOverlayCloseByPredicate(
        state,
        EcsUiOverlayCloseDescendantPredicate,
        &parent);
}

bool EcsUiOverlayIsOpen(
    const EcsUiOverlayState *state,
    EcsUiOverlayId id)
{
    return EcsUiOverlayFindIndex(state, id) >= 0;
}

bool EcsUiOverlayRegisterLayerRoot(
    EcsUiOverlayState *state,
    EcsUiOverlayId id,
    ecs_entity_t root,
    EcsUiOverlayRect bounds)
{
    int32_t index = EcsUiOverlayFindIndex(state, id);
    if (index < 0) {
        return false;
    }
    EcsUiOverlayLayer *layer = &state->layers[(uint32_t)index];
    layer->root = root;
    layer->bounds = bounds;
    layer->registered_this_frame = true;
    return true;
}

bool EcsUiOverlayPruneUnregistered(EcsUiOverlayState *state)
{
    return EcsUiOverlayCloseByPredicate(
        state,
        EcsUiOverlayPrunePredicate,
        NULL);
}

uint32_t EcsUiOverlayLayerCount(const EcsUiOverlayState *state)
{
    return state != NULL ? state->layer_count : 0u;
}

const EcsUiOverlayLayer *EcsUiOverlayLayerAt(
    const EcsUiOverlayState *state,
    uint32_t index)
{
    if (state == NULL || index >= state->layer_count) {
        return NULL;
    }
    return &state->layers[index];
}

const EcsUiOverlayLayer *EcsUiOverlayFindLayer(
    const EcsUiOverlayState *state,
    EcsUiOverlayId id)
{
    const int32_t index = EcsUiOverlayFindIndex(state, id);
    return index >= 0 ? &state->layers[(uint32_t)index] : NULL;
}

bool EcsUiOverlayBlocksPointer(const EcsUiOverlayState *state)
{
    return state != NULL && state->pointer_blocked;
}

bool EcsUiOverlayBlocksKeyboard(const EcsUiOverlayState *state)
{
    return state != NULL && state->keyboard_blocked;
}

bool EcsUiOverlayRectContains(EcsUiOverlayRect rect, float x, float y)
{
    return rect.width > 0.0f && rect.height > 0.0f &&
        x >= rect.x && y >= rect.y &&
        x < rect.x + rect.width &&
        y < rect.y + rect.height;
}
