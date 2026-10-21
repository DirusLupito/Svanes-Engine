#include "network_protocol.hpp"

#include <svanes/network/network_message.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kStepSeconds = 1.0 / 60.0;
constexpr std::size_t kDefaultIterations = 100000;
constexpr std::size_t kDefaultRuns = 5;
constexpr std::size_t kTimelineBroadcastInterval = 15;
constexpr std::size_t kLatencyPatternPeriod = 3000;
constexpr std::size_t kLatencySpikeDuration = 600;

struct Config {
    std::string experiment = "all";
    std::string strategy = "both";
    std::size_t clients = 2;
    std::size_t platforms = 20;
    std::size_t latency_ms = 0;
    std::size_t disruption_ms = 0;
    std::size_t iterations = kDefaultIterations;
    std::size_t runs = kDefaultRuns;
    std::string output = "double_time_benchmark.csv";
};

struct InputEvent {
    float horizontal = 0.0F;
    bool jump = false;
};

struct Position {
    double x = 0.0;
    double y = 0.0;
    double velocity_y = 0.0;
};

struct TimelinePacket {
    std::size_t delivery_iteration = 0;
    std::size_t destination = 0;
    double timeline = 0.0;
};

struct TimelinePacketEarlier {
    bool operator()(const TimelinePacket &left, const TimelinePacket &right) const {
        return left.delivery_iteration > right.delivery_iteration;
    }
};

struct Metrics {
    std::string experiment;
    std::string strategy;
    std::size_t clients = 0;
    std::size_t platforms = 0;
    std::size_t latency_ms = 0;
    std::size_t disruption_ms = 0;
    std::size_t run = 0;
    double total_loop_ms = 0.0;
    double worst_loop_us = 0.0;
    double average_timeline_error = 0.0;
    double max_timeline_error = 0.0;
    double timeline_error_variance = 0.0;
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
        else if (argument == "--disruption-ms") config.disruption_ms = ParseSize(value(), argument);
        else if (argument == "--iterations") config.iterations = ParseSize(value(), argument);
        else if (argument == "--runs") config.runs = ParseSize(value(), argument);
        else if (argument == "--output") config.output = std::string{value()};
        else throw std::invalid_argument("Unknown argument: " + argument);
    }
    if (config.clients == 0 || config.platforms == 0 ||
        config.iterations == 0 || config.runs == 0) {
        throw std::invalid_argument("Clients, platforms, iterations, and runs must be positive.");
    }
    return config;
}

std::size_t DelayIterations(std::size_t milliseconds) {
    return static_cast<std::size_t>(std::ceil(
        static_cast<double>(milliseconds) / (kStepSeconds * 1000.0)));
}

std::size_t LatencyForIteration(const Config &config, std::size_t iteration) {
    if (config.experiment != "latency" && config.experiment != "latency_max_load") {
        return 0;
    }
    const std::size_t phase = iteration % kLatencyPatternPeriod;
    return phase >= kLatencyPatternPeriod - kLatencySpikeDuration
        ? config.latency_ms
        : 0;
}

InputEvent ScriptedInput(std::size_t iteration, std::size_t client) {
    const bool moving_right = ((iteration / 120 + client) % 2) == 0;
    return InputEvent{moving_right ? 1.0F : -1.0F, iteration % 90 == 0};
}

bool IsDisrupted(const Config &config, std::size_t iteration) {
    if (config.experiment != "disruption" || config.disruption_ms == 0) {
        return false;
    }
    const std::size_t duration = std::max<std::size_t>(
        1, DelayIterations(config.disruption_ms));
    const std::size_t start = config.iterations / 2;
    return iteration >= start && iteration < start + duration;
}

void Advance(Position &position, InputEvent input) {
    position.x += static_cast<double>(input.horizontal) * 4.0;
    if (input.jump) position.velocity_y = -8.0;
    position.velocity_y += 0.3;
    position.y += position.velocity_y;
}

void WriteHeader(std::ofstream &output) {
    output << "experiment,strategy,clients,platforms,latency_ms,disruption_ms,run,"
               "total_loop_ms,worst_loop_us,average_timeline_error,max_timeline_error,"
               "timeline_error_variance\n";
}

void WriteMetrics(std::ofstream &output, const Metrics &metrics) {
    output << metrics.experiment << ',' << metrics.strategy << ',' << metrics.clients << ','
           << metrics.platforms << ',' << metrics.latency_ms << ',' << metrics.disruption_ms
           << ',' << metrics.run << ',' << metrics.total_loop_ms << ',' << metrics.worst_loop_us
           << ',' << metrics.average_timeline_error << ',' << metrics.max_timeline_error << ','
           << metrics.timeline_error_variance << '\n';
}

