#include "scenario.hpp"

#include "unit.hpp"
#include "utils/influence_map.hpp"

#include <svanes/network/message_serialization.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <stdexcept>
#include <utility>

namespace marching_orders {

// Side length of a control cell in world units.
constexpr std::uint32_t kControlCellSize = 10;

// Influence added at the center of a control stamp.
constexpr float kControlStampValue = 1.0F;

// Radius in world units over which a stamp adds influence.
constexpr float kControlStampRadius = 80.0F;

// Time in seconds for accumulated influence to fall by half.
constexpr float kControlHalfLifeSeconds = 0.75F;

// Simulation tics between consecutive control stamps.
constexpr svanes::TicCount kControlStampInterval = svanes::TicsPerSecond / 16;

// Minimum cell influence for each successive visible control tier.
constexpr std::array kControlThresholds{0.25F, 1.0F, 3.0F};

// Bytes occupied by a server ID, two coordinates, and a heading on the wire.
constexpr std::size_t kUnitRecordBytes =
    sizeof(std::uint32_t) + 3 * sizeof(float);

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
    InfluenceMap influence;
    svanes::TicCount tics_until_stamp = 0;
};

svanes::NetworkMessage MoveOrder::Encode() const {
    svanes::MessageWriter writer;
    writer.WriteUint32(unit);
    writer.WriteFloat32(destination.x);
    writer.WriteFloat32(destination.y);
    return writer.Finish();
}

MoveOrder MoveOrder::Decode(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    const MoveOrder order{
        .unit = reader.ReadUint32(),
        .destination = {reader.ReadFloat32(), reader.ReadFloat32()}};
    if (reader.Remaining() != 0 || !std::isfinite(order.destination.x) ||
        !std::isfinite(order.destination.y)) {
        throw std::invalid_argument("Invalid unit movement request.");
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
    control.tiers.resize(static_cast<std::size_t>(control.columns) *
                         control.rows);
}

Scenario::Scenario(const Terrain &terrain, svanes::Registry &world)
    : Scenario(terrain) {
    simulation = std::make_unique<Simulation>(
        world, InfluenceMap(control.columns, control.rows, control.cell_size,
                            control.origin));
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

const std::map<svanes::Entity, svanes::Transform> &Scenario::GetUnits() const {
    return units;
}

const ControlGrid &Scenario::GetControl() const { return control; }

svanes::Entity Scenario::SpawnUnit(svanes::Vector2D position, float speed) {
    auto &world = GetSimulation().world;
    Unit unit(world, position, speed);
    const auto id = unit.GetEntity();
    try {
        world.AddComponent<Unit>(id, std::move(unit));
        units.emplace(id, world.GetComponent<svanes::Transform>(id));
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
    server.influence.Fade(static_cast<float>(delta_tics) /
                              svanes::TicsPerSecond,
                          kControlHalfLifeSeconds);
    const bool stamp = delta_tics >= server.tics_until_stamp;
    server.tics_until_stamp =
        stamp ? kControlStampInterval : server.tics_until_stamp - delta_tics;

    // Resolve arrivals before stamping influence at each unit's position.
    for (auto &[id, transform] : units) {
        server.world.GetComponent<Unit>(id).PhysicsUpdate(server.world);
        transform = server.world.GetComponent<svanes::Transform>(id);
        if (stamp) {
            server.influence.Stamp(
                {transform.x, transform.y}, kControlStampValue,
                kControlStampRadius,
                [](svanes::Vector2D, svanes::Vector2D, float distance) {
                    return 1.0F - distance;
                });
        }
    }

    // Convert accumulated influence into the control tiers sent to clients.
    for (std::uint32_t y = 0; y < control.rows; ++y) {
        for (std::uint32_t x = 0; x < control.columns; ++x) {
            const float value = server.influence.ReadCell(x, y);
            const auto tier =
                std::ranges::upper_bound(kControlThresholds, value) -
                kControlThresholds.begin();
            control.tiers[static_cast<std::size_t>(y) * control.columns + x] =
                static_cast<std::uint8_t>(tier);
        }
    }
}

svanes::NetworkMessage Scenario::MakeSnapshot() const {
    if (!std::in_range<std::uint32_t>(units.size())) {
        throw std::length_error("Too many units for a scenario snapshot.");
    }

    svanes::MessageWriter writer;
    writer.WriteUint32(static_cast<std::uint32_t>(units.size()));
    for (const auto &[id, transform] : units) {
        writer.WriteUint32(id);
        writer.WriteFloat32(transform.x);
        writer.WriteFloat32(transform.y);
        writer.WriteFloat32(transform.rotation);
    }

    writer.WriteBytes(std::as_bytes(std::span{control.tiers}));
    return writer.Finish();
}

void Scenario::ApplySnapshot(const svanes::NetworkMessage &message) {
    if (simulation) {
        throw std::logic_error(
            "A server scenario cannot receive a client view.");
    }

    svanes::MessageReader reader(message);
    const auto count = reader.ReadUint32();
    if (reader.Remaining() < control.tiers.size() ||
        reader.Remaining() - control.tiers.size() !=
            static_cast<std::uint64_t>(count) * kUnitRecordBytes) {
        throw std::invalid_argument(
            "Scenario snapshot has an invalid roster or control grid size.");
    }

    // Keep the previous roster and tiers together until every field is valid.
    std::map<svanes::Entity, svanes::Transform> received;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto id = reader.ReadUint32();
        const svanes::Transform transform{.x = reader.ReadFloat32(),
                                          .y = reader.ReadFloat32(),
                                          .rotation = reader.ReadFloat32()};
        if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
            !std::isfinite(transform.rotation)) {
            throw std::invalid_argument(
                "Scenario snapshot contains a nonfinite unit transform.");
        }

        if (!received.emplace(id, transform).second) {
            throw std::invalid_argument(
                "Scenario snapshot contains a duplicate unit ID.");
        }
    }

    const auto tiers = reader.ReadBytes(control.tiers.size());
    if (std::ranges::any_of(tiers, [](std::byte tier) {
            return std::to_integer<std::uint8_t>(tier) >
                   kControlThresholds.size();
        })) {
        throw std::invalid_argument(
            "Scenario snapshot contains an invalid control tier.");
    }

    std::ranges::copy(tiers,
                      std::as_writable_bytes(std::span{control.tiers}).begin());
    units.swap(received);
}

} // namespace marching_orders
