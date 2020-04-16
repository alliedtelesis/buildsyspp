FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.11.3
)
FetchContent_MakeAvailable(catch2)

find_package(Lua REQUIRED)

include_directories(${LUA_INCLUDE_DIR})

if(CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    setup_target_for_coverage_gcovr_html(
               NAME coverage
               BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
               EXCLUDE "unit-test/*" "test/*" "build/*")
endif()

add_executable(builddir_unittests builddir_unittests.cpp ../dir/builddir.cpp ../filesystem.cpp)
append_coverage_compiler_flags()
target_include_directories(builddir_unittests PRIVATE ../)
target_compile_features(builddir_unittests PRIVATE cxx_std_14)
target_link_libraries(builddir_unittests PRIVATE Catch2::Catch2)
add_test(NAME builddir_unittests COMMAND builddir_unittests)

add_executable(logger_unittests logger_unittests.cpp ../logger.cpp ../filesystem.cpp)
append_coverage_compiler_flags()
target_include_directories(logger_unittests PRIVATE ../)
target_compile_features(logger_unittests PRIVATE cxx_std_14)
target_link_libraries(logger_unittests PRIVATE Catch2::Catch2)
add_test(NAME logger_unittests COMMAND logger_unittests)
