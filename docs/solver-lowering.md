# Native Solver Lowering Notes

Stage 0 investigated sharing a lowered layout IR with the Clay emit path. The
Clay path is currently an immediate macro walk: lowering and `CLAY(...)` calls
are interleaved with target registration, text-field synthetic elements, visual
offset wrappers, and paint-only custom/border/bevel declarations. Extracting a
shared IR would require rewriting that walk and would risk behavior drift in the
reference backend before the prototype has a scoreboard.

For this prototype, the native solver therefore consumes `EcsUiTreeSnapshot`
and reimplements only the staged lowering semantics needed by the active parity
stage. Each later stage must pin its lowering rules here before code. The
current Stage 1 rules are:

- The root is solved in logical tree-root coordinates. With frame layout
  options, an explicit preferred root size is fixed even when it is smaller
  than the viewport. Without an explicit fixed preferred size, the viewport is
  the grow baseline and Clay's grow root can still expand to preferred content.
- Stack side padding uses the authored side when positive, otherwise uniform
  padding, clamped to non-negative values. Layout padding and child gaps match
  Clay's `uint16_t` path: scale to physical pixels, clamp/truncate to U16, then
  compare/enrich back in logical units.
- Vertical stacks place children top-to-bottom with `gap`; horizontal stacks,
  buttons, and pressables place children left-to-right with `gap`.
- Buttons use 14 logical units of left/right padding and an 8 unit child gap.
  Pressables use box padding when positive, otherwise 12 logical units, and an
  8 unit child gap unless they host a text-field view.
- Opacity skip is cumulative: a child is emitted only when
  `parent_opacity * clamp01(child.opacity) > 0.01`.
- Explicit `GROW` sizing on stacks overrides preferred fixed sizes, matching
  the Clay bridge's `EcsUiClayApplySizing` order.
- Stage 1 does not solve FIT, main-axis grow distribution, alignment,
  floating, scroll, or text wrapping.
- Default box sizes match the Clay bridge for the staged node kinds: custom
  height defaults to 96, button and pressable heights default to 46, and icons
  are 16 by 16.

Unstaged features fail loudly instead of producing plausible rects. Stage 6c
removes the final emitted-feature guard by solving pressables with
text-field synthetic children; after 6c, the remaining non-coverage is the
adoption-time list outside rect parity.

The Stage 2 grow subset is deliberately narrower than Clay's full grow/shrink
algorithm. It covers positive free space on the parent's layout axis with
unconstrained grow children whose initial main-axis size is zero. Clay adds
`remaining / grow_count` as a float to each such child; there is no explicit
integer rounding after layout, and the scoreboard compares logical floats after
Clay's scale conversion. Compression, max clamps, grow children with non-zero
initial main-axis content, and scroll-container exceptions are left for later
sizing stages.

## Stage 3: FIT and preferred-size semantics

### Where FIT actually reaches Clay (bridge lowering)

All stacks (root, VStack, HStack, ZStack) get their sizing from
`EcsUiClayFlowLayout`, applied in this order:

1. Defaults: width `GROW(0)`, height `GROW(0)` — except a stack whose parent
   is a non-ZStack container, whose height default is
   `CLAY_SIZING_FIXED(EcsUiClayPreferredHeight(tree, index, 0.0f))`, the
   hand-rolled bottom-up walk (physical units, available width 0).
2. Authored `preferred_width/height > 0` replaces the default with
   `FIXED(logical * scale)` (float, no U16 truncation).
3. Explicit sizing overrides both via `EcsUiClayApplySizing`: `GROW` maps to
   `GROW(0)`, `FIT` maps to `CLAY_SIZING_FIT(0)`. Explicit FIT therefore
   DISCARDS an authored preferred size entirely (min stays 0).

FIT never reaches Clay from any other node kind: buttons emit
`preferred_width > 0 ? FIXED : GROW(0)` plus fixed height; text, icon,
pressable, and custom sizing are pinned in Stage 1. Custom nodes lower sizing
through `EcsUiClayApplyCustomSizing`, which maps ONLY `GROW`; `FIT` on a
custom node falls through to the inferred sizing exactly like `AUTO`. The
solver mirrors that by treating custom FIT as AUTO.

### Clay's FIT algorithm (what `FIT(0)` means)

