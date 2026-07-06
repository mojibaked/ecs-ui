cmake_minimum_required(VERSION 3.24)

project(ecs_ui LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ECS_UI_BUILD_TESTS "Build ecs-ui tests" ON)
option(ECS_UI_BUILD_CLAY_ADAPTER "Build the optional Clay adapter" ON)
option(ECS_UI_BUILD_RAYLIB_RENDERER "Build the optional raylib renderer" OFF)
option(ECS_UI_BUILD_RAYLIB_DEMO "Build the raylib demo executable" OFF)
option(ECS_UI_FETCH_RAYLIB "Fetch raylib with CMake FetchContent when needed" ON)

set(ECS_UI_FLECS_SOURCE
    ""
    CACHE PATH
    "Path to a Flecs single-file distribution directory containing flecs.c and flecs.h")
set(ECS_UI_CLAY_SOURCE
    ""
    CACHE PATH
    "Path to a Clay header directory containing clay.h")
set(ECS_UI_RAYLIB_SOURCE
    ""
    CACHE PATH
    "Path to a raylib source checkout containing CMakeLists.txt")
set(ECS_UI_CLAY_RAYLIB_RENDERER_SOURCE
    ""
    CACHE PATH
    "Path to Clay's raylib renderer directory containing clay_renderer_raylib.c")

if(NOT ECS_UI_CLAY_RAYLIB_RENDERER_SOURCE AND
        EXISTS "$ENV{HOME}/source/clay/renderers/raylib/clay_renderer_raylib.c")
    set(ECS_UI_CLAY_RAYLIB_RENDERER_SOURCE
        "$ENV{HOME}/source/clay/renderers/raylib")
endif()

include(cmake/EcsUiWarnings.cmake)

if(TARGET flecs)
    set(ECS_UI_FLECS_TARGET flecs)
elseif(ECS_UI_FLECS_SOURCE AND EXISTS "${ECS_UI_FLECS_SOURCE}/flecs.c")
    add_library(ecs_ui_flecs STATIC
        "${ECS_UI_FLECS_SOURCE}/flecs.c")
    target_include_directories(ecs_ui_flecs
        PUBLIC "${ECS_UI_FLECS_SOURCE}")
    target_compile_definitions(ecs_ui_flecs
        PUBLIC
            FLECS_STATIC
            FLECS_TERM_COUNT_MAX=64)
    if(NOT MSVC)
        set_target_properties(ecs_ui_flecs PROPERTIES C_EXTENSIONS ON)
    endif()
    set(ECS_UI_FLECS_TARGET ecs_ui_flecs)
else()
    find_package(flecs CONFIG QUIET)
    if(TARGET flecs::flecs)
        set(ECS_UI_FLECS_TARGET flecs::flecs)
    elseif(TARGET flecs_static)
        set(ECS_UI_FLECS_TARGET flecs_static)
    else()
        message(FATAL_ERROR
            "Flecs not found. Pass -DECS_UI_FLECS_SOURCE=/path/to/flecs "
            "or provide an existing flecs/flecs::flecs target.")
    endif()
endif()

add_library(ecs_ui STATIC
    src/ecs_ui.c
    src/ecs_ui_gesture.c
    src/ecs_ui_overlay.c
    src/ecs_ui_projection.c
    src/ecs_ui_runner.c)
