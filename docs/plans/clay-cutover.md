# Clay hard cut: native layout + native paint

Status: DRAFT 2026-07-08. This is a planning document only.

This plan moves texelotl and ecs-ui fully off Clay. The end state has no Clay
dependency in the ecs-ui build, no `clay.h` includes, no `Clay_*` symbols in
compiled ecs-ui sources/tests, and no `Clay_RenderCommand` or
`Clay_RenderCommandArray` in the renderer contract.

Clay remains a temporary reference during the plan. Once the verification gate
is green, the reference bridge and adapter are deleted rather than kept as
compatibility scaffolding.

## Objective

- Make the native solver the live layout backend.
- Make `EcsUiPaintList` the live renderer artifact.
- Render texelotl through the native paint renderer, not through Clay commands.
- Delete Clay from ecs-ui source, tests, demos, CMake, and shipped targets.
- Keep every implementation stage independently committable and leave the live
  renderer usable at every point before the final deletion.

## Non-goals

- No backwards compatibility for the old Clay-backed draw-list ABI.
- No long-term adapter from `EcsUiPaintList` to Clay commands.
- No broad product redesign of layout semantics during the dependency removal.
  Current native semantics are frozen as the v1 native contract; intentional
  behavior changes after Clay removal require a separate product-facing plan.
- No retained/damage-tracked renderer cache. The paint list remains a
  frame-owned artifact.

## Current State

- The native solver is layout-complete for the emitted ecs-ui subset, but it is
  selected only through `EcsUiFrameInternalBackend` and test harnesses. In the
  native branch, `backend->draw_list` is currently zeroed and no live rendered
  frame is produced.
- Paint stages 7.0-7.6 build an `EcsUiPaintList` every frame in both backend
  branches. The live renderer still consumes the Clay bridge draw list.
- `EcsUiPaintClayAdapterBuild` converts paint items to
  `Clay_RenderCommandArray`, but it is a bootstrap test/A-B artifact only.
- texelotl already consumes the neutral frame API and links `ecs_ui::frame` /
  `ecs_ui::raylib`; source/build grep shows no Clay symbols in texelotl outside
  historical docs. The expected texelotl change is limited to the render return
  type and renderer call described below.

## Architectural Decisions

### Renderer Boundary

End state: renderers consume `EcsUiPaintList` directly.

Do not keep the Clay-command adapter as a renderer input contract. It preserves
the wrong abstraction: command order, scissor placement, command ids, and Clay
payload structs are bootstrap scaffolding, not ecs-ui design. `EcsUiPaintList`
already carries the native identity, order, resolved clip, logical rects, style
values, and source entity needed by renderers.

No second neutral draw-list type is introduced. `EcsUiPaintList` is the neutral
draw artifact. If a future renderer wants a backend-specific command buffer, it
builds that buffer from `EcsUiPaintList` outside the core frame contract.

Public end-state shape:

- `EcsUiFrameRun(...)` returns `const EcsUiPaintList *`.
- `EcsUiDrawList` and `EcsUiFrameDrawListClayCommands` are deleted.
- `EcsUiRaylibRenderDrawList(...)` is replaced by a direct paint renderer, e.g.
  `EcsUiRaylibRenderPaintList(const EcsUiPaintList *paint,
  const EcsUiTreeSnapshot *tree, Font *fonts,
  const EcsUiRaylibRenderContext *root,
  const EcsUiRaylibDrawOptions *options)`.

The renderer takes the snapshot because paint custom/nine-slice/icon payloads
store source entities, not node pointers. That keeps paint neutral while letting
raylib callbacks continue receiving `const EcsUiTreeNodeSnapshot *`.

Transition shape:

- Do not mutate `EcsUiFrameRun`'s return type until the deletion stage. texelotl
  is a separate repo; changing the return type in one ecs-ui commit would break
  texelotl before its companion commit lands.
- Promote the current internal paint-list accessor to public
  `EcsUiFramePaintList()` during the renderer cutover. Hosts keep calling
  `EcsUiFrameRun(...)`, then render the current paint list through
  `EcsUiRaylibRenderPaintList(...)`.
- Add a private transitional `EcsUiFrameInternalClayCommands()` for A/B and
  bootstrap tests. Those tests must not depend on the public draw-list return
  once hosts stop rendering it.

### Layout Cutover

The native solver becomes the only live layout backend, but not in one step.
The plan first proves the paint renderer while Clay still solves layout, then
proves the native solver through live pixels and interaction, then deletes Clay.

