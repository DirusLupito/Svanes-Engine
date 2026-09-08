#pragma once

#include "game_components.hpp"

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

namespace svanes {

struct FrameContext;
class Registry;

}

struct EnemyIntent {
    svanes::Vector2D move_to;
    bool fire = false;
    svanes::Vector2D aim_point;
};

class Enemy final {
public:
    void Spawn(svanes::Registry& world, svanes::Vector2D position, float size, float health);

    void Update(const svanes::FrameContext& frame, const EnemyIntent& intent);

    void ApplyDamage(svanes::Registry& world, float amount);

    svanes::Vector2D Position(svanes::Registry& world) const;

    bool IsAlive() const;

    svanes::Entity GetEntity() const;

private:
    svanes::Entity entity = 0;
    float move_speed = 700.0F;
    float fire_interval = 0.8F;
    float fire_cooldown = 0.0F;
    float bullet_speed = 600.0F;
    bool alive = false;
};