add_library(ecs_ui::ecs_ui ALIAS ecs_ui)
target_include_directories(ecs_ui
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(ecs_ui
    PUBLIC ${ECS_UI_FLECS_TARGET})
ecs_ui_enable_warnings(ecs_ui)

add_library(ecs_ui_animation STATIC
    src/ecs_ui_animation.c)
add_library(ecs_ui::animation ALIAS ecs_ui_animation)
target_link_libraries(ecs_ui_animation
    PUBLIC ecs_ui)
ecs_ui_enable_warnings(ecs_ui_animation)

add_library(ecs_ui_navigation STATIC
    src/ecs_ui_navigation.c)
add_library(ecs_ui::navigation ALIAS ecs_ui_navigation)
target_link_libraries(ecs_ui_navigation
    PUBLIC ecs_ui)
ecs_ui_enable_warnings(ecs_ui_navigation)

add_library(ecs_ui_text_input STATIC
    src/ecs_ui_text_input.c)
add_library(ecs_ui::text_input ALIAS ecs_ui_text_input)
target_link_libraries(ecs_ui_text_input
    PUBLIC ecs_ui)
ecs_ui_enable_warnings(ecs_ui_text_input)

if(ECS_UI_BUILD_CLAY_ADAPTER)
    if(TARGET clay)
        set(ECS_UI_CLAY_TARGET clay)
    elseif(ECS_UI_CLAY_SOURCE AND EXISTS "${ECS_UI_CLAY_SOURCE}/clay.h")
        add_library(ecs_ui_clay_headers INTERFACE)
        target_include_directories(ecs_ui_clay_headers
            INTERFACE "${ECS_UI_CLAY_SOURCE}")
        set(ECS_UI_CLAY_TARGET ecs_ui_clay_headers)
    else()
        message(STATUS
            "Clay not found; skipping ecs_ui_clay. "
            "Pass -DECS_UI_CLAY_SOURCE=/path/to/clay to build it.")
        set(ECS_UI_CLAY_TARGET "")
    endif()

    if(ECS_UI_CLAY_TARGET)
        add_library(ecs_ui_clay STATIC
            src/ecs_ui_clay.c)
        add_library(ecs_ui::clay ALIAS ecs_ui_clay)
        target_link_libraries(ecs_ui_clay
            PUBLIC ecs_ui ${ECS_UI_CLAY_TARGET})
        ecs_ui_enable_warnings(ecs_ui_clay)
    endif()
endif()

if(ECS_UI_BUILD_RAYLIB_DEMO)
    set(ECS_UI_BUILD_RAYLIB_RENDERER ON)
endif()

if(ECS_UI_BUILD_RAYLIB_RENDERER)
    if(TARGET raylib)
        set(ECS_UI_RAYLIB_TARGET raylib)
    elseif(ECS_UI_RAYLIB_SOURCE AND EXISTS "${ECS_UI_RAYLIB_SOURCE}/CMakeLists.txt")
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        add_subdirectory("${ECS_UI_RAYLIB_SOURCE}" "${CMAKE_BINARY_DIR}/raylib")
        set(ECS_UI_RAYLIB_TARGET raylib)
    elseif(ECS_UI_FETCH_RAYLIB)
        include(FetchContent)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 6.0
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(raylib)
        set(ECS_UI_RAYLIB_TARGET raylib)
    else()
        find_package(raylib CONFIG QUIET)
        if(TARGET raylib)
            set(ECS_UI_RAYLIB_TARGET raylib)
        elseif(TARGET raylib::raylib)
            set(ECS_UI_RAYLIB_TARGET raylib::raylib)
        else()
            message(FATAL_ERROR
                "raylib not found. Pass -DECS_UI_RAYLIB_SOURCE=/path/to/raylib "
                "or enable -DECS_UI_FETCH_RAYLIB=ON.")
        endif()
    endif()

    find_package(Threads REQUIRED)
    add_library(ecs_ui_raylib STATIC
        src/ecs_ui_raylib.c
        src/ecs_ui_raylib_runner.c)
    add_library(ecs_ui::raylib ALIAS ecs_ui_raylib)
    target_link_libraries(ecs_ui_raylib
        PUBLIC ecs_ui ${ECS_UI_RAYLIB_TARGET}
        PRIVATE Threads::Threads)
    if(TARGET glfw)
        # The bounded park shim wakes raylib's glfwWaitEvents path by posting an
        # empty GLFW event from its watcher thread when vendored GLFW is exposed.
        target_link_libraries(ecs_ui_raylib PRIVATE glfw)
        target_compile_definitions(ecs_ui_raylib
            PRIVATE ECS_UI_RAYLIB_HAS_GLFW_POST=1)
    endif()
    ecs_ui_enable_warnings(ecs_ui_raylib)

    if(ECS_UI_BUILD_RAYLIB_DEMO)
        set(ECS_UI_RAYLIB_DEMO_COMMON_SOURCES
            examples/raylib_demo/demo_anim.c
            examples/raylib_demo/demo_app.c
            examples/raylib_demo/demo_nav.c
            examples/raylib_demo/demo_terminal.c
            examples/raylib_demo/demo_text_input.c
            examples/raylib_demo/demo_theme.c
            examples/raylib_demo/demo_ui.c
            examples/raylib_demo/demo_ui_action_button.c
            examples/raylib_demo/demo_ui_build.c
            examples/raylib_demo/demo_ui_events.c
            examples/raylib_demo/demo_ui_item_row_widget.c
            examples/raylib_demo/demo_ui_items.c)

        add_executable(ecs_ui_raylib_demo
            ${ECS_UI_RAYLIB_DEMO_COMMON_SOURCES}
            examples/raylib_demo/main.c)
        target_link_libraries(ecs_ui_raylib_demo
            PRIVATE
                ecs_ui_raylib
                ecs_ui_animation
                ecs_ui_navigation
                ecs_ui_text_input)
        ecs_ui_enable_warnings(ecs_ui_raylib_demo)

        if(TARGET ecs_ui_clay AND
                ECS_UI_CLAY_RAYLIB_RENDERER_SOURCE AND
                EXISTS "${ECS_UI_CLAY_RAYLIB_RENDERER_SOURCE}/clay_renderer_raylib.c")
            add_executable(ecs_ui_clay_raylib_demo
                ${ECS_UI_RAYLIB_DEMO_COMMON_SOURCES}
                examples/raylib_demo/clay_main.c
                examples/raylib_demo/clay_raylib_bridge.c)
            target_include_directories(ecs_ui_clay_raylib_demo
                PRIVATE "${ECS_UI_CLAY_RAYLIB_RENDERER_SOURCE}")
            target_link_libraries(ecs_ui_clay_raylib_demo
                PRIVATE
                    ecs_ui_clay
                    ecs_ui_raylib
                    ecs_ui_animation
                    ecs_ui_navigation
                    ecs_ui_text_input)
        else()
            message(STATUS
                "Clay raylib renderer not found; skipping "
                "ecs_ui_clay_raylib_demo. Pass "
                "-DECS_UI_CLAY_RAYLIB_RENDERER_SOURCE=/path/to/clay/renderers/raylib.")
        endif()
    endif()
endif()

if(ECS_UI_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)

    add_executable(ecs_ui_basic
        tests/ecs_ui_basic.c)
    target_link_libraries(ecs_ui_basic
        PRIVATE
            ecs_ui
            ecs_ui_animation
            ecs_ui_navigation
            ecs_ui_text_input)
    ecs_ui_enable_warnings(ecs_ui_basic)
    add_test(NAME ecs_ui_basic COMMAND ecs_ui_basic)

    add_executable(ecs_ui_wake_registry
        tests/ecs_ui_wake_registry.c)
    target_link_libraries(ecs_ui_wake_registry
        PRIVATE
            ecs_ui
            ecs_ui_animation
            Threads::Threads)
    ecs_ui_enable_warnings(ecs_ui_wake_registry)
    add_test(NAME ecs_ui_wake_registry COMMAND ecs_ui_wake_registry)

    add_executable(ecs_ui_frame_classifier
        tests/ecs_ui_frame_classifier.c)
    target_link_libraries(ecs_ui_frame_classifier
        PRIVATE ecs_ui)
    ecs_ui_enable_warnings(ecs_ui_frame_classifier)
    add_test(NAME ecs_ui_frame_classifier COMMAND ecs_ui_frame_classifier)

    if(TARGET ecs_ui_raylib)
        add_executable(ecs_ui_raylib_runner
            tests/ecs_ui_raylib_runner.c)
        target_link_libraries(ecs_ui_raylib_runner
            PRIVATE
                ecs_ui_raylib
                Threads::Threads)
        ecs_ui_enable_warnings(ecs_ui_raylib_runner)
        add_test(NAME ecs_ui_raylib_runner COMMAND ecs_ui_raylib_runner)

        add_executable(ecs_ui_raylib_presentation_cache
            tests/ecs_ui_raylib_presentation_cache.c)
        target_link_libraries(ecs_ui_raylib_presentation_cache
            PRIVATE ecs_ui_raylib)
        ecs_ui_enable_warnings(ecs_ui_raylib_presentation_cache)
        add_test(
            NAME ecs_ui_raylib_presentation_cache
            COMMAND ecs_ui_raylib_presentation_cache)
    endif()

    if(TARGET ecs_ui_clay)
        add_executable(ecs_ui_clay_parity
            tests/ecs_ui_clay_parity.c)
        target_link_libraries(ecs_ui_clay_parity
            PRIVATE ecs_ui_clay)
        ecs_ui_enable_warnings(ecs_ui_clay_parity)
        add_test(NAME ecs_ui_clay_parity COMMAND ecs_ui_clay_parity)
    endif()
endif()
