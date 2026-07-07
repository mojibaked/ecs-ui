# Layout backend direction: clay containment, possible native engine

Direction note (2026-07-07), not an accepted plan. Context for a future session prototyping a
clay replacement; originated in the texelotl menus-cleanup stream after the input-routing pass.

## Where clay stands after the input-routing pass

Stage B of `input-routing.md` demotes clay to two jobs: solving layout (flexbox-lite: fit/grow/
fixed, padding, gap, alignment, text wrap via the injected measure callback) and emitting the
ordered render-command list. Everything else is ecs-ui-side: hit-testing and capture semantics
(shared walk), scroll targeting (offsets written directly via Clay_GetScrollContainerData;
Clay_UpdateScrollContainers no longer decides targets), placement/flip/clamp, z-order/overlay
conventions, event synthesis.

## Why a native retained engine is on the table

ecs-ui keeps a RETAINED scene (ECS node entities) but clay is IMMEDIATE-MODE, so every frame we
re-emit the whole tree into clay and harvest results back out through the snapshot enrichment
pass. The seams tax us structurally:

- layout is a pass, not queryable state → the attach tree refresh costs a second full layout;
- anchored placement is applied frame-late from the previous snapshot;
- fit-content sizing for floating panels is unavailable (hardcoded menu panel heights in the
  app, already drifted once);
- no incremental relayout (dirty-subtree) is possible;
- future gesture/arena work (press-buffering, cancellable delivery for drag-to-scroll vs click)
  wants hit and layout state as data, not pass output.

A native engine over the ECS nodes dissolves these structurally. Counterweight: layout solvers
are deceptively deep (fit-content is two-pass, wrap width interacts with grow); clay's subset is
battle-tested. Decide on evidence, not elegance.

## Sequencing

1. CONTAIN (small, do first): texelotl's platform layer still names clay directly
   (Clay_SetLayoutDimensions, EcsUiClayInteractionFrame/PointerState, EcsUiClayEmitTreeEx,
   Clay_RenderCommandArray, the clay-raylib renderer entry). Move these behind backend-neutral
   ecs-ui types so clay's entire surface lives in this repo's bridge. Until then, texelotl
   ADOPTION of any new backend is gated; prototyping is not.
2. PROTOTYPE (separate session, this repo only): a sibling solver module consuming the same
   emitted node tree, producing rects + a draw list. Dual-backend behind a build flag with clay
   as the reference implementation. Scoreboard is built in: run identical trees through both
   backends and diff ENRICHED SNAPSHOT RECTS (the enrichment pass already defines the layout
   output contract; goldens + scale-1/2 parity tests are the harness; tests use a mock text
   measure, so no font dependency for solver parity).
3. ADOPT only when goldens match and a pain signal justifies it (fit-content need, relayout
   perf, double-pass cost). With containment done, adoption is a flag flip.

## Design criterion carried into Stage B review

The shared hit-target walk must be written against layout OUTPUT (snapshot rects + capture
flags), not clay internals — then a native backend inherits hit-testing, capture, and scroll
targeting for free and owes only the solver and draw list.
