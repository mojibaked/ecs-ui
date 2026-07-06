# ecs-ui DX Hardening Plan

Author: orchestrator (Claude), from defects hit while integrating ecs-ui into texelotl
(first text input, first floating element, first custom-canvas overlay in that app).
Status: REVIEWED AND AMENDED — see `## Review` (adversarial pass, verdicts per item)
and `## Final implementation contract` (supersedes item text where they conflict).
Ready for implementation in the contract's stage order.

Every item below traces to a real texelotl incident. The goal is that the *next*
integrator (human or agent) cannot hit the same class of bug. Priority order.

## 1. Deterministic end-of-frame focus resolution

**Problem.** `EcsUiTextInputRequestFocusField` / `RequestBlur` mint independent ECS
request entities; `EcsUiTextInputApplyEvent` mints a blur on any click outside a
field. When a blur and a focus request land in the same frame (the texelotl case:
clicking an "Accept" button that also re-focuses the prompt field), the outcome
depends on request-processing order and is effectively undefined. texelotl now
carries a client-side "pending focus intent" retry loop to compensate.

**Proposal.** Collect all focus/blur requests during a frame; resolve once, at a
defined point in the text-input progress system, with documented precedence:
explicit focus-field > focus-next/previous > blur. Multiple focus requests in one
frame: last writer wins. Blur only wins when no focus request exists that frame.

**Acceptance.** A test that mints blur+focus in the same frame in both orders and
asserts the field ends focused. texelotl deletes its pending-focus-intent code and
the re-roll flow still passes its harness screenshots.

**Risk/effort.** Small. Localized to `src/ecs_ui_text_input.c` request consumption.

## 2. Computed logical layout rects in the tree snapshot

**Problem.** `EcsUiTreeNodeSnapshot` records authored properties only; where a node
actually landed exists solely inside Clay render commands, in physical pixels, at
render time. Apps that need real bounds (texelotl: canvas surface bounds for
pointer mapping and overlay placement) must round-trip them out of their renderer
callback — which is how texelotl shipped a physical-pixels leak and a double-scale
bug that took an agent-driven screenshot harness to diagnose.

**Proposal.** After layout, write each node's computed rect — LOGICAL units,
tree-root relative — into the snapshot: `float layout_x, layout_y, layout_width,
layout_height; bool has_layout;`. Provide a lookup by stable id (reuse the existing
id field) so apps can ask "where is node X" without touching the renderer.

**Acceptance.** Parity test: snapshot rect × tree scale equals the Clay render
command bounds for a nontrivial tree (nested stacks, placement, scroll). texelotl
replaces its RecordSurfaceBounds plumbing with a snapshot lookup and its
pointer-mapping/prompt-bar tests still pass.

**Risk/effort.** Medium. Requires capturing Clay layout output per node post-layout;
watch cost on ECS_UI_TREE_NODE_MAX-sized trees (fill during the existing snapshot
walk, no extra pass).

## 3. One coordinate-space contract: logical everywhere except render bridges

**Problem.** The scale story is implicit: `EcsUiSetScale` exists, the Clay adapter
multiplies via a file-global, `EcsUiPlacement` is consumed in logical space but
nothing says so, and custom-draw callbacks receive physical bounds with no scale
parameter. texelotl guessed wrong (double-scale, off-screen widget) and the guess
survived 30 green unit tests.

**Proposal.** (a) Write the contract down in the headers: every public API value —
placement, event coordinates, snapshot rects, preferred sizes — is logical; scale
exists only inside render bridges. (b) Pass scale explicitly to custom-draw/nine-
slice/icon callbacks (add a `float scale` parameter or a bridge-context struct) so
renderer-side code never reaches for globals or re-derives it. (c) Debug-build
assertion helpers where cheap (e.g., reject NaN/negative placement).

**Acceptance.** Header docs on every affected type; callback signature carries
scale; the raylib bridge and demo compile against it; texelotl's bridge updated.
Grep finds no consumer of the adapter's scale global outside the bridge files.

