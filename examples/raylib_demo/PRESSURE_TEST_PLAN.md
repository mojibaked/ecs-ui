# Raylib Demo Pressure Test Plan

This demo exists to prove whether `ecs-ui` can support Glowfish-style UI before
we try to integrate it into `glowfish-mobile`. Each phase should keep the UI
tree retained: app state changes should add, update, or remove only the affected
entities.

## Current Baseline

- UI hierarchy is authored once with `VStack`, `HStack`, `Button`, `Icon`, and
  `Text`.
- `Button` actions use `(EcsUiOnClick, ActionEntity)`.
- Clicking `add item` submits `(EcsUiPresentRouteRequest, DemoAddItemRoute)`.
- The add-item route materializes a retained presentation subtree; `create item`
  submits `DemoAddItemRequest` and then dismisses the presentation.
- A scheduled app system creates `DemoItem` entities.
- UI systems materialize retained item rows from `DemoItem` state.
- Item row buttons carry `(EcsUiOnClick, SelectItemAction)` plus
  `(DemoUiForItem, item)`.
- Row controls submit relationship-backed request entities such as
  `(DemoSelectItemRequest, item)` and `(DemoDeleteItemRequest, item)`.
- App state stores `(DemoSelectedItem, item)` on `DemoSelection`.
- Selected row styling is projected from `(DemoSelectedItem, item)` onto row UI
  buttons.
- Navigation state stores `(EcsUiActivePresentation, presentation)` on
  `EcsUiNavigation`; each presentation stores `(EcsUiPresentationRoute, route)`.

## Phase 1: Relationship-Backed Row Actions

Prove that reusable UI nodes can carry both an action and entity-specific
context without custom event payload structs for every button.

- Add row-level actions such as `SelectItemAction`, `DeleteItemAction`, and
  `MoveItemUpAction`.
- Attach context with relationships:

  ```c
  ecs_add_pair(world, row_button, EcsUiOnClick, SelectItemAction);
  ecs_add_pair(world, row_button, DemoUiForItem, item);
  ```

- Resolve the clicked item from `event.node` by reading `(DemoUiForItem, *)`.
- Submit app requests like `DemoSelectItemRequest { item }` or represent the
  request itself as an entity with relationships.

Definition of done:

- [x] Clicking a row selects exactly that item.
- [x] Clicking row controls mutates only the related item.
- [x] The event bridge is generic enough to avoid string matching on node ids.

## Phase 2: Selection Through Relationships

Selection should be modeled as ECS relationships, not copied booleans on rows or
items.

- Add a singleton/root entity such as `DemoSelection`.
- Store selection as:

  ```c
  ecs_add_pair(world, DemoSelection, DemoSelectedItem, item);
  ```

- Make `DemoSelectedItem` exclusive so there is at most one selected item.
- Observe selection changes and update row UI state/style.
- Keep row identity linked with `(DemoItemUiNode, row)`, `(DemoItemSelectUiNode,
  select_button)`, and `(DemoUiForItem, item)`.

Definition of done:

- [x] Selecting item B removes selected styling from item A.
- [x] Deleting the selected item clears or retargets the selection.
- [x] Selection survives row label/style updates without rebuilding the list.

## Phase 3: List Mutation And Ordering

Prove ordered children and app-state ordering work together.

- Add delete, rename, and reorder actions.
- Keep app order through Flecs ordered children under `DemoItems`.
- Keep UI row order through Flecs ordered children under `ItemList`.
- Use systems to apply minimal row moves instead of rebuilding the
  entire list.

Definition of done:

- [x] Rows stay visually ordered after repeated add/delete/reorder operations.
- [x] `DemoItems` child order and `ItemList` child order agree.
- [x] No orphan UI rows remain after item deletion.

## Phase 4: Navigation And Presentation

Model a small Glowfish-like route surface.

- Add a home screen and an `Add Item` sheet or full-screen route.
- Represent route definitions and active presentation state in ECS.
- Use button actions such as `PresentRouteAction` and `DismissRouteAction`.
- Keep the static shell retained while presentations appear and disappear as
  entities.

Definition of done:

- [x] Opening and dismissing a sheet changes ECS navigation state first.
- [x] UI systems/materializers project navigation state into retained UI nodes.
- [x] Dismissing a presentation removes its UI subtree cleanly.