Metrics RunTrial(const Config &config, std::string strategy, std::size_t run) {
    Metrics metrics{config.experiment, std::move(strategy), config.clients,
                    config.platforms,
                    (config.experiment == "latency" ||
                     config.experiment == "latency_max_load") ? config.latency_ms : 0,
                    config.experiment == "disruption" ? config.disruption_ms : 0, run};
    const bool input_strategy = metrics.strategy == "input";
    const bool measure_timeline = metrics.experiment == "latency" ||
        metrics.experiment == "latency_max_load";
    std::vector<Position> server_positions(config.clients);
    std::vector<Position> client_positions(config.clients);
    std::vector<InputEvent> server_inputs(config.clients);
    std::vector<double> client_timelines(config.clients, 0.0);
    std::priority_queue<TimelinePacket, std::vector<TimelinePacket>,
                        TimelinePacketEarlier> timeline_packets;
    double server_timeline = 0.0;
    double timeline_error_sum = 0.0;
    double timeline_error_squared_sum = 0.0;
    std::size_t timeline_samples = 0;
    const std::size_t progress_step = std::max<std::size_t>(1, config.iterations / 10);

    for (std::size_t iteration = 0; iteration < config.iterations; ++iteration) {
        if (iteration % progress_step == 0 || iteration + 1 == config.iterations) {
            std::cout << "\r    progress " << ((iteration + 1) * 100 / config.iterations)
                      << '%' << std::flush;
        }
        const auto loop_start = std::chrono::steady_clock::now();
        const bool disrupted = IsDisrupted(config, iteration);
        ++server_timeline;

        for (std::size_t platform = 0; platform < config.platforms; ++platform) {
            const double phase = (static_cast<double>(iteration) + platform) * 0.01;
            const double x = std::sin(phase) * 20.0 + platform;
            const double y = std::cos(phase) * 10.0;
            for (std::size_t client = 0; client < config.clients; ++client) {
                volatile double observed_x = x + static_cast<double>(client) * 0.001;
                volatile double observed_y = y;
                (void)observed_x;
                (void)observed_y;
            }
        }

        for (std::size_t client = 0; client < config.clients; ++client) {
            const InputEvent input = ScriptedInput(iteration, client);
            Advance(client_positions[client], input);
            if (input_strategy) {
                if (!disrupted) {
                    server_inputs[client] = input;
                }
            } else if (!disrupted) {
                server_positions[client] = client_positions[client];
            }
        }

        if (input_strategy) {
            for (std::size_t client = 0; client < config.clients; ++client) {
                Advance(server_positions[client], server_inputs[client]);
            }
        }

        if (measure_timeline && iteration % kTimelineBroadcastInterval == 0 && !disrupted) {
            const std::size_t delay = DelayIterations(
                LatencyForIteration(config, iteration));
            for (std::size_t client = 0; client < config.clients; ++client) {
                timeline_packets.push(TimelinePacket{iteration + delay, client,
                                                      server_timeline});
            }
        }
        while (measure_timeline && !timeline_packets.empty() &&
               timeline_packets.top().delivery_iteration <= iteration) {
            const TimelinePacket packet = timeline_packets.top();
            timeline_packets.pop();
            client_timelines[packet.destination] = packet.timeline;
        }
        for (double client_timeline : client_timelines) {
            if (!measure_timeline) break;
            const double error = std::abs(server_timeline - client_timeline);
            timeline_error_sum += error;
            timeline_error_squared_sum += error * error;
            ++timeline_samples;
            metrics.max_timeline_error = std::max(metrics.max_timeline_error, error);
        }

        const auto loop_end = std::chrono::steady_clock::now();
        const double loop_us = std::chrono::duration<double, std::micro>(
            loop_end - loop_start).count();
        metrics.total_loop_ms += loop_us / 1000.0;
        metrics.worst_loop_us = std::max(metrics.worst_loop_us, loop_us);
    }
    if (timeline_samples > 0) {
        metrics.average_timeline_error = timeline_error_sum /
            static_cast<double>(timeline_samples);
        const double mean = metrics.average_timeline_error;
        metrics.timeline_error_variance = timeline_error_squared_sum /
            static_cast<double>(timeline_samples) - mean * mean;
    }
    std::cout << '\n';
    return metrics;
}

void RunExperiment(const Config &config, std::ofstream &output,
                   std::string_view experiment) {
    const std::vector<std::string> strategies = config.strategy == "both"
        ? std::vector<std::string>{"input", "client_transform"}
        : std::vector<std::string>{config.strategy};
    for (const std::string &strategy : strategies) {
        for (std::size_t run = 0; run < config.runs; ++run) {
            std::cout << experiment << " | strategy=" << strategy
                      << " | run=" << (run + 1) << '/' << config.runs
                      << " | clients=" << config.clients
                      << " | platforms=" << config.platforms
                      << " | latency=" << config.latency_ms << "ms"
                      << " | disruption=" << config.disruption_ms << "ms\n";
            Config trial_config = config;
            trial_config.experiment = std::string{experiment};
            WriteMetrics(output, RunTrial(trial_config, strategy, run));
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
            RunExperiment(config, output, "client_count");
            RunExperiment(config, output, "platform_count");
            RunExperiment(config, output, "latency");
            RunExperiment(config, output, "latency_max_load");
        } else if (config.experiment == "client_count" ||
                   config.experiment == "platform_count" ||
                   config.experiment == "latency" ||
                   config.experiment == "latency_max_load") {
            RunExperiment(config, output, config.experiment);
        } else {
            throw std::invalid_argument(
                "Experiment must be client_count, platform_count, latency, latency_max_load, or all.");
        }
        std::cout << "Wrote benchmark results to " << config.output << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
