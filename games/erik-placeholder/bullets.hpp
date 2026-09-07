#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <vector>

namespace svanes {

class Registry;

}

struct Bullet {
    svanes::Entity owner = 0;
};

struct BulletHit {
    svanes::Entity target = 0;
    svanes::Entity owner = 0;
    svanes::Vector2D direction;
};

void SpawnBullet(
    svanes::Registry& world, svanes::Entity owner,
    svanes::Vector2D origin, svanes::Vector2D direction,
    float speed, svanes::Color color
);

std::vector<BulletHit> UpdateBullets(svanes::Registry& world, const svanes::Rectangle2D& bounds);
