#include "marching_orders_server.hpp"

#include <svanes/network/clientserver/network_server.hpp>

#include <iostream>

int main(int argc, char *argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: marching_orders_server [listen IPv4:port]\n";
        return 1;
    }

    marching_orders::MarchingOrdersServer server;
    svanes::NetworkServer application;
    application.Listen(argc == 2 ? argv[1] : "127.0.0.1:5811");
    return application.run(server);
}