The solver currently reproduces several Clay-derived semantics. During this cut,
those semantics become the v1 native contract when they are required for current
rect parity. We do not redesign layout while removing the dependency. The
exception is paint-only cleanup where the native artifact already exposes a
better model and the change can be guarded by pixels; the known border-fold
issue falls into that category.

Known paint divergence to resolve before the live renderer cut:

- Border fold: current paint stores border in the box payload and adapters draw
  it before children; Clay draws borders on unwind after children. For a clean
  end state, split border into a real `ECS_UI_PAINT_ROLE_BORDER` item emitted at
  the node unwind position before the paint renderer becomes live. Do not carry
  the folded-border shortcut into the final renderer.

### Verification Gate

The existing command diff is not enough for a no-backwards-compatibility removal.
The required gate is staged pixel verification:

1. In-repo offscreen pixel harness under Xvfb:
   - Render the Clay bridge draw list and the direct native paint renderer to
     raylib `RenderTexture` targets.
   - Run at scale 1 and 2.
   - Cover boxes, borders, bevel, nine-slice, custom, icon, text, text-field
     caret/selection, scroll, clip, z-overlap, visual offsets, culling on/off,
     and HiDPI bounds.
   - Attribute every pixel delta as SEMANTIC or CLAY-ONLY. SEMANTIC deltas are
     fixed before continuing. CLAY-ONLY deltas are documented and accepted only
     when the native behavior is the desired long-term behavior.
2. In-repo native-layout pixel harness:
   - Render Clay-layout + paint and native-layout + paint through the same direct
     paint renderer, isolating layout differences from rendering differences.
   - Run the same scale 1/2 cases and include pointer/scroll setup where layout
     depends on state.
3. texelotl Xvfb screenshot A/B on real screens:
   - Run current bridge rendering versus native solver + native paint renderer.
   - Include at least menus/dialogs, canvas surface, scroll containers, text
     fields, overlapping floating UI, scale 2, and idle/parked frames.
   - Zero unattributed deltas is the gate before deletion.

Manual smoke and the existing bootstrap command diff remain useful diagnostics,
but they are not sufficient gates for deleting Clay.

## Stages

### Stage 0: Native Cut Contract and Inventory

Goal: freeze the exact end-state contract and close known inventory gaps before
changing runtime behavior, and make the frame/renderer targets buildable without
Clay before any later stage depends on that property.

Concrete changes:

- Add or update docs to state that `EcsUiPaintList` is the renderer artifact and
  Clay commands are transition-only.
- Re-home CMake targets before feature work:
  - `ecs_ui_frame` is created unconditionally from native/neutral frame code
    (`src/ecs_ui_frame.c` after any necessary split) and `src/ecs_ui_solver.c`,
    gated only on the core flecs dependency.
  - `ecs_ui_frame` must not link `ecs_ui_clay`.
  - `src/ecs_ui_raylib_frame.c` / the paint renderer are compiled when
    `ECS_UI_BUILD_RAYLIB_RENDERER=ON`, independent of `TARGET ecs_ui_clay`.
  - The Clay bridge and Clay-command adapter move behind optional test/reference
    targets. They may still exist for A/B, but they no longer own the frame or
    raylib renderer targets texelotl links.
  - If a Clay-enabled live/reference target is temporarily needed while the
    renderer and native interaction stages land, it is a separate optional
    target, not `ecs_ui::frame`, and it is deleted with the other transition
    artifacts.
- Inventory emitted command kinds from current ecs-ui and texelotl fixtures:
  rectangle, border, text, custom, scissor, and IMAGE.
- Decide IMAGE by evidence:
  - If no current emitted tree uses raw Clay IMAGE commands, add tests asserting
    no raw image commands are produced by current ecs-ui/texelotl fixtures and do
    not add an image paint primitive.
  - If a current real screen emits IMAGE, add a neutral image paint item before
    renderer cutover. The item carries source entity plus renderer-owned image
    handle/tint data; it does not carry Clay image data.
- Pin native renderer culling policy: culling is a renderer optimization, not
  layout or paint truth. Pixel diff runs with culling off; culling-on tests must
  prove identical visible pixels.
- Pin text font identity before direct rendering. Either keep a documented
  single-font contract for v1 native paint, or add `font_id` to
  `EcsUiPaintTextRun` and thread it through the direct renderer. Prefer adding
  `font_id`: it preserves today's `0` behavior and prevents a future multi-font
  need from silently rendering with font 0.

