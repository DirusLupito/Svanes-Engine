cmake_minimum_required(VERSION 3.25)

project(
    SvanesEngine
    VERSION 0.1.0
    DESCRIPTION "A small SDL-based game engine"
    LANGUAGES C CXX
)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "Build SDL as a shared library")
set(SDL_STATIC ON CACHE BOOL "Build SDL as a static library")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL test library")
set(SDL_TESTS OFF CACHE BOOL "Build the SDL tests")

# We save the SDL (and other third party dependencies tbh) in the thirdparty
# folder so that they should only ever need to be downloaded once.
FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
    URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/sdl3-src"
)

set(CMAKE_POLICY_DEFAULT_CMP0219 NEW)
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

set(SDLMIXER_TESTS OFF CACHE BOOL "")
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "")
set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "")
set(SDLMIXER_AIFF OFF CACHE BOOL "")
set(SDLMIXER_VOC OFF CACHE BOOL "")
set(SDLMIXER_AU OFF CACHE BOOL "")
set(SDLMIXER_FLAC OFF CACHE BOOL "")
set(SDLMIXER_MOD OFF CACHE BOOL "")
set(SDLMIXER_MIDI OFF CACHE BOOL "")
set(SDLMIXER_OPUS OFF CACHE BOOL "")
set(SDLMIXER_WAVPACK OFF CACHE BOOL "")
set(SDLMIXER_WAVE ON CACHE BOOL "")
set(SDLMIXER_MP3 ON CACHE BOOL "")
set(SDLMIXER_MP3_DRMP3 ON CACHE BOOL "")
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "")
set(SDLMIXER_VORBIS_STB ON CACHE BOOL "")
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "")
set(SDLMIXER_GME OFF)

FetchContent_Declare(
    SDL3_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz
    URL_HASH SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/sdl3_mixer-src"
)
FetchContent_MakeAvailable(SDL3_mixer)

block(SCOPE_FOR VARIABLES)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED OFF)
    set(BUILD_STATIC ON)
    set(BUILD_TESTS OFF)
    set(ZMQ_BUILD_TESTS OFF)
    set(WITH_PERF_TOOL OFF)
    set(WITH_DOC OFF)
    set(WITH_DOCS OFF)
    set(ENABLE_CPACK OFF)
    set(ENABLE_DRAFTS OFF)
    set(WITH_LIBBSD OFF)
    set(CPPZMQ_BUILD_TESTS OFF)

    FetchContent_Declare(
        ZeroMQ
        URL https://github.com/zeromq/libzmq/releases/download/v4.3.5/zeromq-4.3.5.tar.gz
        URL_HASH SHA256=6653ef5910f17954861fe72332e68b03ca6e4d9c7160eb3a8de5a5a913bfab43
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/zeromq-src"
    )
    FetchContent_MakeAvailable(ZeroMQ)

    FetchContent_Declare(
        cppzmq
        URL https://github.com/zeromq/cppzmq/archive/refs/tags/v4.10.0.tar.gz
        URL_HASH SHA256=c81c81bba8a7644c84932225f018b5088743a22999c6d82a2b5f5cd1e6942b74
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/cppzmq-src"
    )
    FetchContent_MakeAvailable(cppzmq)
endblock()

add_library(svanes_engine STATIC)
add_library(Svanes::Engine ALIAS svanes_engine)

target_sources(
    svanes_engine
    PRIVATE
        src/application.cpp
        src/attractor_system.cpp
        src/audio/audio_manager.cpp
        src/audio/audio_manager_internal.hpp
        src/camera2d.cpp
        src/circle_geometry.cpp
        src/convex_polygon_geometry.cpp
        src/collision_system.cpp
        src/geometry.cpp
        src/input.cpp
        src/kinematic_system.cpp
        src/rectangle_geometry.cpp
        src/triangle_geometry.cpp
        src/input_manager_internal.hpp
        src/render/render_queue.cpp
        src/render/render_queue_executor.cpp
        src/render/render_queue_executor.hpp
        src/render/render_system.cpp
        src/render/texture_manager.cpp
        src/render/texture_manager_internal.hpp
        src/registry.cpp
        src/sprite_animation_system.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/svanes/application.hpp
            include/svanes/attractor_system.hpp
            include/svanes/audio/audio_manager.hpp
            include/svanes/camera2d.hpp
            include/svanes/circle_geometry.hpp
            include/svanes/convex_polygon_geometry.hpp
            include/svanes/collision_system.hpp
            include/svanes/entity.hpp
            include/svanes/game.hpp
            include/svanes/geometry.hpp
            include/svanes/input.hpp
            include/svanes/kinematic_system.hpp
            include/svanes/rectangle_geometry.hpp
            include/svanes/triangle_geometry.hpp
            include/svanes/render/basic_render_types.hpp
            include/svanes/render/render_queue.hpp
            include/svanes/render/render_system.hpp
            include/svanes/render/texture_manager.hpp
            include/svanes/registry.hpp
            include/svanes/sprite_animation_system.hpp
            include/svanes/vector2d.hpp
)

