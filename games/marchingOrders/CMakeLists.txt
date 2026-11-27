add_executable(svanes_game_marching_orders
    client/client_main.cpp
    client/marching_orders_client.cpp
    client/marching_orders_client.hpp
    client/terrain_image.cpp
    client/terrain_image.hpp
    terrain.cpp
    terrain.hpp
)
target_link_libraries(svanes_game_marching_orders PRIVATE Svanes::Engine)
svanes_game_package(svanes_game_marching_orders marchingOrders marchingOrders marchingOrders)

add_executable(svanes_game_marching_orders_server
    terrain.cpp
    terrain.hpp
    utils/influence_map.cpp
    utils/influence_map.hpp
    unit.cpp
    unit.hpp
    server/server_main.cpp
    server/marching_orders_server.cpp
    server/marching_orders_server.hpp
)
target_link_libraries(svanes_game_marching_orders_server PRIVATE Svanes::Engine)
add_dependencies(svanes_game_marching_orders_server svanes_game_marching_orders_assets)
