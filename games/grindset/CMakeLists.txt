add_executable(
    svanes_game_grindset
    main.cpp
    grindset.cpp
    road.cpp
)
target_link_libraries(svanes_game_grindset PRIVATE Svanes::Engine)