target_compile_features(svanes_engine PUBLIC cxx_std_23)
target_link_libraries(
    svanes_engine
    PUBLIC SDL3::SDL3 cppzmq-static
    PRIVATE SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
)

if(MSVC)
    target_compile_options(svanes_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(svanes_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(
    svanes_game_erik
    games/erik-placeholder/main.cpp
    games/erik-placeholder/erik_game.cpp
    games/erik-placeholder/goose.cpp
    games/erik-placeholder/enemy.cpp
    games/erik-placeholder/bullets.cpp
)
target_link_libraries(svanes_game_erik PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_erik PRIVATE
    ERIK_GAME_ASSETS_DIR="${CMAKE_SOURCE_DIR}/games/erik-placeholder/assets"
)

add_executable(
    svanes_game_orbital_escalation
    games/orbitalEscalation/main.cpp
    games/orbitalEscalation/orbital_escalation_game.cpp
)
target_link_libraries(svanes_game_orbital_escalation PRIVATE Svanes::Engine)

add_executable(svanes_game_chris games/chris-placeholder/main.cpp games/chris-placeholder/chris_game.cpp)
target_link_libraries(svanes_game_chris PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_chris PRIVATE
    CHRIS_GAME_ASSETS_DIR="${CMAKE_SOURCE_DIR}/games/chris-placeholder/assets"
)

add_executable(svanes_game_chris_server games/chris-placeholder/server_main.cpp)
target_link_libraries(svanes_game_chris_server PRIVATE Svanes::Engine)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT svanes_game_orbital_escalation)

set_target_properties(
    svanes_engine svanes_game_erik svanes_game_orbital_escalation svanes_game_chris svanes_game_chris_server
    PROPERTIES
        CXX_EXTENSIONS OFF
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header Files" FILES
    include/svanes/application.hpp
    include/svanes/attractor_system.hpp
    include/svanes/audio/audio_manager.hpp
    include/svanes/camera2d.hpp
    include/svanes/circle_geometry.hpp
    include/svanes/convex_polygon_geometry.hpp
    include/svanes/collision_system.hpp
    include/svanes/entity.hpp
    include/svanes/game.hpp
    include/svanes/geometry.hpp
    include/svanes/input.hpp
    include/svanes/kinematic_system.hpp
    include/svanes/rectangle_geometry.hpp
    include/svanes/triangle_geometry.hpp
    include/svanes/render/basic_render_types.hpp
    include/svanes/render/render_queue.hpp
    include/svanes/render/render_system.hpp
    include/svanes/render/texture_manager.hpp
    include/svanes/registry.hpp
    include/svanes/sprite_animation_system.hpp
    include/svanes/vector2d.hpp
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES
    src/application.cpp
    src/attractor_system.cpp
    src/audio/audio_manager.cpp
    src/audio/audio_manager_internal.hpp
    src/camera2d.cpp
    src/circle_geometry.cpp
    src/convex_polygon_geometry.cpp
    src/collision_system.cpp
    src/geometry.cpp
    src/input.cpp
    src/kinematic_system.cpp
    src/rectangle_geometry.cpp
    src/triangle_geometry.cpp
    src/input_manager_internal.hpp
    src/render/render_queue.cpp
    src/render/render_queue_executor.cpp
    src/render/render_queue_executor.hpp
    src/render/render_system.cpp
    src/render/texture_manager.cpp
    src/render/texture_manager_internal.hpp
    src/registry.cpp
    src/sprite_animation_system.cpp
)
