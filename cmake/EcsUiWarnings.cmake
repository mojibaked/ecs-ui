function(ecs_ui_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Wstrict-prototypes
                -Wmissing-prototypes)
    endif()
endfunction()

