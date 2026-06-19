# Phase 29: Glowfish Mobile Integration Spike

## Purpose

This spike defines how `ecs-ui` should be introduced into `glowfish-mobile`
without replacing the parts Glowfish already owns well. The goal is to prove
the library boundary, not to rewrite the app shell in one step.

The main constraint from Glowfish is already clear:

- `glowfish_core` owns durable app state in a Flecs world.
- `GlowfishUiBuildFrame` currently reads core snapshots and returns a
  `GlowfishUiActionList`.
- `src/app/ui_shell.c` is an immediate Clay builder and manual action collector.
- terminal viewport and soft keyboard widgets are custom Glowfish surfaces.

That shape maps well to an `ecs-ui` UI-world projection layer. It does not argue
for moving Glowfish domain state into `ecs-ui`.

## Relevant Glowfish Boundaries

Current Glowfish files that matter for the spike:

- `include/glowfish_mobile/ui.h` exposes `GlowfishUiRuntime`,
  `GlowfishUiInput`, `GlowfishUiActionList`, and `GlowfishUiBuildFrame`.
- `src/app/ui_shell.c` builds the Clay UI, emits render commands, and collects
  pointer actions from Clay ids.
- `src/app/clay_runtime.c` owns the Clay arena/context and pointer setup.
- `src/app/terminal_viewport_widget.*` reserves UI space, emits terminal render
  surfaces, and collects resize/pan actions.
- `src/app/terminal_soft_keyboard_widget.*` owns terminal-key UI behavior.
- `src/core/glowfish.c` progresses the core app world and refreshes navigation,
  lifecycle, and terminal viewport leases after `ecs_progress`.
- `navigation-ecs-plan.md` already defines Glowfish navigation as a first-class
  core ECS graph.

The important architectural seam is snapshot/action based. UI code receives
`GlowfishAppSnapshot`, `GlowfishMachineListSnapshot`,
`GlowfishNavigationSnapshot`, text-field snapshots, and terminal snapshots. It
returns typed UI actions that platform/app code can submit back into core.

## Recommended Initial Shape

Use two Flecs worlds at first:

```text
Glowfish core world
  durable app, navigation, machine, terminal, and request state

Glowfish UI world
  retained ecs-ui tree, style tokens, text-field views, hit-test state,
  app-authored widgets, and renderer-adapter state
```

The frame should keep the same external contract as `GlowfishUiBuildFrame`:

```text
GlowfishTick(core_world)
  -> read core snapshots
  -> sync snapshots into ui_world projections
  -> progress ui_world systems
  -> read ecs-ui tree snapshot
  -> emit Clay layout/render commands
  -> collect renderer-neutral ecs-ui events
  -> route reusable text-input events
  -> translate app actions into GlowfishUiActionList
```

The UI world should store stable keys from Glowfish snapshots, not raw core
`ecs_entity_t` ids. That keeps `ecs-ui` reusable and avoids cross-world entity
relationships.

## What Maps Cleanly To ecs-ui

`ecs-ui` core:

- retained UI hierarchy with `EcsChildOf` and ordered children;
- `VStack`, `HStack`, `ZStack`, `Text`, `Icon`, `Custom`, and `Pressable`;
- renderer-neutral tree snapshots;
- click/drag/text events with action entities and node ids;
- `EcsUiHitTest` for capture, pass-through, and children-only hit policy.

Projection:

- use `EcsUiProjectionCollectionBuffer` and
  `EcsUiProjectionSyncCollectionView` for snapshot-backed lists like machines,
  tabs, terminal sessions, or settings rows;
- keep app policy in Glowfish code: route choice, copy, selected/connected
  state, and action translation;
- retain UI rows and update only changed projected nodes.

Style and theme:

- define Glowfish semantic style tokens such as `ShellPanel`, `TopBar`,
  `PrimaryAction`, `SubtleAction`, `DangerAction`, `TextField`,
  `MachineRow`, `BottomTab`, `Sheet`, and `Scrim`;
- provide both `EcsUiBoxStyle` and `EcsUiTextStyle` values for tokens that
  represent interactive widgets, so app-authored pressables control foreground
  as well as background treatment;
- keep light/dark mode as active theme state in the UI world unless Glowfish
  later needs theme as durable app state;
- let app-authored widgets attach tokens to `Pressable` and container nodes.

Text input:

- use `ecs_ui_text_input` for field editing behavior where the UI world can own
  the field, especially add-machine/settings forms;
- bridge final form submissions to `GlowfishUiActionList`;
- keep terminal input separate, since terminal keystrokes are domain input, not
  generic text-field editing.

Custom widgets:

- model terminal viewport and soft keyboard as custom escape hatches, not as
  generic `ecs-ui` widgets;
- reserve layout through `EcsUiCustom`;
- let the Glowfish Clay bridge translate those custom nodes into existing
  terminal widget declaration, surface emission, resize, pan, and key actions.

## Button Direction

Buttons should be app-authored widgets over `EcsUiPressable`.

`ecs-ui` should keep `Pressable` as the low-level primitive for hit testing,
disabled state, style token attachment, and action relationships. Glowfish can
then define its own widgets:

- `GlowfishActionButton`
- `GlowfishIconButton`
- `GlowfishBottomTabButton`
- `GlowfishMachineRow`
- `GlowfishSheetHandle`

Those widgets can compose `Pressable`, `Text`, and `Icon`, attach Glowfish style
tokens, and decide which action entity or app context relationships they need.
The existing core `Button` can remain as compatibility/demo sugar, but it should
not be the long-term semantic widget model for Glowfish.