Clay solves each axis in a full pass (width first, then height; text wrapping
between them is irrelevant under WRAP_NONE). Per axis:

1. Bottom-up content pass (element close): every element's working size
   starts at content size — along its own layout axis
   `padding + sum(children working sizes) + gap * (n - 1)`, off its layout
   axis `max(child working size) + padding` — then is clamped to the sizing
   min/max. FIXED(v) clamps to exactly v; FIT(0) and GROW(0) leave the
   content size standing (min 0, max unbounded). So FIT *and GROW* elements
   both enter the top-down pass at content size. A parallel `minDimensions`
   accumulates the same sums (floored by sizing min), and is the compression
   floor; clip containers exclude children's mins on the clipped axis.
2. Top-down BFS (`Clay__SizeContainersAlongAxis`), parents final before
   children. "Resizable" children are those not FIXED (and not PERCENT):
   i.e. FIT and GROW; WRAP_NONE text is never resizable.
   Clay compares `CLAY__EPSILON` in physical pixels; the solver keeps logical
   sizes but scales this threshold to `0.01 / scale` for water-fill,
   compression, and equality grouping.
   - Along the parent's layout axis, with
     `free = parent - padding - (sum child sizes + gaps)`:
     - `free > 0` with at least one GROW child: iterative water-fill over the
       GROW children only — repeatedly raise the smallest (toward the second
       smallest, else split equally), until free space is spent. FIT children
       never receive free space. NOTE: this supersedes the Stage 2 shortcut
       (`remaining / grow_count` added once), which coincides with Clay only
       while every grow child starts at the same (zero) main size. Once
       content-bearing GROW children exist — which FIT trees introduce — the
       solver needs true water-fill.
     - `free < 0`: compression — repeatedly lower the LARGEST resizable
       children (FIT and GROW alike) toward the second largest, each floored
       at its `minDimensions`; floored children drop out. A parent that clips
       on this axis skips compression entirely (ecs-ui only emits clip for
       scroll containers — stage 6 — so in stage 3 scope compression always
       applies). For the Stage 3 supported nodes, minDimensions equal content
       size, so compression has no shrinking headroom in practice; the solver
       still mirrors Clay's buffer/dropout loop, and later text/scroll stages
       must add goldens once size can exceed min.
   - Off the parent's layout axis: GROW children snap to the parent's inner
     size; then EVERY resizable child (FIT included) is clamped to
     `[its minDimensions, parent inner size]`. An oversized off-axis FIT
     child is therefore held at its min content size, not truncated below it.

### Interaction with the hand-rolled preferred walk

The Stage 1/2 preferred walk (`EcsUiClayPreferredHeight/Width`) is unchanged
by this stage but interacts with FIT in two pinned ways:

- The walk's authored-size early-out requires `sizing == AUTO`; a FIT-sized
  stack inside an AUTO-height stack contributes its recursive children-sum
  (with all walk quirks: HSTACK equal-split width, ZStack summing heights
  without gap) — NOT Clay's true FIT result for that stack. The two can
  disagree; the FIXED height computed by the walk wins for the outer stack,
  while the inner FIT stack is then solved by Clay against that fixed box.
  Goldens must cover a FIT stack nested inside an AUTO-height stack.
- Units mismatch, pinned as-is: the bridge walk sums LOGICAL padding/gap and
  scales without U16 truncation, while the real layout pass truncates each
  side to physical U16. The two agree only when scaled paddings are integral.
  The solver's walk must reproduce the bridge's UNtruncated arithmetic for
  preferred sizes while keeping truncated padding for actual placement — a
  fractional-padding golden under an AUTO-height stack pins the difference.

### Stage 3 scope

In scope: explicit FIT on stack axes (two-pass: bottom-up content +
minDimensions, top-down distribute/clamp), FIT-in-GROW trees (requiring the
water-fill grow upgrade above), deep FIT chains, along-axis compression with
min floors, off-axis clamping, custom-FIT-as-AUTO. Root FIT behaves like any
stack FIT (content-sized below the viewport). Stage 3 still left root
horizontal-axis layout and child alignment to Stage 4, and text (stage 5),
ZStack/floating/scroll (stage 6) continued to fail loudly.

## Stage 4: child alignment and layout direction

### What the bridge emits

