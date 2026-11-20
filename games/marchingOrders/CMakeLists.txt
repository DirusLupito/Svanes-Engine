add_executable(svanes_game_marching_orders
    client_main.cpp
    marching_orders_client.cpp
    marching_orders_client.hpp
)
target_link_libraries(svanes_game_marching_orders PRIVATE Svanes::Engine)

add_executable(svanes_game_marching_orders_server
    server_main.cpp
    marching_orders_server.cpp
    marching_orders_server.hpp
)
target_link_libraries(svanes_game_marching_orders_server PRIVATE Svanes::Engine)
