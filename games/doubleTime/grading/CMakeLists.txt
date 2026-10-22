set(DOUBLE_TIME_GRADING_NETWORKING_STRATEGY "input" CACHE STRING
    "Grading networking strategy: input or client_transform")
set_property(CACHE DOUBLE_TIME_GRADING_NETWORKING_STRATEGY PROPERTY STRINGS
    input client_transform)

if (DOUBLE_TIME_GRADING_NETWORKING_STRATEGY STREQUAL "input")
    set(DOUBLE_TIME_GRADING_CLIENT_SOURCE double_time_game_input.cpp)
    set(DOUBLE_TIME_GRADING_SERVER_SOURCE server_input_main.cpp)
elseif (DOUBLE_TIME_GRADING_NETWORKING_STRATEGY STREQUAL "client_transform")
    set(DOUBLE_TIME_GRADING_CLIENT_SOURCE double_time_game_transform.cpp)
    set(DOUBLE_TIME_GRADING_SERVER_SOURCE server_transform_main.cpp)
else()
    message(FATAL_ERROR
        "DOUBLE_TIME_GRADING_NETWORKING_STRATEGY must be input or client_transform")
endif()

add_executable(svanes_game_double_time_grading
    main.cpp
    ${DOUBLE_TIME_GRADING_CLIENT_SOURCE}
)
target_link_libraries(svanes_game_double_time_grading PRIVATE Svanes::Engine)
target_link_libraries(svanes_game_double_time_grading PRIVATE SDL3_image::SDL3_image)
target_compile_definitions(svanes_game_double_time_grading PRIVATE
    DOUBLE_TIME_GAME_ASSETS_DIR="assets/doubleTime-grading"
)

add_executable(svanes_game_double_time_grading_server
    ${DOUBLE_TIME_GRADING_SERVER_SOURCE}
)
target_link_libraries(svanes_game_double_time_grading_server PRIVATE Svanes::Engine)
target_link_libraries(svanes_game_double_time_grading_server PRIVATE SDL3_image::SDL3_image)
target_compile_definitions(svanes_game_double_time_grading_server PRIVATE
    DOUBLE_TIME_GAME_ASSETS_DIR="assets/doubleTime-grading"
)

add_executable(svanes_game_double_time_grading_benchmark
    benchmark_main.cpp
)
target_link_libraries(svanes_game_double_time_grading_benchmark PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_double_time_grading_benchmark PRIVATE
    DOUBLE_TIME_GAME_ASSETS_DIR="assets/doubleTime-grading"
)

svanes_game_package(svanes_game_double_time_grading doubleTime-grading doubleTime/grading doubleTime-grading)
svanes_game_package(svanes_game_double_time_grading_server doubleTime-grading-server doubleTime/grading doubleTime-grading)