Acceptance:

- No host source changes in this prep stage.
- The Clay-enabled transition build remains green while the no-Clay build proves
  the future target topology.
- Inventory doc lists every currently emitted visual primitive.
- `ecs_ui_frame`, `ecs_ui::frame`, and `ecs_ui::raylib` build with
  `-DECS_UI_BUILD_CLAY_ADAPTER=OFF` and no Clay checkout installed.
- Grep confirms texelotl source/build files name no Clay symbols. Exclude
  `scripts/check_guardrails.sh` or match only code/build statements because that
  script intentionally contains the string `clay` to enforce the guardrail.

Verification:

- Existing full ecs-ui and texelotl builds/tests stay green.
- The inventory commands and grep commands are recorded in the stage notes.

### Stage 1: Direct Raylib Paint Renderer, Not Live Yet

Goal: render `EcsUiPaintList` directly while the live path still uses the Clay
bridge draw list.

Concrete changes:

- Add `EcsUiRaylibRenderPaintList(...)`.
- First split border out of `EcsUiPaintBox` into explicit
  `ECS_UI_PAINT_ROLE_BORDER` items emitted at node unwind, after children and
  before a clip-scope END marker. The direct renderer is accepted against this
  final border item model, not the current folded box-border payload.
- Implement paint primitives directly in raylib:
  box fill, border items, bevel edges, nine-slice/custom/icon callbacks, text
  runs, caret/selection boxes, clip scope markers, z/list order, and optional
  viewport culling.
- Preserve existing callback ergonomics by resolving paint item source entities
  against the provided snapshot before calling custom/icon/nine-slice callbacks.
- Pin coordinate snapping to the existing Clay-command raylib renderer:
  convert logical root-relative paint rects to physical window coordinates using
  `scale + physical_bounds.xy`, then `roundf` x/y/width/height exactly like
  `EcsUiRaylibDrawListRect`. Text origins use the same physical snapped x/y as
  boxes. Do not round in logical units before scaling.
- Keep `EcsUiRaylibRenderDrawList(...)` and the Clay-command adapter only for
  comparison during this stage.

Acceptance:

- Direct renderer structural tests cover every paint primitive at scale 1 and 2.
- Border-overlapping-child cases prove borders draw at the unwind position.
- Snap-rule tests include fractional logical rects/text at scale 2 and fail on
  round-before-scale or unsnapped text origins.
- Culling-off and culling-on direct renderer tests produce identical visible
  output for covered scenes.
- Existing live demo/texelotl still render through the old path.

Verification:

- Full ecs-ui test-only and raylib test trees green.
- Add a direct renderer unit/pixel test that fails on wrong primitive order,
  wrong clip bounds, wrong text color/font size, and wrong callback source node.

### Stage 2: Remaining Paint Cleanup Before Renderer Flip

Goal: remove known paint shortcuts that would become long-term debt once the
direct renderer is live.

Concrete changes:

- Resolve IMAGE inventory from Stage 0. Add a neutral image item only if current
  product usage requires it.
- Apply the IMAGE decision only to the new paint/direct-renderer path: either
  assert paint emits no image primitive for current product trees, or add the
  neutral image paint item if a real screen needs it. Leave the legacy
  `EcsUiRaylibRenderDrawList` IMAGE branch alive for A/B until the Clay
  draw-list renderer is deleted in Stage 9.
- Keep any accepted CLAY-ONLY visual differences documented in
  `docs/solver-lowering.md`; do not hide them in renderer shims.

Acceptance:

- The bootstrap diff and direct renderer tests are load-bearing for
  IMAGE/no-IMAGE behavior and culling policy.
- No folded-border behavior remains in paint or renderer code.

Verification:

- Full ecs-ui suites green.
- In-repo bridge-vs-paint pixel cases for text, scroll/clip, z-overlap, culling,
  and scale 2 produce zero SEMANTIC deltas.

### Stage 3: Pixel A/B Harness

Goal: build the verification gate before any live cutover or deletion.

This is the load-bearing renderer verification. The 7.3-7.6 bootstrap command
diff proved paint -> Clay-command adapter output against the bridge; it did not
prove the direct raylib paint renderer that will ship.

Concrete changes:

- Add an ecs-ui raylib offscreen pixel harness that can render:
  - Clay bridge draw list.
  - Direct paint renderer from the same Clay-enriched snapshot.
  - Direct paint renderer from a native-solved snapshot.
