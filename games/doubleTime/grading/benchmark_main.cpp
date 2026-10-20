#include "network_protocol.hpp"

#include <svanes/network/network_message.hpp>

#include <algorithm>
#include <chrono>
#include <deque>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kStepSeconds = 1.0 / 60.0;
constexpr std::size_t kDefaultIterations = 100000;
constexpr std::size_t kDefaultRuns = 5;
constexpr std::size_t kDisruptionDuration = 30;
constexpr float kConvergenceThreshold = 0.5F;
constexpr std::size_t kTimelineSyncInterval = 30;

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;
};

struct Config {
    std::string experiment = "all";
    std::string strategy = "both";
    std::size_t clients = 2;
    std::size_t platforms = 4;
    std::size_t latency_ms = 100;
    std::size_t iterations = kDefaultIterations;
    std::size_t runs = kDefaultRuns;
    std::string output = "double_time_benchmark.csv";
};

enum class PacketKind : std::uint8_t { Input, Transform, State };

struct Packet {
    std::size_t delivery_iteration = 0;
    PacketKind kind = PacketKind::Input;
    std::size_t source = 0;
    std::size_t destination = 0;
    svanes::NetworkMessage message;
};

struct TimelinePacket {
    std::size_t delivery_iteration = 0;
    std::size_t destination = 0;
    double timeline = 0.0;
};

struct ClientState {
    Vec2 position{0.0F, 0.0F};
    Vec2 velocity{0.0F, 0.0F};
    std::vector<Vec2> visible_players;
    std::vector<Vec2> platforms;
    std::size_t lost_actions = 0;
    std::size_t timeline = 0;
};

struct Metrics {
    std::string experiment;
    std::string strategy;
    std::size_t clients = 0;
    std::size_t platforms = 0;
    std::size_t latency_ms = 0;
    std::size_t run = 0;
    double total_loop_ms = 0.0;
    double worst_loop_us = 0.0;
    double server_us_per_loop = 0.0;
    double client_us_per_loop = 0.0;
    double messages_per_second = 0.0;
    double bytes_sent_per_second = 0.0;
    double bytes_received_per_second = 0.0;
    std::size_t max_queue_backlog = 0;
    std::size_t delayed_messages = 0;
    std::size_t memory_bytes = 0;
    float max_client_server_player_error = 0.0F;
    float max_client_server_platform_error = 0.0F;
    float max_client_player_error = 0.0F;
    float max_client_platform_error = 0.0F;
    std::size_t divergence_iterations = 0;
    std::size_t convergence_iteration = 0;
    float average_correction_distance = 0.0F;
    std::size_t lost_actions = 0;
    double max_timeline_error = 0.0;
    bool permanently_divergent = false;
};

std::size_t ParseSize(std::string_view value, std::string_view name) {
    try {
        return static_cast<std::size_t>(std::stoull(std::string{value}));
    } catch (...) {
        throw std::invalid_argument("Invalid value for " + std::string{name});
    }
}

Config ParseConfig(int argc, char **argv) {
    Config config;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&]() -> std::string_view {
            if (index + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + argument);
            }
            return argv[++index];
        };
        if (argument == "--experiment") config.experiment = std::string{value()};
        else if (argument == "--strategy") config.strategy = std::string{value()};
        else if (argument == "--clients") config.clients = ParseSize(value(), argument);
        else if (argument == "--platforms") config.platforms = ParseSize(value(), argument);
        else if (argument == "--latency-ms") config.latency_ms = ParseSize(value(), argument);
        else if (argument == "--iterations") config.iterations = ParseSize(value(), argument);
        else if (argument == "--runs") config.runs = ParseSize(value(), argument);
        else if (argument == "--output") config.output = std::string{value()};
        else throw std::invalid_argument("Unknown argument: " + argument);
    }
    if (config.clients == 0 || config.iterations == 0 || config.runs == 0) {
        throw std::invalid_argument("Clients, iterations, and runs must be positive.");
    }
    return config;
}

