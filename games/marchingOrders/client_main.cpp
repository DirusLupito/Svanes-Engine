#include "marching_orders_client.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <exception>
#include <iostream>

int main(int argc, char *argv[]) {
    try {
        if (argc > 2) {
            std::cerr << "Usage: marching_orders [server IPv4:port]\n";
            return 1;
        }

        marching_orders::MarchingOrdersClient client;
        svanes::NetworkClient application(
            {.title = "marchingOrders", .width = 1280, .height = 720});
        application.Connect(argc == 2 ? argv[1] : "127.0.0.1:5811");
        const auto result = application.run(client);
        application.Disconnect();
        return result;
    } catch (const std::exception &error) {
        std::cerr << "marchingOrders Error: " << error.what() << '\n';
        return 1;
    }
}