- Add deterministic scene fixtures for:
  boxes/borders, bevel, nine-slice, custom/icon, text, text-field synthetics,
  scroll offsets, nested clips, ZStack/floating z, visual offsets, root z bases,
  scale 1 and 2, and culling on/off.
- Add diff reporting that prints first mismatching pixel, mismatch count,
  bounding box of mismatch, and the fixture name.
- Include snap-specific fixtures: fractional logical boxes and text at scale 2
  whose expected physical rects follow scale-then-`roundf`.

Acceptance:

- Harness itself proves it catches wrong color, wrong order, missing scissor,
  wrong z, wrong text run, and wrong scale conversion via mutation checks.
- Harness runs in both normal local builds and Xvfb CI-style environments.

Verification:

- The command bootstrap diff remains green.
- The new pixel harness is green with culling disabled.
- Culling-enabled runs are either green or documented as a perf follow-up kept
  disabled for the live cut.

### Stage 4: Native Interaction Frame Builder

Goal: remove the remaining live interaction dependency on Clay before the native
solver can drive a real frame.

Concrete changes:

- Pin hit-test order to the 7.6 root-sort model: z-sorted roots, stable root
  creation order for equal z, then within-root DFS order. Pointer-over priority
  and target resolution must use the same model that paint uses for visual order.
- Build `EcsUiInteractionFrame` directly from the solved snapshot:
  target rects, depth, emit/list order, blocking, capture flags, pressable
  actions, scroll containers, disabled state, and tree scale.
- Inventory and port the target-resolution algorithm from `src/ecs_ui_clay.c`:
  wrapper id conventions, blocker/area targets, emit-order and depth priority,
  disabled filtering, scroll-container data, direct scroll-container wheel
  routing versus subscribed scroll events, and overlap across z roots.
- Port the full capture lifecycle: `capture_pointer` bounded roots, stale
  capture targets, missed release reporting, drag threshold timing, secondary
  and middle button capture, capture covered by higher-priority targets, and
  release outside the original target.
- Replace Clay pointer-over queries with snapshot hit testing against layout
  output and paint/root order. Interaction must consume layout results, not
  backend internals.
- Route wheel, capture, drag, hover, press, release, and scroll events through
  the neutral frame only. `EcsUiScrollState` remains the sole scroll offset
  authority.
- Rewire `EcsUiFrameCollectEvents` to consume the neutral
  `EcsUiInteractionFrame` for both Clay-reference and native frames. A native
  frame must not trip the stale-frame guard merely because `backend->active_frame`
  was cleared by the native branch.
- Keep the Clay interaction path only as a test reference until deletion.

Acceptance:

- Pointer/capture/scroll parity tests pass for scale 1 and 2:
  press -> drag -> release, overlapping blockers, ZStack floaters, visual
  offsets, scroll consumption, text-field pressables, and bounded roots.
- texelotl input tests still pass without changing app event handling.

Verification:

- Existing frame containment tests are converted from Clay-command assertions to
  native interaction assertions where possible.
- A temporary dual-run interaction harness compares Clay-collected and
  native-collected events on the same frames. This harness must be green at
  scale 2 before Stage 6 starts; do not defer interaction parity to deletion.

### Stage 5: Paint Renderer Live Cutover

Goal: make the live renderer consume `EcsUiPaintList` while Clay can still solve
layout as a fallback/reference.

Concrete changes:

- Promote `EcsUiFrameInternalPaintList()` to public
  `EcsUiFramePaintList()`. It returns the paint list produced by the most recent
  successful frame run, or `NULL` if the most recent run did not produce paint.
- Keep `EcsUiFrameRun(...)` returning `const EcsUiDrawList *` during this
  transition. Hosts that have not migrated keep building, and texelotl can
  switch in its own commit without requiring a simultaneous ecs-ui API break.
- Add private `EcsUiFrameInternalClayCommands()` for A/B/bootstrap tests so
  tests can keep reading Clay commands after hosts no longer use the draw-list
  return.
- Update examples to render the accessor-provided paint list through
  `EcsUiRaylibRenderPaintList(...)`.
- Update texelotl's desktop renderer:
  - Keep calling `EcsUiFrameRun(...)` for layout/event production.
  - Read `const EcsUiPaintList *paint = EcsUiFramePaintList()`.
  - `EcsUiRaylibRenderDrawList(draw_list, ...)` becomes
    `EcsUiRaylibRenderPaintList(paint, &context->tree, ...)`.
  - Attach/headless layout calls that ignore the frame return do not need source
    changes.