**Risk/effort.** Small-medium; the callback signature change is a breaking API
touch for both known consumers (texelotl, blocksmith-style hosts, demo).

## 4. Tree introspection dump (devtools for agents and humans)

**Problem.** When a widget silently fails to appear (emit skipped, placement
declined, builder error, node clipped), ecs-ui offers no way to see what the tree
actually contains — the debugging loop becomes screenshot archaeology. Three of
texelotl's four UI incidents would have been one-look diagnoses with a tree dump.

**Proposal.** `EcsUiTreeDebugDump(world, root, buffer, size)` (and/or a JSON
variant): one line per node — id, kind, depth, visibility, authored size,
computed layout rect (from item 2), placement, focus/hover state, truncation
flags. Pairs directly with attach-harness `state` commands so agents can assert
on structure, not just pixels.

**Acceptance.** Dump of the raylib demo tree matches a golden snapshot in tests;
texelotl exposes it through its attach channel as a `tree` command.

**Risk/effort.** Small once item 2 lands (it supplies the interesting data).

## 5. Canonical frame-event pipeline helper

**Problem.** Correct per-frame event handling now requires app-side lore: apply
text-input events before action dispatch, skip consumed events, preserve
TEXT_SUBMIT/TEXT_CANCEL for app policy, pop clipboard writes after. texelotl
reimplemented this (`TexelotlUiRuntimePreprocessTextEvents`) by reading demo
source; the next consumer will too, and order mistakes are silent.

**Proposal.** `EcsUiApplyFrameEvents(world, in_events, out_remaining)` implementing
the canonical order (text input → overlay layers → remaining for app dispatch),
returning unconsumed events, with TEXT_SUBMIT/TEXT_CANCEL passthrough documented.
Keep the low-level pieces public; this is a convenience with the ordering baked in.

**Acceptance.** Demo and texelotl both migrate onto it with no behavior change
(texelotl harness flows stay green); ordering documented in the header.

**Risk/effort.** Small. Mostly moving existing logic behind one entry point.

## Explicit non-goals (this pass)

- Overlay/popover system changes (texelotl ended up not using it).
- New widgets, theming, animation work.
- Headless renderer. (Snapshot rects + tree dump deliver most of the testing value.)

## Sequencing and migration

2 → 4 share data (do 2 first); 1, 3, 5 are independent. Each item ends with the
texelotl consumer migration that deletes the corresponding workaround, verified via
texelotl's attach harness + full ctest. ecs-ui house style applies (this repo uses
enums/switch freely — texelotl guardrails do NOT apply here).

## Review

### 1. Deterministic end-of-frame focus resolution

**Verdict: APPROVE-WITH-CHANGES.** The problem is real, but it is an implicit-order
gap more than a random race. Focus/blur/traversal requests are separate one-shot
entities (`src/ecs_ui_text_input.c:354`, `src/ecs_ui_text_input.c:755`), and
outside-click blur intentionally enqueues blur while returning the event to the
app (`src/ecs_ui_text_input.c:803`, `src/ecs_ui_text_input.c:808`). The systems
that consume those requests are independent `EcsOnUpdate` systems with no
explicit ordering contract (`src/ecs_ui_text_input.c:1351`,
`src/ecs_ui_text_input.c:1418`, `src/ecs_ui_text_input.c:1445`,
`src/ecs_ui_text_input.c:1702`). Current registration order appears to make blur
win after focus, but that is not documented or tested.

**Amendment.** Keep the public request API, but add an explicit per-frame resolver
or sequence-stamped request accumulator. Define precedence in the plan: explicit
field focus wins over same-frame blur; traversal focus has defined precedence
relative to blur; repeated focus requests are last-writer-wins. Without a
sequence/accumulator, "last writer wins" is not implementable from today's request
entities.

