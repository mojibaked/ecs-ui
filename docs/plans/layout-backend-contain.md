# Layout-backend containment: clay behind backend-neutral ecs-ui types

Accepted 2026-07-07 (Codex design review round 1 amendments folded in). Step 1 of
`layout-backend-direction.md`: move clay's entire surface into this repo's bridge so
texelotl names zero clay symbols in source AND build files. Gates texelotl adoption of
any future native backend; with this done, backend swap is a flag flip inside ecs-ui.

## Current texelotl surface (inventory, post input-routing Stage B cc23f85/2153a74)

1. Frame-driving cluster (`platform/desktop/main.c`, `platform/desktop/attach.c`):
   arena init (`Clay_MinMemorySize`/`Clay_CreateArenaWithCapacityAndMemory`/
   `Clay_Initialize`/`Clay_ErrorHandler`), `Clay_SetMeasureTextFunction`,
   `Clay_SetLayoutDimensions`, `Clay_BeginLayout`/`Clay_EndLayout` around
   `EcsUiClayEmitTreeEx`, `EcsUiClayEnrichSnapshotLayout`, `Clay_SetPointerState`,
   `Clay_UpdateScrollContainers`, `EcsUiClayInteractionState/FrameBegin/CollectFrameEvents/
   ApplyInteractionFrame`, `EcsUiClayLayoutOptions`, `Clay_RenderCommandArray` handed to
   the renderer. attach.c re-runs a headless layout (SetLayoutDimensions + Begin/Emit/End
   + Enrich) for tree reads at forced dimensions.
2. Renderer cluster (`platform/desktop/clay_raylib_bridge.{c,h}`): walks
   `Clay_RenderCommandArray` (rect/border/text/image/custom/scissor render data types),
   plus the `Clay_StringSlice`/`Clay_TextElementConfig` measure-text callback.
3. Interaction-frame type threaded through app code (`ui_frame.h`, `ui_runtime.{c,h}`,
   `ui_shell.{c,h}`, `canvas_widget.c`, `tests/ui_canvas_input.c`):
   `EcsUiClayInteractionFrame` only.
4. Test fixtures: `tests/attach_protocol.c` (clay init + measure fn + culling toggle)
   AND `tests/clay_test_impl.c` (missed by the first inventory).
5. Build system (review finding): texelotl CMake owns clay as a dependency —
   `TEXELOTL_CLAY_SOURCE`, `texelotl_clay` target + `clay` alias, forced
   `ECS_UI_BUILD_CLAY_ADAPTER`, `ecs_ui::clay` links. Source-level cleanliness alone
   does not meet the goal; the build plumbing goes too.

## Design constraints (carry into type shapes)