- Keep the Clay bridge draw list and adapter available only for A/B tests.
- Define paint failure semantics:
  - If paint emission fails for the current frame, `EcsUiFramePaintList()`
    returns `NULL`; hosts skip UI rendering for that frame rather than rendering
    stale paint against the current snapshot.
  - The last-good artifact may remain available to ECS observers through
    `EcsUiFrameArtifacts`, but it is not a render source unless paired with the
    exact snapshot generation that produced it.
- Define lifetime/generation guards:
  - `EcsUiRaylibRenderPaintList` asserts or fails loudly when
    `paint->generation != tree->generation`.
  - Any `EcsUiFrameRun`, including a headless attach refresh on another tree,
    invalidates the previous render pairing. texelotl must render immediately
    after the frame run that produced `context->tree`, before any interleaved
    headless run.

Acceptance:

- texelotl builds and runs with paint rendering while still using the current
  Clay layout path.
- No texelotl code names Clay.
- No app-level event API changes.
- Paint-capacity/failure tests assert `EcsUiFramePaintList() == NULL` for the
  failed current frame and prove the renderer rejects generation mismatches.

Verification:

- ecs-ui full suites green.
- texelotl full build/test suites green.
- texelotl Xvfb A/B: old Clay draw-list renderer versus paint renderer on the
  same Clay-enriched snapshot has zero SEMANTIC deltas.

### Stage 6: Native Layout Shadow Mode, Then Live Flip

Goal: make native layout solve real frames without losing the Clay reference
until the final gate is complete.

Concrete changes:

- Add an internal shadow mode that runs Clay layout and native layout on the
  same snapshot, then renders both through the direct paint renderer for pixel
  comparison.
- Make native layout produce the live snapshot, interaction frame, scroll
  reports, and paint list.
- Make `EcsUiFrameSettleScroll` and `EcsUiFrameApply` consume only native
  scroll reports on the native path. The native path already stores reports from
  solver content output; settle/apply must stop consulting
  `Clay_GetScrollContainerData`, `Clay_GetElementData`, or
  `Clay_UpdateScrollContainers` once native is live.
- Change the default `EcsUiFrameRun` path to native solver + native paint after
  shadow cases are green.
- Keep `EcsUiFrameInternalBackend` only for tests during this stage; it is not a
  public adoption flag and is deleted later.

Acceptance:

- Native path returns a non-empty paint list and drives live rendering.
- Native path drives interaction and scroll in texelotl.
- Native settle/apply clamps and writes `EcsUiScrollState` from
  `native_scroll_reports` only.
- Clay shadow comparison is green or has only documented CLAY-ONLY deltas.

Verification:

- Rect scoreboard remains green until it is retired.
- Native-layout pixel harness is green at scale 1 and 2.
- texelotl live acceptance covers menu/dialog open, canvas zoom vs UI scroll
  arbitration, middle-drag pan blocked behind scrims, text-field caret/selection,
  attach refresh ordering, scale 2 pointer coordinates, and idle render counts.

### Stage 7: Native Frame Error Contract

Goal: replace Clay-originated public frame errors with native validation before
the Clay bridge is removed.

Concrete changes:

- Audit `EcsUiFrameErrorKind` in `include/ecs_ui/ecs_ui_frame.h`.
- Keep and implement native reporting for errors that remain meaningful without
  Clay, including: missing measure callback, element/paint capacity, text
  measure capacity, duplicate ids, floating/placement parent not found, invalid
  arguments, stale interaction frames, allocation failure, and internal errors.
- Delete or rename Clay-only errors whose conditions no longer exist in the
  native path. In particular, do not keep `ARENA_CAPACITY`,
  `INVALID_PERCENT`, or `UNBALANCED_LAYOUT` as public promises unless native
  code can actually produce and test those conditions.
- Move duplicate-id, placement-parent, text-measure-capacity, and capacity
  validation into native solver/paint/frame code before deleting Clay's error
  callback.
- Update tests to assert native error paths directly, not Clay callback
  translations.

Acceptance:

- Every surviving public frame error kind has a native producer and a test.
- Every deleted/renamed error kind has a release-note or migration note in the
  stage commit.
- No test depends on Clay's error callback for public error coverage.

Verification:

