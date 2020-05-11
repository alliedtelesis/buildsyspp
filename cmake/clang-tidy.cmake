find_program(CLANG_TIDY NAMES clang-tidy clang-tidy-6.0)
if (CLANG_TIDY)
    find_package(Lua REQUIRED)
    add_custom_target(
            clang-tidy
            COMMAND ${CLANG_TIDY}
            ${SOURCE_FILES}
            -quiet -extra-arg-before=-xc++
            --
            -std=c++14
            -I ${CMAKE_SOURCE_DIR}/src
            -I ${CMAKE_SOURCE_DIR}/src/include
            -I ${LUA_INCLUDE_DIR}
    )
endif ()