#include "ghqcxx_client.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <iostream>

int main(int argc, char *argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: ghqcxx [server IPv4:port]\n";
        return 1;
    }

    ghqcxx::GHQCXXClient client;
    svanes::NetworkClient application(
        {.title = "GHQCXX", .width = 1280, .height = 720});
    application.Connect(argc == 2 ? argv[1] : "127.0.0.1:5810");
    const auto result = application.run(client);
    application.Disconnect();
    return result;
}
