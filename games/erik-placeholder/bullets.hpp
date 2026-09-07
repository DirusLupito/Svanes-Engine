#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

namespace svanes {

class Registry;

}

struct Bullet {
    svanes::Entity owner = 0;
};

void SpawnBullet(
    svanes::Registry& world, svanes::Entity owner,
    svanes::Vector2D origin, svanes::Vector2D direction,
    float speed, svanes::Color color
);

void UpdateBullets(svanes::Registry& world, const svanes::Rectangle2D& bounds);