**Acceptance.** Add unit tests for both mint orders of blur+focus, two same-frame
focuses, traversal+blur, and the outside-click-plus-app-focus repro. These fit
`tests/ecs_ui_basic.c`, which already covers basic click focus, outside blur,
submit, and cancel (`tests/ecs_ui_basic.c:2566`).

**Effort/API.** M. No breaking API if existing request functions remain. texelotl
can simplify the retry aspect of `focus_when_enabled`
(`../texelotl/src/features/prompt_bar/prompt_bar_widget.c:151`) after migration;
the demo should not break.

### 2. Computed logical layout rects in tree snapshot

**Verdict: APPROVE-WITH-CHANGES.** The problem claim is correct. The snapshot has
authored tree data and scale, but no resolved layout rect
(`include/ecs_ui/ecs_ui.h:431`, `include/ecs_ui/ecs_ui.h:468`). `EcsUiReadTree`
copies ECS state before layout (`src/ecs_ui.c:1876`, `src/ecs_ui.c:2049`), while
texelotl currently discovers canvas bounds through a render callback
(`../texelotl/platform/desktop/main.c:2132`) and stores logical bounds for prompt
placement (`../texelotl/src/features/canvas/canvas_widget.c:304`,
`../texelotl/src/features/canvas/canvas_prompt_placement.c:43`). The incident is
also documented in the port plan (`../texelotl/docs/plans/genai-port/02-prompt-toolbar.md:21`).

**Amendment.** Do not describe this as something that can be filled during the
existing snapshot walk. Clay layout rects exist only after `EcsUiClayEmitTreeEx`,
which currently takes a `const EcsUiTreeSnapshot *` and scales authored values
into Clay (`src/ecs_ui_clay.c:2212`, `src/ecs_ui_clay.c:2244`). Use a post-layout
Clay enrichment API, or make the emit path explicitly mutable. Specify logical
tree-root-relative rects by dividing Clay physical bounds by `tree->scale` and
normalizing any layout-options origin. Scope the first version if needed: all-node
rects are L; custom-node rects only are M. Also avoid keying solely by `node->id`;
element IDs are made unique by appending entity identity in the Clay adapter
(`src/ecs_ui_clay.c:34`).

**Acceptance.** In `tests/ecs_ui_clay_parity.c`, assert rects for a custom canvas,
a placed/floating child, a scrolled or clipped child, and at least one ordinary
container. Existing scale parity already proves authored snapshot values remain
logical while Clay commands scale (`tests/ecs_ui_clay_parity.c:725`); extend that
to assert `logical_rect * scale + root_origin == Clay boundingBox`. texelotl
migration should delete the custom-renderer bounds feedback loop.

**Effort/API.** L for all nodes; M for a custom-node MVP. Adding fields is source
compatible but ABI-visible; a separate enrichment API is safer for consumers.
texelotl benefits materially; the in-tree demo need not migrate immediately.

### 3. One coordinate-space contract

**Verdict: APPROVE-WITH-CHANGES.** The diagnosis is right, but the proposed scope
understates event coordinates. Root scale is part of the snapshot
(`include/ecs_ui/ecs_ui.h:468`), Clay scaling is held in adapter globals
(`src/ecs_ui_clay.c:6`), and placement is scaled before Clay sees it
(`src/ecs_ui_clay.c:1730`). Raylib/custom/icon/nine-slice callbacks receive only
physical bounds, not scale (`include/ecs_ui/ecs_ui_raylib.h:12`,
`src/ecs_ui_raylib.c:444`, `src/ecs_ui_raylib.c:1345`,
`src/ecs_ui_raylib.c:1374`). More importantly, current Clay and raylib event
bridges emit physical coordinates directly (`src/ecs_ui_clay.c:2408`,
`src/ecs_ui_clay.c:2479`, `src/ecs_ui_raylib.c:1694`,
`src/ecs_ui_raylib.c:1761`), which is why texelotl has a manual
`TexelotlDesktopEventListToLogical` pass (`../texelotl/platform/desktop/main.c:252`,
`../texelotl/platform/desktop/main.c:2940`).

