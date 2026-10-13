add_executable(
    svanes_game_goose
    main.cpp
    goose_game.cpp
    pause_menu.cpp
    goose_session.cpp
    peer_session.cpp
    server_session.cpp
    server_protocol.cpp
    goose.cpp
    goose_simulation.cpp
    enemy.cpp
    bullets.cpp
)
target_link_libraries(svanes_game_goose PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_goose PRIVATE
    GOOSE_ASSETS_DIR="assets/goose"
)

add_executable(
    svanes_game_goose_server
    goose_server.cpp
    goose_session.cpp
    server_protocol.cpp
    goose.cpp
    goose_simulation.cpp
    enemy.cpp
    bullets.cpp
)
target_link_libraries(svanes_game_goose_server PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_goose_server PRIVATE
    GOOSE_ASSETS_DIR="assets/goose"
)


svanes_game_package(svanes_game_goose goose goose goose)
svanes_game_package(svanes_game_goose_server goose-server goose goose)