## Navigation Boundary

Glowfish core navigation should remain the source of truth.

`ecs-ui-navigation` is still useful as a proving ground and possibly as a
library for simpler apps, but `glowfish-mobile` already has a richer navigation
plan and implementation:

- bottom tabs with preserved stacks;
- presentation host and layered presentations;
- route lifecycle and visibility;
- terminal viewport leases;
- back handling and interactive dismiss requests.

For the first integration, project `GlowfishNavigationSnapshot` into UI-world
nodes. Do not make `ecs-ui-navigation` decide which Glowfish route is active.
The UI world can still own visual presentation state if it is derived from the
core snapshot, but route policy belongs to core.

## First Spike Slice

Start with the add-machine bottom sheet, not the terminal route.

Why this route:

- it exercises presentation layering, scrim capture, text input, theme tokens,
  app-authored buttons, and form submit;
- it avoids the terminal viewport's native surface complexity;
- it has clear current behavior in `ui_shell.c` for comparison.

Suggested implementation steps:

1. Add `ecs-ui` as a direct sibling dependency of Glowfish Mobile.
2. Add an `ecs_world_t *ui_world` and retained root handles inside
   `GlowfishUiRuntime`.
3. Import the needed `ecs-ui` modules into that UI world.
4. Define Glowfish UI style tokens and a minimal dark theme.
5. Build the static shell and add-machine sheet as retained UI entities.
6. Project `GlowfishNavigationSnapshot` so the sheet subtree is present only
   when core says the add-machine sheet is visible.
7. Use `ecs_ui_text_input` for the host field in the UI world.
8. Translate create/dismiss/focus outcomes into existing `GlowfishUiActionList`
   entries.
9. Emit through Clay using the existing `GlowfishClayRuntime` context.

Glowfish Mobile is new enough that this should be a direct port, not a
compile-flagged alternate path. Tests can keep narrow helper entry points where
useful, but production UI should converge on the ECS path instead of maintaining
parallel immediate-mode and retained-mode implementations.

Current integration status:

- `GlowfishUiRuntime` owns an imported `ecs-ui` UI world.
- `glowfish_app` links `ecs-ui` directly through the sibling checkout.
- `src/app/ecs_ui_bridge.h` defines the Glowfish custom-node kind strings for
  terminal viewport and soft keyboard nodes.
- The custom-node bridge decodes Clay custom render commands back into
  `EcsUiTreeNodeSnapshot` data and resolved bounds. This keeps native terminal
  surfaces app-owned while letting `ecs-ui` own layout.

## Acceptance Criteria

The first Glowfish spike is useful if it proves:

- the public `GlowfishUiBuildFrame` contract can stay snapshot/action based;
- the add-machine sheet renders through Clay from an `ecs-ui` tree;
- scrim/sheet hit testing prevents background actions from firing;
- text input focus, blur, caret movement, selection, paste, and submit do not
  require app-authored editing code;
- create and dismiss actions become the same `GlowfishUiAction` records as the
  current immediate UI;
- light/dark or token value changes update visuals without rebuilding the tree;
- no terminal viewport or soft-keyboard behavior regresses because those remain
  on the old/custom path.

## Risks To Resolve

Clay ownership:

- Glowfish already owns a Clay context and render-packet conversion.
- The `ecs-ui` Clay adapter must either emit into that active context or expose
  enough lower-level hooks for Glowfish's existing render-packet path.

Frame ordering:

- current UI actions are collected after Clay layout.
- if UI-world text input mutates immediately, decide whether form text is UI
  state submitted on action or mirrored back into core text-field state.
- avoid making core systems depend on UI-world entities during `ecs_progress`.

Navigation duplication:

- do not run independent core and UI navigation sources of truth.
- UI retained presentation nodes should project from core navigation snapshots.

Custom nodes:

- terminal viewport requires render-surface emission, resize collection, pan
  collection, and visibility gating.
- this should remain a custom-node bridge instead of becoming a generic
  `ecs-ui` terminal widget.
- the current bridge contract is: `EcsUiCustom.kind` selects the app surface,
  the Clay adapter provides final bounds through `CLAY_RENDER_COMMAND_TYPE_CUSTOM`,
  and Glowfish render/input code maps those bounds to existing terminal
  viewport or soft-keyboard logic.

Performance:

- projection helpers should use stable keys and bounded buffers.
- avoid per-frame full tree rebuilds for machines, tabs, or terminal rows.
- keep mobile allocations out of the hot frame path where practical.

## Non-Goals

- Replacing Glowfish core navigation with `ecs-ui-navigation`.
- Moving terminal glyph buffers, native clients, or GPU objects into `ecs-ui`.
- Making `Button` a core semantic widget.
- Rewriting every route before the add-machine sheet proves the boundary.
- Requiring Glowfish app systems to query the UI world.

## Follow-Up Slices

After the sheet spike:

1. Project the machine list with `EcsUiProjectionCollectionBuffer`.
2. Replace bottom tabs with app-authored `Pressable` widgets over projected
   `GlowfishBottomTabsSnapshot`.
3. Bridge terminal viewport as an `EcsUiCustom` node that delegates to the
   existing Glowfish terminal widget code.
4. Decide whether Glowfish core text-field systems should remain domain-owned
   or be simplified around `ecs_ui_text_input`.
5. Add regression tests that compare old and new `GlowfishUiActionList`
   behavior for the sheet and machine list.
