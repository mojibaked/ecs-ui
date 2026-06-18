# Retained UI Projection Scheduling

This note captures the class of bugs found while pressure testing the raylib
demo item list. The root issue is not specific to buttons, rename, or delete.
It is about keeping a retained UI projection coherent with an ECS domain tree
while Flecs is staging mutations.

## Shape Of The Problem

The demo has two ordered trees:

- `DemoItems -> DemoItem` is app/domain state.
- `ItemList -> item row widgets` is retained UI state.

The UI tree is a projection of the app tree. Rows are not recreated every frame;
they are retained entities linked back to their source `DemoItem` with
projection relationships. This is the behavior we want for `glowfish-mobile`,
but it means add, delete, reorder, selection, and event routing all need a
clear lifecycle.

## Failure Mode

Flecs has two behaviors that interact badly when they are mixed casually:

- Most mutations inside scheduled systems are deferred until a merge point.
  Examples: `ecs_delete`, `ecs_add`, `ecs_remove`, and relationship changes.
- `ecs_set_child_order` is immediate. It validates against the parent's current
  live child set and is not deferred.

That means this shape is unsafe:

```c
ecs_delete(world, row);                 /* deferred */
ecs_set_child_order(world, list, rows);  /* immediate */
```

The row still exists in the live child set when `ecs_set_child_order` validates,
so omitting it from `rows` can assert.

This shape is also unsafe:

```c
ecs_delete(world, item);                       /* deferred */
EcsUiProjectionSyncOrderedChildren(world, ...); /* reads source order now */
```

The source item may still appear in `DemoItems` until Flecs reaches a merge
point, so projection can build UI order from stale source children.

Trying to force immediate deletion by suspending deferral inside a normal
scheduled system is also unsafe. The Flecs pipeline runs systems against a
read-only/staged world unless a system is explicitly configured for immediate
access, so mutating the real world from that context can assert.

## Current Lesson

The projection layer needs scheduling rules, not scattered cleanup patches.
The useful invariant is:

> Projection should reconcile from a coherent source snapshot into a coherent UI
> tree. It should not run while relevant source deletes/reorders are still in a
> deferred queue.

For retained UI, "logically deleted" and "actually absent from the child list"
are different states during a Flecs frame. Ordered UI sync must account for that
difference.

Phase ordering alone is not enough. Flecs inserts merge points from pipeline
access analysis, immediate-system boundaries, and explicit stage merges. An
`AppRequests -> Projection` phase relationship can describe intended order, but
it does not by itself guarantee deferred app mutations have been merged before
projection reads the source tree.

## Proposed Rules

1. Event handling creates app requests only.
2. App request systems mutate domain state.
3. A merge boundary makes deferred domain mutations visible.
4. Projection materialize/cleanup systems create missing retained rows and
   remove stale retained rows.
5. A second merge boundary makes projection tree membership visible.
6. Projection order systems call `ecs_set_child_order` through
   `EcsUiProjectionSyncOrderedChildren`; they do not create or delete the
   children they order.
7. Any system that calls immediate ordering APIs must either run after the merge
   that makes its input state coherent, or be explicitly designed as an
   immediate system with no staged-world assumptions.

The practical phase shape should look like:

```text
EventBridge -> AppRequests -> merge -> ProjectionMaterialize -> merge -> ProjectionOrder -> Cleanup
```

The raylib demos now pressure test a stricter two-world version of this rule:

```text
CollectEvents
  -> DemoUiApplyEvents(ui_world, app_world)
  -> ecs_progress(app_world)
  -> DemoUiSyncProjection(ui_world, app_world)
  -> ecs_progress(ui_world)
  -> EcsUiReadTree(ui_world)
```

`app_world` owns `DemoItem` entities, selection, ordering, and request systems.
`ui_world` owns retained UI entities, navigation, text input, animation, and
renderable state. The bridge never stores app entity ids in UI relationships.
Instead, `DemoUiSyncProjection` reads app items after `ecs_progress(app_world)`,
mirrors them into ordered UI-world `DemoUiItemSource` proxies keyed by stable
item id, creates/deletes/updates retained rows, and only then lets
`EcsUiProjectionSyncOrderedChildren` update `ItemList` order.

## What The Library Should Provide

The library should make the safe path easy:

- A projection API that maps source entities to retained UI roots and named UI
  slots.
- A delete/reconcile path for projected UI roots whose source is gone.
- An ordered-child sync function that documents its scheduling assumptions.
- A recommended phase or pipeline setup for apps that use deferred request
  systems plus retained UI projection, including where merge boundaries are
  required.
- Tests that cover delete, reorder, and delete-after-reorder for retained
  projected rows.

## Validation Findings

### Blocksmith

`../blocksmith` solves the broader scheduling problem, but it avoids the exact
retained ECS UI tree problem.

- App code submits intent and reads DTOs through adapters instead of touching
  Flecs directly.
- Requests are one-tick ECS entities and are deleted by cleanup systems.
- Ordering uses an explicit phase DAG instead of registration order.
- Outliner rows are regenerated read-model rows, not retained ECS UI entities.
  App UI state is keyed by stable ids and pruned when rows disappear.
- Tests cover same-tick ordering races, including delete-before-mutate cases.

The takeaway for `ecs-ui` is not to copy the outliner implementation directly.
The useful pattern is the separation between app intent, ECS mutation phases,
read/projection phases, and cleanup phases.

### Flecs

The local Flecs headers/source confirm the diagnosis:

- Readonly pipeline execution is stronger than ordinary deferral. Normal
  scheduled systems run against staged/read-only execution unless configured
  otherwise.
- `ecs_defer_suspend` does not flush queued work. It only suspends deferral and
  is not a general-purpose way to make pipeline work visible.
- `ecs_delete` is queued while deferred.
- `ecs_set_child_order` is explicitly immediate and validates against the
  current live children of the parent.
- Immediate systems can access the real world, but Flecs still manages deferred
  execution around them. They should be used deliberately, not as a blanket fix.
- Flecs computes merge points from system access analysis. Hidden reads/writes
  need explicit annotations or a forced boundary.

That made the earlier `ecs_defer_suspend` workaround in the raylib demo a bad
direction. It has been replaced by an explicit app-world to UI-world projection
sync, and `EcsUiProjectionSyncOrderedChildren` now refuses to call
`ecs_set_child_order` if its computed order does not cover the UI parent's live
children.

## Open Questions

- Should `ecs-ui` provide a projection phase/pipeline helper, or should it only
  document where projection systems must run?
- Should app item deletion use a "pending delete" marker and let a later
  cleanup system perform actual deletion, or should the delete request system be
  immediate?
- Should projected UI stale cleanup happen in the same projection sync or in a
  separate cleanup phase?
- Which Flecs mechanism should `ecs-ui` expose for forced boundaries:
  documented query access annotations, custom phases, explicit merges, immediate
  systems, or a small helper API?
- Is a pending-delete marker useful only for presentation/exit animations, while
  ordinary delete/reorder correctness should come from merge boundaries?

## Validation Needed

Before locking in the design, pressure test this against:

- The raylib demo reproduction:
  add several items, reorder them, rename one, delete another, delete down to
  zero, then add again.
- A scheduled-system regression where delete and projected order sync happen in
  one `ecs_progress` without stale rows or child-order assertions.
- Reorder then delete in the same frame.
- Create a missing retained row, then order it through the intended phase chain.
- Stale projected root cleanup separated from ordered sync.
- A phase/pipeline test proving projection order runs after the intended merge,
  similar to `blocksmith` phase plumbing tests.
