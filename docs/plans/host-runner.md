# ecs-ui Host Runner Plan (foundation pass)

Author: orchestrator (Claude).
Status: REVIEWED AND ACCEPTED — see `## Review` (adversarial pass, verdicts per
item) and `## Final implementation contract`. Where they conflict, the review
and contract supersede the item text. Ready for implementation in the
contract's stage order.

## Motivation

Every ecs-ui consumer hand-rolls its host loop. The in-tree demos do a bare
`while (!WindowShouldClose())`; texelotl's `platform/desktop/main.c` has grown
to 2,962 lines, most of which is generic loop machinery (idle-park policy,
frame-dirty gating, presentation cache, wake bookkeeping) that has nothing to do
with pixel editing. The loop is where power efficiency, responsiveness, and
async features compose — and today that composition is app-side lore.

**Motivating incident (2026-07-06).** texelotl's agent-attach harness (texelotl
88d7cde) needed the loop awake to service socket commands. The loop had no way
to express "wake me on fd activity," so the harness vetoed idle parking
globally: `event_waiting_allowed = ... && !TexelotlAttachEnabled(&attach)`
(`../texelotl/platform/desktop/main.c:2543`), and attach initializes
unconditionally on POSIX (`main.c:2464`). Result: the park path became dead
code, the app presents a full-window blit at 60Hz forever, and idle GPU went
from ~6% to ~50% on the user's machine. Measured with the app's own counters:
`rendered=0 blit-only=60` per second while completely idle. The app's comment
records the intended behavior: "Event waiting blocks the loop at idle (measured
~0% GPU)" (`main.c:2536`).

The bug was not carelessness — it was **missing vocabulary**. Today the loop's
stay-awake decision is a hand-maintained boolean soup
(`keep_loop_hot = ticked_pending_events || auth_pending_work ||
genai_pending_work || authoring_operation_active || selection_visible ...`,
`main.c:2738`), and every new async feature must remember to patch it. The
selection marching-ants animation is likewise a hand-rolled wake hack
(`main.c:2727`). texelotl is ~10% of its intended product surface; every
future feature (more AI jobs, timers, watchers, background imports) grows the
soup, and every future ecs-ui app starts the lore from zero.

## Design principles

1. **Parked is the default.** An idle app costs ~0% GPU/CPU. Staying awake is
   an explicit, attributable state — you can always answer "who is keeping the
   loop hot?"
2. **Wake sources are first-class.** OS input, pending work, deadlines, fds,
   and cross-thread posts are registered objects, not booleans in a private
   expression. Features compose by *adding* a wake source, never by editing
   loop policy.
