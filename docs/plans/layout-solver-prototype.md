# Native layout solver prototype (dual-backend, clay as reference)

DRAFT 2026-07-07 for Codex design review. Step 2 of `layout-backend-direction.md`.
Depends on the containment pass (`layout-backend-contain.md`): the solver plugs in
BEHIND the backend-neutral `EcsUiFrameRun` surface; hosts never change.

## Contract (user-fixed 2026-07-07)

The solver is layout-ONLY, Yoga-like: emitted node tree + measure callback in →
queryable layout state out. Output = enriched snapshot rects (the existing enrichment
contract IS the output contract), later wrapped-text line boxes and clip/scroll metadata
as data. Paint (draw-list emission) is a SEPARATE stage: a walk over tree + computed
rects in z/paint order producing the command stream — required for adoptability, NOT for
scoring. The parity scoreboard never inspects draw commands.

## Solver feature scope — exactly the clay subset ecs-ui emits (grep of ecs_ui_clay.c)

- Sizing: GROW, FIXED, FIT (fit-content is the two-pass part; note wrap-width × grow
  interaction called out in the direction doc), per-axis.
- Layout direction: left-to-right, top-to-bottom; childGap; padding.
- Child alignment: x left/center/right, y top/center/bottom.
- Text: measure-callback-driven wrap (default wrap + WRAP_NONE), align left/center/right.
- Floating elements: attach to parent/root, all 9 attach points, z-index — feeds the
  existing ecs-ui-side placement/flip/clamp which already consumes layout output.
- Scroll containers: clip + externally-written scroll offsets (post-Stage-B, targeting
  is ecs-ui-side; the solver only stores/applies offsets and reports content dims).
- NOT in scope: anything clay supports that ecs-ui never emits (percent sizing, aspect
  ratio, border-affecting-layout, wrap direction variants). Scope is the emitted subset,
  not clay parity in general.

## Shape

- `src/ecs_ui_solver.c` (+ internal header), a sibling backend consuming the SAME
  emitted node tree the clay bridge consumes, producing enriched-snapshot rects.
- Backend selection: compile-time default (clay) + runtime override hook reachable from
  tests (`EcsUiFrameBackendSelect(ECS_UI_BACKEND_CLAY|NATIVE)` or build flag
  `ECS_UI_NATIVE_LAYOUT` — review question), so the parity harness can run BOTH in one
  process and diff.
- Scoreboard harness: `tests/ecs_ui_solver_parity.c` — runs identical trees through both
  backends with the mock text measure, diffs every node's enriched rect (epsilon 0 for
  integer cases, documented epsilon for accumulated float paths), at scale 1 and 2.
  Reuses/extends the golden trees from `ecs_ui_clay_parity.c`; adds feature-targeted
  trees per stage below. A failing diff prints the first divergent node path + both rects.
- Adoption flag stays OFF; nothing in hosts changes in this pass.

## Stages (each: harness trees first, then solver code to green, commit)

0. Dual-backend plumbing + scoreboard harness running with the native backend returning
   NOT-IMPLEMENTED (harness proves it can detect divergence via a deliberate stub error).
1. Boxes: fixed sizing, padding, gap, both directions, nesting — rect parity.
2. Grow distribution (multiple growers, min sizes, mixed fixed+grow).
3. Fit-content (two-pass), including fit-in-grow and deep fit chains.
4. Alignment (x/y all combinations, with grow/fit interplay).
5. Text: mock-measure wrap, line boxes as data, align variants; wrap-width × grow.
6. Floating (attach points, parent/root, z) + scroll (clip rects, offset application,
   content dims).
7. Stretch (only if 0-6 green and time remains): minimal paint pass emitting the
   draw-list for the native path behind the opaque handle — visual smoke via the demo,
   not scored.

## Acceptance

Stages 0-6: scoreboard green (zero rect divergence within epsilon) on all golden +
feature trees at scale 1 and 2; full existing ctest stays green with the default (clay)
backend; no host-visible API change. Explicitly logged non-coverage: real-font text
metrics (mock measure only — same as existing parity tests).

## Open questions for design review

- Build flag vs runtime-selectable backend for the harness (one process diffing both
  needs runtime switch OR two binaries + serialized rects; which is less machinery?).
- Epsilon policy: clay uses floats with specific rounding; is exact-match realistic for
  stages 1-4 (integer inputs), text stages excepted?
- Solver working memory: per-frame arena mirroring clay's model, or malloc/free per run
  (prototype simplicity) with the arena as adoption-time work?
- Line boxes as data: minimal representation now (per-line rect + byte range?) so stage
  5 doesn't over-design the future text contract.
- Is reusing ecs_ui_clay_parity golden trees as-is safe, or do they encode clay-specific
  quirks we should quarantine behind separate goldens?
