include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.11.3
)
FetchContent_MakeAvailable(catch2)

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${LUA_INCLUDE_DIR})

option(CODE_COVERAGE "" OFF)
if(CMAKE_COMPILER_IS_GNUCXX AND CODE_COVERAGE)
    include(CodeCoverage)
    setup_target_for_coverage_gcovr_html(
               NAME coverage
               BASE_DIRECTORY "${PROJECT_SOURCE_DIR}/src"
               EXCLUDE "unit-test/*" "functional-test/*" "build/*")
    append_coverage_compiler_flags()
endif()

add_library(filesystem OBJECT ../src/filesystem.cpp)
add_library(builddir OBJECT ../src/dir/builddir.cpp)
add_library(logger OBJECT ../src/logger.cpp)
add_library(packagecmd OBJECT ../src/packagecmd.cpp)
add_library(buildinfo OBJECT ../src/buildinfo.cpp)
add_library(lua OBJECT ../src/lua.cpp)
add_library(hash OBJECT ../src/hash.cpp)
add_library(featuremap OBJECT ../src/featuremap.cpp)
add_library(namespace OBJECT ../src/namespace.cpp)
add_library(package OBJECT ../src/package.cpp)
add_library(extraction OBJECT ../src/extraction.cpp)
add_library(fetch OBJECT ../src/fetch.cpp)
add_library(interface_builddir OBJECT ../src/interface/builddir.cpp)
add_library(interface_luainterface OBJECT ../src/interface/luainterface.cpp)
add_library(interface_toplevel OBJECT ../src/interface/toplevel.cpp)
add_library(interface_fetchunit OBJECT ../src/interface/fetchunit.cpp)
add_library(extraction_git OBJECT ../src/extraction/git.cpp)

add_executable(builddir_unittests builddir_unittests.cpp $<TARGET_OBJECTS:builddir> $<TARGET_OBJECTS:filesystem>)
target_include_directories(builddir_unittests PRIVATE ../src/)
target_compile_features(builddir_unittests PRIVATE cxx_std_14)
target_link_libraries(builddir_unittests PRIVATE Catch2::Catch2)
target_link_libraries(builddir_unittests PRIVATE ${LUA_LIBRARIES})
target_link_libraries(builddir_unittests PRIVATE Threads::Threads)
target_link_libraries(builddir_unittests PRIVATE OpenSSL::Crypto)
add_test(NAME builddir_unittests COMMAND builddir_unittests)

add_executable(logger_unittests logger_unittests.cpp $<TARGET_OBJECTS:logger> $<TARGET_OBJECTS:filesystem>)
target_include_directories(logger_unittests PRIVATE ../src/)
target_compile_features(logger_unittests PRIVATE cxx_std_14)
target_link_libraries(logger_unittests PRIVATE Catch2::Catch2)
add_test(NAME logger_unittests COMMAND logger_unittests)

add_executable(packagecmd_unittests packagecmd_unittests.cpp $<TARGET_OBJECTS:packagecmd> $<TARGET_OBJECTS:logger> $<TARGET_OBJECTS:filesystem>)
target_include_directories(packagecmd_unittests PRIVATE ../src/)
target_compile_features(packagecmd_unittests PRIVATE cxx_std_14)
target_link_libraries(packagecmd_unittests PRIVATE Catch2::Catch2)
target_link_libraries(packagecmd_unittests PRIVATE Threads::Threads)
add_test(NAME packagecmd_unittests COMMAND packagecmd_unittests)

add_executable(buildinfo_unittests buildinfo_unittests.cpp $<TARGET_OBJECTS:buildinfo>)
target_include_directories(buildinfo_unittests PRIVATE ../src/)
target_compile_features(buildinfo_unittests PRIVATE cxx_std_14)
target_link_libraries(buildinfo_unittests PRIVATE Catch2::Catch2)
add_test(NAME buildinfo_unittests COMMAND buildinfo_unittests)

add_executable(hash_unittests hash_unittests.cpp $<TARGET_OBJECTS:hash> $<TARGET_OBJECTS:packagecmd> $<TARGET_OBJECTS:logger> $<TARGET_OBJECTS:filesystem>)
target_include_directories(hash_unittests PRIVATE ../src/)
target_compile_features(hash_unittests PRIVATE cxx_std_14)
target_link_libraries(hash_unittests PRIVATE Catch2::Catch2)
target_link_libraries(hash_unittests PRIVATE OpenSSL::Crypto)
target_link_libraries(hash_unittests PRIVATE Threads::Threads)
add_test(NAME hash_unittests COMMAND hash_unittests)

add_executable(exceptions_unittests exceptions_unittests.cpp)
target_include_directories(exceptions_unittests PRIVATE ../src/)
target_compile_features(exceptions_unittests PRIVATE cxx_std_14)
target_link_libraries(exceptions_unittests PRIVATE Catch2::Catch2)
add_test(NAME exceptions_unittests COMMAND exceptions_unittests)

