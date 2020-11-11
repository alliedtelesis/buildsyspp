cmake_minimum_required(VERSION 3.1...3.15)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(Buildsys++ LANGUAGES CXX)

option(BUILD_TESTING "" OFF)
include(CTest)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

file(GLOB_RECURSE SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/src/*.cpp 
    ${CMAKE_SOURCE_DIR}/src/*.hpp
    ${CMAKE_SOURCE_DIR}/src/*.h)

include(cpplint)
include(clang-format)
include(clang-tidy)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenSSL REQUIRED)

if(BUILD_TESTING)
    add_subdirectory(unit-test)
else()
    FILE(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    add_executable(buildsyspp ${sources})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)
    include_directories(${LUA_INCLUDE_DIR})
    target_link_libraries(buildsyspp PRIVATE Threads::Threads)
    target_link_libraries(buildsyspp PRIVATE ${LUA_LIBRARIES})
    target_link_libraries(buildsyspp PRIVATE OpenSSL::Crypto)
    target_link_libraries(buildsyspp PRIVATE util)
endif()

add_subdirectory(functional-test)

