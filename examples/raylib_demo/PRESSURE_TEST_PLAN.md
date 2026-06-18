# Raylib Demo Pressure Test Plan

This demo exists to prove whether `ecs-ui` can support Glowfish-style UI before
we try to integrate it into `glowfish-mobile`. Each phase should keep the UI
tree retained: app state changes should add, update, or remove only the affected
entities.

## Current Baseline

- UI hierarchy is authored once with `VStack`, `HStack`, `Button`, `Icon`, and
  `Text`.
- `Button` actions use `(EcsUiOnClick, ActionEntity)`.
- Clicking `add item` submits `DemoAddItemRequest`.
- A scheduled app system creates `DemoItem` entities.
- UI observers materialize retained item rows from `DemoItem` changes.
- Item row buttons carry `(EcsUiOnClick, SelectItemAction)` plus
  `(DemoUiForItem, item)`.
- Row controls submit relationship-backed request entities such as
  `(DemoSelectItemRequest, item)` and `(DemoDeleteItemRequest, item)`.
- App state stores `(DemoSelectedItem, item)` on `DemoSelection`.
- Selected row styling is projected from `(DemoSelectedItem, item)` onto row UI
  buttons.

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
- Use observers/systems to apply minimal row moves instead of rebuilding the
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

- Opening and dismissing a sheet changes ECS navigation state first.
- UI observers/materializers project navigation state into retained UI nodes.
- Dismissing a presentation removes its UI subtree cleanly.

## Phase 5: Animation Channels

Prove the demo can consume Glowfish-style animated values from ECS.

- Add `DemoAnimatedFloat`, `DemoSpring1f`, and `DemoLinear1f` or import a small
  equivalent of Glowfish's animation components.
- Animate sheet offset, scrim opacity, selected-row highlight, and row insert
  opacity/height.
- Let the renderer read animated values from the UI snapshot or from linked ECS
  entities.

Definition of done:

- Animations advance from `ecs_progress`.
- UI rendering reflects animated ECS values without reauthoring the tree each
  frame.
- Completed transition entities/channels are cleaned up.

## Phase 6: Interactive Gesture

Pressure-test pointer state beyond simple clicks.

- Drag a sheet down to update an interactive transition entity.
- On release, commit or cancel based on distance/velocity.
- Reuse action/request flow for begin, update, and end drag events.

Definition of done:

- Dragging directly changes the rendered sheet offset.
- Canceling springs back; committing dismisses and cleans up the presentation.
- Pointer capture prevents unrelated controls from receiving the same gesture.

## Phase 7: Text Input

Prove text fields can map to Glowfish's focus and request patterns.

- Add a text field to the sheet.
- Use ECS relationships for focus, for example `(DemoFocusedField,
  text_field_entity)`.
- Submit insert/delete/focus/blur requests from input events.
- Update text nodes through observers when field state changes.

Definition of done:

- Focus, blur, typing, and deletion are represented in ECS.
- Field visuals are driven by focus/editing state.
- The event bridge remains renderer-agnostic.

## Phase 8: Custom Widget Escape Hatch

Glowfish needs widgets that are not just stacks, text, icons, and buttons.

- Add a custom node kind for a fake terminal viewport.
- Let layout reserve space through normal UI components.
- Let the raylib renderer dispatch custom drawing based on an ECS component or
  node kind.

Definition of done:

- Custom nodes participate in layout and hit testing.
- Custom rendering does not leak Glowfish-specific code into core `ecs-ui`.
- The API shape suggests how `glowfish-mobile` would host terminal viewport and
  soft keyboard widgets.

## Phase 9: Clay Adapter Compatibility

The raylib demo should not become a renderer-only design.

- Keep the UI tree snapshot expressive enough for the Clay adapter.
- Add at least one pressure-test path that emits Clay from the same ECS UI tree.
- Avoid raylib-only state in core components.

Definition of done:

- A retained ECS UI tree can be read by both raylib and Clay-oriented code.
- Renderer-specific event data stays at the edge.
- Core `ecs-ui` remains reusable by Glowfish and other projects.
