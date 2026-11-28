#include "scenario.hpp"

#include "unit.hpp"
#include "utils/influence_map.hpp"

#include <svanes/network/message_serialization.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

namespace marching_orders {

// Side length of a control cell in world units.
constexpr std::uint32_t kControlCellSize = 10;

// Influence added at the center of a control stamp.
constexpr float kControlStampValue = 1.0F;

// Radius in world units over which a stamp adds influence.
constexpr float kControlStampRadius = 80.0F;

// Multiplier that reduces control values as terrain elevation increases.
constexpr float kControlUphillPenalty = 0.025F;

// Time in seconds for accumulated influence to fall by half.
constexpr float kControlHalfLifeSeconds = 0.75F;

// Simulation tics between consecutive control stamps.
constexpr svanes::TicCount kControlStampInterval = svanes::TicsPerSecond / 16;

// Minimum cell influence for each successive visible control tier.
constexpr std::array kControlThresholds{0.25F, 1.0F, 3.0F};

// Bytes occupied by a server ID, two coordinates, and a heading on the wire.
constexpr std::size_t kUnitRecordBytes =
    2 * sizeof(std::uint32_t) + 3 * sizeof(float);

// Define team colors for control mapping
static svanes::Color TeamColor(std::uint32_t team) {
    if (team == 1) {
        return {255, 0, 0};
    }
    if (team == 2) {
        return {0, 0, 255};
    }
    const double hue = std::fmod(team * 0.618033988749895, 1.0) * 6.0;
    const auto channel = static_cast<std::uint8_t>(
        std::lround(255.0 * (1.0 - std::abs(std::fmod(hue, 2.0) - 1.0))));
    const std::array<svanes::Color, 6> colors{
        svanes::Color{255, channel, 0}, svanes::Color{channel, 255, 0},
        svanes::Color{0, 255, channel}, svanes::Color{0, channel, 255},
        svanes::Color{channel, 0, 255}, svanes::Color{255, 0, channel}};
    return colors[static_cast<std::size_t>(hue)];
}

static auto MakeLandControlDecay(const Terrain &terrain,
                                 svanes::Vector2D location,
                                 svanes::Vector2D origin, float radius) {
    const auto source = location - origin;
    const auto bound = [](float coordinate, std::int32_t count) {
        return static_cast<std::int32_t>(std::clamp(
            std::floor(coordinate), 0.0F, static_cast<float>(count)));
    };
    const auto begin_x = bound(source.x - radius, terrain.Width());
    const auto begin_y = bound(source.y - radius, terrain.Height());
    const auto end_x = bound(source.x + radius + 1.0F, terrain.Width());
    const auto end_y = bound(source.y + radius + 1.0F, terrain.Height());
    const auto width = static_cast<std::size_t>(end_x - begin_x);
    // Get elevation costs for stamp grid area
    std::vector<float> costs(width * static_cast<std::size_t>(end_y - begin_y),
                             std::numeric_limits<float>::infinity());
    using Candidate = std::pair<float, std::size_t>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>>
        pending;
    // Recursive function for flood searching connected squares
    const auto visit = [&](std::int32_t x, std::int32_t y, float cost) {
        if (x < begin_x || x >= end_x || y < begin_y || y >= end_y ||
            cost >= radius) {
            return;
        }
        const auto index =
            static_cast<std::size_t>(y - begin_y) * width + x - begin_x;
        if (cost >= costs[index]) {
            return;
        }
        if (terrain.IsWaterAt(x, y) ||
            std::hypot(x + 0.5F - source.x, y + 0.5F - source.y) > radius) {
            return;
        }
        costs[index] = cost;
        pending.emplace(cost, index);
    };
    if (source.x >= begin_x && source.x < end_x && source.y >= begin_y &&
        source.y < end_y) {
        visit(static_cast<std::int32_t>(std::floor(source.x)),
              static_cast<std::int32_t>(std::floor(source.y)), 0.0F);
    }
    // Execute flood search. Costs are factored in when visiting squares.
    while (!pending.empty()) {
        const auto [cost, index] = pending.top();
        pending.pop();
        if (cost > costs[index]) {
            continue;
        }
        const auto x = begin_x + static_cast<std::int32_t>(index % width);
        const auto y = begin_y + static_cast<std::int32_t>(index / width);
        const auto elevation = terrain.ElevationAt(x, y);
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                const auto next_x = x + dx;
                const auto next_y = y + dy;
                if ((dx == 0 && dy == 0) || next_x < begin_x ||
                    next_x >= end_x || next_y < begin_y || next_y >= end_y) {
                    continue;
                }
                if (dx != 0 && dy != 0 &&
                    (terrain.IsWaterAt(next_x, y) ||
                     terrain.IsWaterAt(x, next_y))) {
                    continue;
                }
                const auto rise = std::max(
                    0, terrain.ElevationAt(next_x, next_y) - elevation);
                const float step =
                    std::hypot(static_cast<float>(dx), static_cast<float>(dy));
                visit(next_x, next_y,
                      cost + step + kControlUphillPenalty * rise);
            }
        }
    }
    return [origin, begin_x, begin_y, end_x, end_y, width, radius,
            costs = std::move(costs)](svanes::Vector2D, svanes::Vector2D cell,
                                      float) {
        const auto position = cell - origin;
        if (position.x < begin_x || position.x >= end_x ||
            position.y < begin_y || position.y >= end_y) {
            return 0.0F;
        }
        const auto x = static_cast<std::int32_t>(std::floor(position.x));
        const auto y = static_cast<std::int32_t>(std::floor(position.y));
        const auto index =
            static_cast<std::size_t>(y - begin_y) * width + x - begin_x;
        return std::max(0.0F, 1.0F - costs[index] / radius);
    };
}