- Layout direction comes from `node->stack.axis` in `EcsUiClayEmitStack`, for
  EVERY stack including the root: `ECS_UI_AXIS_HORIZONTAL` maps to
  `CLAY_LEFT_TO_RIGHT`, everything else (VERTICAL and DEPTH alike) to
  `CLAY_TOP_TO_BOTTOM`. Stage 4 removes the stage-3 loud failure for
  horizontal-axis roots and derives the solver's direction from `stack.axis`
  for root/VStack/HStack uniformly (the builder couples kind and axis for
  non-root stacks, so only the root can actually vary). ZStack still fails
  loudly (stage 6). Known latent asymmetry, unreachable today: the solver's
  preferred-height walk dispatches sum-vs-max on `stack.axis` while the
  bridge walk dispatches on node KIND, so a horizontal-axis ROOT would sum
  children's heights in the bridge walk but take the max in the solver's —
  the walk is only ever invoked on non-root stacks, where kind and axis
  coincide. Revisit if a later stage walks the root.
- Stacks (root included) emit authored alignment:
  `childAlignment.x = align_x` (START→LEFT, CENTER→CENTER, END→RIGHT) and
  `childAlignment.y = align_y` (START→TOP, CENTER→CENTER, END→BOTTOM).
- Buttons hardcode `childAlignment = {CENTER, CENTER}`.
- Pressables hardcode `childAlignment.y = CENTER` and leave x at LEFT.
- Text-field synthetic elements and ZStack child wrappers also carry
  alignment, but those subtrees stay behind the stage 5/6 loud failures.

### Clay's positioning algorithm (final layout pass)

Positioning happens AFTER all sizing, top-down, using final (post-grow,
post-compression) child sizes; alignment never changes a size. Per parent:

- Main-axis alignment is computed ONCE per parent:
  `extra = parent_size - padding_both - (sum final child sizes + gaps)`,
  then LEFT/TOP keeps 0, CENTER adds `extra / 2`, RIGHT/BOTTOM adds `extra`.
  ASYMMETRIC OVERFLOW QUIRK, pinned as-is: on the horizontal main axis
  (LEFT_TO_RIGHT parents) negative `extra` IS applied — a CENTER/RIGHT row
  whose content overflows shifts left by half/all of the deficit. On the
  vertical main axis (TOP_TO_BOTTOM parents) `extra` is clamped to >= 0
  BEFORE it is applied — an overflowing CENTER/BOTTOM column never shifts up.
- Off-axis alignment is computed PER CHILD: the off-axis cursor resets to the
  leading padding, then `white = parent_size - padding_both - child_size`
  adds 0 / half / all for START/CENTER/END. `white` is NOT clamped: a child
  held above the parent's inner size by its minDimensions floor (stage 3
  off-axis clamp) gets a NEGATIVE offset — it overhangs up/left.
- Children advance along the main axis by `final child size + gap`; padding
  and gap here are the truncated physical-U16 values (matching placement
  arithmetic already pinned in stage 1, not the untruncated preferred walk).

### Stage 4 scope

In scope: authored align_x/align_y on root/VStack/HStack, button
CENTER/CENTER, pressable y-CENTER, both main-axis and off-axis semantics
including the overflow quirks above, alignment interacting with GROW leftover
space (grow consumes free space first; alignment moves only what remains) and
FIT children inside larger boxes, and stack.axis-derived direction (removing
the root-horizontal loud failure). Out of scope, still failing loudly: text
(stage 5), ZStack/floating/scroll (stage 6).

## Stage 5: text (WRAP_NONE)

### Bridge lowering for TEXT nodes

A TEXT node lowers to a WRAPPER container carrying the node's id — the
wrapper is the ONLY rect the scoreboard sees for the node — with:
width `GROW(0)`; height `FIXED(scaled(resolved_size + 8))` (untruncated,
via `EcsUiClayFixed`); `childAlignment.x` LEFT, `.y` from the authored text
layout's `align_y`, defaulting to CENTER (`EcsUiClayDefaultTextLayout`:
align_x START, align_y CENTER). Inside sits one Clay text element
(`CLAY_TEXT`) with `wrapMode = CLAY_TEXT_WRAP_NONE`, `lineHeight` 0,
`letterSpacing` 0, `textAlignment` from the text layout's `align_x` (render
only, never affects rects), and
`fontSize = U16(scaled(resolved_size))` — physical, truncated. The inner
text element has an auto-generated Clay id (never a node id), so it is
INVISIBLE to rect parity; it matters only through the sizes it feeds the
wrapper.