3. **Inversion of control.** The app supplies callbacks; the runner owns the
   loop. This is also the only shape that works on Emscripten (browser owns the
   loop; cf. SDL3's `SDL_AppIterate` transition) — attach.c already stubs for
   `__EMSCRIPTEN__`, so web support is a plausible future.
4. **Observable by contract.** The runner exposes its per-second
   rendered/blitted/parked counters and current wake-source states. Agents and
   tests can assert "idle app is parked" — turning this incident's bug class
   into a permanent automated regression guard.
5. **Escape hatches.** A `Step` API lets an app keep its own `while` loop
   during migration or for exotic needs. The full `Run` API is the paved road.

## 1. Wake-source registry (core, backend-neutral)

**Problem.** See motivation: stay-awake policy is an app-side boolean soup with
no attribution, and fd-driven features cannot participate at all, forcing
global vetoes.

**Proposal.** A small registry in ecs-ui core (no raylib dependency):

- `EcsUiWakeSourcePending(reg, id, bool)` — level-triggered "work pending"
  flags (replaces auth/genai/authoring booleans). Sticky until cleared by the
  owner; the runner reads them each frame.
- `EcsUiWakeSourceDeadline(reg, id, seconds_from_now)` — the loop must run no
  later than the deadline (animations, blink, debounce). One-shot; re-arm each
  frame while animating. The animation module arms this itself for its active
  animations, so apps get animation wakes for free.
- `EcsUiWakeSourceFd(reg, id, fd)` — POSIX readability wake (sockets, pipes,
  inotify). This is the primitive the attach harness needed.
- `EcsUiWakePost(reg)` — thread-safe explicit wake for completion callbacks
  from worker threads (AI jobs, downloads).

Attribution: each source has an id/label; a debug query returns which sources
are currently keeping the loop awake (feeds item 4 and tree-dump-style
devtools).

**Acceptance.** Unit tests for each source kind driving a mock loop: pending
holds hot, deadline wakes at/after the deadline (never before park exit),
fd readability wakes, post from another thread wakes exactly once. Attribution
query names the culprit in each case.

**Risk/effort.** M. New API surface; the design must nail thread-safety rules
(post is any-thread; registry mutation is loop-thread only).

## 2. Frame lifecycle and dirty gating (extract from texelotl)

**Problem.** Render-vs-blit-vs-skip gating lives in the app: texelotl's
`src/render/frame_dirty.c` (revision-compare + transient signals) is generic,
tested, and in the wrong repo. Its input signal struct is hand-assembled from
projections each frame (`main.c:2753`). Other consumers get nothing and will
re-render every frame forever.

**Proposal.** Move the mechanism into ecs-ui core as the runner's frame
decision: each frame classify as RENDER (some revision/signal changed), PRESENT
(nothing changed but the backend requires a present — occlusion, park
disabled), or PARK (nothing changed, no wake source hot). ecs-ui already knows
most inputs (tree revision, projection revisions, animation activity); the app
contributes extra revisions (e.g., texelotl's canvas surface revision and
selection mask revision) through a small signal API rather than a hand-built
struct.

**Acceptance.** frame_dirty's existing test matrix ports over; new tests cover
the RENDER/PRESENT/PARK classification against combinations of revision change
× wake-source state. texelotl deletes `src/render/frame_dirty.c` on migration.

**Risk/effort.** S-M. Mostly relocation plus a signal API; the classification
contract is the design work.

## 3. Runner driver for raylib (backend-specific)

**Problem.** Parking, event pumping, and presentation are backend-specific and
subtle: raylib's `EnableEventWaiting` blocks indefinitely in `glfwWaitEvents`,
which services neither fds nor deadlines — exactly why attach vetoed it. The
presentation cache (window-sized render target management, `main.c:2697`) is
also generic raylib machinery apps shouldn't own.

**Proposal.** In the `ecs_ui_raylib` layer:

- `EcsUiRaylibRun(config, callbacks)` — owns the loop. Callbacks: `tick(dt)`,
  `render(ctx)`, optional `frame_begin/frame_end` hooks (texelotl needs these
  for attach pump and screenshot capture during migration).
- `EcsUiRaylibStep(...)` — single-iteration escape hatch; `Run` is a loop over
  `Step`.
- Park implementation must satisfy the registry contract: block until OS
  event, fd readability, post, or earliest deadline — whichever first.
  Candidate mechanisms (review should pick): `glfwWaitEventsTimeout` with
  poll()-integrated fds; or a watcher thread poll()ing fds + post pipe that
  calls `glfwPostEmptyEvent`. Wake latency for fd/post sources while parked
  must be bounded (target: ≤ a few ms, not "next user input").
- Present policy per item 2's classification; presentation cache folds in.

**Acceptance.** Demo apps migrate to `Run` (their loops delete). A harness
test drives an idle runner and asserts PARK frames dominate (the regression
guard); an fd wake test asserts a socket write is serviced while parked
without synthetic user input.

**Risk/effort.** M-L. The park mechanism is the hard part; raylib may need a
thin platform shim (direct glfw calls from the bridge, or a custom frame
control build flag). Review must verify what vendored raylib exposes.

## 4. Loop observability

**Problem.** The incident was invisible until the user watched a GPU meter.
texelotl's ad-hoc `TEXELOTL_DESKTOP_STATS` counters (`main.c:2534`) were the
diagnostic that localized it — that facility should be standard.

**Proposal.** Runner-owned counters (rendered / presented / parked per second,
current park state, hot wake sources with labels) queryable via API; env-var
stderr reporting comes with the runner. Pairs with dx-hardening item 4 (tree
dump) and the attach harness so agents can assert loop health in state
commands.

**Acceptance.** Counter values asserted in runner tests (idle ⇒ parked
dominates; animation ⇒ deadline wakes at expected cadence; pending flag ⇒
attribution names it). texelotl deletes its stats code and exposes runner
stats through attach `state`.

**Risk/effort.** S. Data already exists inside the runner; this is surfacing.

## Explicit non-goals (this pass)

- Fixed-timestep/game-style simulation loops (tick receives real dt; nothing
  more).
- Multi-window, vsync policy control, frame-rate limiting beyond raylib's
  existing target-FPS.
- Moving texelotl's attach harness into ecs-ui as a reusable module. It is the
  obvious *next* foundation pass (every future app wants agent-drive), and the
  runner should make it trivial (attach becomes: register fd wake source +
  frame hooks) — but socket protocol design is its own review. Item 3's frame
  hooks must be sufficient for texelotl's attach to live outside the runner;
  that constraint is in scope.
- Emscripten driver implementation (the callback shape must permit it; the
  driver itself is future work).

## Sequencing and migration

Item 1 → 2 → 3, then 4 inside 3's structure (1 and 2 are core-only and
testable without a window; 3 consumes both). Independent of dx-hardening
stages 2–5; can interleave in the same Codex session. ecs-ui house style
applies (enums/switch fine here; texelotl guardrails do NOT apply).