struct TeamControl {
    TeamControl(std::uint32_t team, const ControlGrid &control)
        : influence(control.columns, control.rows, control.cell_size,
                    control.origin),
          color(TeamColor(team)) {}

    InfluenceMap influence;
    svanes::Color color;
};

/**
 * Holds the state used to advance a match on the server.
 *
 * FIELDS:
 * - world contains the units' movement and physics components.
 * - influence accumulates the exact control value in each cell.
 * - tics_until_stamp counts the simulation tics before units contribute again.
 */
struct Scenario::Simulation {
    svanes::Registry &world;
    std::map<std::uint32_t, TeamControl> teams;
    svanes::TicCount tics_until_stamp = 0;
};

svanes::NetworkMessage MoveOrder::Encode() const {
    svanes::MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(OrderType::Move));
    writer.WriteUint32(unit);
    writer.WriteFloat32(destination.x);
    writer.WriteFloat32(destination.y);
    return writer.Finish();
}

MoveOrder MoveOrder::Decode(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    if (reader.ReadUint8() != static_cast<std::uint8_t>(OrderType::Move)) {
        throw std::invalid_argument("Expected a unit movement request.");
    }
    const MoveOrder order{
        .unit = reader.ReadUint32(),
        .destination = {reader.ReadFloat32(), reader.ReadFloat32()}};
    if (reader.Remaining() != 0 || !std::isfinite(order.destination.x) ||
        !std::isfinite(order.destination.y)) {
        throw std::invalid_argument("Invalid unit movement request.");
    }
    return order;
}

svanes::NetworkMessage SpawnOrder::Encode() const {
    svanes::MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(OrderType::Spawn));
    writer.WriteFloat32(position.x);
    writer.WriteFloat32(position.y);
    return writer.Finish();
}

SpawnOrder SpawnOrder::Decode(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    if (reader.ReadUint8() != static_cast<std::uint8_t>(OrderType::Spawn)) {
        throw std::invalid_argument("Expected a unit spawn request.");
    }
    const SpawnOrder order{
        .position = {reader.ReadFloat32(), reader.ReadFloat32()}};
    if (reader.Remaining() != 0 || !std::isfinite(order.position.x) ||
        !std::isfinite(order.position.y)) {
        throw std::invalid_argument("Invalid unit spawn request.");
    }
    return order;
}

