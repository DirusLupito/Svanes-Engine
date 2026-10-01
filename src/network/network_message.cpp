#include <svanes/network/network_message.hpp>

#include <random>

namespace svanes {

ClientId GenerateClientId() {
    thread_local std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<ClientId> distribution;
    return distribution(generator);
}

std::string MakeTcpEndpoint(const std::string &host, std::uint16_t port) {
    return "tcp://" + host + ":" + std::to_string(port);
}

std::string MakeUdpEndpoint(const std::string &host, std::uint16_t port) {
    return "udp://" + host + ":" + std::to_string(port);
}

} // namespace svanes