Consumer migration (the stage that proves it, per house doctrine): texelotl
adopts `Run` (or `Step` first if the diff is too large), registers its wake
sources (auth pending, genai pending, authoring-op, attach fd), deletes
`frame_dirty.c`, the presentation cache, the stats code, the
`keep_loop_hot`/`event_waiting_allowed` policy, and the selection-dash wake
hack (animation module arms the deadline). Verification: full ctest both
repos, attach-harness drive still instant, **and idle GPU back to ~0% measured
via runner counters** — the counter assertion lands in texelotl's harness
tests so this class of regression can never ship silently again.

## Open questions for review

1. Park mechanism on vendored raylib: what does it actually expose
   (`glfwWaitEventsTimeout`? custom frame control? direct glfw linkage from
   the bridge)? Pick the mechanism with bounded fd/post wake latency.
2. Registry threading contract details (post-from-any-thread is required;
   is fd registration loop-thread-only acceptable?).
3. Does `Step` receive the frame classification, or does it own less than
   that? (texelotl's attach screenshot capture needs a defined point after
   render.)
4. Windows: fd wake sources are POSIX-first; is the Windows story "post only"
   for now, and is that acceptable to document?
5. Should item 2's signal API accept app revisions as (id, u64) pairs or a
   caller-owned struct? (Attribution favors pairs.)

## Review

### 1. Wake-source registry

**Verdict: APPROVE-WITH-CHANGES.** The motivating gap is real. texelotl disables
raylib event waiting whenever attach is enabled (`../texelotl/platform/desktop/main.c:2543`)
because the loop has no way to wake on attach sockets. It also hand-rolls pending
work and animation-ish hot-loop policy (`../texelotl/platform/desktop/main.c:2690`,
`../texelotl/platform/desktop/main.c:2738`). The attach harness proves this is
not just a boolean wake source: it owns a listener plus client fds
(`../texelotl/platform/desktop/attach.h:58`), makes them nonblocking
(`../texelotl/platform/desktop/attach.c:309`), accepts dynamically
(`../texelotl/platform/desktop/attach.c:1586`), reads until EAGAIN
(`../texelotl/platform/desktop/attach.c:1526`), and may need write readiness for
pending replies (`../texelotl/platform/desktop/attach.c:714`,
`../texelotl/platform/desktop/attach.c:1629`). The plan's "animation module arms
the deadline" assumption is not true today: animation state is just
`EcsUiLinear1f`, advanced by an `EcsOnUpdate` system using `it->delta_time`
(`src/ecs_ui_animation.c:134`, `src/ecs_ui_animation.c:164`), with no public
active-animation or deadline API.