Scenario::Scenario(const Terrain &terrain) : terrain(terrain) {
    // A partial cell along the far edge still belongs to the control grid.
    const auto cells_along = [](std::int32_t length) {
        return (static_cast<std::uint32_t>(length) + kControlCellSize - 1) /
               kControlCellSize;
    };
    control.columns = cells_along(terrain.Width());
    control.rows = cells_along(terrain.Height());
    control.cell_size = kControlCellSize;
    control.origin = {-0.5F * terrain.Width(), -0.5F * terrain.Height()};
    control.tiles.resize(static_cast<std::size_t>(control.columns) *
                         control.rows);
}

Scenario::Scenario(const Terrain &terrain, svanes::Registry &world)
    : Scenario(terrain) {
    simulation = std::make_unique<Simulation>(world);
}

Scenario::~Scenario() {
    if (simulation) {
        for (const auto &[id, transform] : units) {
            simulation->world.DestroyEntity(id);
        }
    }
}

Scenario::Simulation &Scenario::GetSimulation() {
    if (!simulation) {
        throw std::logic_error(
            "Only the server can advance or command a scenario.");
    }

    return *simulation;
}

const Terrain &Scenario::GetTerrain() const { return terrain; }

const std::map<svanes::Entity, UnitState> &Scenario::GetUnits() const {
    return units;
}

const ControlGrid &Scenario::GetControl() const { return control; }

svanes::Entity Scenario::SpawnUnit(svanes::Vector2D position, float speed,
                                   std::uint32_t team) {
    auto &server = GetSimulation();
    auto &world = server.world;
    server.teams.try_emplace(team, team, control);
    Unit unit(world, position, speed, team);
    const auto id = unit.GetEntity();
    try {
        world.AddComponent<Unit>(id, std::move(unit));
        units.emplace(
            id,
            UnitState{.transform = world.GetComponent<svanes::Transform>(id),
                      .team = team});
    } catch (...) {
        world.DestroyEntity(id);
        throw;
    }
    return id;
}

void Scenario::RemoveUnit(svanes::Entity id) {
    auto &world = GetSimulation().world;
    if (units.erase(id) != 0) {
        world.DestroyEntity(id);
    }
}

bool Scenario::MoveUnit(const MoveOrder &order) {
    auto &world = GetSimulation().world;
    if (!units.contains(order.unit)) {
        return false;
    }

    world.GetComponent<Unit>(order.unit)
        .SetDestination(world, order.destination);
    return true;
}

