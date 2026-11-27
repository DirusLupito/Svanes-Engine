add_library(svanes_marching_orders_common STATIC
    scenario.cpp
    scenario.hpp
    terrain.cpp
    terrain.hpp
    unit.cpp
    unit.hpp
    utils/influence_map.cpp
    utils/influence_map.hpp
)
target_link_libraries(svanes_marching_orders_common PUBLIC Svanes::Engine)

add_executable(svanes_game_marching_orders
    client/client_main.cpp
    client/marching_orders_client.cpp
    client/marching_orders_client.hpp
    client/terrain_image.cpp
    client/terrain_image.hpp
)
target_link_libraries(svanes_game_marching_orders PRIVATE svanes_marching_orders_common)
svanes_game_package(svanes_game_marching_orders marchingOrders marchingOrders marchingOrders)

add_executable(svanes_game_marching_orders_server
    server/server_main.cpp
    server/marching_orders_server.cpp
    server/marching_orders_server.hpp
)
target_link_libraries(svanes_game_marching_orders_server PRIVATE svanes_marching_orders_common)
add_dependencies(svanes_game_marching_orders_server svanes_game_marching_orders_assets)