**Amendments.** Define registry sources as: pending/post flag, absolute
monotonic deadline, and POSIX fd with read/write interest masks. Fd registration,
interest updates, and removal may be loop-thread-only; `EcsUiWakePost` must be
callable from any thread. Handles need generation IDs so stale removals cannot
tear down reused slots. Animation integration must be explicit: either add a
small `EcsUiAnimationHasActive(world)`/`EcsUiAnimationArmDeadline(...)` helper or
make the runner's ecs-ui integration query `EcsUiLinear1f`; while active, the
deadline is "next frame", not a per-animation completion time. Labels are part of
the API, not debug-only, because item 4 attribution depends on them.

**Acceptance.** Add core tests for pending/post coalescing, deadline ordering,
fd read readiness, fd write readiness, dynamic fd add/remove, stale-handle
removal, and label attribution. Add an animation-active test that proves an
active `EcsUiLinear1f` prevents indefinite park, then clears after completion.

**Effort/API.** L, not M. Public API is additive for ecs-ui and the in-tree demo.
texelotl migration is behaviorally large but source-compatible until it opts into
the registry; it should then delete the attach veto and pending-work hot-loop
policy.

### 2. Frame lifecycle and dirty gating

**Verdict: APPROVE-WITH-CHANGES.** The diagnosis is correct, but the plan
overstates what ecs-ui can infer. texelotl's dirty gate compares a stable signal
and transient flags (`../texelotl/src/render/frame_dirty.c:39`,
`../texelotl/src/render/frame_dirty.c:41`), and that signal is mostly app-domain
data: projection frame revision, canvas surface revision, selection-mask
revision, plus input/window/document flags (`../texelotl/src/render/frame_dirty.h:7`).
The loop assembles those values after app tick and projection reads
(`../texelotl/platform/desktop/main.c:2705`, `../texelotl/platform/desktop/main.c:2753`)
and then ORs in attach screenshot/force-render state
(`../texelotl/platform/desktop/main.c:2767`). Current ecs-ui snapshots expose
logical tree state and scale but no revision value (`include/ecs_ui/ecs_ui.h:452`,
`include/ecs_ui/ecs_ui.h:490`), so a core-only "ecs-ui knows most inputs" design
would miss the texelotl case.

**Amendments.** Make dirty gating a runner-owned signal accumulator, not a
texelotl struct clone. Use stable revision pairs `(id, label, uint64_t value)`
for app/domain state and separate transient marks for input/window/document-like
events. The runner may contribute ecs-ui-known signals later, but app-supplied
signals are primary. Define frame classification as `PARK`, `PRESENT_ONLY`, or
`RENDER_AND_PRESENT`, with explicit reasons and labels. Keep presentation-cache
policy backend-owned; texelotl's cache is tied to raylib render textures and
window scale (`../texelotl/platform/desktop/main.c:2697`), while screenshot
capture needs a defined point after blit and before frame cleanup
(`../texelotl/platform/desktop/main.c:2932`, `../texelotl/platform/desktop/main.c:2944`).

**Acceptance.** Port texelotl's `frame_dirty.c` cases into ecs-ui tests with
generic signals: first frame renders, unchanged stable signals park or
present-only according to backend policy, changed stable revision renders,
transient mark renders once, force-render renders once, screenshot request forces
render and gets its post-present hook. Tests must assert reason labels, not just
booleans.

**Effort/API.** M. API is additive. The demo can use minimal signals. texelotl
will delete `src/render/frame_dirty.c` only after it maps projection/canvas
revisions and attach force/screenshot state into runner signals.

