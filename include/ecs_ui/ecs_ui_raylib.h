#ifndef ECS_UI_ECS_UI_RAYLIB_H
#define ECS_UI_ECS_UI_RAYLIB_H

#include <raylib.h>

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Raylib is a render bridge, so callback geometry is physical pixels. The core
 * tree snapshot remains logical; `scale` is the tree scale used by the bridge.
 * `physical_bounds` is the node's draw box, `physical_root_bounds` is the root
 * box passed to EcsUiRaylibDrawTreeEx, and `logical_origin` is that root origin
 * in window-origin logical units (`physical_root_bounds.xy / scale`).
 */
typedef struct EcsUiRaylibRenderContext {
    Rectangle physical_bounds;
    Rectangle physical_root_bounds;
    Vector2 logical_origin;
    float scale;
} EcsUiRaylibRenderContext;

typedef void (*EcsUiRaylibCustomDrawFn)(
    const EcsUiTreeNodeSnapshot *node,
    const EcsUiRaylibRenderContext *context,
    float opacity,
    void *user_data);

typedef struct EcsUiRaylibDrawOptions {
    EcsUiRaylibCustomDrawFn custom_draw;
    void *user_data;
    EcsUiRaylibCustomDrawFn icon_draw;
    EcsUiRaylibCustomDrawFn nine_slice_draw;
} EcsUiRaylibDrawOptions;

/* `bounds` is the physical pixel root box for this render bridge. */
void EcsUiRaylibDrawTree(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme);
/* `bounds` is the physical pixel root box for this render bridge. */
void EcsUiRaylibDrawTreeEx(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    const EcsUiTheme *theme,
    const EcsUiRaylibDrawOptions *options);
/*
 * Collect pointer/keyboard events for the direct raylib bridge. `bounds` and
 * raylib mouse input are physical pixels; emitted EcsUiEvent pointer coordinates
 * are window-origin logical units divided by tree->scale.
 */
void EcsUiRaylibCollectEvents(
    const EcsUiTreeSnapshot *tree,
    Rectangle bounds,
    EcsUiEventList *events);

/*
 * Override the font used for all UI text. Pass a caller-owned font (e.g. a TTF
 * atlas) for crisp text at any size; the renderer does not take ownership and
 * the font must outlive its use. Defaults to raylib's bitmap GetFontDefault().
 */
void EcsUiRaylibSetFont(Font font);
/* Revert to GetFontDefault(). */
void EcsUiRaylibResetFont(void);

#ifdef __cplusplus
}
#endif

#endif
