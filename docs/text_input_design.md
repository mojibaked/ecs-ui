# Text Input Boundary

`ecs-ui` should provide enough text-input behavior that apps do not rebuild
focus, editing requests, traversal, selection, and clipboard mechanics for every
project. It should not own app-specific form semantics.

## Library-Owned

The reusable text-input layer owns renderer-agnostic ECS state and requests:

- Field state: value, placeholder, future cursor and selection range.
- Focus state: one focused field under the text-input root.
- Focus requests: focus field and blur focused field.
- Editing requests: insert text/codepoint and delete backward.
- UI links: relationships from field state to retained UI nodes and from UI
  nodes back to fields.
- Future traversal: tab and shift-tab should move focus through ordered fields.
- Future clipboard: copy, cut, and paste should be represented as requests, but
  platform clipboard access stays at the renderer/app edge.

The layer may provide projection helpers for common field visuals, but it should
not require a specific renderer or widget shape.

## App-Owned

Apps still own:

- Field meaning, such as "add item name", "search query", or "username".
- Validation rules and error copy.
- Submit behavior, including whether submit creates an item, dismisses a route,
  sends a command, or does nothing.
- Route-specific lifecycle, such as clearing a field after a successful submit.
- Styling beyond generic focused/editing/disabled/error states.

## Current Slice

This slice extracts the mechanics already proven in the raylib demo:

- `EcsUiTextField`
- `EcsUiFocusedTextField`
- `EcsUiFocusTextFieldRequest`
- `EcsUiBlurTextFieldRequest`
- `EcsUiTextInsertRequest`
- `EcsUiTextDeleteRequest`
- field-to-UI and UI-to-field relationships

The demo keeps the add-item field projection and submit behavior. Cursor,
selection, copy/paste, and tab traversal remain planned follow-ups.
