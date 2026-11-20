add_executable(svanes_game_marching_orders
    client/client_main.cpp
    client/marching_orders_client.cpp
    client/marching_orders_client.hpp
)
target_link_libraries(svanes_game_marching_orders PRIVATE Svanes::Engine)

add_executable(svanes_game_marching_orders_server
    server/server_main.cpp
    server/marching_orders_server.cpp
    server/marching_orders_server.hpp
)
target_link_libraries(svanes_game_marching_orders_server PRIVATE Svanes::Engine)
