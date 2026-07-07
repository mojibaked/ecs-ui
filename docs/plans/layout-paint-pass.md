# Native paint pass (stage 7) — v3, ECS-aligned

Status: ACCEPTED 2026-07-07 (v3). Implementation not started — pick up at
stage 7.0. Last reconciled 2026-07-07.
History: v1 draft -> v2 (clay-parity-shaped; fresh-codex + Opus reviews, two
strata minings folded) -> v3 (this): rewritten after the ECS architecture
consultation established that clay compatibility is SCAFFOLDING, not a goal,
and that the durable artifacts must align with the project's ECS design.
Superseded ideas from v2 are recorded in "Demoted from v2" at the end.

## Principles (the belief boundary, made explicit)

- ECS OWNS semantic truth and scheduling: durable node identity,
  parent/order relationships, authored components, style tokens, text
  edit/focus state, hover/capture/press state, SCROLL STATE, dirty facts,
  and ownership handles for frame artifacts.
- Hot per-frame derived data is packed POD owned by the frame, NOT
  entities: solver metrics, layout scratch, text lines, text-field
  segments, bevel strips, clip scopes, floating wrappers, paint items,
  backend commands. ECS orchestrates these; they are never app truth.
  (Settled twice before by this author: strata-ecs-render-thread's
  ecs-architecture + dense-presentation-tree docs — "don't pay for both
  models" — and strata-retained's snapshot split from the other direction.)
- Clay is scaffolding. The bridge remains the frozen reference for
  BOOTSTRAP verification, but no clay quirk (command order, culling
  asymmetries, scissor semantics, command structs) becomes durable design.

## Architecture

    world ──snapshot──> [lower + solve, packed]
                             │
                             ▼
                      EcsUiPaintList            (durable, renderer-neutral,
                      frame-owned POD)           the project's paint artifact)
                             │
              ┌──────────────┴──────────────┐
              ▼                             ▼
    clay-command ADAPTER            (future renderer adapters)
    (transition-only: feeds the
     existing raylib renderer)

- Paint runs as a host-scheduled SUBPHASE inside `EcsUiFrameRun`, after
  solve (frame execution is host-driven today; "ECS-scheduled" would be
  aspirational). The frame backend owns the paint list storage; the world
  holds the artifact handle as a singleton component
  (`EcsUiFrameArtifacts { const EcsUiPaintList *paint; uint32_t generation; }`)
  written by the frame apply step — ECS owns the handle and generation,
  never the storage.
- Paint is a pure function: placed layout + snapshot styling + theme in,
  paint list out. It never walks layout internals to recover order,
  parentage, clipping, or synthetics (strata lesson), and never feeds back.
- Paint code is split by primitive/helper from day one — no godfile.

## EcsUiPaintList: item schema

Every item carries an ECS-native identity WITHOUT being an entity:

    typedef struct EcsUiPaintKey {
        ecs_entity_t source;    // owning UI entity; 0 ONLY for tree-scoped
                                // art (viewport/root background), which is
                                // disambiguated by role + the list's tree id
        uint16_t     role;      // box | border | text-run | bevel-edge |
                                // caret | selection | nine-slice | custom |
                                // icon | clip-scope
        uint16_t     part;      // structured ordinal within a role:
                                // (sub << 8) | index — e.g. text runs use
                                // sub = segment (text-field range ordinal),
                                // index = line; bevel uses index = edge;
                                // clip scopes use index = scope ordinal.
                                // Widen to uint32 if any golden overflows.
        uint32_t     generation;// frame generation: a counter the frame
                                // backend increments per EcsUiFrameRun and
                                // stamps into the snapshot (new snapshot
                                // field) and the paint list
    } EcsUiPaintKey;

The paint list itself carries the tree/root id and generation once; item
keys are unique within (tree, generation).

Item fields: key; primitive kind + payload (fill color, corner radius,
border widths/color, text run ref {byte range into node->text.text, line
box, physical fontSize, resolved color, alignment offset}, image/nine-slice
ref, custom ref = source entity); rect in LOGICAL root-relative space;
RESOLVED clip (clip-scope id + resolved clip rect — strata lesson: resolved
clips, not just start/end markers); cumulative opacity; draw order = the
list's order (z-sorted roots, then depth-first — the NATIVE semantic order,
not clay's emission accidents).

Coordinate/lifetime contract: adapters convert logical -> physical exactly
once (scale + physical bounds origin); all string/data references alias the
SNAPSHOT and remain valid until the next paint-list reset AND only while
the source snapshot storage remains alive and unmodified (the same rule the
frame API already documents for draw lists); the list storage follows the
solver's arena discipline (sized up front, no realloc after capture).

No dirty stamps, no retained storage, no damage tracking in this plan: the
key's (source, role, part) triple is sufficient identity for tests,
debugging, and any FUTURE retained/caching design (which must bring its own
plan; strata-retained shows the stamp machinery is a project of its own).

## Pre-step: scroll state moves into ECS (7.0)

Scroll offsets are semantic runtime state currently OWNED by clay's
retained internals (`Clay_ScrollContainerData` mutation in the wheel path)
— app truth living inside scaffolding, and the worst duplication found in
the consultation. New component:

    EcsUiScrollState { float offset_x, offset_y; float content_w, content_h; }

Precise migration contract (the current flow mutates clay retained state
DURING wheel routing, and `EcsUiFrameCollectEvents` has no world access —
so ownership moves via the interaction frame, not by threading the world
into routing):
1. Register `EcsUiScrollState` in the module; snapshot build
   (`EcsUiReadTree`) copies it into new snapshot fields (logical units).
2. The bridge feeds `childOffset` from the SNAPSHOT state (scaled to
   physical) instead of `Clay_GetScrollOffset()` — the declaration helper
   already takes a `child_offset` argument, so lowering is untouched. The
   native solver's existing offset plumbing reads the same snapshot state.
3. Wheel routing computes offset changes into PENDING updates stored in
   the interaction frame (it already clamps against content/container dims
   there); it stops mutating clay's retained `scrollPosition` as truth.
4. `EcsUiFrameApply` (which has the world) commits pending scroll updates
   into `EcsUiScrollState` — before the next `EcsUiReadTree` by contract.
5. `EcsUiFrameSettleScroll` clamps and writes back the COMPONENT (content
   dims come from the active backend's report); clay's
   `Clay_UpdateScrollContainers` remains only as inert internal upkeep
   until clay retires, synchronized FROM the component each emit so it can
   never diverge as a second truth.
One-frame-lag note: today's flow already consumes wheel input at the NEXT
emit (retained state read at declaration time); the component path
preserves exactly that timing, so behavior goldens must not shift.

This lands FIRST as risk burn-down — it changes ownership of live
behavior, so every existing scroll golden, ECS_UI_EVENT_SCROLLED behavior,
and the texelotl live scroll acceptance must stay green. (It is not a hard
prerequisite for 7.1-7.5 and may proceed in parallel if sequencing demands,
but 7.6's acceptance requires it done.)

## Style resolution: extract shared leaf resolvers (unchanged from v2)

The pure snapshot+theme -> value resolvers (text/button/pressable colors,
hover/highlight lerp x0.42, disabled fallbacks, opacity multiply, the
corner-radius `<1 -> x50` quirk, nine-slice tint, bevel light/dark,
selection alpha x0.35, icon opacity encoding) are extracted into a shared
unit (`src/ecs_ui_style.c`) used by the bridge AND paint. Unanimous across
all reviews; the freeze rationale never applied to leaf resolvers, and the
divergence trap is already live (the legacy snapshot renderer disagrees
with the clay path on the x50 radius quirk today — paint matches the
BRIDGE; the legacy renderer is not a parity target). Mechanical move,
behavior-guarded by full ctest before/after.

## Text: new unscored layout, named as such (unchanged from v2)

Paint requires layout the rect stages deliberately never computed:
inner-text-element placement inside wrappers (childAlignment.y), per-line
boxes for newline strings, per-line textAlignment offsets against the
INNER element's box, and real placement for text-field virtual entries.
Pinned in prose first (solver-lowering.md "Paint" section), produced as
text-run paint items with run identity so paint never re-measures, and
guarded by dedicated structural goldens — this is the largest genuinely
new surface in the plan; treat it accordingly.

## Verification

1. PAINT-LIST STRUCTURAL TESTS (primary, durable):
   `tests/ecs_ui_paint_list.c` — golden trees assert item keys, order,
   rects, resolved values, clip scopes at scale 1+2; divergence proofs
   (wrong color AND wrong order) before trusting green; diagnostics output
   (counts by role, clip balance, first mismatch).
2. BOOTSTRAP CLAY DIFF (temporary guardrail, thin subset):
   compare VALUES, not command streams — box geometry/colors/radii, text
   line placement, resolved clip rects — between the paint list and clay's
   commands on simple trees, normalizing clay-only artifacts (command
   order, its culling asymmetries, scissor pair placement). JOIN RULE
   (concrete, no order dependence): bridge commands are matched by their
   deterministic Clay element id — the harness computes the same id the
   bridge does (`EcsUiClayElementId` from node id + suffix) for each paint
   key's (source, role, part), plus command type; synthetic auto-id text
   lines join through their parent element id + line ordinal. Items with
   no computable bridge id (clay-internal artifacts) are excluded from the
   subset by definition. Every divergence is classified SEMANTIC (fix) or
   CLAY-ONLY (document, keep native behavior). This harness is
   scaffolding: it retires after 7.6's A/B gate is green (kept
   quarantined, not in the default suite).
3. LIVE A/B smoke (7.6): texelotl screens via the clay-command adapter at
   scale 1+2 under Xvfb; any pixel delta is a finding to attribute.
   Culling/perf note: the paint list does not cull (clay's culling is a
   demoted quirk), and the raylib renderer draws every command it
   receives — the ADAPTER owns an optional simple viewport-rect cull as
   the perf story (off during diffing, on for live use). Until 7.6 lands
   scissor derivation, adapter output is not visually correct for
   scroll/clip trees — 7.3-7.5 verification excludes them by construction.
4. The rect scoreboard runs unchanged and stays green throughout (paint
   must not perturb solved rects).

## Stages (commit per verified stage; tests first, then code)

- 7.0 Scroll state into ECS (pre-step above; all scroll goldens + live
  scroll acceptance green; no paint code).
- 7.1 Style unit extraction (mechanical, behavior-guarded).
- 7.2 EcsUiPaintList skeleton: paint-key schema, frame-owned storage,
  ECS-scheduled paint phase wiring, structural-test harness with both
  divergence proofs. Rect scoreboard proves layout untouched.
- 7.3 Boxes: fills/radius/borders as paint items + the clay-command
  adapter for boxes + bootstrap value-diff on box trees.
- 7.4 Decorations: bevel edges, nine-slice, custom/icon items.
- 7.5 Text: the new unscored layout -> text-run items (+ text-field
  caret/selection/segment items).
- 7.6 Order + clip: z-sorted emission, resolved clip scopes (adapter
  derives scissor pairs), full paint suite green, live A/B smoke gate.
  THEN STOP — the backend flip stays behind the direction doc's
  pain-signal rule; retiring the bootstrap diff and clay itself are
  post-adoption decisions.

## Non-goals

No entity-per-paint-item, no retained storage/damage tracking, no renderer
rewrite (adapter only), no backend flip, no bridge lowering changes (leaf
resolvers excepted), no clay-quirk conformance in the durable artifact.
Note what this plan achieves as a byproduct: EcsUiPaintList IS the neutral
draw format — after adoption, clay commands survive only inside the
transition adapter, which can then be replaced renderer-side at leisure.

## Demoted from v2 (recorded so nobody resurrects them silently)

- Clay command stream as primary verification target -> values-level
  bootstrap diff on a thin subset, retired after 7.6.
- Exact clay emission order / culling asymmetries / scissor semantics as
  durable behavior -> native semantic order; adapters own scissor
  derivation; clay-only diffs documented, not conformed to.
- Clay_RenderCommand as the paint IR -> transition adapter output only.
- Dirty-stamp seeds in the item schema -> dropped until a retained-design
  plan exists; (source, role, part, generation) suffices.

## Acceptance

Per stage: paint-list structural tests green for that stage's scope at
scale 1+2; bootstrap diff green-or-classified on its subset; rect
scoreboard and both build trees green. Plan-level: 7.6 A/B shows zero
unattributed pixel deltas, with scroll state ECS-owned end to end.