- Full ecs-ui suites green.
- Mutation checks prove duplicate id, capacity, and floating-parent validation
  failures are caught by native tests.

### Stage 8: Hard Verification Gate

Goal: prove the native live path is good enough to delete Clay.

Concrete changes:

- No feature work. This stage is verification and classification only.
- Run the full in-repo pixel harness:
  - bridge renderer vs paint renderer;
  - Clay-layout+paint vs native-layout+paint;
  - native live path end to end.
- Run texelotl Xvfb screenshot A/B on real screens.
- Triage every delta:
  - SEMANTIC: fix solver, paint, renderer, or interaction before proceeding.
  - CLAY-ONLY: document why native behavior is the desired end state.

Acceptance:

- Zero unattributed pixel deltas.
- Full ecs-ui tests green in test-only and raylib builds.
- Full texelotl builds/tests green.
- texelotl live smoke green at scale 1 and 2.
- Grep gate in texelotl source/build files returns no Clay symbols, excluding
  the guardrail script that intentionally contains the forbidden string.

Verification:

- Store the exact commands and fixture list in the stage commit message or a
  short verification note.
- A fresh reviewer can rerun the gate without app-specific manual steps beyond
  launching Xvfb and the scripted texelotl harness.

### Stage 9: Delete Clay

Goal: remove all Clay build and source surfaces after the gate is green.

Concrete changes, in order:

1. Remove live Clay code from frame backend:
   - Delete `src/ecs_ui_clay.c`.
   - Delete `include/ecs_ui/ecs_ui_clay.h`.
   - Remove `CLAY_IMPLEMENTATION` and Clay error/arena handling from
     `src/ecs_ui_frame.c`.
   - Delete `EcsUiFrameApplyClayScrollReports` and every
     `Clay_UpdateScrollContainers`, `Clay_GetScrollContainerData`, and
     `Clay_GetElementData` call from frame settle/apply code.
   - Delete `EcsUiFrameDrawListClayCommands`.
   - Delete `EcsUiDrawList`; change `EcsUiFrameRun` to return
     `const EcsUiPaintList *` now that texelotl no longer reads the draw-list
     return.
   - Rewrite `src/ecs_ui_frame_internal.h` so it no longer includes
     `ecs_ui_clay.h`, no longer defines `struct EcsUiDrawList` around
     `Clay_RenderCommandArray`, and no longer declares Clay command accessors.
     Delete the `NATIVE_DIVERGE` / `NATIVE_DEEP_DIVERGE` test-only enum values
     once equivalent native failure hooks exist.
2. Remove transition artifacts:
   - Delete `src/ecs_ui_paint_clay_adapter.c`.
   - Delete `src/ecs_ui_paint_clay_adapter.h`.
   - Delete `tests/ecs_ui_paint_clay_diff.c`.
   - Delete `tests/ecs_ui_clay_parity.c`.
   - Delete or rewrite Clay-dependent parts of `tests/ecs_ui_frame_containment.c`.
   - Rewrite `tests/ecs_ui_solver_parity.c` from a Clay scoreboard into native
     golden rect/content-dim tests.
   - Remove dual-backend uses in `tests/ecs_ui_paint_list.c` and any remaining
     `EcsUiFrameInternalSelectBackend(...CLAY...)` call sites.
   - Retire the rect scoreboard's Clay side; preserve native layout regression
     fixtures as golden tests against expected rects/content dims.
3. Remove Clay build plumbing:
   - Delete `ECS_UI_BUILD_CLAY_ADAPTER`.
   - Delete `ECS_UI_CLAY_SOURCE`.
   - Delete `ecs_ui_clay`, `ecs_ui::clay`, `ecs_ui_clay_headers`.
   - Remove all private links to `ecs_ui_clay`.
   - Remove Clay from raylib target links.
4. Remove Clay demo and names:
   - Delete `examples/raylib_demo/clay_main.c` and the
     `ecs_ui_clay_raylib_demo` target.
   - Rename any remaining "clay" demo labels that now describe the native
     frame path.
5. Remove or rewrite Clay-specific tests:
   - `ecs_ui_raylib_draw_list` becomes a paint-renderer test.
   - Drop the live `CLAY_RENDER_COMMAND_TYPE_IMAGE` branch in
     `src/ecs_ui_raylib_frame.c` with `EcsUiRaylibRenderDrawList`; Stage 0
     already proved current product trees do not emit raw IMAGE commands, or
     added a neutral image primitive if they do.
   - Any raw `Clay_RenderCommandArray` assertions become paint-list or pixel
     assertions.