Resolved size: with an inherited text style whose per-role size field is
positive, that styled size; otherwise role defaults — TITLE 28,
BUTTON/LABEL/BODY 18, CAPTION 13. Text style INHERITS down the tree: each
node with `has_text_style` replaces the style for its subtree
(`EcsUiClayEmitNodeContent`), so a TEXT node's font size can come from any
ancestor. The solver must reproduce this inheritance walk.

### Clay's measured-text model

Clay never measures whole strings: `Clay__MeasureTextCached` splits on
spaces and newlines and calls the user measure callback PER WORD (plus one
call for a single space to get `spaceWidth`). Per line, width accumulates
word widths with `spaceWidth` added for each interior space; the element's
unwrapped width is the max line width minus `letterSpacing` (0 for us);
unwrapped height is the MAX single-word measured height (NOT lines x
height); `minWidth` is the LONGEST WORD's width. The text element enters
layout with `dimensions = {unwrapped width, unwrapped height}` and
`minDimensions = {minWidth, height}`. All in physical pixels; the callback
receives the truncated physical fontSize.

Under WRAP_NONE the text element is NEVER in the resizable buffer (only
WRAP_WORDS text is): it is skipped by grow, compression, AND the off-axis
clamp — it keeps its measured size unconditionally. Clay's wrap stage still
runs (it does not check wrapMode): text without newlines whose width fits
its own element takes a single-line fast path; text WITH embedded newlines
is split into per-line boxes and the text ELEMENT's height becomes
`line_height * line_count`, propagated to ancestors through a post-wrap
height DFS clamped by each ancestor's sizing min/max. The TEXT wrapper's
FIXED height is that clamp, so newline-driven height NEVER escapes the
wrapper — rect parity above the wrapper is unaffected, and the overflow of
the inner element within its wrapper is invisible to the scoreboard. The
solver therefore models text as single-box measured dims and does not need
line boxes.

### What text feeds the surrounding layout

- The wrapper's GROW width enters the top-down pass at CONTENT size = the
  text element's measured width (stage-3 rule: GROW starts at content), so
  measured text width propagates into FIT ancestors' content sizes.
- The wrapper's `minDimensions.width` = the text `minWidth` (longest word):
  with text present, `min < size` becomes REACHABLE and stage 3's
  compression loop finally has shrinking headroom. Stage 5 MUST add
  compression goldens (wrapper squeezed between full content width and
  longest-word floor, multi-child largest-first sequences, at-min children
  staying in the divisor per the Clay-faithful loop). Stage 5 can make
  at-min buffer membership load-bearing, but has not produced a distinct
  rect endpoint for Clay's order-dependent `widthToAdd` scan: WRAP_NONE text
  is the only supported node kind with `min < size`, while non-text resizable
  siblings still start at their min floor. A later stage that introduces more
  `min < size` resizable kinds must add an order-sensitive golden.
- Root viewport interaction changes once text makes `min < size` reachable.
  Clay sizes the ecs root as a child of its fixed viewport root container
  (`LEFT_TO_RIGHT`). Root width is along that container axis: GROW fills the
  viewport when content is smaller, and both GROW and FIT compress overflowing
  content to `max(root_min_width, viewport_width)`. Root height is off-axis:
  GROW becomes `max(root_min_height, viewport_height)`, while FIT becomes
  `max(root_min_height, min(content_height, viewport_height))`.
- The hand-rolled preferred walk is asymmetric for TEXT, pinned as-is:
  `EcsUiClayPreferredHeight` returns `scaled(size + 8)` for TEXT, but
  `EcsUiClayPreferredWidth` returns 0 — an AUTO-height stack sees text height,
  but the walk never sees text width. Unlike actual TEXT wrapper emission, the
  preferred walk does NOT carry inherited ancestor text style; it only uses the
  TEXT node's snapshot-local `text_style` / `has_text_style`. The solver walk
  must match that bridge quirk (its TEXT height currently returns 0 and must
  change).

### Stage 5 scope

