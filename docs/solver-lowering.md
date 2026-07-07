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
native solver reports unsupported text as stage 5, ZStack as stage 6, and FIT
sizing as stage 3 through the frame backend error callback, and returns no draw
list for that run.

The Stage 2 grow subset is deliberately narrower than Clay's full grow/shrink
algorithm. It covers positive free space on the parent's layout axis with
unconstrained grow children whose initial main-axis size is zero. Clay adds
`remaining / grow_count` as a float to each such child; there is no explicit
integer rounding after layout, and the scoreboard compares logical floats after
Clay's scale conversion. Compression, max clamps, grow children with non-zero
initial main-axis content, and scroll-container exceptions are left for later
sizing stages.