**Amendment.** Define the coordinate contract first, including origin: window/global
logical or tree-root-relative logical, especially when `EcsUiClayLayoutOptions`
supplies bounds. Prefer a small render/event context struct over a trailing
`float scale` on every callback; the same context can carry scale, physical root
bounds, and logical origin. If event normalization is out of scope, explicitly say
the pointer state passed into ecs-ui must already be logical; otherwise implement
conversion in the bridges and let texelotl delete its conversion helper.

**Acceptance.** Add scale=2 tests for pointer x/y, start, delta, and velocity from
the frame-event collection path, or document and test that callers must provide
logical pointer state. Header docs must cover every affected callback/event type,
and both the in-tree bridge and demo must compile against the new callback shape.

**Effort/API.** M for callback context plus docs only; L if event semantics change.
This is breaking for `EcsUiRaylibCustomDrawFn` consumers, including texelotl's
desktop bridge (`../texelotl/platform/desktop/clay_raylib_bridge.c:264`) and the
in-tree demo bridge (`examples/raylib_demo/clay_raylib_bridge.c:227`).

### 4. Tree introspection dump

**Verdict: APPROVE-WITH-CHANGES.** The gap is real, but the plan should be precise
about what data already exists. `EcsUiReadTree` already provides deterministic
structure and authored/runtime widget state; it does not provide layout rects or
full visibility semantics (`include/ecs_ui/ecs_ui.h:431`,
`src/ecs_ui.c:1876`). Focus is only visible in the snapshot where projected
text-field view data exists, not as a general global focus dump.

**Amendment.** Make this snapshot-first:
`EcsUiTreeDebugDumpSnapshot(const EcsUiTreeSnapshot *, ...)`, with an optional
`world, root` convenience wrapper. JSON is useful for attach harnesses but should
not be required for the first text dump. Include `tree.truncated`, index, parent,
child range, entity, id, kind, hit-test, opacity/offset, placement presence,
text-field focused state when present, and layout rect only once item 2 lands.

**Acceptance.** Do not use the full raylib demo tree as the primary golden; entity
IDs and unrelated demo edits make that brittle. Use a synthetic test tree with
normalized entity IDs, plus coverage for truncation and duplicate IDs. A texelotl
attach `tree` command is a good downstream migration, but not a blocker for this
repo.

**Effort/API.** S for text dump without JSON/layout; M with JSON and layout rects.
No breaking API; demo and texelotl impact is additive.

### 5. Canonical frame-event pipeline helper

**Verdict: APPROVE-WITH-CHANGES.** The problem is verified. The raylib demo and
texelotl both hand-code the same ordering: text input preprocessing first, skip
consumed events, preserve app-owned submit/cancel, then dispatch remaining events
(`examples/raylib_demo/demo_ui_events.c:58`,
`../texelotl/src/app/ui_runtime.c:75`, `../texelotl/src/app/ui_runtime.c:111`).
The low-level helper consumes `TEXT_CANCEL` by requesting blur
(`src/ecs_ui_text_input.c:770`), which is exactly the kind of lore this wrapper
should hide or make explicit.

**Amendment.** Scope the first helper to text input and event filtering. Do not
include "overlay layers" in the initial contract: overlay state is driven by a
separate input/state API (`include/ecs_ui/ecs_ui_overlay.h:31`,
`src/ecs_ui_overlay.c:222`), so folding it into an `EcsUiEventList` helper is not
"mostly moving existing logic." Provide flags or defaults that preserve
`TEXT_SUBMIT` and `TEXT_CANCEL` for app policy, and return truncation/result
metadata.

**Acceptance.** Unit-test that text input events are consumed, field clicks are
consumed, outside-click blur is enqueued while the pointer event remains for the
app, submit/cancel pass through by default, and output truncation is reported.
Migrate the in-tree demo as the repo-local proof; texelotl migration can follow.