Acceptance:

- `rg -n "(Clay_|CLAY_|clay\\.h|Clay_RenderCommand|Clay_RenderCommandArray|ecs_ui::clay|ecs_ui_clay|ECS_UI_CLAY|paint_clay_adapter)" include src tests examples CMakeLists.txt`
  returns nothing except historical docs/plans if those are intentionally kept.
- ecs-ui builds with no Clay checkout installed.
- texelotl builds against ecs-ui with no Clay source, target, or include path.
- The direct paint renderer and native solver are the only shipped frame path.

Verification:

- Full ecs-ui test-only and raylib suites green from a clean build directory
  configured without any Clay variables.
- Full texelotl build/test suites green against that ecs-ui tree.
- One final texelotl Xvfb smoke run on the native-only build.

### Stage 10: Post-deletion Cleanup

Goal: remove remaining transition language and stabilize native contracts.

Concrete changes:

- Move enduring layout/paint semantics from `docs/solver-lowering.md` into a
  native contract doc. Keep Clay notes only as historical background.
- Remove "bootstrap", "adapter", and "Clay reference" language from public docs.
- Review API names for native-first clarity:
  `EcsUiFrameBackendSetCullingEnabled` may remain as a native renderer perf
  option or be deleted if unused.

Acceptance:

- Docs describe the native architecture without requiring Clay knowledge to
  understand current behavior.
- No code changes beyond docs/API cleanup unless separately reviewed.

## What Gets Deleted and When

Deleted only after Stage 8 is green:

- `src/ecs_ui_clay.c`
- `include/ecs_ui/ecs_ui_clay.h`
- `src/ecs_ui_paint_clay_adapter.c`
- `src/ecs_ui_paint_clay_adapter.h`
- `src/ecs_ui_frame_internal.h` Clay includes, Clay draw-list struct, Clay
  command accessors, and divergence-only backend enums
- `tests/ecs_ui_clay_parity.c`
- `tests/ecs_ui_paint_clay_diff.c`
- Clay scoreboard logic in `tests/ecs_ui_solver_parity.c`
- Clay-dependent raw-command sections of `tests/ecs_ui_frame_containment.c`
- Dual-backend Clay selection in `tests/ecs_ui_paint_list.c`
- Clay-dependent `tests/ecs_ui_raylib_draw_list.c` assertions, replaced by
  paint renderer assertions
- `examples/raylib_demo/clay_main.c`
- `ecs_ui_clay`, `ecs_ui::clay`, `ecs_ui_clay_headers`,
  `ECS_UI_BUILD_CLAY_ADAPTER`, `ECS_UI_CLAY_SOURCE`, and all Clay target links
- `EcsUiDrawList`, `EcsUiFrameDrawListClayCommands`,
  `EcsUiFrameInternalClayCommands`, `EcsUiRaylibRenderDrawList`
- `EcsUiFrameApplyClayScrollReports` and the Clay scroll update/data calls in
  frame settle/apply
- The dead Clay IMAGE render branch in `src/ecs_ui_raylib_frame.c`
- `EcsUiFrameInternalBackend` Clay/native selector and divergence-only backends,
  after native regression tests no longer need a dual backend

The deletion stage is intentionally late. Before that point, the tree always has
either the old Clay renderer or the new paint renderer available for live use.

## Risk Register

### Border Fold

Risk: folded border items draw before children, while Clay draws borders after
children. This can produce pixels differences where children overlap the border
band.

Mitigation: split border into an explicit late paint role before renderer live
cutover. Pixel cases must include a child overlapping the parent's border.

### Native Solver Has Never Driven a Live Frame

Risk: rect parity did not exercise live rendering, interaction target building,
or frame ordering.

Mitigation: paint renderer cutover happens first while Clay still solves layout.
Native layout then runs in shadow mode and must drive paint, interaction, scroll,
and texelotl live screens before the default flips.

### Clay-Gated Build Targets

Risk: today `ecs_ui_frame` and the raylib frame renderer are created only when
the Clay adapter target exists, so simply deleting Clay would also delete the
targets texelotl depends on.

Mitigation: Stage 0 re-homes frame, solver, and raylib paint renderer targets to
unconditional native builds and explicitly verifies
`-DECS_UI_BUILD_CLAY_ADAPTER=OFF`.

### Paint/Snapshot Lifetime

