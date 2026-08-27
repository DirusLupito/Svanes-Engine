cmake_minimum_required(VERSION 3.25)

project(
    SvanesEngine
    VERSION 0.1.0
    DESCRIPTION "A small SDL-based game engine"
    LANGUAGES CXX
)

include(FetchContent)

# Fetch third-party sources into a persistent thirdparty/ directory instead of
# the build tree, so they survive `just clean`/deleting out/ and aren't
# re-downloaded on every configure.
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/thirdparty" CACHE PATH "Third-party dependency cache" FORCE)

# Keep the first build self-contained: SDL is pinned and built as a static
# dependency instead of requiring every developer to install it globally.
set(SDL_SHARED OFF CACHE BOOL "Build SDL as a shared library")
set(SDL_STATIC ON CACHE BOOL "Build SDL as a static library")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL test library")
set(SDL_TESTS OFF CACHE BOOL "Build the SDL tests")

FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
    URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3)

add_library(svanes_engine STATIC)
add_library(Svanes::Engine ALIAS svanes_engine)

target_sources(
    svanes_engine
    PRIVATE
        src/application.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/svanes/application.hpp
)

target_compile_features(svanes_engine PUBLIC cxx_std_23)
target_link_libraries(svanes_engine PUBLIC SDL3::SDL3)

if(MSVC)
    target_compile_options(svanes_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(svanes_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(svanes_sandbox apps/sandbox/main.cpp)
target_link_libraries(svanes_sandbox PRIVATE Svanes::Engine)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT svanes_sandbox)

set_target_properties(
    svanes_engine svanes_sandbox
    PROPERTIES
        CXX_EXTENSIONS OFF
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES
    include/svanes/application.hpp
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES
    src/application.cpp
)