In scope: TEXT nodes (wrapper semantics above), measure-callback plumbing
into the solver (same per-word model, physical px, same truncated fontSize),
text style inheritance, wrapper y-alignment within parents (stage 4 machinery
reused), text-driven FIT content and compression floors. Out of scope, still
failing loudly: pressables with a text-field view (their synthetic
value/caret/selection elements are stage 6 — once TEXT stops failing loudly
this needs its OWN explicit guard), ZStack/floating/scroll (stage 6).
Newline-in-text line boxes need no solver support (see above) but a golden
should prove wrapper-rect parity for newline text.

## Stage 6: floating, ZStack, placement, visual offsets, scroll

Implemented in sub-stages, committed separately: 6a floating/ZStack/placement/
visual offsets; 6b scroll; 6c text-field synthetic elements (removing the
final emitted-feature guard).

### Clay's floating machinery (what the solver reimplements)

- A floating element is REMOVED from parent flow entirely: it is not in the
  parent's children for content sums, gaps, alignment, or minDimensions (the
  close pass only bumps `floatingChildrenCount`); it becomes its own layout
  tree root.
- Floating root sizing (per axis, before the normal top-down pass runs inside
  the floating subtree): GROW snaps to the ATTACH PARENT's final dimension on
  that axis; FIXED stays fixed; FIT keeps bottom-up content. Then the root is
  clamped only by its own sizing min/max; descendant content minDimensions do
  NOT floor the floating wrapper. The standard along/off-axis passes then run
  within the subtree, where oversized children can overhang or compress.
- Floating root positioning (final layout pass):
  `target = parent_attach_point(parent bounding box) - element_attach_offset
  (0 / half / full of the floating root's dims per element attach point)
	  + floating offset`. ATTACH_TO_PARENT uses the declaring parent's box.
	  ATTACH_TO_ROOT uses Clay's `Clay__RootContainer`, whose dimensions are the
	  full surface passed to `Clay_SetLayoutDimensions`, not the bounded viewport
	  wrapper used by frame layout options.
- ecs-ui never enables external scroll handling, so clay applies a clip's
  `childOffset` directly as the scroll shift during positioning, and floating
  roots attached inside clip contents do NOT get the extra childOffset
  correction.

### Bridge lowering: viewport wrapper and z-base

With layout options (the harness path), the whole tree is emitted inside a
floating viewport wrapper: FIXED physical bounds size, offset = bounds
origin, ATTACH_TO_ROOT, z = options z_index, which also becomes the base
added to every relative z below. Enrichment subtracts the bounds origin and
divides by scale, so node rects are logical root-relative — the solver's
existing coordinate space. z affects paint order only, never rects: the
solver ignores z entirely (adoption-time concern, already on the non-coverage
list).

### Bridge lowering: ZStack

The ZStack node itself is a normal container: FlowLayout sizing + stack
padding, NO direction/gap/childAlignment overrides — so it keeps Clay
defaults (LEFT_TO_RIGHT, gap 0, LEFT/TOP alignment). Empty-container off-axis
padding is direction-generic in Clay: a LEFT_TO_RIGHT container with no flow
children has cross-axis height 0, while a TOP_TO_BOTTOM container with no flow
children has cross-axis width 0. Main-axis padding still contributes. Children:

- The FIRST child, if it has NO placement, is emitted as a normal flow child
  (including the visual-offset wrapper path below).
- Every other child — and the first child too when it HAS a placement — is
  wrapped in a floating wrapper. z starts at 1 for the first floating child
  and increments per floating child (base + z). The opacity gate for floating
  children multiplies the ZStack's cumulative opacity with the child's own.
- Floating wrapper id: child id + "Floating" suffix (the child's own element,
  emitted inside via EmitNodeContent, carries the node id and is what the
  scoreboard sees) — EXCEPT a placed TEXT child, where the wrapper itself
  takes the NODE id (no suffix) and directly contains the text content, with
  childAlignment from the child's text layout (x and y both, unlike the
  normal TEXT wrapper which only sets y).
- Floating wrapper sizing: with placement, width/height are FIXED(scaled
  value) when the placement value > 0, else GROW(0); without placement,
  GROW(0) both axes. A GROW axis on a non-point floating root snaps to the
  ZStack's final dims per the floating-root sizing rule above. A point-anchored
  GROW axis snaps to Clay's full root container/surface dimension because
  `attachTo = ROOT`; this is separate from the flip/clamp root-size below.