float Distance(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

std::size_t DelayIterations(std::size_t latency_ms) {
    return static_cast<std::size_t>(std::ceil(
        static_cast<double>(latency_ms) / (kStepSeconds * 1000.0)));
}

bool InDisruption(std::string_view experiment, std::size_t iteration, std::size_t iterations) {
    if (experiment != "disruption") return false;
    const std::size_t start = iterations / 2;
    return iteration >= start && iteration < start + kDisruptionDuration;
}

void WriteHeader(std::ofstream &output) {
    output << "experiment,strategy,clients,platforms,latency_ms,run,total_loop_ms,worst_loop_us,"
              "server_us_per_loop,client_us_per_loop,messages_per_second,bytes_sent_per_second,"
              "bytes_received_per_second,max_queue_backlog,delayed_messages,memory_bytes,"
              "max_client_server_player_error,max_client_server_platform_error,max_client_player_error,"
              "max_client_platform_error,divergence_iterations,convergence_iteration,"
              "average_correction_distance,lost_actions,max_timeline_error,permanently_divergent\n";
}

void WriteMetrics(std::ofstream &output, const Metrics &metrics) {
    output << metrics.experiment << ',' << metrics.strategy << ',' << metrics.clients << ','
           << metrics.platforms << ',' << metrics.latency_ms << ',' << metrics.run << ','
           << metrics.total_loop_ms << ',' << metrics.worst_loop_us << ','
           << metrics.server_us_per_loop << ',' << metrics.client_us_per_loop << ','
           << metrics.messages_per_second << ',' << metrics.bytes_sent_per_second << ','
           << metrics.bytes_received_per_second << ',' << metrics.max_queue_backlog << ','
           << metrics.delayed_messages << ',' << metrics.memory_bytes << ','
           << metrics.max_client_server_player_error << ','
           << metrics.max_client_server_platform_error << ',' << metrics.max_client_player_error
           << ',' << metrics.max_client_platform_error << ',' << metrics.divergence_iterations
           << ',' << metrics.convergence_iteration << ',' << metrics.average_correction_distance
           << ',' << metrics.lost_actions << ',' << metrics.max_timeline_error << ','
           << (metrics.permanently_divergent ? 1 : 0) << '\n';
}

Metrics RunTrial(const Config &config, std::string strategy, std::string experiment,
                 std::size_t run) {
    const std::size_t effective_latency = experiment == "latency" ? config.latency_ms : 0;
    Metrics metrics{.experiment = std::move(experiment), .strategy = std::move(strategy),
                     .clients = config.clients, .platforms = config.platforms,
                     .latency_ms = effective_latency, .run = run};
    const bool input_strategy = metrics.strategy == "input";
    const std::size_t delay = DelayIterations(effective_latency);
    std::vector<ClientState> clients(config.clients);
    std::vector<Vec2> server_positions(config.clients);
    std::vector<Vec2> server_velocities(config.clients);
    for (auto &client : clients) {
        client.visible_players.resize(config.clients);
        client.platforms.resize(config.platforms);
    }
    std::vector<Vec2> server_platforms(config.platforms);
    std::vector<Vec2> client_platforms(config.platforms * config.clients);
    std::deque<Packet> client_to_server;
    std::deque<Packet> server_to_clients;
    std::deque<TimelinePacket> timeline_packets;
    std::vector<double> client_timelines(config.clients, 0.0);
    double server_timeline = 0.0;
    std::size_t messages = 0;
    std::size_t sent_bytes = 0;
    std::size_t received_bytes = 0;
    std::size_t delayed_messages = 0;
    std::optional<std::size_t> divergence_start;
    std::size_t convergence_iteration = 0;
    bool converged = false;
    float correction_total = 0.0F;
    std::size_t correction_count = 0;
    double server_time_us = 0.0;
    double client_time_us = 0.0;
    double total_loop_ms = 0.0;
    double worst_loop_us = 0.0;
    const std::size_t progress_step = std::max<std::size_t>(1, config.iterations / 10);
    for (std::size_t iteration = 0; iteration < config.iterations; ++iteration) {
        if (iteration % progress_step == 0 || iteration + 1 == config.iterations) {
            const std::size_t completed = iteration + 1;
            const std::size_t percent = completed * 100 / config.iterations;
            std::cout << "\r    progress " << percent << "%" << std::flush;
        }
        const auto loop_start = std::chrono::steady_clock::now();
        server_timeline += 1.0;
        for (std::size_t client_id = 0; client_id < clients.size(); ++client_id) {
            client_timelines[client_id] += 1.0 + 0.005 * static_cast<double>(client_id + 1);
        }
        const auto server_start = std::chrono::steady_clock::now();
        for (std::size_t platform = 0; platform < config.platforms; ++platform) {
            const float phase = static_cast<float>(iteration + platform) * 0.01F;
            server_platforms[platform] = Vec2{std::sin(phase) * 20.0F + static_cast<float>(platform),
                                               std::cos(phase) * 10.0F};
        }
        while (!client_to_server.empty() &&
               client_to_server.front().delivery_iteration <= iteration) {
            Packet packet = std::move(client_to_server.front());
            client_to_server.pop_front();
            if (packet.kind == PacketKind::Input) {
                const auto input = packet.message.As<PlayerInputMessage>();
                auto &velocity = server_velocities[input.client_id % server_velocities.size()];
                velocity.x = input.horizontal * 4.0F;
                if (input.action_requested) velocity.y = -8.0F;
            } else {
                const auto update = packet.message.As<PlayerTransformMessage>();
                server_positions[update.client_id % server_positions.size()] =
                    Vec2{update.transform.x, update.transform.y};
                server_velocities[update.client_id % server_velocities.size()] =
                    Vec2{update.velocity.x, update.velocity.y};
            }
        }
        if (input_strategy) {
            for (std::size_t client_id = 0; client_id < clients.size(); ++client_id) {
                server_velocities[client_id].y += 0.3F;
                server_positions[client_id].x += server_velocities[client_id].x;
                server_positions[client_id].y += server_velocities[client_id].y;
            }
        }
        const auto server_end = std::chrono::steady_clock::now();
        server_time_us += std::chrono::duration<double, std::micro>(server_end - server_start).count();
        for (std::size_t client_id = 0; client_id < clients.size(); ++client_id) {
            auto &client = clients[client_id];
            const float horizontal = ((iteration / 120 + client_id) % 2 == 0) ? 1.0F : -1.0F;
            const bool jump = iteration % 90 == 0;
            if (input_strategy) {
                const PlayerInputMessage input{static_cast<svanes::ClientId>(client_id), horizontal, jump,
                                               jump};
                const auto encoded = svanes::NetworkMessage::From(input);
            if (!InDisruption(metrics.experiment, iteration, config.iterations)) {
                client_to_server.push_back(Packet{iteration + delay, PacketKind::Input, client_id, 0, encoded});
                sent_bytes += encoded.bytes.size();
                ++messages;
                } else {
                    ++client.lost_actions;
                }
            } else {
                client.velocity.x = horizontal * 4.0F;
                if (jump) client.velocity.y = -8.0F;
                client.velocity.y += 0.3F;
                client.position.x += client.velocity.x;
                client.position.y += client.velocity.y;
                const svanes::Transform transform{client.position.x, client.position.y, 0.0F};
                const PlayerTransformMessage update{static_cast<svanes::ClientId>(client_id), transform,
                                                    svanes::Vector2D{client.velocity.x, client.velocity.y}};
                const auto encoded = svanes::NetworkMessage::From(update);
                if (!InDisruption(metrics.experiment, iteration, config.iterations)) {
                    client_to_server.push_back(Packet{iteration + delay, PacketKind::Transform, client_id, 0, encoded});
                    sent_bytes += encoded.bytes.size();
                    ++messages;
                } else {
                    ++client.lost_actions;
                }
            }
            if (input_strategy) {
                client.velocity.y += 0.3F;
                client.position.x += client.velocity.x;
                client.position.y += client.velocity.y;
            }
        }
        for (std::size_t client_id = 0; client_id < clients.size(); ++client_id) {
            const auto &client = clients[client_id];
            const DoubleTimeEntityState player_state{
                svanes::Transform{server_positions[client_id].x, server_positions[client_id].y, 0.0F}, false,
                static_cast<std::uint32_t>(client_id), DoubleTimeEntityKind::Character,
                kDoubleTimeCharacterWidth, kDoubleTimeCharacterHeight};
            const auto encoded = svanes::NetworkMessage::From(player_state);
            for (std::size_t destination = 0; destination < clients.size(); ++destination) {
                if (!InDisruption(metrics.experiment, iteration, config.iterations)) {
                    server_to_clients.push_back(Packet{iteration + delay, PacketKind::State, client_id,
                                                        destination, encoded});
                    sent_bytes += encoded.bytes.size();
                    ++messages;
                }
            }
        }
        for (std::size_t platform = 0; platform < config.platforms; ++platform) {
            const DoubleTimeEntityState platform_state{
                svanes::Transform{server_platforms[platform].x, server_platforms[platform].y, 0.0F},
                false, static_cast<std::uint32_t>(platform), DoubleTimeEntityKind::MovingPlatform,
                10.0F, 10.0F};
            const auto encoded = svanes::NetworkMessage::From(platform_state);
            for (std::size_t destination = 0; destination < clients.size(); ++destination) {
                if (!InDisruption(metrics.experiment, iteration, config.iterations)) {
                    server_to_clients.push_back(Packet{iteration + delay, PacketKind::State, 0, destination, encoded});
                    sent_bytes += encoded.bytes.size();
                    ++messages;
                }
            }
        }
        if (iteration % kTimelineSyncInterval == 0) {
            for (std::size_t destination = 0; destination < clients.size(); ++destination) {
                if (!InDisruption(metrics.experiment, iteration, config.iterations)) {
                    timeline_packets.push_back(
                        TimelinePacket{iteration + delay, destination, server_timeline});
                }
            }
        }
        while (!timeline_packets.empty() &&
               timeline_packets.front().delivery_iteration <= iteration) {
            const TimelinePacket packet = timeline_packets.front();
            timeline_packets.pop_front();
            client_timelines[packet.destination] = packet.timeline;
        }
        for (const double client_timeline : client_timelines) {
            metrics.max_timeline_error = std::max(
                metrics.max_timeline_error, std::abs(client_timeline - server_timeline));
        }
        while (!server_to_clients.empty() &&
               server_to_clients.front().delivery_iteration <= iteration) {
            Packet packet = std::move(server_to_clients.front());
            server_to_clients.pop_front();
            received_bytes += packet.message.bytes.size();
            if (packet.kind == PacketKind::State &&
                packet.message.bytes.size() == sizeof(DoubleTimeEntityState)) {
                const auto state = packet.message.As<DoubleTimeEntityState>();
                if (state.kind == DoubleTimeEntityKind::MovingPlatform &&
                    state.entity_id < config.platforms) {
                    const Vec2 position{state.transform.x, state.transform.y};
                    client_platforms[packet.destination * config.platforms + state.entity_id] = position;
                    clients[packet.destination].platforms[state.entity_id] = position;
                } else if (state.kind == DoubleTimeEntityKind::Character &&
                           state.entity_id < config.clients) {
                    const Vec2 position{state.transform.x, state.transform.y};
                    clients[packet.destination].visible_players[state.entity_id] = position;
                    if (packet.destination == state.entity_id) {
                        const float correction = Distance(clients[packet.destination].position, position);
                        if (correction > 0.0F) {
                            correction_total += correction;
                            ++correction_count;
                        }
                        clients[packet.destination].position = position;
                    }
                }
            }
        }
        if (!client_to_server.empty() || !server_to_clients.empty()) delayed_messages += client_to_server.size() + server_to_clients.size();
        metrics.max_queue_backlog = std::max(metrics.max_queue_backlog, client_to_server.size() + server_to_clients.size());
        float max_server_player_error = 0.0F;
        float max_server_platform_error = 0.0F;
        float max_client_player_error = 0.0F;
        float max_client_platform_error = 0.0F;
        for (std::size_t client_id = 0; client_id < clients.size(); ++client_id) {
            clients[client_id].visible_players[client_id] = clients[client_id].position;
            for (std::size_t player = 0; player < clients.size(); ++player) {
                max_server_player_error = std::max(
                    max_server_player_error,
                    Distance(clients[client_id].visible_players[player],
                             server_positions[player]));
                for (std::size_t other = client_id + 1; other < clients.size(); ++other) {
                    max_client_player_error = std::max(
                        max_client_player_error,
                        Distance(clients[client_id].visible_players[player],
                                 clients[other].visible_players[player]));
                }
            }
            for (std::size_t platform = 0; platform < config.platforms; ++platform) {
                max_server_platform_error = std::max(
                    max_server_platform_error,
                    Distance(clients[client_id].platforms[platform],
                             server_platforms[platform]));
                for (std::size_t other = client_id + 1; other < clients.size(); ++other) {
                    max_client_platform_error = std::max(
                        max_client_platform_error,
                        Distance(clients[client_id].platforms[platform],
                                 clients[other].platforms[platform]));
                }
            }
        }
        const float current_error = std::max(max_server_player_error, max_server_platform_error);
        if (current_error > kConvergenceThreshold && !divergence_start.has_value()) {
            divergence_start = iteration;
        }
        if (!converged && divergence_start.has_value() && current_error <= kConvergenceThreshold &&
            iteration > *divergence_start) {
            converged = true;
            convergence_iteration = iteration;
        }
        metrics.max_client_server_player_error = std::max(
            metrics.max_client_server_player_error, max_server_player_error);
        metrics.max_client_server_platform_error = std::max(
            metrics.max_client_server_platform_error, max_server_platform_error);
        metrics.max_client_player_error = std::max(
            metrics.max_client_player_error, max_client_player_error);
        metrics.max_client_platform_error = std::max(
            metrics.max_client_platform_error, max_client_platform_error);
        const auto loop_end = std::chrono::steady_clock::now();
        const double loop_us = std::chrono::duration<double, std::micro>(loop_end - loop_start).count();
        worst_loop_us = std::max(worst_loop_us, loop_us);
        total_loop_ms += loop_us / 1000.0;
        client_time_us += loop_us;
    }
    metrics.total_loop_ms = total_loop_ms;
    metrics.worst_loop_us = worst_loop_us;
    metrics.server_us_per_loop = server_time_us / static_cast<double>(config.iterations);
    metrics.client_us_per_loop = client_time_us / static_cast<double>(config.iterations * config.clients);
    metrics.messages_per_second = static_cast<double>(messages) / (static_cast<double>(config.iterations) * kStepSeconds);
    metrics.bytes_sent_per_second = static_cast<double>(sent_bytes) / (static_cast<double>(config.iterations) * kStepSeconds);
    metrics.bytes_received_per_second = static_cast<double>(received_bytes) / (static_cast<double>(config.iterations) * kStepSeconds);
    metrics.delayed_messages = delayed_messages;
    metrics.memory_bytes = (clients.size() * sizeof(ClientState)) + (server_platforms.size() * sizeof(Vec2)) +
                           (client_platforms.size() * sizeof(Vec2)) +
                           (client_to_server.size() + server_to_clients.size()) * sizeof(Packet) +
                           timeline_packets.size() * sizeof(TimelinePacket);
    metrics.divergence_iterations = !divergence_start.has_value()
        ? 0
        : (converged ? convergence_iteration - *divergence_start : config.iterations - *divergence_start);
    metrics.convergence_iteration = converged ? convergence_iteration : 0;
    metrics.average_correction_distance = correction_count == 0 ? 0.0F : correction_total / static_cast<float>(correction_count);
    for (const auto &client : clients) metrics.lost_actions += client.lost_actions;
    metrics.permanently_divergent = divergence_start.has_value() && !converged;
    std::cout << "\n";
    return metrics;
}

void RunExperiment(const Config &config, std::ofstream &output, std::string_view experiment) {
    const std::vector<std::string> strategies = config.strategy == "both"
        ? std::vector<std::string>{"input", "client_transform"}
        : std::vector<std::string>{config.strategy};
    for (const auto &strategy : strategies) {
        for (std::size_t run = 0; run < config.runs; ++run) {
            std::cout << experiment << " | strategy=" << strategy
                      << " | run=" << (run + 1) << "/" << config.runs
                      << " | clients=" << config.clients
                      << " | platforms=" << config.platforms
                      << " | latency=" << (experiment == "latency" ? config.latency_ms : 0) << "ms\n";
            WriteMetrics(output, RunTrial(config, strategy, std::string{experiment}, run));
        }
    }
}

} 

int main(int argc, char **argv) {
    try {
        const Config config = ParseConfig(argc, argv);
        std::ofstream output(config.output);
        if (!output) throw std::runtime_error("Could not open benchmark output.");
        WriteHeader(output);
        if (config.experiment == "all") {
            RunExperiment(config, output, "normal");
            RunExperiment(config, output, "latency");
            RunExperiment(config, output, "disruption");
        } else if (config.experiment == "normal" || config.experiment == "latency" || config.experiment == "disruption") {
            RunExperiment(config, output, config.experiment);
        } else {
            throw std::invalid_argument("Experiment must be normal, latency, disruption, or all.");
        }
        std::cout << "Wrote benchmark results to " << config.output << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
