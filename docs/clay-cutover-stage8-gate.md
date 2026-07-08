# Clay Cutover Stage 8 Verification Gate

Date: 2026-07-08

This note records the hard verification gate run for Stage 8 of
`docs/plans/clay-cutover.md`. The gate is verification and classification only:
no Clay deletion happens in this stage.

## Prerequisites

- `Pillow` is required by `docs/clay-cutover-stage8-texelotl-ab.py`.
- `Xvfb` is required for raylib and texelotl screenshot runs.
- `xdotool` is required for the small-window texelotl fixture.
- `ECS_UI_FRAME_BACKEND` must be unset in the parent shell. The driver sets the
  child backend explicitly per run and fails if the parent environment is
  ambiguous.
- `/home/mojibake/source/clay/clay.h` must be present when configuring
  `texelotl/build-desktop`. The driver verifies Clay liveness by requiring
  `Clay_BeginLayout` in `build-desktop/texelotl_desktop` before trusting a Clay
  capture.

Fresh configure commands:

```sh
cmake -S /home/mojibake/repos/ecs-ui -B /home/mojibake/repos/ecs-ui/build \
  -DECS_UI_BUILD_TESTS=ON
cmake -S /home/mojibake/repos/ecs-ui -B /home/mojibake/repos/ecs-ui/build-raylib \
  -DECS_UI_BUILD_TESTS=ON \
  -DECS_UI_BUILD_RAYLIB_RENDERER=ON \
  -DECS_UI_BUILD_RAYLIB_DEMO=ON
cmake -S /home/mojibake/repos/ecs-ui -B /home/mojibake/repos/ecs-ui/build-noclay-stage0 \
  -DECS_UI_BUILD_TESTS=ON \
  -DECS_UI_BUILD_RAYLIB_RENDERER=ON \
  -DECS_UI_BUILD_CLAY_ADAPTER=OFF

cmake -S /home/mojibake/repos/texelotl -B /home/mojibake/repos/texelotl/build \
  -DTEXELOTL_ECS_UI_SOURCE=/home/mojibake/repos/ecs-ui
cmake -S /home/mojibake/repos/texelotl -B /home/mojibake/repos/texelotl/build-desktop \
  -DTEXELOTL_ECS_UI_SOURCE=/home/mojibake/repos/ecs-ui
```

## In-Repo Pixel Harness

Command:

```sh
cmake --build /home/mojibake/repos/ecs-ui/build -j
ctest --test-dir /home/mojibake/repos/ecs-ui/build --output-on-failure

cmake --build /home/mojibake/repos/ecs-ui/build-raylib -j
Xvfb :98 -screen 0 1280x800x24 >/tmp/ecs-ui-stage8-xvfb.log 2>&1 &
XVFB_PID=$!
DISPLAY=:98 ctest --test-dir /home/mojibake/repos/ecs-ui/build-raylib --output-on-failure
kill "$XVFB_PID"

cmake --build /home/mojibake/repos/ecs-ui/build-noclay-stage0 -j
ctest --test-dir /home/mojibake/repos/ecs-ui/build-noclay-stage0 --output-on-failure
```

Result:

- `build`: 11/11 passed.
- `build-raylib` under Xvfb: 16/16 passed.
- `build-noclay-stage0`: 8/8 passed.

The raylib suite includes `ecs_ui_raylib_pixel_ab`. It exercises each fixture at
scale 1 and 2, with culling off and on.

Harness comparisons:

- Clay bridge draw-list renderer vs direct `EcsUiPaintList` renderer on the same
  Clay-enriched snapshot.
- Clay-layout + paint vs native-layout + paint through the same direct renderer.
- Native backend frame run building a native paint list and rendering it through
  the direct renderer.

Fixtures:

- `pixel-composite`
- `pixel-root-z`
- `pixel-snap`
- `pixel-letter-spacing`
- `pixel-equal-z`
- `pixel-cull`

Covered behavior:

- Boxes, borders, bevel, nine-slice, custom/icon, text, text-field
  caret/selection/segments, scroll, nested clip/scissor, z-overlap, visual
  offsets, root z bases, culling on/off, scale-then-round snapping, non-zero
  letter spacing, scale 1, and scale 2.

The harness is exact-pixel; no tolerance was used.

## Texelotl Screenshot A/B Driver

Driver:

```sh
unset ECS_UI_FRAME_BACKEND
python3 /home/mojibake/repos/ecs-ui/docs/clay-cutover-stage8-texelotl-ab.py \
  --texelotl-root /home/mojibake/repos/texelotl \
  --out /tmp/texelotl-stage8-ab-fixed2 \
  --display :98 \
  --start-xvfb
```

The driver launches texelotl with native layout and with Clay layout
(`ECS_UI_FRAME_BACKEND=clay`) at scale 1 and scale 2. It drives the attach socket
with physical pointer/scroll coordinates derived from the enriched tree's
logical rects multiplied by the tree scale, captures screenshots, and exact-
compares native vs Clay for the same named state. Xvfb is started and killed by
PID; the script does not use `pkill -f`.

The driver now fails loudly for false-green conditions:

- Parent `ECS_UI_FRAME_BACKEND` set:
  `ECS_UI_FRAME_BACKEND=clay python3 docs/clay-cutover-stage8-texelotl-ab.py --self-check-only`
  exits 1 with `unset ECS_UI_FRAME_BACKEND before running the gate`.
- Non-live Clay build:
  `python3 docs/clay-cutover-stage8-texelotl-ab.py --self-check-only --clay-symbol DefinitelyMissing`
  exits 1 with the missing-symbol clay-liveness error.
- Xvfb display collision:
  starting `Xvfb :97` first, then running the driver with
  `--self-check-only --start-xvfb --display :97`, exits 1 with
  `Xvfb failed to start on :97`.

The driver also asserts that required dialog/menu nodes are present before
capturing those states, so a failed open cannot produce a meaningless zero diff.

Artifacts:

- Screenshots: `/tmp/texelotl-stage8-ab-fixed2/*-scale{1,2}/*.png`
- Per-screen state JSON:
  `/tmp/texelotl-stage8-ab-fixed2/*-scale{1,2}/states/*.json`
- Pixel report: `/tmp/texelotl-stage8-ab-fixed2/comparison-report.json`

Driven states at scale 1 and scale 2:

- `idle`: base workspace, panels, toolbar, canvas custom surface, icons.
- `canvas-zoom`: wheel over `CanvasSurface`; camera zoom changes on both
  backends.
- `toolbar-hover`: hovered toolbar button state.
- `toolbar-pressed`: pressed toolbar button state.
- `file-menu`: File menu open, floating menu panel and scrim.
- `open-dialog-top`: Open dialog, modal scrim, file-dialog list at top.
- `open-dialog-caret`: focused file-dialog path field with long text and caret
  movement.
- `open-dialog-field-drag`: pointer drag inside the path field, covering the
  text-field selection/drag path.
- `open-dialog-scroll-mid`: file-dialog scroll list after a mid scroll.
- `open-dialog-scroll-bottom`: file-dialog scroll list after a larger scroll.
- `dialog-after-blocked-middle-drag`: middle-drag over the canvas while the
  file-dialog scrim is up; camera state remains unchanged.
- `save-as-dialog`: Save As dialog.
- `export-dialog`: Export PNG dialog.
- `canvas-context-menu`: canvas secondary-click context menu.
- `palette-scroll-attempt`: wheel over the palette panel/list.
- `prompt-busy`: GenAI mock prompt submitted and busy. The selected canvas
  region is masked for the same backend-independent content reason described
  below; the busy prompt UI remains compared.
- `prompt-preview`: GenAI mock preview ready. The selected generated canvas
  region is masked because an unmasked native-vs-native run proved that region
  can differ across same-backend runs; the prompt UI chrome remains compared.
- `prompt-error`: GenAI mock failure. The same selected canvas region is masked;
  error UI chrome remains compared.
- `small-window-idle`: 640x480 resized window.
- `small-window-open-dialog`: Open dialog in the 640x480 resized window.
- `idle-after-tour`: parked/idle frame after the full interaction tour.

States not reached because the current product UI does not expose them as
distinct screens:

- Settings dialog: no settings dialog is present in the current texelotl UI.
- Menu submenus: the current File/context menus have no nested submenu.
- Multi-line text field: the current visible text fields are single-line; text
  caret/drag/selection behavior was covered on the file-dialog field and prompt
  text-field synthetics.

Screenshot A/B result:

```text
scale 1 canvas-context-menu: diff=0
scale 1 canvas-zoom: diff=0
scale 1 dialog-after-blocked-middle-drag: diff=0
scale 1 export-dialog: diff=0
scale 1 file-menu: diff=0
scale 1 idle: diff=0
scale 1 idle-after-tour: diff=0
scale 1 open-dialog-caret: diff=0
scale 1 open-dialog-field-drag: diff=0
scale 1 open-dialog-scroll-bottom: diff=0
scale 1 open-dialog-scroll-mid: diff=0
scale 1 open-dialog-top: diff=0
scale 1 palette-scroll-attempt: diff=0
scale 1 prompt-busy: diff=0
scale 1 prompt-error: diff=0
scale 1 prompt-preview: diff=0
scale 1 save-as-dialog: diff=0
scale 1 small-window-idle: diff=0
scale 1 small-window-open-dialog: diff=0
scale 1 toolbar-hover: diff=0
scale 1 toolbar-pressed: diff=0
scale 2 canvas-context-menu: diff=0
scale 2 canvas-zoom: diff=0
scale 2 dialog-after-blocked-middle-drag: diff=0
scale 2 export-dialog: diff=0
scale 2 file-menu: diff=0
scale 2 idle: diff=0
scale 2 idle-after-tour: diff=0
scale 2 open-dialog-caret: diff=0
scale 2 open-dialog-field-drag: diff=0
scale 2 open-dialog-scroll-bottom: diff=0
scale 2 open-dialog-scroll-mid: diff=0
scale 2 open-dialog-top: diff=0
scale 2 palette-scroll-attempt: diff=0
scale 2 prompt-busy: diff=0
scale 2 prompt-error: diff=0
scale 2 prompt-preview: diff=0
scale 2 save-as-dialog: diff=0
scale 2 small-window-idle: diff=0
scale 2 small-window-open-dialog: diff=0
scale 2 toolbar-hover: diff=0
scale 2 toolbar-pressed: diff=0
RESULT ZERO_DELTAS
```

Prompt-preview triage:

- Unmasked native-vs-native focused prompt run:
  scale 1 `prompt-preview` delta count 136, bbox `[380,171,449,240]`,
  first mismatch `(383,171) nativeA=(0,0,0,255) nativeB=(255,255,255,255)`.
  Scale 2 was zero in that run.
- Unmasked clay-vs-clay focused prompt run: zero deltas at scale 1 and scale 2.
- Unmasked native-vs-clay focused prompt run: zero deltas at scale 1 and scale 2.

The same-backend native delta proves the selected generated preview region is
backend-independent canvas-content variability, not a native-layout or paint
divergence. The reusable gate masks only the selected canvas region for prompt
states and still compares all prompt UI chrome.

Behavior checks from the saved state JSON:

- Canvas zoom changed identically:
  - scale 1: `zoom 7 -> 9.31700038909912`
  - scale 2: `zoom 1 -> 1.3310000896453857`
- Middle-drag behind the modal scrim did not change camera zoom or pan on either
  backend.
- Idle rendered-frame counts matched native vs Clay:
  - scale 1: `rendered=7` at initial idle and `rendered=106` after the tour.
  - scale 2: `rendered=7` at initial idle and `rendered=106` after the tour.

## Texelotl Builds and Tests

Commands:

```sh
cmake --build /home/mojibake/repos/texelotl/build -j
ctest --test-dir /home/mojibake/repos/texelotl/build --output-on-failure

cmake --build /home/mojibake/repos/texelotl/build-desktop -j
ctest --test-dir /home/mojibake/repos/texelotl/build-desktop --output-on-failure
```

Result:

- `build`: 43/43 passed.
- `build-desktop`: 45/45 passed.

## Texelotl No-Clay-Symbol Grep Gate

Command:

```sh
cd /home/mojibake/repos/texelotl
rg -n "(Clay_|EcsUiClay|CLAY_|clay\\.h|ecs_ui::clay|ecs_ui_clay|TEXELOTL_CLAY|clay_raylib)" \
  CMakeLists.txt cmake include src platform tests \
  --glob '!third_party/**' \
  --glob '!build*/**' \
  --glob '!scripts/check_guardrails.sh'
```

Result: no output. `rg` exited 1 because there were no matches.

## Delta Triage

No unmasked native-vs-Clay pixel deltas were observed outside the selected
GenAI prompt canvas content region, and that region was proven same-backend
variable by the native-vs-native focused prompt run.

- SEMANTIC deltas fixed in Stage 8: none.
- CLAY-ONLY deltas accepted in Stage 8: none.
- Backend-independent masked content variability: prompt selected canvas
  preview/selection region only.
- Unattributed deltas: zero.

The gate is green for Stage 9 deletion review.