### 3. Runner driver for raylib

**Verdict: APPROVE-WITH-CHANGES.** The problem is verified, and the open
mechanism must be resolved before implementation. In vendored raylib,
`EnableEventWaiting()` only flips `CORE.Window.eventWaiting`
(`build-raylib/_deps/raylib-src/src/rcore.c:845`), and `PollInputEvents()` maps
that flag to a bare `glfwWaitEvents()` (`build-raylib/_deps/raylib-src/src/platforms/rcore_desktop_glfw.c:1402`).
`EndDrawing()` calls `PollInputEvents()` when custom frame control is disabled
(`build-raylib/_deps/raylib-src/src/rcore.c:905`, `build-raylib/_deps/raylib-src/src/rcore.c:927`),
and raylib's default config does disable custom frame control
(`build-raylib/_deps/raylib-src/src/config.h:110`). Although raylib declares
custom frame-control entry points (`build-raylib/_deps/raylib-src/src/raylib.h:1097`),
this repo's CMake fetch/link path does not enable that option
(`CMakeLists.txt:141`, `CMakeLists.txt:165`). GLFW does expose
`glfwWaitEventsTimeout` and `glfwPostEmptyEvent`, and the latter may be called
from any thread and wakes `glfwWaitEvents`
(`build-raylib/_deps/raylib-src/src/external/glfw/include/GLFW/glfw3.h:4628`,
`build-raylib/_deps/raylib-src/src/external/glfw/include/GLFW/glfw3.h:4630`).

**Amendments.** Implement the raylib/GLFW park path as a watcher thread plus
`glfwPostEmptyEvent`, guarded by backend capability checks. The watcher blocks in
`poll`/`ppoll` on registered POSIX fds plus an internal post pipe/eventfd and the
nearest deadline. On fd readiness, post, or deadline expiry it records the wake
reason and calls `glfwPostEmptyEvent`; the main thread remains in raylib's normal
`glfwWaitEvents` path and wakes without waiting for user input. Do not choose
"single-thread `glfwWaitEventsTimeout` with poll-integrated fds" as the primary
design: GLFW wait and POSIX fd wait are separate blocking APIs, so that version
either spins on short timeouts or misses fd wakeups. Add a compile-time fallback
that disables fd wake sources, loudly, when GLFW post is unavailable.

The runner callback shape must preserve texelotl's frame phases: attach pump
before input/tick (`../texelotl/platform/desktop/main.c:2571`), render when dirty,
screenshot after presentation blit (`../texelotl/platform/desktop/main.c:2932`),
and after-frame cleanup after `EndDrawing()` (`../texelotl/platform/desktop/main.c:2943`,
`../texelotl/platform/desktop/main.c:2944`). `Step` and `Run` should use the same
phase implementation; `Step` returns the classification, wake reason, counters,
and whether callbacks requested another immediate step.

**Acceptance.** Migrate the in-tree raylib demo loop to `Run` or the shared
`Step` loop. Add a headless/mock runner test for phase ordering and classification.
Add a POSIX fd wake test with a socketpair/pipe that writes while parked and
asserts service without synthetic user input. Add a post-from-worker-thread test
and a deadline wake test. The raylib bridge must still pass existing scale parity
tests from dx-hardening stage 1.

**Effort/API.** L. API is new but architecture-shaping. Demo migration is
breaking only inside the repo. texelotl migration will be breaking at the host
loop boundary but should not require changing app UI code if hooks cover the
phases above.

### 4. Loop observability

**Verdict: APPROVE-WITH-CHANGES.** The problem is real. texelotl added
`TEXELOTL_DESKTOP_STATS` and counters because the idle-burn failure was otherwise
invisible (`../texelotl/platform/desktop/main.c:2534`,
`../texelotl/platform/desktop/main.c:2551`), then reports rendered/blit-only
frames from inside the loop (`../texelotl/platform/desktop/main.c:2771`). The
plan's proposed counters are necessary but too small for the next regression.

