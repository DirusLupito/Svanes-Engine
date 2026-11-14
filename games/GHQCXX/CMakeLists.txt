add_executable(svanes_game_ghqcxx
    client_main.cpp
    ghqcxx_client.cpp
    ghqcxx_client.hpp
    pieces/PieceVisuals.cpp
    pieces/PieceVisuals.hpp
    pieces/Headquarters.cpp
    pieces/Headquarters.hpp
    pieces/BasicArtillery.cpp
    pieces/BasicArtillery.hpp
    pieces/HeavyArtillery.cpp
    pieces/HeavyArtillery.hpp
    pieces/ArmoredArtillery.cpp
    pieces/ArmoredArtillery.hpp
    pieces/BasicInfantry.cpp
    pieces/BasicInfantry.hpp
    pieces/ParachuteInfantry.cpp
    pieces/ParachuteInfantry.hpp
    pieces/ArmoredInfantry.cpp
    pieces/ArmoredInfantry.hpp
)
target_link_libraries(svanes_game_ghqcxx PRIVATE Svanes::Engine)

add_executable(svanes_game_ghqcxx_server
    server_main.cpp
    ghqcxx_server.cpp
    ghqcxx_server.hpp
)
target_link_libraries(svanes_game_ghqcxx_server PRIVATE Svanes::Engine)
