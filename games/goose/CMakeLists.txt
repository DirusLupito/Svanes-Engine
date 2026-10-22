add_executable(
    svanes_game_goose
    goose_client.cpp
    pause_menu.cpp
    protocol.cpp
    arena.cpp
)
target_link_libraries(svanes_game_goose PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_goose PRIVATE
    GOOSE_ASSETS_DIR="assets/goose"
)

add_executable(
    svanes_game_goose_server
    goose_server.cpp
    protocol.cpp
    arena.cpp
    goose.cpp
    enemy.cpp
    bullets.cpp
    minions.cpp
)
target_link_libraries(svanes_game_goose_server PRIVATE Svanes::Engine)


svanes_game_package(svanes_game_goose goose goose goose)
svanes_game_package(svanes_game_goose_server goose-server goose goose)
