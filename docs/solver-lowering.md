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

Unstaged features must fail loudly instead of producing plausible rects. The
native solver reports unsupported root horizontal-axis layout as stage 4, text
as stage 5, and ZStack/floating/scroll as stage 6 through the frame backend
error callback, and returns no draw list for that run.

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
stack FIT (content-sized below the viewport). Still failing loudly: root
horizontal-axis layout (stage 4), text (stage 5), ZStack and floating
(stage 6), scroll/clip (stage 6). Child alignment remains stage 4 scope and is
not exercised by Stage 3 goldens.