## Phase 5: Animation Channels

Prove the demo can consume Glowfish-style animated values from ECS.

- Add `EcsUiAnimatedFloat`, `EcsUiLinear1f`, or a small spring component as the
  equivalent of Glowfish's animation components.
- Animate sheet offset, scrim opacity, selected-row highlight, and row insert
  opacity/height.
- Let the renderer read animated values from the UI snapshot or from linked ECS
  entities.

Current progress:

- [x] `EcsUiAnimatedFloat` and `EcsUiLinear1f` advance from `ecs_progress`.
- [x] Presentation sheet offset and opacity are projected through `EcsUiVisual`.
- [x] Row insert opacity/offset is projected through `EcsUiVisual`.
- [x] Selected-row highlight is projected through `EcsUiVisual`.
- [x] Exit animation completion deletes the presentation and retained UI subtree.

Definition of done:

- [x] Animations advance from `ecs_progress`.
- [x] UI rendering reflects animated ECS values without reauthoring the tree each
  frame.
- [x] Completed transition entities/channels are cleaned up.

## Phase 6: Interactive Gesture

Pressure-test pointer state beyond simple clicks.

- [x] Drag a sheet down to update an interactive transition entity.
- [x] On release, commit or cancel based on distance/velocity.
- [x] Reuse action/request flow for begin, update, and end drag events.
- [x] Pointer capture keeps the release attached to the pressed handle.
- [x] Release requests carry drag distance and vertical velocity.

Definition of done:

- [x] Dragging directly changes the rendered sheet offset.
- [x] Canceling springs back; committing dismisses and cleans up the presentation.
- [x] Pointer capture prevents unrelated controls from receiving the same gesture.

## Phase 7: Text Input

Prove text fields can map to Glowfish's focus and request patterns.

- [x] Add a text field to the sheet.
- [x] Use ECS relationships for focus, for example `(EcsUiFocusedTextField,
  text_field_entity)`.
- [x] Submit insert/delete/focus/blur requests from input events.
- [x] Update text nodes through scheduled projection systems when field state
  changes.
- [x] Use the text field value to create named demo items.

Definition of done:

- [x] Focus, blur, typing, and deletion are represented in ECS.
- [x] Field visuals are driven by focus/editing state.
- [x] The event bridge remains renderer-agnostic.

## Phase 8: Custom Widget Escape Hatch

Glowfish needs widgets that are not just stacks, text, icons, and buttons.

- [x] Add a custom node kind for a fake terminal viewport.
- [x] Let layout reserve space through normal UI components.
- [x] Let the raylib renderer dispatch custom drawing based on an ECS component
  or node kind.
- [x] Keep terminal-specific state in the demo, attached to the custom UI node.

Definition of done:

- [x] Custom nodes participate in layout and hit testing.
- [x] Custom rendering does not leak Glowfish-specific code into core `ecs-ui`.
- [x] The API shape suggests how `glowfish-mobile` would host terminal viewport and
  soft keyboard widgets.

## Phase 9: Clay Adapter Compatibility

The raylib demo should not become a renderer-only design.

- [x] Keep the UI tree snapshot expressive enough for the Clay adapter.
- [x] Add a Clay raylib executable that emits from the same ECS UI tree.
- [x] Collect Clay pointer events at the adapter edge and feed the existing demo
  event bridge.
- [x] Represent custom nodes in Clay without leaking renderer-only state into
  core components.

Definition of done:

- [x] A retained ECS UI tree can be read by both raylib and Clay-oriented code.
- [x] Renderer-specific event data stays at the edge.
- [x] Core `ecs-ui` remains reusable by Glowfish and other projects.

## Phase 10: Projection Glue Layer

The demo has enough app-state-to-UI-state glue to prove the model, but the
pattern needs structure before using it in `glowfish-mobile`.

- [x] Define a small projection API for retained UI subtrees owned by app
  entities.
- [x] Standardize source-to-UI relationships such as projection root, projected
  source, and named projected child nodes.
- [x] Replace demo-specific row lifecycle glue with the projection helpers.
- [x] Provide a reusable ordered-children sync helper for source order to UI
  order.