**Amendments.** Stats must include cumulative and one-second-window counts for
rendered, presented-only, parked, wake-by-fd, wake-by-post, wake-by-deadline,
force-render, and park failures/fallbacks. Include last park exit reason, last
render reason, blocked duration, active wake-source labels, and a truncation flag
if the source list is too long. Expose stats through both the runner API and the
per-step return; env-var stderr reporting should be a thin consumer of that API.

**Acceptance.** Tests assert idle parks dominate, animation/deadline produces
deadline wakes at the expected cadence, pending/post attribution names the source,
fd attribution names the source, and fallback/no-backend capability increments a
visible counter instead of silently spinning. The eventual texelotl attach `state`
command should include these stats, but that downstream migration is not a
blocker for this repo.

**Effort/API.** M, not S, because useful observability must be wired through the
registry, classification, backend capability fallback, and `Step` result. Public
API is additive. The demo should expose or log it only if the runner's env-var
reporting is enabled.

### Global review

**Verdict: APPROVE-WITH-CHANGES.** These are the right foundation items, but the
plan is not independent of dx-hardening stages 2-5. The final dx contract puts
coordinate semantics first, then focus resolution, frame-event application,
custom-node rect enrichment, and debug dumping (`docs/plans/dx-hardening.md:320`).
The host runner should assume stage 1 coordinates are done; it should consume
the focus resolver and `EcsUiApplyFrameEvents` rather than inventing its own
event-ordering contract; it should let custom-node layout rect enrichment replace
texelotl's canvas-bounds render feedback path; and item 4 should share labels and
snapshot references with `EcsUiTreeDebugDumpSnapshot`.

The plan also mischaracterizes one texelotl detail: selection outline animation
does mark the frame dirty when committed, floating, or preview selection is
visible (`../texelotl/platform/desktop/main.c:2723`), but `keep_loop_hot` omits
`selection_preview_visible` (`../texelotl/platform/desktop/main.c:2738`), so the
current "selection-dash wake hack" is incomplete. The runner's animation/deadline
source should fix that class of bug instead of preserving the exact current
logic.

**Sequencing amendment.** Implement in this order: (1) registry core plus mock
park tests; (2) raylib/GLFW bounded wake shim and shared `Step` skeleton; (3)
frame classification/dirty signal API; (4) full `Run` migration for the demo;
(5) observability wired throughout. Interleave dx stages by depending on, not
duplicating, the focus resolver/event helper/debug dump as they land.

**Non-goals.** Multi-window, fixed-timestep simulation, attach protocol
standardization, and Emscripten implementation remain correct non-goals. Clarify
the frame-rate non-goal: the runner owns park deadlines and idle wake policy, but
not a new game-style frame limiter beyond raylib/vsync/backend timing.

## Review summary

**Verdicts.**

- Item 1, wake-source registry: APPROVE-WITH-CHANGES; effort L; additive API,
  texelotl migration deletes attach veto/hot-loop policy.
- Item 2, frame lifecycle and dirty gating: APPROVE-WITH-CHANGES; effort M;
  use app revision pairs plus transient marks, not a caller-owned struct clone.
- Item 3, raylib runner driver: APPROVE-WITH-CHANGES; effort L; use
  watcher-thread `poll`/post plus `glfwPostEmptyEvent` for bounded fd/post/deadline
  wake latency.
- Item 4, loop observability: APPROVE-WITH-CHANGES; effort M; stats must expose
  wake reasons, blocked durations, active source labels, and fallback counters.

**Answered open questions.**

1. Park mechanism: watcher thread waits on fds/post/deadline and calls
   `glfwPostEmptyEvent`; this wakes raylib's `glfwWaitEvents` with latency bounded
   by kernel wake plus GLFW event dispatch, not by the next user input.
2. Threading: fd/deadline registration and interest changes are loop-thread-only;
   `EcsUiWakePost` is any-thread; handles are generation-checked.
