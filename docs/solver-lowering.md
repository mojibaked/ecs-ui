# Native Layout And Paint Contract

This document describes the shipped ecs-ui frame path after the native cutover.
The frame backend is native-only: `EcsUiFrameRun` solves layout, builds the
interaction frame, records scroll content, builds an `EcsUiPaintList`, and
returns that paint list to the host. Renderers consume `EcsUiPaintList`
directly.

The old verification notes in `docs/plans/` and the Stage 8 gate artifacts are
history. They explain how this contract was proven, but they are not required to
understand or use the current backend.

## Frame Data Flow

1. The app builds an `EcsUiTreeSnapshot` from ECS components.
2. `EcsUiFrameRun` applies native layout to the snapshot in place. Layout rects
   are logical, root-relative coordinates.
3. The frame builds an `EcsUiInteractionFrame` from solved snapshot rects when a
   frame argument is supplied.
4. Scroll content reports are stored from the native solver. `EcsUiScrollState`
   remains the only authority for scroll offsets and content dimensions.
5. The paint pass builds `EcsUiPaintList` from the solved snapshot and theme.
6. Hosts render the returned paint list with the matching snapshot generation.

The paint list and any aliased snapshot data remain valid only until the next
frame run or backend shutdown, and only while the source snapshot remains alive
and unmodified. `paint->generation` must match `tree->generation` before
rendering.

## Coordinates And Scale

Frame inputs that describe the platform surface are physical pixels. Snapshot
layout and paint output are logical coordinates. The raylib paint renderer
converts logical rects to physical rects as:

```text
physical = roundf(logical * tree->scale + physical_bounds_origin)
```

The renderer applies this scale-then-round rule to x, y, width, height, and text
origins. Culling is a renderer optimization: correctness tests run with culling
off, and culling on must preserve visible pixels.

Padding and child gaps use the solver's stable U16 physical snap on layout
paths:

```text
logical_used = (uint16_t)(authored_logical * scale) / scale
```

This applies to stack padding, stack gaps, and pressable/text-field content
origins that derive from padding.

## Layout Semantics

The native solver consumes the snapshot tree, a measure callback, surface size,
layout options, and optional test scroll-offset inputs. It writes rects,
scroll-content dimensions, and failure information through native data
structures only.

Core layout rules:

- Root layout uses the authored root sizing against the viewport. Grow fills the
  viewport when content is smaller and compresses to the greater of min content
  and viewport when content overflows.
- Stack direction comes from `stack.axis`; depth maps to vertical layout.
- Main-axis grow distribution starts from each child's solved content size,
  water-fills remaining space, and uses physical-pixel epsilon.
- Overflow compression is largest-first with dropout at min dimensions. The loop
  has a defensive iteration bound and reports an internal error if it cannot
  converge.
- Fit sizing is solved bottom-up for content/min dimensions and top-down for
  final sizes. Empty fit containers contribute zero on axes where they have no
  visible flow children, except fixed authored axes keep their fixed value.
- Child alignment changes placement only, never solved sizes. Main-axis overflow
  follows the pinned asymmetric rule: horizontal center/end may shift left under
  overflow; vertical center/end do not shift upward before the nonnegative clamp.
- Text nodes with `WRAP_NONE` measure by word/range through the shared style text
  measure helper. Wrapper height is resolved text size plus 8 logical units;
  text min width is the longest measured word.
- Text-field pressables synthesize virtual flow entries for value segments,
  caret, and selection. The value text node itself has no emitted layout.
- ZStack has at most one normal flow child; additional unplaced children and
  placed children are solved as floating roots.
- Visual offsets reserve an empty layout slot using the node's flow sizing, then
  place the real subtree at slot position plus offset. Text offset-slot height
  uses the node-local text style, while the real text subtree uses inherited
  style.
- Scroll containers clip along their authored axes. Clipped axes exclude child
  mins for parent compression, skip compression inside the clipped axis, and
  report content dimensions from final child placement.

## Paint Semantics

