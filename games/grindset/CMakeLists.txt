add_executable(
    svanes_game_grindset
    main.cpp
    grindset.cpp
    road.cpp
    phone.cpp
    car_interior.cpp
    blackjack.cpp
    casino_app.cpp
    reels_app.cpp
    reel_view.cpp
    reel_generator.cpp
    text_lines.cpp
)
target_link_libraries(svanes_game_grindset PRIVATE Svanes::Engine)
target_compile_definitions(svanes_game_grindset PRIVATE
    GRINDSET_ASSETS_DIR="assets/grindset"
)

svanes_game_package(svanes_game_grindset grindset grindset grindset)