- Floating wrapper childAlignment: LEFT/TOP except the placed-text case.
- Floating config:
  - NOT point-anchored: attachTo PARENT (the ZStack); attach points from the
    placement's child_x/child_y (element side) and parent_x/parent_y (parent
    side) via the all-9 mapping when placed, else LEFT_TOP/LEFT_TOP; offset =
    scaled(visual.offset + placement.offset) (placement part only when
    placed).
  - Point-anchored (placement.mode == POINT): attachTo ROOT with
    LEFT_TOP/LEFT_TOP attach and offset computed by the flip/clamp below.
    `placement.offset_x/y` are ignored in this mode; only `visual.offset_x/y`
    are added to the authored point before flip/clamp.

### Point-anchor flip/clamp (solver-owned, logical root space)

Per axis, with point = placement.point + visual.offset (logical), size =
the AUTHORED placement size (NOT the solved size), root_size = logical root
bounds:
1. if size > 0 and root_size > 0 and point + size > root_size: flip to
   point - size;
2. clamp to >= 0;
3. if root_size > 0 and 0 < size <= root_size and still overflowing: pin to
   root_size - size.
A zero/GROW placement size disables flip and overflow-pin (but not the >= 0
clamp). The bridge then emits physical `root_origin + resolved * scale`
attached LEFT_TOP to Clay's root; after enrichment subtracts the bounded
viewport origin, the node rect lands at the resolved logical point. If that
axis is GROW, the floating wrapper's size still comes from the full surface
root container as described above. The solver computes the resolved point
directly in logical space.

### Bridge lowering: visual-offset wrapper pairs (ANY node kind, any parent)

A non-root node with |visual.offset_x| or |visual.offset_y| > 0.01 is
emitted as a Layout/Visual wrapper pair: the Layout wrapper (id suffix
"Layout") takes the node's FlowLayout SIZING and occupies the node's flow
slot; inside it a Visual wrapper (suffix "Visual") floats ATTACH_TO_PARENT
LEFT_TOP/LEFT_TOP with offset = scaled(visual.offset), GROW/GROW (= sized to
the Layout wrapper), z unset (0); the node's own element is emitted inside.
Because the Visual wrapper is floating, the Layout wrapper's content and
minDimensions are EMPTY: FIXED axes keep their fixed FlowLayout size, while
GROW/FIT axes contribute 0 content/min to the parent. For TEXT specifically,
the Layout wrapper's fixed height is `EcsUiClayFlowLayout`'s value:
snapshot-local role/style size + 8, not inherited style; the real TEXT wrapper
inside the floating Visual wrapper still uses inherited style. Net rect effect:
the node and its whole subtree sit at the resolved empty/fixed flow slot
SHIFTED by the scaled visual offset, while flow around the node is unaffected
by the node's real content. NOTE: stages 1-5 never covered visual offsets
(goldens never authored them, and the solver ignored them before 6a) — stage
6a closes that silent gap for all node kinds, not just ZStack children.

### Bevel edge floaters

Bevelled boxes emit four 1px floating strips (suffixed ids, z base+20,
corner attach points, GROW/FIXED sizing). They are floating (no flow
impact), never carry node ids, and are paint-only: the solver ignores them.

### Bridge lowering: scroll containers (6b)

A stack with has_scroll_view emits the same container declaration plus
`clip = {horizontal, vertical, childOffset}` where the flags come from the
authored scroll axes and childOffset is the snapshot scroll-state offset,
scaled to physical px for Clay. Layout effects the solver must reproduce:
- No compression on a clipped axis (children keep their sizes on overflow).
- Off-axis GROW children get their max extended to max(parent inner size,
  inner content size) only when the axis currently being solved is clipped.
  For example, a vertical stack with horizontal clip extends x-axis grow
  children; a vertical-only scroll container does not extend those x sizes.
- The close pass EXCLUDES children's minDimensions on clipped axes (the
  scroll container's own min on that axis is padding only) — this feeds
  ancestor compression floors.
- Normal descendant positions shift by childOffset during placement (scroll
  offsets move children; the container rect is unaffected). Floating roots
  attached inside clip contents do not receive Clay's extra clip-root
  childOffset correction because ecs-ui leaves external scroll handling off;
  ZStack floaters inside a normally scrolled child still move when their
  attach parent rect was shifted by normal placement.
