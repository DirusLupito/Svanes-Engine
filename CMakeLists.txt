cmake_minimum_required(VERSION 3.25)

project(
    SvanesEngine
    VERSION 0.1.0
    DESCRIPTION "A small SDL-based game engine"
    LANGUAGES CXX
)

include(FetchContent)

# Keep the first build self-contained: SDL is pinned and built as a static
# dependency instead of requiring every developer to install it globally.
set(SDL_SHARED OFF CACHE BOOL "Build SDL as a shared library")
set(SDL_STATIC ON CACHE BOOL "Build SDL as a static library")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL test library")
set(SDL_TESTS OFF CACHE BOOL "Build the SDL tests")

# The extracted source is cached in thirdparty/ so it survives `just clean`
# and deleted build trees without being re-downloaded. The subbuild/build
# directories stay under the (per-preset, generator-specific) binary
# directory, since those aren't safe to share across generators/compilers.
FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
    URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/sdl3-src"
)
FetchContent_MakeAvailable(SDL3)

set(BUILD_SHARED_LIBS OFF)
set(SDLIMAGE_AVIF OFF CACHE BOOL "")
set(SDLIMAGE_BMP OFF CACHE BOOL "")
set(SDLIMAGE_GIF OFF CACHE BOOL "")
set(SDLIMAGE_JPG OFF CACHE BOOL "")
set(SDLIMAGE_JXL OFF CACHE BOOL "")
set(SDLIMAGE_LBM OFF CACHE BOOL "")
set(SDLIMAGE_PCX OFF CACHE BOOL "")
set(SDLIMAGE_PNG ON CACHE BOOL "")
set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "")
set(SDLIMAGE_PNM OFF CACHE BOOL "")
set(SDLIMAGE_QOI OFF CACHE BOOL "")
set(SDLIMAGE_SVG OFF CACHE BOOL "")
set(SDLIMAGE_TGA OFF CACHE BOOL "")
set(SDLIMAGE_TIF OFF CACHE BOOL "")
set(SDLIMAGE_WEBP OFF CACHE BOOL "")
set(SDLIMAGE_XCF OFF CACHE BOOL "")
set(SDLIMAGE_XPM OFF CACHE BOOL "")
set(SDLIMAGE_XV OFF CACHE BOOL "")

FetchContent_Declare(
    SDL3_image
    URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.4/SDL3_image-3.4.4.tar.gz
    URL_HASH SHA256=29751304a13d25ac513f24305fa25b06a6edd9607718c90129b8350d35fc5573
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/sdl3_image-src"
)
FetchContent_MakeAvailable(SDL3_image)

add_library(svanes_engine STATIC)
add_library(Svanes::Engine ALIAS svanes_engine)

target_sources(
    svanes_engine
    PRIVATE
        src/application.cpp
        src/input.cpp
        src/registry.cpp
        src/sprite_animation_system.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/svanes/application.hpp
            include/svanes/entity.hpp
            include/svanes/game.hpp
            include/svanes/input.hpp
            include/svanes/registry.hpp
            include/svanes/sprite_animation_system.hpp
)

target_compile_features(svanes_engine PUBLIC cxx_std_23)
target_link_libraries(svanes_engine PUBLIC SDL3::SDL3)

if(MSVC)
    target_compile_options(svanes_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(svanes_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(svanes_sandbox apps/sandbox/main.cpp apps/sandbox/sandbox_game.cpp)
target_link_libraries(svanes_sandbox PRIVATE Svanes::Engine)

add_executable(svanes_game_erik games/erik-placeholder/main.cpp games/erik-placeholder/erik_game.cpp)
target_link_libraries(svanes_game_erik PRIVATE Svanes::Engine SDL3_image::SDL3_image)

add_custom_command(
    TARGET svanes_game_erik POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/games/erik-placeholder/assets/darkworld_spawn_swirlingorb_idle.png"
        "$<TARGET_FILE_DIR:svanes_game_erik>/darkworld_spawn_swirlingorb_idle.png"
)

add_executable(svanes_game_alex games/alex-placeholder/main.cpp games/alex-placeholder/alex_game.cpp)
target_link_libraries(svanes_game_alex PRIVATE Svanes::Engine)

add_executable(svanes_game_chris games/chris-placeholder/main.cpp games/chris-placeholder/chris_game.cpp)
target_link_libraries(svanes_game_chris PRIVATE Svanes::Engine)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT svanes_sandbox)

set_target_properties(
    svanes_engine svanes_sandbox svanes_game_erik svanes_game_alex svanes_game_chris
    PROPERTIES
        CXX_EXTENSIONS OFF
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES
    include/svanes/application.hpp
    include/svanes/entity.hpp
    include/svanes/game.hpp
    include/svanes/input.hpp
    include/svanes/registry.hpp
    include/svanes/sprite_animation_system.hpp
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES
    src/application.cpp
    src/input.cpp
    src/registry.cpp
    src/sprite_animation_system.cpp
)