- [x] Keep event context relationship-backed so actions can resolve the app
  entity without string matching.

Definition of done:

- [x] Creating an app entity materializes its retained UI subtree once.
- [x] Updating source components mutates only the affected projected UI nodes.
- [x] Deleting an app entity deletes its projected UI subtree with no orphan
  nodes.
- [x] Reordering source entities updates UI order through shared projection code.
- [x] The demo makes clear which glue is generic projection infrastructure and
  which glue remains app-specific policy.

## Phase 11: Reusable Sub-Libraries

Move proven demo concepts into focused `ecs-ui` libraries only where the shape is
general enough for Glowfish and other projects.

- [x] Extract animation primitives into an `ecs-ui-animation` layer that can
  drive `EcsUiVisual` without depending on raylib or Clay.
- [x] Extract navigation primitives into an `ecs-ui-navigation` layer for route
  definitions, active presentations, presentation hosts, and present/dismiss
  requests.
- [x] Evaluate whether text input belongs in a reusable layer or should remain a
  demo/app integration pattern for now.
- [x] Keep renderer adapters separate from app-state libraries.
- [x] Update CMake targets so sub-libraries can be adopted independently.

Definition of done:

- [x] The raylib demo consumes extracted navigation and animation APIs instead
  of demo-local copies.
- [x] Public headers expose renderer-agnostic ECS components, tags, and helper
  functions/systems.
- [x] Demo-specific route names, item state, and UI copy remain in the demo.
- [x] `glowfish-mobile` can choose projection/navigation/animation pieces
  without taking the raylib demo.
- [x] Any feature too broad for this plan is split into a dedicated design doc
  before implementation.

## Phase 12: App World / UI World Split

Pressure test whether the retained UI tree should live in a separate Flecs world
from app state.

- [x] Keep `DemoItem`, selection, app ordering, and app request systems in an
  app world.
- [x] Keep retained UI nodes, navigation, text input, animation, and renderer
  state in a UI world.
- [x] Bridge UI events to app requests through stable item ids instead of
  cross-world entity relationships.
- [x] Mirror app items into ordered UI-world source proxies before creating or
  ordering retained rows.
- [x] Run the frame as app progress, projection sync, UI progress, then tree
  read/render.

Definition of done:

- [x] App entity ids are not stored as UI relationship targets.
- [x] UI systems progress after app state has been projected into the UI world.
- [x] Add, select, rename, reorder, delete, delete-to-zero, and add-again work
  without ordered-child assertions.
- [x] The scheduling note documents the two-world bridge.

## Phase 13: Keyed Collection Projection

The item-list bridge should be a reusable library pattern, not a bespoke demo
reconciler.

- [x] Add a core `EcsUiProjectionSyncCollection` helper for stable-keyed source
  proxies.
- [x] Let callers provide source sync, retained-root build, and retained-root
  update callbacks.
- [x] Move source proxy creation, stale proxy cleanup, retained root deletion,
  and source/UI ordering into core projection code.
- [x] Keep demo policy in the demo: reading app DTOs, building item rows,
  applying selection state, and choosing row copy.
- [x] Cover create, update, reorder, stale delete, and empty collection cleanup
  in projection tests.

Definition of done:

- [x] The raylib demo item list uses the core keyed collection projection helper.
- [x] Empty collections can be synced without a dummy item array.
- [x] Delete-to-zero does not call Flecs ordered-child APIs with an empty child
  array.
- [x] The helper stays renderer-agnostic and app-agnostic.

## Phase 14: Text Field Traversal

Move the first richer text-input affordance into the reusable text-input layer.

- [x] Add focus-next and focus-previous request tags to `ecs-ui-text-input`.
- [x] Traverse ordered text fields under the text-input root, wrapping at both
  ends.
- [x] Emit focus traversal events from raylib Tab and Shift+Tab.
- [x] Keep traversal renderer-agnostic and route/app-submit behavior demo-owned.
- [x] Add a second field to the add-item sheet so traversal is visible in the
  demo.

Definition of done:

- [x] Unit tests cover next/previous traversal and wraparound.
- [x] The raylib demo can move focus between two sheet fields with Tab and
  Shift+Tab.
- [x] Existing create-item submit behavior still uses the item-name field only.
