# Input routing and frame-signal foundation pass

Accepted 2026-07-07 (Codex design review round 1 amendments folded in). Fixes three
live-verified problems found during the texelotl menus/file-dialogs cleanup session
(texelotl 59ed339 context):

1. UI-world state resolved after widget emit (text-input focus, programmatic value
   changes) paints one wake late: the runner parks on the stale frame because no frame
   signal represents text-input state.
2. Wheel input never enters the hit-tested event pipeline: `Clay_UpdateScrollContainers`
   and app-side raw polling (canvas zoom, middle-drag pan) both consume the same wheel
   with no arbitration — scrolling a modal list also zooms the canvas behind it.
3. Deferred attach replies block until a frame *presents*; a hidden Wayland surface gets
   no frame callbacks, so injected-input replies hang until the window's desktop is
   viewed (measured: state read 21ms, pointer reply >8s).

Design rules (user-set): consumers of routed input are plain event listeners — no
app-side scene-topology queries; routing goes through the SAME capture-respecting hit
walk as presses so a future gesture/disambiguation stage has one interposition point.
v1 deliberately excludes bubbling, scroll chaining, and gesture arenas — additive later.

## Stage A — text-input state revision as a frame signal

- ecs-ui: `uint64_t EcsUiTextInputStateRevision(const ecs_world_t *world)` — a
  world-level monotonic counter. Bumped ONLY on actual state changes, centralized in the
  private mutation helpers (focus resolution when the focused field changes, blur,
  SetValue/SetPlaceholder when the stored text differs, edit-applying systems when they
  mutate). Never unconditional per-frame bumps; cursor blink is presentation-side and
  MUST NOT ride this counter.
- Host (texelotl desktop): register as a stable frame signal alongside window metrics /
  ui scale / projections / canvas surface / selection mask; set revision each step.
  A change during progress marks the frame unsettled → exactly one follow-up frame.

Acceptance: file dialog opened on a parked app paints the path field focused with no
input nudge; repeated identical SetValue/SetPlaceholder and focus-request-on-already-
focused-field do NOT advance the signal; a cursor move/edit wakes exactly once; blink
never wakes; parity tests at scale 1 and 2; 20s idle after dialog open = 0 renders.

## Stage B — hit-tested wheel routing + ECS_UI_EVENT_SCROLLED + middle-drag routing

- New node capability `scroll_subscribed` (builder desc flag, like pressable/on_click).
- Shared capture-respecting hit-target helper (one walk used by presses and wheel):
  topmost eligible target under the pointer wins; scrims/capture nodes block behind.
  Eligible = clay scroll container OR scroll_subscribed node. Priority when both on one
  node: scroll-container-first. Axis-aware: a delta axis the target cannot consume
  (horizontal over a vertical-only list) does NOT fall through to anything behind — v1
  drops it (no chaining).
- Scroll containers: the WINNING container is scrolled directly via clay's per-container
  scroll data — `Clay_UpdateScrollContainers`' own internal hit test must not re-decide
  the target (it can disagree with capture/z-order and reintroduce double-consume).
- Subscribed nodes: emit `ECS_UI_EVENT_SCROLLED` into the frame event list with
  dedicated float fields `scroll_x`/`scroll_y` plus the pointer position in LOGICAL
  coordinates (same conversion as pointer events). No payload packing.
- Nothing eligible under the pointer → wheel dropped.
- Middle button: add middle down/pressed/released to `EcsUiClayPointerState`; capture
  the press target; deliver middle-drag events only to that node (press-target capture,
  same as primary drags). This closes the same routing-leak class for pan.
- texelotl adoption: canvas surface subscribes for wheel (zoom moves out of
  `TexelotlDesktopHandleCanvasInput` raw polling into the canvas widget event path) and
  consumes middle-drag for pan via the routed path. `ECS_UI_EVENT_SCROLLED` added to the
  desktop follow-up-render allowlist (allowlist replacement is out of scope). Attach
  `scroll` verb feeds the same route as hardware wheel.

Acceptance: wheel over the dialog list scrolls the list and does NOT zoom the canvas;
wheel over the canvas zooms; wheel over the dialog scrim (outside the list) does
nothing; attach scroll verb drives the dialog list through the routed path with correct
physical→logical scaling; middle-drag pan blocked behind modal/menu scrims; A4 clipped
hit-test acceptance repeated; marching-ants/parking behavior unchanged.

## Stage C — apply-complete attach replies (present-independent)

- Split reply completion: normal commands (pointer/text/key/scroll/tool/ticks) complete
  "post-apply, pre-present", where apply includes the FULL UI pipeline (event
  collection, widget dispatch, action dispatch, state/tree updates) so a following
  `state`/`tx.read.tree()` observes the effect. Screenshots keep waiting for a real
  rendered frame on an isolated path — a pending screenshot must not head-of-line-block
  other clients' replies or the `attach.reply` wake source.

Acceptance: pointer reply <100ms with the window on a hidden desktop; an immediately
following state/tree read observes the injected effect; screenshot still waits for a
real frame; unrelated commands reply while a screenshot is pending; runner returns to
park; full mock-inpaint drive green.

## Watch-item (verify after Stage A)

Transient UNCLIPPED file-dialog list overflow seen once pre-cleanup (not reproducible).
Clay clip is same-frame by construction (clip config declared with the container,
ecs_ui_clay.c:750), so the suspect is a layout transient frozen by parking. After Stage
A: repeatedly open the dialog in a large directory; assert the list rect stays fixed via
tree dump AND assert draw-time clipping via screenshot pixel check or render-command
clip inspection (tree rects cannot prove clipping).