void Scenario::Advance(svanes::TicCount delta_tics) {
    auto &server = GetSimulation();

    // Fade influence once per step and check whether a stamp is due.
    for (auto &[team, state] : server.teams) {
        state.influence.Fade(static_cast<float>(delta_tics) /
                                 svanes::TicsPerSecond,
                             kControlHalfLifeSeconds);
    }
    const bool stamp = delta_tics >= server.tics_until_stamp;
    server.tics_until_stamp =
        stamp ? kControlStampInterval : server.tics_until_stamp - delta_tics;

    // Resolve arrivals before stamping influence at each unit's position.
    for (auto &[id, state] : units) {
        server.world.GetComponent<Unit>(id).PhysicsUpdate(server.world);
        state.transform = server.world.GetComponent<svanes::Transform>(id);
        if (stamp) {
            const svanes::Vector2D position{state.transform.x,
                                            state.transform.y};
            server.teams.at(state.team)
                .influence.Stamp(
                    position, kControlStampValue, kControlStampRadius,
                    MakeLandControlDecay(terrain, position, control.origin,
                                         kControlStampRadius));
        }
    }

    // Convert accumulated influence into the control tiers sent to clients.
    for (std::uint32_t y = 0; y < control.rows; ++y) {
        for (std::uint32_t x = 0; x < control.columns; ++x) {
            // Calculate what color a tile should be based on all teams current control there
            double value = 0.0;
            double red = 0.0;
            double green = 0.0;
            double blue = 0.0;
            for (const auto &[team, state] : server.teams) {
                const float contribution = state.influence.ReadCell(x, y);
                value += contribution;
                red += contribution * state.color.red;
                green += contribution * state.color.green;
                blue += contribution * state.color.blue;
            }
            const auto tier =
                std::ranges::upper_bound(kControlThresholds, value) -
                kControlThresholds.begin();
            auto &tile =
                control
                    .tiles[static_cast<std::size_t>(y) * control.columns + x];
            tile = 0;
            if (tier == 0) {
                continue;
            }
            const auto channel = [value](double weighted) {
                return static_cast<std::uint32_t>(
                    std::lround(weighted / value *
                                (ControlGrid::kColorLevels - 1) / 255.0));
            };
            const auto color =
                (channel(red) * ControlGrid::kColorLevels + channel(green)) *
                    ControlGrid::kColorLevels +
                channel(blue);
            tile = static_cast<std::uint16_t>(
                color * ControlGrid::kOpacities.size() + tier);
        }
    }
}

svanes::NetworkMessage Scenario::MakeSnapshot() const {
    if (!std::in_range<std::uint32_t>(units.size())) {
        throw std::length_error("Too many units for a scenario snapshot.");
    }

    svanes::MessageWriter writer;
    writer.WriteUint32(static_cast<std::uint32_t>(units.size()));
    for (const auto &[id, state] : units) {
        writer.WriteUint32(id);
        writer.WriteFloat32(state.transform.x);
        writer.WriteFloat32(state.transform.y);
        writer.WriteFloat32(state.transform.rotation);
        writer.WriteUint32(state.team);
    }

    for (const auto tile : control.tiles) {
        writer.WriteUint16(tile);
    }
    return writer.Finish();
}

void Scenario::ApplySnapshot(const svanes::NetworkMessage &message) {
    if (simulation) {
        throw std::logic_error(
            "A server scenario cannot receive a client view.");
    }

    svanes::MessageReader reader(message);
    const auto count = reader.ReadUint32();
    const auto grid_bytes = control.tiles.size() * sizeof(std::uint16_t);
    if (reader.Remaining() < grid_bytes ||
        reader.Remaining() - grid_bytes !=
            static_cast<std::uint64_t>(count) * kUnitRecordBytes) {
        throw std::invalid_argument(
            "Scenario snapshot has an invalid roster or control grid size.");
    }

    // Keep the previous roster and tiers together until every field is valid.
    std::map<svanes::Entity, UnitState> received;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto id = reader.ReadUint32();
        const svanes::Transform transform{.x = reader.ReadFloat32(),
                                          .y = reader.ReadFloat32(),
                                          .rotation = reader.ReadFloat32()};
        const auto team = reader.ReadUint32();
        if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
            !std::isfinite(transform.rotation)) {
            throw std::invalid_argument(
                "Scenario snapshot contains a nonfinite unit transform.");
        }

        if (!received
                 .emplace(id, UnitState{.transform = transform, .team = team})
                 .second) {
            throw std::invalid_argument(
                "Scenario snapshot contains a duplicate unit ID.");
        }
    }

    std::vector<std::uint16_t> tiles;
    tiles.reserve(control.tiles.size());
    for (std::size_t index = 0; index < control.tiles.size(); ++index) {
        const auto tile = reader.ReadUint16();
        if (tile > ControlGrid::kTileCount) {
            throw std::invalid_argument(
                "Scenario snapshot contains an invalid control tile.");
        }
        tiles.push_back(tile);
    }

    control.tiles.swap(tiles);
    units.swap(received);
}

} // namespace marching_orders