**Effort/API.** M for a useful helper with preserve flags and truncation reporting;
S only for a thin wrapper. No breaking API if low-level functions remain public.

### Global verdict

These are the right top-five themes for the texelotl incidents: focus ordering,
post-layout bounds, coordinate-space clarity, tree visibility, and event ordering.
The main correction is coupling: item 3 must define coordinate/origin semantics
before item 2 can specify rects, and item 5 should not absorb overlay behavior in
this pass. The non-goals are mostly correct; keep widgets/theming/animation and a
headless renderer out. Also keep overlay/popover redesign out by removing overlay
from item 5's first implementation.

**Recommended stage order.**

1. Item 3 contract/docs first, including whether events become logical inside
   bridges or remain caller-normalized.
2. Item 1 focus resolver.
3. Item 5 text-input frame-event helper, without overlay.
4. Item 2 layout rect enrichment, preferably custom-node MVP first if schedule
   matters.
5. Item 4 debug dump, adding layout fields after item 2.

**Ready for implementation: no.** The plan is directionally right, but should be
amended with the scope and acceptance changes above before implementation starts.

## Final implementation contract

All review amendments accepted. Two decisions the review left open are resolved
here by the plan author:

**Decision A (item 3): bridges emit LOGICAL event coordinates.** The Clay/raylib
event bridges convert pointer state to logical (window-origin, tree-root scale)
before events enter `EcsUiEventList`. Caller-normalized was rejected because it
preserves exactly the class of leak that caused the texelotl double-scale bug.
texelotl deletes `TexelotlDesktopEventListToLogical` on migration. This makes
item 3 effort L, accepted.

**Decision B (item 2): custom-node MVP first.** Post-layout rect enrichment ships
for custom nodes first (M), covering the texelotl canvas-bounds case; all-node
enrichment is a follow-on in the same API shape. Enrichment is a separate
post-`EcsUiClayEmitTreeEx` API (`EcsUiClayEnrichSnapshotLayout(...)` or similar),
not a snapshot-walk fill; rect keying accounts for the adapter's entity-suffixed
element IDs.

**Stage order (per review):**

1. Item 3 — coordinate contract: origin semantics documented in headers, render
   callback context struct (scale + physical root bounds + logical origin;
   replaces bare bounds params), bridges emit logical events (Decision A).
   Breaking for custom-draw consumers; migrate in-tree demo bridge in the same
   change. scale=2 pointer tests (x/y, start, delta, velocity).
2. Item 1 — focus resolver: sequence-stamped request accumulator resolved once
   per frame; precedence explicit-focus > traversal > blur; last-writer-wins
   within a class. Test matrix: both mint orders of blur+focus, double focus,
   traversal+blur, outside-click-plus-app-focus.
3. Item 5 — `EcsUiApplyFrameEvents`: text input + filtering only, NO overlay
   routing; preserve-flags for TEXT_SUBMIT/TEXT_CANCEL (default preserved);
   returns consumed-count/truncation metadata. Migrate in-tree demo as proof.
4. Item 2 — layout rect enrichment per Decision B, logical tree-root-relative,
   parity tests in tests/ecs_ui_clay_parity.c (custom node, placed child,
   clipped child, plain container; logical_rect * scale + origin == Clay bounds).
5. Item 4 — `EcsUiTreeDebugDumpSnapshot` (snapshot-first, text format, synthetic
   golden tree with normalized entity IDs, truncation + duplicate-id coverage);
   JSON variant and layout fields after item 2.

**Consumer migrations** (tracked per stage, verified via texelotl attach harness +
full ctest): item 3 → texelotl bridge signature + delete event conversion helper;
item 1 → delete pending-focus-intent in prompt_bar_widget; item 5 → replace
TexelotlUiRuntimePreprocessTextEvents; item 2 → replace RecordSurfaceBounds
feedback loop; item 4 → attach `tree` command.