`EcsUiPaintList` is the durable renderer artifact. Item order is draw order.
Items are grouped by visual roots, sorted by `(z, root_creation_order)`, then
flattened in each root's depth-first order.

Roles currently emitted:

- `BOX`: resolved fill only.
- `BORDER`: per-side logical widths and resolved color. Border items are emitted
  on node unwind after children and before the node's clip-scope end marker.
- `BEVEL_EDGE`: four computed edge strips in top, left, bottom, right order.
- `NINE_SLICE`, `CUSTOM`, `ICON`: source-entity references plus resolved color or
  tint data as needed by render callbacks.
- `TEXT_RUN`: byte range into the snapshot text, font id, font size, letter
  spacing, and resolved color.
- `CARET` and `SELECTION`: rectangle fills emitted for text-field synthetics.
- `CLIP_SCOPE`: start and end markers for scroll containers.

No image paint primitive exists. Current product trees render image-like content
through custom, icon, or nine-slice callbacks. If a future product needs raw
image painting, add a neutral paint role deliberately instead of smuggling
backend-specific payloads through an existing role.

The paint pass is pure with respect to layout: it reads solved snapshot rects and
theme/style data, writes a frame-owned paint list, and never feeds back into
layout or interaction.

## Clip And Z Order

The base tree uses `options.z_index`. ZStack floating children use base plus a
one-based ordinal in child order. Bevel edges use base plus 20. Visual-offset
wrapper subtrees use z 0. Absolute z values clamp to the signed 16-bit range
before sorting or renderer emission.

Scroll containers emit `CLIP_SCOPE` start before their own drawable item and end
after the clipped subtree. Drawable items inside the nearest scroll container
carry that scope's logical bbox. Floating roots inside a clipped subtree are not
tagged with the clip when their sorted root is outside the clipped root.

Nested clip scopes are emitted in draw order. The raylib scissor API is
non-stacking, so paint emits the exact start/end sequence required by the native
renderer rather than relying on implicit scissor nesting.

## Interaction Semantics

Interaction is built from solved snapshot rects. Hit testing uses the same visual
root order as paint: sorted roots, stable root order, then within-root DFS.
Targets include area blockers, pressables, scroll containers, and capture roots.

Pointer capture is stored in `EcsUiInteractionState`. Collection handles covered
captures, stale captures, missed release, secondary/middle capture, release
outside the original target, subscribed wheel events, and direct scroll-container
wheel routing through the neutral interaction frame.

`EcsUiFrameCollectEvents` must be called for the frame produced by the most
recent `EcsUiFrameRun` before any other run, including headless runs. Otherwise
the frame is stale and collection reports `ECS_UI_FRAME_ERROR_STALE_INTERACTION_FRAME`.

## Scroll Ownership

`EcsUiScrollState` stores logical `offset_x`, `offset_y`, `content_w`, and
`content_h`. Wheel routing writes pending absolute scroll updates into the
interaction frame. `EcsUiFrameApply` commits pending updates to ECS. 
`EcsUiFrameSettleScroll` clamps and writes the component from native scroll
content reports. Layout consumes the component on the next frame, preserving the
one-frame-lag input timing.

## Error Contract

Surviving public frame errors are native-produced and native-tested:

- already initialized
- not initialized
- missing text measure callback
- invalid argument
- allocation failure
- element or paint capacity
- text measure capacity
- duplicate id
- floating parent not found
- stale interaction frame
- internal error

Text line overflow is a capacity error and fails the frame rather than silently
truncating. This matches the frame contract for other capacity failures.

## Native Renderer Contract

`EcsUiRaylibRenderPaintList` renders the paint list directly. It asserts the
paint generation matches the snapshot generation and that the render context
scale matches the snapshot scale. It resolves custom/icon/nine-slice source
entities against the supplied snapshot before invoking callbacks.

The frame-level culling setter is retained as a stable API hook; current native
renderer culling is controlled by raylib paint render options. Future frame-level
culling must remain a performance optimization only and may not alter visible
pixels.