- Content dims: Clay reports contentSize = content + padding via
  Clay_GetScrollContainerData, computed in the positioning pass from final
  child sizes. The solver computes and reports the same per scroll node; the
  harness compares them against Clay's (extending the scoreboard beyond
  rects for scroll nodes only). The bridge ALSO keeps a hand-rolled fallback
  (EcsUiClayStackContentWidth/Height) used only when Clay reports zero — the
  fallback is input-routing code, not layout, and is NOT part of solver
  parity.
- Native solver run options normalize scroll offsets to authored clipped axes
  before placement. Clay's positioning would add a nonzero off-axis childOffset
  component if one were present, but ecs-ui's scroll-state mutation path does
  not produce that off-axis state.

### Scroll state ownership (stage 7.0)

Scroll offsets and content dimensions are ECS runtime state, not backend
retained state. A scrollable entity owns an optional `EcsUiScrollState`
component:

```
EcsUiScrollState { float offset_x, offset_y; float content_w, content_h; }
```

`EcsUiReadTree` copies that component into the logical tree snapshot beside
`EcsUiScrollView`. Clay lowering scales the snapshot offsets to physical px
and passes them as `clip.childOffset`; it must not read
`Clay_GetScrollOffset()` or Clay's retained `scrollPosition` as layout input.
The native solver consumes the same snapshot offsets, so both backends observe
one source of truth.

Wheel routing still runs without world access. It clamps the proposed offset
using the current backend-reported content dimensions and container dimensions,
then records a pending scroll update on the interaction frame keyed by the
scroll container entity. It does not mutate Clay's retained `scrollPosition`
as truth. Routing runs after emit-time global scale is restored, so it scales
the existing logical snapshot offset with the target's captured tree scale
before adding physical wheel deltas, then stores the pending update back in
logical units. `EcsUiFrameApply` has the world and commits those pending
updates to `EcsUiScrollState` before the next `EcsUiReadTree`.

`EcsUiFrameSettleScroll` clamps the ECS component against the latest reported
`content_w/content_h` and viewport dimensions from the active backend and
writes the clamped component back. Valid reports write content dimensions
unconditionally, including zero-size content, so removed content clears stale
scroll range and clamps offsets to zero. Clay's `Clay_UpdateScrollContainers`
remains only inert internal upkeep; each emit synchronizes Clay from the
snapshot component so retained Clay state cannot become a second authority.

Timing remains one-frame-lagged: a wheel collected from frame N produces a
pending update, `EcsUiFrameApply` commits it, and frame N+1's snapshot/emit
consumes the new offset. The frame that collected the wheel does not move
content in-place.

### Paint pass (stage 7.2 skeleton)

The paint pass produces a durable renderer-neutral `EcsUiPaintList` from the
PLACED snapshot plus theme. It is a pure subphase of `EcsUiFrameRun`: both the
native solver path and the Clay path run paint only after they have written
root-relative logical layout rectangles into `tree->nodes[]`. Paint never reads
Clay, re-measures, re-walks backend layout internals, or feeds back into
layout.

The artifact is frame-owned packed POD. ECS stores only the current handle:
`EcsUiFrameArtifacts { const EcsUiPaintList *paint; uint32_t generation; }`.
The frame backend advances one generation counter only when a new paint list is
successfully produced, stamps the snapshot and list with that generation, and
`EcsUiFrameApply` writes the handle/generation into the world.
`artifacts.paint->generation` is authoritative; the duplicated
`artifacts.generation` mirrors it for cheap consumers. On a failed or aborted
run, generation does not advance and the last coherent artifact is held.
Item keys are unique within `(paint->tree, paint->generation)`.

Each paint item has an `EcsUiPaintKey { source, role, part, generation }`, a
primitive kind, a logical root-relative rect, a resolved clip field, cumulative
opacity, and a payload. The 7.2 skeleton defines all roles and primitive slots
but emits only box items: one `ECS_UI_PAINT_ROLE_BOX` item for each placed node
whose resolved background fill has nonzero alpha. Box items carry only key,
rect, neutral `EcsUiColorF` fill, cumulative opacity, and their list order.
Radius, borders, bevel, nine-slice, text, selection/caret, icons, custom items,
and real clip scopes are intentionally left for later stages.
If the source snapshot was truncated, that source truncation is surfaced through
`paint_list->truncated` even when the paint list itself has remaining capacity.

