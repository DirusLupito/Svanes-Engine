#include "unit.hpp"
#include "terrain.hpp"

#include <svanes/geometry/geometry.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace marching_orders {

constexpr float kMovementUphillPenalty = 0.005F;
constexpr float kTerrainMovementStep = 0.5F;

using TerrainCell = std::pair<std::int32_t, std::int32_t>;

// Helper that returns the cell at a given world coordinate.
static std::optional<TerrainCell> CellAt(const Terrain &terrain,
                                         svanes::Vector2D position) {
    const double x = static_cast<double>(position.x) + terrain.Width() * 0.5;
    const double y = static_cast<double>(position.y) + terrain.Height() * 0.5;
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0 || y < 0.0 ||
        x >= terrain.Width() || y >= terrain.Height()) {
        return std::nullopt;
    }
    return TerrainCell{static_cast<std::int32_t>(std::floor(x)),
                       static_cast<std::int32_t>(std::floor(y))};
}

// Helper for checking that terrain cells are traversable
static bool CanTraverse(const Terrain &terrain, TerrainCell from,
                        TerrainCell to) {
    return !terrain.IsWaterAt(from.first, from.second) &&
           !terrain.IsWaterAt(to.first, to.second);
}

Unit::Unit(svanes::Registry &world, const Terrain &terrain,
           svanes::Vector2D position, float speed, std::uint32_t team)
    : terrain(terrain), previous_position(position),
      speed(svanes::PerSecondToPerTic(speed)), team(team) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(speed) || speed <= 0.0F) {
        throw std::invalid_argument(
            "Unit requires a finite position and positive speed.");
    }
    const auto cell = CellAt(terrain, position);
    if (!cell || terrain.IsWaterAt(cell->first, cell->second)) {
        throw std::invalid_argument("Unit must spawn on land within the map.");
    }
    entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::Kinematic2D>(entity);
}

void Unit::SetDestination(svanes::Registry &world, svanes::Vector2D position) {
    const auto cell = CellAt(terrain, position);
    if (!cell || terrain.IsWaterAt(cell->first, cell->second)) {
        throw std::invalid_argument(
            "Unit destination must be on land within the map.");
    }
    destination = position;
    UpdateVelocity(world);
}

// Reduces unit velocity when traversing cells with significant elevation difference
void Unit::UpdateVelocity(svanes::Registry &world) {
    auto &motion = world.GetComponent<svanes::Kinematic2D>(entity);
    motion.velocity_x = 0.0F;
    motion.velocity_y = 0.0F;
    if (!destination) {
        return;
    }
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    const svanes::Vector2D position{transform.x, transform.y};
    const auto offset = *destination - position;
    const float distance = std::hypot(offset.x, offset.y);
    if (distance == 0.0F) {
        destination.reset();
        return;
    }
    // Get the next cell in the direction of movement
    const auto direction = offset / distance;
    const auto cell = CellAt(terrain, position).value();
    const auto boundary_distance = [](float coordinate, float direction) {
        if (direction == 0.0F) {
            return std::numeric_limits<float>::infinity();
        }
        const float boundary =
            std::floor(coordinate) + (direction > 0.0F ? 1.0F : 0.0F);
        return (boundary - coordinate) / direction;
    };
    const float next_x =
        boundary_distance(position.x + terrain.Width() * 0.5F, direction.x);
    const float next_y =
        boundary_distance(position.y + terrain.Height() * 0.5F, direction.y);
    auto next = cell;
    if (next_x <= next_y) {
        next.first += direction.x > 0.0F ? 1 : -1;
    }
    if (next_y <= next_x) {
        next.second += direction.y > 0.0F ? 1 : -1;
    }
    float movement_speed = speed;
    // Calculate the move speed penalty
    if (std::min(next_x, next_y) < distance && next.first >= 0 &&
        next.first < terrain.Width() && next.second >= 0 &&
        next.second < terrain.Height()) {
        const auto rise =
            std::max(0, terrain.ElevationAt(next.first, next.second) -
                            terrain.ElevationAt(cell.first, cell.second));
        const float spacing =
            std::hypot(next.first - cell.first, next.second - cell.second);
        movement_speed /= 1.0F + kMovementUphillPenalty * rise / spacing;
    }
    transform.rotation = std::atan2(offset.y, offset.x);
    motion.velocity_x = direction.x * movement_speed;
    motion.velocity_y = direction.y * movement_speed;
}

void Unit::PhysicsUpdate(svanes::Registry &world) {
    if (!destination) {
        return;
    }
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    auto &motion = world.GetComponent<svanes::Kinematic2D>(entity);
    const auto offset =
        *destination - svanes::Vector2D{transform.x, transform.y};
    // Validate any transform updates. Check to ensure that all traversed tiles are traversable
    const bool arrived =
        offset.x * motion.velocity_x + offset.y * motion.velocity_y <= 0.0F;
    const svanes::Vector2D position =
        arrived ? *destination : svanes::Vector2D{transform.x, transform.y};
    auto previous = CellAt(terrain, previous_position);
    bool traversable = previous && CellAt(terrain, position) &&
                       !terrain.IsWaterAt(previous->first, previous->second);
    const auto movement = position - previous_position;
    const auto steps = traversable
                           ? std::max(std::uint32_t{1},
                                      static_cast<std::uint32_t>(std::ceil(
                                          std::hypot(movement.x, movement.y) /
                                          kTerrainMovementStep)))
                           : std::uint32_t{0};
    for (std::uint32_t step = 1; step <= steps; ++step) {
        const auto cell =
            CellAt(terrain, previous_position +
                                movement * (static_cast<float>(step) / steps));
        if (!cell || !CanTraverse(terrain, *previous, *cell)) {
            traversable = false;
            break;
        }
        if (cell->first != previous->first &&
            cell->second != previous->second) {
            const TerrainCell horizontal{cell->first, previous->second};
            const TerrainCell vertical{previous->first, cell->second};
            if (!CanTraverse(terrain, *previous, horizontal) ||
                !CanTraverse(terrain, *previous, vertical) ||
                !CanTraverse(terrain, horizontal, *cell) ||
                !CanTraverse(terrain, vertical, *cell)) {
                traversable = false;
                break;
            }
        }
        previous = cell;
    }
    // If the transform change was not valid, reset
    if (!traversable) {
        transform.x = previous_position.x;
        transform.y = previous_position.y;
        destination.reset();
    } else {
        transform.x = position.x;
        transform.y = position.y;
        if (arrived) {
            destination.reset();
        }
    }
    previous_position = {transform.x, transform.y};
    UpdateVelocity(world);
}

svanes::Entity Unit::GetEntity() const { return entity; }

std::uint32_t Unit::GetTeam() const { return team; }
}