Risk: paint text runs alias snapshot strings and custom/icon/nine-slice render
callbacks resolve source entities against the supplied snapshot. Rendering a
last-good paint list against a newer snapshot can read stale or mismatched data.

Mitigation: Stage 5 makes paint failure return no renderable paint for the
current frame, requires `paint->generation == tree->generation` in the direct
renderer, and documents that any intervening frame run invalidates the previous
paint/snapshot pair.

### Interaction Still Depends on Clay Internals

Risk: current native path ignores pointer/frame arguments; Clay target lists and
pointer-over ids still feed live events.

Mitigation: Stage 4 builds interaction frames from solved snapshots and paint
order. Native event tests cover capture, blockers, scroll, floating z, visual
offsets, bounded roots, and scale 2.

### Clay-Conformance Quirks

Risk: the solver/paint currently reproduce some Clay quirks because they were
the reference, not because they are desirable forever.

Mitigation: during this cut, freeze existing native behavior unless a clean
paint-only correction is already identified and guarded by pixels. Record
remaining quirks as v1 native contract debt after deletion; do not keep Clay
shims to preserve them.

### IMAGE Commands

Risk: the legacy renderer supports raw Clay IMAGE commands, but paint has no
image primitive today.

Mitigation: Stage 0 inventories actual emitted command kinds. If current product
trees do not emit IMAGE, add a no-IMAGE regression and delete the legacy IMAGE
renderer branch in Stage 9 with the Clay draw-list renderer. If current product
trees do emit IMAGE, add a neutral image paint item before Stage 1 acceptance.

### Culling Parity and Performance

Risk: Clay culling and paint renderer culling can hide different command subsets,
especially text lines and scroll/clip trees.

Mitigation: pixel diff runs with culling off. Native culling is a renderer perf
option with separate culling-on pixel tests. It must not be required for
correctness.

### HiDPI / Scale 2

Risk: prior bugs repeatedly hid at scale 1 and zero offsets.

Mitigation: every harness stage runs scale 1 and 2 with fractional positions,
padding, text sizes, scroll offsets, and bounded roots. Pixel comparisons report
physical and logical coordinates.

### Text-Field Synthetics

Risk: text-field value nodes are not enriched, and caret/selection/inline
segments have synthetic flow and paint identity.

Mitigation: keep text-field cases in structural paint tests, pixel harness,
native interaction tests, and texelotl A/B. Include repeated equal substrings,
selection overflow, disabled/inherited styles, opacity-zero value nodes, and
additional non-value children.

### Scroll State

Risk: scroll offsets are already ECS-owned, but native live frames must consume,
clamp, report, and render the state without Clay retained data.

Mitigation: Stage 6 verifies native `EcsUiFrameSettleScroll`, content reports,
wheel pending updates, one-frame-lag timing, and scale-2 accumulated offsets.

### Source Entity Lookup Cost

Risk: current adapter-style source entity resolution is a linear scan. A direct
paint renderer that resolves source entities per item can become O(nodes *
items).

Mitigation: not a cutover gate. Keep the direct renderer simple first; add a
snapshot entity->node index as a measured perf follow-up if profiles show it.

## texelotl Impact

texelotl already has no Clay source/build dependency. Expected app changes:

- During Stage 5, keep `EcsUiFrameRun(...)` for frame production, then read
  `const EcsUiPaintList *paint = EcsUiFramePaintList()`.
- Replace `EcsUiRaylibRenderDrawList(draw_list, fonts, root_context, options)`
  with `EcsUiRaylibRenderPaintList(paint, &context->tree, fonts, root_context,
  options)`.
- No CMake Clay cleanup is expected in texelotl; it already links neutral
  `ecs_ui::frame` / `ecs_ui::raylib`.
- Attach/headless refresh calls that ignore `EcsUiFrameRun`'s return should not
  need source changes.
- Event, scroll, hover/capture, and custom/icon/nine-slice callback APIs should
  remain unchanged.

If implementation discovers additional texelotl changes, treat them as a design
finding. Do not add app-side Clay shims.

## Needs User

Resolved for this plan:

- texelotl Xvfb A/B screen list is approved as written: menus/dialogs, canvas
  surface, scroll containers, text fields, overlapping floating UI, scale 2, and
  idle/parked frames.
- Delta policy is approved: fix every SEMANTIC delta; accept and document a
  CLAY-ONLY delta only when the native behavior is the desired long-term look.

No open architecture decisions remain for the plan draft.
