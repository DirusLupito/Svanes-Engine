if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    get_filename_component(compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(SVANES_DUMPBIN NAMES dumpbin HINTS "${compiler_directory}" REQUIRED)
endif()

function(svanes_game_package target name source_folder asset_folder)
    set(asset_source "${CMAKE_SOURCE_DIR}/games/${source_folder}/assets")
    add_custom_target(${target}_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${asset_source}" "$<TARGET_FILE_DIR:${target}>/assets/${asset_folder}"
        VERBATIM)
    add_dependencies(${target} ${target}_assets)

    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/packages/${name}-$<CONFIG>.cmake"
        CONTENT "set(package_name [==[${name}]==])
set(package_executable [==[$<TARGET_FILE:${target}>]==])
set(package_assets [==[${asset_source}]==])
set(package_asset_folder [==[${asset_folder}]==])
set(package_runtime [==[${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}]==])
set(package_dlls [==[$<TARGET_RUNTIME_DLLS:${target}>]==])
set(package_output [==[${CMAKE_SOURCE_DIR}/out/releases]==])
set(package_staging [==[${CMAKE_BINARY_DIR}/packages/staging]==])
set(package_config [==[$<CONFIG>]==])
set(package_windows [==[${WIN32}]==])
set(package_arch [==[${CMAKE_SIZEOF_VOID_P}]==])
set(package_system [==[${CMAKE_SYSTEM_NAME}]==])
set(package_processor [==[${CMAKE_SYSTEM_PROCESSOR}]==])
set(package_objdump [==[${CMAKE_OBJDUMP}]==])
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL dumpbin)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND [==[${SVANES_DUMPBIN}]==])
include([==[${CMAKE_SOURCE_DIR}/scripts/package-game.cmake]==])
")
    add_custom_target(package-${name}
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/packages/${name}-$<CONFIG>.cmake"
        DEPENDS ${target}
        VERBATIM)
endfunction()