add_executable(lua_unittests lua_unittests.cpp $<TARGET_OBJECTS:lua> $<TARGET_OBJECTS:filesystem>)
target_include_directories(lua_unittests PRIVATE ../src/)
target_compile_features(lua_unittests PRIVATE cxx_std_14)
target_link_libraries(lua_unittests PRIVATE Catch2::Catch2)
target_link_libraries(lua_unittests PRIVATE ${LUA_LIBRARIES})
add_test(NAME lua_unittests COMMAND lua_unittests)

add_executable(featuremap_unittests featuremap_unittests.cpp $<TARGET_OBJECTS:featuremap>)
target_include_directories(featuremap_unittests PRIVATE ../src/)
target_compile_features(featuremap_unittests PRIVATE cxx_std_14)
target_link_libraries(featuremap_unittests PRIVATE Catch2::Catch2)
add_test(NAME featuremap_unittests COMMAND featuremap_unittests)

add_executable(namespace_unittests namespace_unittests.cpp $<TARGET_OBJECTS:namespace> $<TARGET_OBJECTS:lua>
                                   $<TARGET_OBJECTS:package> $<TARGET_OBJECTS:logger> $<TARGET_OBJECTS:packagecmd>
                                   $<TARGET_OBJECTS:hash> $<TARGET_OBJECTS:builddir> $<TARGET_OBJECTS:filesystem>
                                   $<TARGET_OBJECTS:buildinfo> $<TARGET_OBJECTS:extraction> $<TARGET_OBJECTS:interface_luainterface>
                                   $<TARGET_OBJECTS:interface_toplevel> $<TARGET_OBJECTS:featuremap> $<TARGET_OBJECTS:interface_builddir>
                                   $<TARGET_OBJECTS:fetch> $<TARGET_OBJECTS:extraction_git> $<TARGET_OBJECTS:interface_fetchunit>)
target_include_directories(namespace_unittests PRIVATE ../src/)
target_compile_features(namespace_unittests PRIVATE cxx_std_14)
target_link_libraries(namespace_unittests PRIVATE Catch2::Catch2)
target_link_libraries(namespace_unittests PRIVATE OpenSSL::Crypto)
target_link_libraries(namespace_unittests PRIVATE ${LUA_LIBRARIES})
target_link_libraries(namespace_unittests PRIVATE Threads::Threads)
add_test(NAME namespace_unittests COMMAND namespace_unittests)

add_executable(toplevel_unittests toplevel_unittests.cpp $<TARGET_OBJECTS:namespace> $<TARGET_OBJECTS:lua>
                                   $<TARGET_OBJECTS:package> $<TARGET_OBJECTS:logger> $<TARGET_OBJECTS:packagecmd>
                                   $<TARGET_OBJECTS:hash> $<TARGET_OBJECTS:builddir> $<TARGET_OBJECTS:filesystem>
                                   $<TARGET_OBJECTS:buildinfo> $<TARGET_OBJECTS:extraction> $<TARGET_OBJECTS:interface_luainterface>
                                   $<TARGET_OBJECTS:interface_toplevel> $<TARGET_OBJECTS:featuremap> $<TARGET_OBJECTS:interface_builddir>
                                   $<TARGET_OBJECTS:fetch> $<TARGET_OBJECTS:extraction_git> $<TARGET_OBJECTS:interface_fetchunit>)
target_include_directories(toplevel_unittests PRIVATE ../src/)
target_compile_features(toplevel_unittests PRIVATE cxx_std_14)
target_link_libraries(toplevel_unittests PRIVATE Catch2::Catch2)
target_link_libraries(toplevel_unittests PRIVATE OpenSSL::Crypto)
target_link_libraries(toplevel_unittests PRIVATE ${LUA_LIBRARIES})
target_link_libraries(toplevel_unittests PRIVATE Threads::Threads)
add_test(NAME toplevel_unittests COMMAND toplevel_unittests)

add_executable(package_unittests package_unittests.cpp $<TARGET_OBJECTS:namespace> $<TARGET_OBJECTS:lua>
                                 $<TARGET_OBJECTS:package> $<TARGET_OBJECTS:logger> $<TARGET_OBJECTS:packagecmd>
                                 $<TARGET_OBJECTS:hash> $<TARGET_OBJECTS:builddir> $<TARGET_OBJECTS:filesystem>
                                 $<TARGET_OBJECTS:buildinfo> $<TARGET_OBJECTS:extraction> $<TARGET_OBJECTS:interface_luainterface>
                                 $<TARGET_OBJECTS:interface_toplevel> $<TARGET_OBJECTS:featuremap> $<TARGET_OBJECTS:interface_builddir>
                                 $<TARGET_OBJECTS:fetch> $<TARGET_OBJECTS:extraction_git> $<TARGET_OBJECTS:interface_fetchunit>)
target_include_directories(package_unittests PRIVATE ../src/)
target_compile_features(package_unittests PRIVATE cxx_std_14)
target_link_libraries(package_unittests PRIVATE Catch2::Catch2)
target_link_libraries(package_unittests PRIVATE OpenSSL::Crypto)
target_link_libraries(package_unittests PRIVATE ${LUA_LIBRARIES})
target_link_libraries(package_unittests PRIVATE Threads::Threads)
add_test(NAME package_unittests COMMAND package_unittests)
