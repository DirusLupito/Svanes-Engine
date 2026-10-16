add_executable(svanes_game_double_time
    main.cpp
    double_time_game.cpp
)
target_link_libraries(svanes_game_double_time PRIVATE Svanes::Engine)
target_link_libraries(svanes_game_double_time PRIVATE SDL3_image::SDL3_image)
target_compile_definitions(svanes_game_double_time PRIVATE
    DOUBLE_TIME_GAME_ASSETS_DIR="assets/doubleTime"
)

add_executable(svanes_game_double_time_server server_main.cpp)
target_link_libraries(svanes_game_double_time_server PRIVATE Svanes::Engine)
target_link_libraries(svanes_game_double_time_server PRIVATE SDL3_image::SDL3_image)
target_compile_definitions(svanes_game_double_time_server PRIVATE
    DOUBLE_TIME_GAME_ASSETS_DIR="assets/doubleTime"
)

add_executable(svanes_game_double_time_input_bridge input_bridge_main.cpp)
target_link_libraries(svanes_game_double_time_input_bridge PRIVATE Svanes::Engine)

svanes_game_package(svanes_game_double_time doubleTime doubleTime doubleTime)
svanes_game_package(svanes_game_double_time_server doubleTime-server doubleTime doubleTime)
svanes_game_package(svanes_game_double_time_input_bridge doubleTime-input-bridge doubleTime doubleTime)
