add_executable(svanes_game_ghqcxx
    client_main.cpp
    ghqcxx_client.cpp
    ghqcxx_client.hpp
    PieceVisuals.cpp
    PieceVisuals.hpp
    BasicArtillery.cpp
    BasicArtillery.hpp
    HeavyArtillery.cpp
    HeavyArtillery.hpp
    ArmoredArtillery.cpp
    ArmoredArtillery.hpp
    BasicInfantry.cpp
    BasicInfantry.hpp
    ParachuteInfantry.cpp
    ParachuteInfantry.hpp
    ArmoredInfantry.cpp
    ArmoredInfantry.hpp
)
target_link_libraries(svanes_game_ghqcxx PRIVATE Svanes::Engine)

add_executable(svanes_game_ghqcxx_server
    server_main.cpp
    ghqcxx_server.cpp
    ghqcxx_server.hpp
)
target_link_libraries(svanes_game_ghqcxx_server PRIVATE Svanes::Engine)