3. `Step`: owns the same classification and phases as `Run` and returns
   classification, wake/render reasons, counters, and immediate-step request.
4. Windows: document POSIX fd sources as POSIX-only for this pass. Windows gets
   post/deadline sources now; HANDLE/WSA wake sources are future work.
5. Signal API: use `(id, label, uint64_t revision)` stable pairs plus transient
   marks. This enables attribution and avoids baking app structs into core.

**Missing foundation requirements.**

- Backend capability reporting: tests and stats must say when fd wake is
  unavailable instead of silently degrading to a hot loop.
- Phase contract for external harnesses: pre-input pump, tick, render, post-blit
  screenshot, and after-present cleanup must be named hooks.
- Idle-burn regression test: a runner harness must assert parked frames dominate
  at idle and fail if a backend falls back to present-every-frame without an
  explicit capability counter.

**Ready for implementation:** yes, if this review section supersedes the item
text. Recommended stage order: registry core, raylib wake shim plus `Step`
skeleton, dirty classification, demo `Run` migration, observability hardening.

## Final implementation contract

All review amendments accepted; the review section supersedes item text. The
plan author pins the remaining decisions:

**Decision A (park mechanism, item 3): watcher thread + `glfwPostEmptyEvent`.**
The watcher blocks in `poll`/`ppoll` on registered fds, an internal post
pipe/eventfd, and the nearest deadline; on wake it records the reason and posts
an empty GLFW event. Single-thread `glfwWaitEventsTimeout` variants are
rejected per review. Backends without a post primitive disable fd wake sources
loudly (capability counter + stats flag), never by silently going hot.

**Decision B (signals, item 2): `(id, label, uint64_t revision)` stable pairs
plus transient marks.** Labels are required API, not debug-only; frame
classification is `PARK` / `PRESENT_ONLY` / `RENDER_AND_PRESENT` with reasons.

**Decision C (threading, item 1): fd/deadline registration and interest
changes loop-thread-only; `EcsUiWakePost` any-thread; generation-checked
handles.** POSIX-only fd sources this pass; Windows gets post/deadline now,
HANDLE/WSA wake later.

**Decision D (phase contract, item 3): named hooks are part of the public
runner API** — pre-input pump, tick, render, post-blit screenshot,
after-present cleanup — sufficient for texelotl's attach harness to live
entirely outside the runner. `Step` returns classification, wake/render
reasons, counters, and immediate-step request; `Run` is a loop over the same
implementation.

**Stage order (per review):**

1. Wake-source registry core + mock park tests (labels, generations,
   coalescing, deadline ordering, fd read/write interest).
2. raylib/GLFW bounded wake shim + shared `Step` skeleton (fd socketpair wake
   test, post-from-thread test, deadline test, phase-ordering test).
3. Frame classification / dirty-signal API (port frame_dirty matrix; reason
   labels asserted).
4. Demo `Run` migration (in-tree loops delete; scale parity stays green).
5. Observability hardening (wake-reason counters, blocked durations, fallback
   counters, idle-burn regression test: parked frames dominate at idle or the
   test fails).

**dx-hardening interleave:** depend on, never duplicate — consume the focus
resolver (dx item 1) and `EcsUiApplyFrameEvents` (dx item 5) as they land; let
custom-node rect enrichment (dx item 2) replace texelotl's canvas-bounds
feedback; share labels/snapshot references with the debug dump (dx item 4).

**Consumer migration (final stage, in texelotl):** adopt `Run` (or `Step`
first), register wake sources (auth, genai, authoring, attach fds with
read/write interest), delete `src/render/frame_dirty.c`, the presentation
cache, stats code, `keep_loop_hot`/`event_waiting_allowed`, and the
selection-dash wake hack — including the latent `selection_preview_visible`
omission the review found at `main.c:2738`. Acceptance: full ctest both repos,
attach drive instant while parked, idle GPU ~0% asserted via runner counters
through the attach `state` command.
