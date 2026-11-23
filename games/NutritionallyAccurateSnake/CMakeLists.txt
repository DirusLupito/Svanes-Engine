add_executable(svanes_game_nutritionally_accurate_snake
    main.cpp
    snake.cpp
    snake.hpp
)
target_link_libraries(svanes_game_nutritionally_accurate_snake PRIVATE Svanes::Engine)