- Backend contract is LAYOUT-ONLY (user decision 2026-07-07, supersedes the direction
  note's "rects + a draw list" phrasing): tree + measure callback in → queryable layout
  state out (enriched snapshot rects; later line boxes / clip+scroll metadata). Paint is
  a separate stage. The neutral API exposes layout results ONLY via the enriched
  snapshot, and the draw list ONLY as an opaque handle.
- NO backend element ids in public neutral types (review finding 1): public interaction
  targets carry node/action/payload, node_id, depth/emit order, capture/block/scroll
  flags, scale — everything app-side code reads today. Clay element ids stay private to
  the clay backend (it still needs them internally for pointer-over resolution and
  scroll-container mutation).
- Draw-list lifetime (review finding 2): backend-owned single slot; valid only until the
  next frame run OR backend shutdown, and only while the exact source tree snapshot is
  alive and unmodified (clay custom commands point into the snapshot's nodes). A
  headless re-layout invalidates the previous draw list. texelotl renders before the
  attach refresh path, so ordering is already compatible.
- Coordinate contracts unchanged: physical in, logical out. Neutral layout options use
  explicit `physical_` field naming (review finding 6) so the physical-input contract is
  visible; the neutral header restates the conversion rules currently documented in
  ecs_ui_clay.h.

## API (accepted shape; reviewed smaller than first draft)

New public header `include/ecs_ui/ecs_ui_frame.h` (backend-neutral; including it must
not require clay.h):

- `EcsUiPointerState` — rename of `EcsUiClayPointerState`, unchanged fields.
- `EcsUiInteractionTarget/PointerCapture/InteractionState/InteractionFrame` — neutral
  renames WITHOUT element-id fields; attach-point expression reuses existing
  `EcsUiAlign` pairs instead of clay-like enums.
- `EcsUiFrameLayoutOptions` — `physical_bounds` rect fields, `EcsUiAlign`-pair attach
  points, `z_index`, `capture_pointer`.
- Measure callback: `EcsUiSize (*EcsUiMeasureTextFn)(const char *utf8, int32_t len,
  const EcsUiTextMeasureSpec *spec, void *user_data)`; spec carries font id, font size,
  letter/line spacing. Clay backend adapts internally.
- Error callback: neutral `kind + message + user_data` (small neutral kind enum; no clay
  error types).
- Lifecycle: `EcsUiFrameBackendDesc { surface_w/h, measure fn + user_data, error cb }`,
  `bool EcsUiFrameBackendInit(desc)` (arena allocated internally, clean failure, single
  active backend, main-thread only — documented), `EcsUiFrameBackendShutdown(void)`,
  `EcsUiFrameBackendSetSurfaceSize(float,float)`, `EcsUiFrameBackendSetCullingEnabled(bool)`.
- Frame run: `const EcsUiDrawList *EcsUiFrameRun(EcsUiTreeSnapshot *tree,
  const EcsUiTheme*, const EcsUiFrameLayoutOptions*, const EcsUiPointerState
  *pointer_or_null, EcsUiInteractionFrame *frame_or_null)` — wraps BeginLayout +
  EmitTreeEx + EndLayout + EnrichSnapshotLayout + (when pointer given) pointer commit.
  NULL pointer + NULL frame covers attach.c's headless re-layout.
- `EcsUiFrameSettleScroll(double dt)` stays separate (routed wheel handling mutates
  scroll state between collect and settle), wrapping Clay_UpdateScrollContainers under
  the Stage B contract (no internal re-targeting).
- Events/apply keep shapes: `EcsUiFrameInteractionStateInit`, `EcsUiFrameCollectEvents`,
  `EcsUiFrameApply`, `EcsUiFrameTreePointerInside`.
- Renderer entry moves INTO ecs-ui: `void EcsUiRaylibRenderDrawList(const EcsUiDrawList*,
  Font *fonts, const EcsUiRaylibRenderContext *root, const EcsUiRaylibDrawOptions
  *options)` — texelotl's clay_raylib_bridge.c relocated; options struct replaced by
  existing `EcsUiRaylibDrawOptions`; default raylib measure adapter `EcsUiRaylibMeasureText`
  provided for `EcsUiFrameBackendDesc`; bridge shutdown folds into backend/raylib cache
  release. Target layering (review finding 5): the clay dependency of the relocated
  renderer is PRIVATE in the ecs-ui target graph and deliberate — hosts link a neutral
  target and never inherit a clay include path.
- `ecs_ui_clay.h` remains as the clay backend's implementation surface (parity tests
  keep using it); `src/ecs_ui_frame.c` is a thin layer over it.

## Stages

1. ecs-ui: add `ecs_ui_frame.h` + `src/ecs_ui_frame.c` + renderer/measure relocation
   with private clay linkage; migrate `ecs_ui_clay_raylib_demo` to the neutral API as
   in-repo proof; unit coverage: (a) neutral frame run produces identical enriched rects
   to the raw EcsUiClay* sequence on parity trees, scale 1 and 2, mock measure;
   (b) compile check that `ecs_ui_frame.h`/`ecs_ui_raylib.h` build without clay.h on the
   include path; (c) renderer relocation covered for scissor clipping and
   custom/icon/nine-slice draw paths (tree rects cannot catch clip regressions).
   All targets + full ctest green.
2. texelotl: migrate the 13 inventory files + CMake (delete `texelotl_clay` target,
   `TEXELOTL_CLAY_SOURCE`, `ecs_ui::clay` links, forced adapter flag; delete
   `clay_raylib_bridge.{c,h}` and `tests/clay_test_impl.c` in favor of ecs-ui entries;
   tests move to the neutral fixture init). Acceptance grep-gate:
   `rg -n "(Clay_|EcsUiClay|CLAY_|clay\.h|ecs_ui::clay|ecs_ui_clay|TEXELOTL_CLAY|clay_raylib)"
   --glob '!third_party/**' --glob '!build*/**'` returns NOTHING in texelotl, and
   texelotl targets compile without any texelotl-owned clay include path/target.
   Builds (both trees, all targets), full ctest, live attach-harness acceptance under
   Xvfb: dialog-list wheel scroll vs canvas zoom arbitration, middle-drag pan blocked
   behind scrims, dialog open/focus paint, pointer/scroll at ui scale 2
   (physical→logical), attach apply/read timing (reply post-apply; immediate
   `state`/`tree` read observes the effect), 20s idle = 0 renders, `tree` rect read
   matches `state` canvas_surface.
3. Follow-up (separate plan): native solver prototype per `layout-solver-prototype.md`.
