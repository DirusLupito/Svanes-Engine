#pragma once

#include "terrain.hpp"

#include <svanes/entity.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/network/network_message.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace marching_orders {

enum class OrderType : std::uint8_t { Move, Spawn };

/**
 * Directs a unit to a position on the map.
 *
 * FIELDS:
 * - unit is the server entity ID from the scenario.
 * - destination is the requested position in world coordinates.
 * - Encode() builds a movement request to send to the server.
 * - Decode() reads and validates a received movement request.
 */
struct MoveOrder {
    svanes::Entity unit;
    svanes::Vector2D destination;

    /**
     * Encodes the order for delivery to the server.
     *
     * @return The movement request.
     */
    svanes::NetworkMessage Encode() const;

    /**
     * Reads a movement request from a client.
     *
     * @param message The complete request.
     *
     * @return The addressed unit and destination.
     *
     * @throws std::invalid_argument if the request is malformed.
     */
    static MoveOrder Decode(const svanes::NetworkMessage &message);
};

struct SpawnOrder {
    svanes::Vector2D position;

    svanes::NetworkMessage Encode() const;
    static SpawnOrder Decode(const svanes::NetworkMessage &message);
};

struct UnitState {
    svanes::Transform transform;
    std::uint32_t team;
};

/**
 * Describes territorial control as it is visible to players.
 *
 * FIELDS:
 * - columns gives the width in cells.
 * - rows gives the height in cells.
 * - cell_size is the side length of a cell in world units.
 * - origin is the upper-left corner of the grid in world coordinates.
 * - tiers holds the control level of each cell in row-major order.
 */
struct ControlGrid {
    static constexpr std::uint32_t kColorLevels = 16;
    static constexpr std::array<std::uint8_t, 3> kOpacities{24, 48, 72};
    static constexpr std::uint32_t kTileCount =
        kColorLevels * kColorLevels * kColorLevels * kOpacities.size();

    std::uint32_t columns;
    std::uint32_t rows;
    std::uint32_t cell_size;
    svanes::Vector2D origin;
    std::vector<std::uint16_t> tiles;
};

/**
 * Holds the state of a Marching Orders match.
 */
class Scenario {
public:
    /**
     * Creates a client view ready to receive the match state.
     *
     * @param terrain The map used throughout the match.
     */
    explicit Scenario(const Terrain &terrain);

    /**
     * Creates an authoritative match in the server's physics world.
     *
     * @param terrain The map used throughout the match.
     * @param world The registry in which units live. It must outlive the
     * scenario.
     */
    Scenario(const Terrain &terrain, svanes::Registry &world);

    /**
     * Removes the scenario's simulated units from the physics world.
     */
    ~Scenario();

    /**
     * Provides the terrain on which orders and unit positions are interpreted.
     *
     * @return The map shared by all units in the match.
     */
    const Terrain &GetTerrain() const;

    /**
     * Provides the latest unit poses in world coordinates.
     *
     * @return Unit transforms keyed by their server entity IDs.
     */
    const std::map<svanes::Entity, UnitState> &GetUnits() const;

    /**
     * Provides the current territorial control visible to players.
     *
     * @return The control grid for this match.
     */
    const ControlGrid &GetControl() const;

    /**
     * Adds a unit to the server simulation.
     *
     * @param position Initial world position.
     * @param speed Travel speed in world units per second.
     *
     * @return The server entity ID used to address the unit in orders.
     */
    svanes::Entity SpawnUnit(svanes::Vector2D position, float speed,
                             std::uint32_t team = 0);

    /**
     * Removes a unit from the server simulation and subsequent snapshots.
     *
     * @param id Server entity ID of the unit to remove.
     */
    void RemoveUnit(svanes::Entity id);

    /**
     * Issues a movement order to a unit in the server simulation.
     *
     * @param order The addressed unit and its requested destination.
     *
     * @return True if the unit exists and received the order.
     */
    bool MoveUnit(const MoveOrder &order);

    /**
     * Resolves unit arrivals and updates territorial control after physics.
     *
     * @param delta_tics Elapsed simulation time for the match, in microseconds.
     */
    void Advance(svanes::TicCount delta_tics);

    /**
     * Encodes the complete unit roster and control tiers for this map.
     *
     * @return A snapshot that a client can apply independently of earlier ones.
     */
    svanes::NetworkMessage MakeSnapshot() const;

    /**
     * Replaces a client's match state after validating the complete snapshot.
     *
     * @param message The snapshot received from the server.
     *
     * @throws std::invalid_argument if the snapshot does not describe a valid
     * match state for this map.
     */
    void ApplySnapshot(const svanes::NetworkMessage &message);

private:
    struct Simulation;

    /**
     * Provides the server's simulation state for an authoritative operation.
     *
     * @return The simulation that owns the match's physics entities.
     *
     * @throws std::logic_error if called on a client scenario.
     */
    Simulation &GetSimulation();

    // Terrain for the lifetime of the match.
    const Terrain &terrain;

    // Latest completed world transform for each server unit.
    std::map<svanes::Entity, UnitState> units;

    // Territorial control as reported to clients.
    ControlGrid control;

    // Movement and exact influence state owned by the authoritative peer.
    std::unique_ptr<Simulation> simulation;
};

} // namespace marching_orders
