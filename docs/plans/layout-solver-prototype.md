# Native layout solver prototype (dual-backend, clay as reference)

Accepted 2026-07-07 (Codex design review round 1 amendments folded in). Step 2 of
`layout-backend-direction.md`. Depends on the containment pass (done: ecs-ui a192a0a,
texelotl 98cff47): the solver plugs in BEHIND the backend-neutral `EcsUiFrameRun`
surface; hosts never change.

## Contract (user-fixed 2026-07-07)

The solver is layout-ONLY, Yoga-like: emitted node tree + measure callback in →
queryable layout state out. Output = enriched snapshot rects (the existing enrichment
contract IS the output contract). Paint (draw-list emission) is a SEPARATE stage, not
scored, and NOT attempted in this prototype pass. The parity scoreboard never inspects
draw commands.

Text wrap and line boxes (review finding 1): ecs-ui always emits `CLAY_TEXT_WRAP_NONE`
today — there is no authored wrap feature. Wrap/line boxes are FUTURE contract
scaffolding, excluded from prototype parity scope. If a line-box placeholder is kept it
is internal-only: `{node_index, byte_start, byte_end, logical rect}`.

## Input contract: the solver consumes the SNAPSHOT, reproducing bridge lowering

(Review finding 2.) The clay bridge does layout-significant LOWERING while emitting:
viewport wrapper; visual-offset layout/visual wrapper pairs; ZStack children after the
first unplaced one become floating wrappers with incrementing z; text-field
caret/selection/range elements; bevel edge floaters. The native solver consumes
`EcsUiTreeSnapshot` and must reproduce these lowering semantics. Stage 0 decides,
recorded in-code: extract a SHARED internal lowered layout IR from the emit path if
that is achievable without behavior change (preferred); otherwise pin the lowering
semantics in prose (docs/solver-lowering.md) and reimplement, with parity trees
covering each synthetic-element case.

Point-anchor placement flip/clamp (review finding 3) happens during EMISSION from
placement + root bounds — it is solver-side work, not inherited from ecs-ui: the
solver owns it in its floating stage.

## Solver feature scope — the emitted subset, corrected

- Sizing: GROW, FIXED, FIT per axis, with IMPLICIT ZERO minimums (grow/fit are emitted
  with 0 constraints; ecs-ui has no public min/max sizing — review finding 5).
- Direction: left-to-right, top-to-bottom; childGap; SIDE-SPECIFIC padding with
  clamp-to-positive/U16 conversions.
- Emitted defaults that are layout semantics (review finding 4): default node sizes,
  button/pressable padding+gap, custom-node default height, icon fixed 16, flow
  defaults, visual-opacity <= 0.01 subtree skip.
- Child alignment: x left/center/right, y top/center/bottom.
- Text: measure-callback-driven, WRAP_NONE only, align left/center/right.
- Floating/ZStack per the lowering contract above: attach points (parent/root, all 9),
  z-index incrementing for later ZStack children, point-anchor flip/clamp.
- Scroll containers: clip + externally-written offsets; content dims reported.
- NOT emitted, out of scope (review finding 7): percent sizing, aspect ratio, wrap
  variants; border never affects layout; nine-slice/image affect paint only.

## Shape

- `src/ecs_ui_solver.c` (+ internal header), consuming the snapshot per the input
  contract, producing enriched-snapshot rects.
- Backend selection: RUNTIME-selectable INTERNAL TEST HOOK, not host-facing, no public
  adoption flag, no second binary. Harness pattern: run clay, copy enriched rects, run
  native, compare in one process.
- Working memory: grow-only per-frame arena from the start (not per-node malloc).
- Scoreboard harness: `tests/ecs_ui_solver_parity.c` — identical trees through both
  backends, mock text measure, scale 1 and 2. Epsilon: 0.001f logical units default;
  exact match only for deliberately-integer fixed-box smoke cases. Failing diff prints
  first divergent node path + both rects. Golden policy: reuse tree-building helpers
  and broad cases from `ecs_ui_clay_parity.c`, but QUARANTINE tests asserting clay
  render commands/pointer internals/known quirks; the solver scoreboard owns its own
  feature-targeted goldens.
- Explicitly logged non-coverage of the rect scoreboard (review finding 6): clip and
  scissor behavior, scroll content dims and retained offset mutation, pointer-over
  ordering, capture modes, z/paint order. These are adoption-time verification work,
  recorded here so nobody mistakes green rects for full parity.

## Stages (each: harness trees first, then solver to green, commit per stage)

0. Lowering decision (shared IR vs pinned prose) + dual-backend test hook + scoreboard
   harness proving it detects divergence via a deliberate stub error.
1. Boxes: fixed sizing, side-specific padding, gap, both directions, nesting, emitted
   defaults — rect parity.
2. Grow: multiple growers, mixed fixed+grow, implicit zero minimums, distribution
   rounding pinned in prose first.
--- STOP LINE for the overnight session (review call): stages 3+ need pinned prose on
    FIT/preferred-size semantics (the bridge itself hand-rolls an approximation) and a
    fresh session. ---
3. Fit-content (two-pass), fit-in-grow, deep fit chains — after pinning semantics.
4. Alignment combinations with grow/fit interplay.
5. Text (WRAP_NONE measure paths, alignment).
6. Floating/ZStack lowering + point-anchor flip/clamp + scroll (clip rects, offsets,
   content dims).
7. (Separate plan revision) minimal unscored paint pass for adoptability.

## Acceptance

Per stage: scoreboard green at scale 1 and 2 on that stage's feature trees + all prior
stages'; full existing ctest green with clay as the active backend; no host-visible API
change; non-coverage list above logged by the harness output.
