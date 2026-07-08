# Clay Cutover Stage 0 Inventory

Stage 0 freezes the native renderer contract and records the command inventory
that justifies the no-image native paint path.

## Target Seam

`ecs_ui_frame` is the long-term frame target. It is built from neutral frame
code and the native solver unconditionally, with optional Clay sources compiled
in only when `ECS_UI_BUILD_CLAY_ADAPTER=ON` and a Clay header is available.
The target does not link `ecs_ui_clay`; the Clay bridge target is reference-only
for direct bridge tests. This keeps the host-facing target topology available in
no-Clay builds before the later live cutover stages.

## Renderer Contract

`EcsUiPaintList` is the renderer artifact. Clay command arrays are
transition-only and exist for parity/bootstrap diffing and the temporary live
renderer path. New rendering work should consume paint items directly.

## Emitted Command Kinds

Current ecs-ui fixtures exercise these Clay command kinds:

- `CLAY_RENDER_COMMAND_TYPE_RECTANGLE`
- `CLAY_RENDER_COMMAND_TYPE_BORDER`
- `CLAY_RENDER_COMMAND_TYPE_TEXT`
- `CLAY_RENDER_COMMAND_TYPE_CUSTOM`
- `CLAY_RENDER_COMMAND_TYPE_SCISSOR_START`
- `CLAY_RENDER_COMMAND_TYPE_SCISSOR_END`

Current ecs-ui bridge code has no raw image emission path:

- `rg -n "CLAY_RENDER_COMMAND_TYPE_IMAGE|CLAY_IMAGE|imageData" src/ecs_ui_clay.c`
  returns no matches.

The executable inventory guard is `ecs_ui_clay_command_inventory`: it renders a
representative Clay-backed frame, requires the visual command kinds above, and
fails if any `CLAY_RENDER_COMMAND_TYPE_IMAGE` appears. The source grep is the
authoritative guarantee that no bridge emission path can produce a raw IMAGE
command; the runtime test keeps the current fixture inventory executable.

Texelotl source is already on the neutral frame API and names no Clay command
types. The Stage 0 host inventory grep:

- `rg -n "CLAY_RENDER_COMMAND_TYPE_IMAGE|CLAY_IMAGE|imageData|Texture2D \\*imageData" /home/mojibake/repos/texelotl --glob '!third_party/**' --glob '!build*/**'`
  returns no matches.

## IMAGE Decision

Do not add a native image paint primitive in this cut. The existing live
`EcsUiRaylibRenderDrawList` image branch stays until the Clay draw-list renderer
is deleted, but no current product tree feeds it. If a future screen proves a
real image command is needed, add a neutral paint image item before direct
renderer acceptance.

## Culling Policy

Culling is a renderer optimization, not paint or layout truth. Pixel-diff gates
run with culling disabled. Any culling-enabled path must prove identical visible
pixels against the culling-disabled path.

## Font Identity

`EcsUiPaintTextRun` carries `font_id`. Stage 0 threads the field with value `0`
to preserve the current bridge behavior while avoiding an implicit single-font
renderer contract.
