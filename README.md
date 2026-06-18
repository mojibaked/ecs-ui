# ecs-ui

`ecs-ui` is a small C library for authoring retained UI structure in a Flecs
world and exporting it as a renderer-neutral tree. Clay and raylib adapters are
provided as optional layers.

The library is intended for projects that already use:

- Flecs for retained state and relationships.
- An immediate layout/rendering layer such as Clay.
- Snapshot/read boundaries between app state and presentation code.

## Shape

```c
ecs_world_t *world = ecs_init();
EcsUiImport(world);

ecs_entity_t root = EcsUiRootEntity(world, "Home");
EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
ecs_entity_t present_add_machine =
    ecs_entity(world, {.name = "PresentAddMachineAction"});

VStack(&builder, {.id = "HomeStack", .gap = 10.0f}) {
    Button(
        &builder,
        {
            .id = "AddMachine",
            .variant = ECS_UI_BUTTON_PRIMARY,
            .on_click = present_add_machine,
        }) {
        Text(&builder, {.id = "AddLabel", .text = "add machine"});
    }
}

EcsUiBuilderEnd(&builder);

EcsUiTreeSnapshot tree = {0};
EcsUiReadTree(world, root, &tree);
```

Containers use `EcsChildOf` for hierarchy and `EcsOrderedChildren` for stable
sibling order. The exported tree is a flat preorder snapshot with
`first_child`/`next_sibling` indices, so render adapters do not need direct Flecs
access.

Click behavior is modeled as an ECS relationship:

```c
ecs_add_pair(world, button, EcsUiOnClick, present_add_machine);
```

Renderer/input adapters return generic events with the clicked node and action
entity. Application code decides what the action entity means.

## Build

Pass a Flecs single-file distribution directory:

```sh
cmake -S . -B build \
  -DECS_UI_FLECS_SOURCE=/path/to/flecs \
  -DECS_UI_CLAY_SOURCE=/path/to/clay
cmake --build build
ctest --test-dir build --output-on-failure
```

`ecs_ui` requires Flecs. `ecs_ui_clay` is optional and is skipped unless Clay is
available through `ECS_UI_CLAY_SOURCE` or an existing CMake target named `clay`.

## Raylib Demo

The raylib renderer is optional. When enabled, CMake fetches raylib 6.0 with
`FetchContent`, matching Blocksmith's dependency shape:

```sh
cmake -S . -B build-raylib \
  -DECS_UI_FLECS_SOURCE=/path/to/flecs \
  -DECS_UI_BUILD_RAYLIB_DEMO=ON
cmake --build build-raylib --target ecs_ui_raylib_demo
./build-raylib/ecs_ui_raylib_demo
```

If you already have a raylib checkout, pass
`-DECS_UI_RAYLIB_SOURCE=/path/to/raylib` instead of fetching it.

The demo also models app state in Flecs. Clicking `add item` emits the
`AddItemAction` target from `(EcsUiOnClick, AddItemAction)`, the demo submits a
`DemoAddItemRequest`, and a Flecs system creates a `DemoItem` entity. UI
observers materialize retained item rows and status text under `ItemList` from
`DemoItem` changes. The static UI shell is authored once; adding an item does
not rebuild the whole UI tree.

## Status

This is an initial scaffold. It covers core UI hierarchy authoring, ordered
children, text/button/icon/stack nodes, pair-based click action targets,
snapshot export, a minimal Clay emitter, and a simple raylib renderer/demo.
Theming, style entities, richer event routing, pruning stale authored nodes, and
install/package rules are intentionally left for follow-up slices.