Draw order is the native semantic order: z-sorted roots, then depth-first. 7.2
has one root and no floating/z paint, so this reduces to snapshot depth-first
order, but the implementation owns the rule as a paint-list property rather
than inheriting Clay command order.

String/data references in future payloads alias the source snapshot and remain
valid until the next paint-list reset and only while the snapshot storage is
alive and unmodified. The list storage is fixed-capacity frame storage; emission
fails loudly by truncating the list if capacity is exceeded, and it does not
reallocate after item pointers are captured.

### Stage 6 scope

6a in scope: ZStack containers, floating wrappers with attach-point
placement (all 9 x 9), placement sizing, point-anchor flip/clamp, z-index
bookkeeping ONLY insofar as it never affects rects, visual-offset wrapper
pairs for all node kinds, bevel floaters as paint-only no-ops. 6b in scope:
clip sizing rules, scroll offsets, content dims. 6c in scope: pressables with
a text-field view. Out of parity scope permanently (adoption-time list):
z/paint order, pointer-over ordering, capture modes, scissor commands,
retained offset MUTATION (the wheel path).

### Bridge lowering: text-field pressables (6c)

A pressable with `has_text_field_view` keeps its normal declaration
(LEFT_TO_RIGHT, box padding else 12, gap 0, childAlignment y CENTER) but its
VALUE child — the TEXT child whose entity matches `text_field_view.value_node`
— is replaced by synthetic inline segments; all other children emit normally.

- Segment order (view state; cursor/selection indices clamped to the value
  string length, selection normalized so start <= end, selection counts only
  when `focused && start < end`):
  - not focused: one inline range [0, end);
  - focused, no selection: [0, cursor), caret, [cursor, end);
  - focused with selection: [0, sel_start), caret if cursor == sel_start,
    selection wrapper containing [sel_start, sel_end), caret if cursor !=
    sel_start, [sel_end, end).
- Inline ranges are BARE `CLAY_TEXT` elements (auto ids, NO size+8 wrapper):
  dims = per-word measured size of the substring, min = longest word in the
  substring, non-resizable (WRAP_NONE). An empty range emits NOTHING (segment
  count varies with cursor position). Inline ranges use
  `EcsUiClayDefaultTextLayout()` rather than the VALUE node's authored text
  layout.
- The caret is a fixed element, id "_Caret" on the value node:
  width FIXED(scaled(caret_width > 0 ? caret_width : 2)), height
  FIXED(scaled(resolved size + 8)) — size resolution uses the INHERITED value
  style chain (value node's own style wins), same as segment fonts.
- The selection wrapper, id "_Selection" on the value node: default sizing
  (FIT both axes -> sizes to its inline range), childAlignment y CENTER. It is
  its own resizable flow item in the pressable and compresses independently
  from fixed text segments, the fixed caret, and any other FIT/GROW siblings.
- RECT-PARITY CONSEQUENCE: no element ever carries the VALUE node's id — the
  value TEXT node is NOT enriched on the clay side (`has_layout` false), and
  the solver must likewise emit no layout for it. The synthetic segments
  affect layout only through the pressable's content: measured text widths,
  caret width, and longest-word mins feed the pressable's flow sums and min
  floors (pressable gap is 0 for text fields).
- The value child is routed directly to `EcsUiClayEmitTextFieldValue`, bypassing
  normal `EmitNode` opacity gating. The value node's own `visual.opacity`
  therefore has no layout effect; parent opacity still gates the pressable
  subtree before child emission is reached.
- The pressable's preferred walk contributions are unchanged (walk height =
  preferred/46, walk width = 0 — the walk never sees the synthetics).
- If `value_node` matches no child, or matches a non-TEXT child, no child is
  replaced and the pressable emits its children normally.

### Stage 6c scope

Text-field pressables lower to the synthetic segment flow above; the loud
failure is removed. After 6c no emitted feature fails loudly; the adoption-
time non-coverage list (z/paint order, pointer-over, capture, scissor,
retained offset mutation) is unchanged.
