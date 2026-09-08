#pragma once

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>

namespace svanes {

struct GameContext;
struct FrameContext;
class Registry;

}

struct Solid {};

struct GooseIntent {
    svanes::Vector2D move;
    float dash = 0.0F;
    bool jump = false;
    bool fire = false;
    svanes::Vector2D aim_point;
};

enum class GooseState : std::uint8_t {
    Idle,
    Walking,
    Flying,
};

class Goose final {
public:
    void Spawn(svanes::GameContext& context, float x, float y);

    void Update(const svanes::FrameContext& frame, const GooseIntent& intent);

    void SetState(svanes::Registry& world, GooseState next);

    void ApplyKnockback(svanes::Registry& world, svanes::Vector2D direction, float speed);

    svanes::Entity GetEntity() const;

private:
    void ResolveCollisions(svanes::Registry& world);

    svanes::Entity entity = 0;
    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle walk_texture{};
    GooseState state = GooseState::Idle;
    float time_in_state = 0.0F;
    bool grounded = false;
    bool spawned = false;
    float speed = 300.0F;
    float jump_speed = 900.0F;
    float max_fall_speed = 1500.0F;
    float fly_rise_speed = 450.0F;
    float max_fly_seconds = 2.0F;
    float fly_time_remaining = 0.0F;
    float dash_speed = 1400.0F;
    float dash_seconds = 0.15F;
    float dash_cooldown_seconds = 1.0F;
    float dash_timer = 0.0F;
    float dash_cooldown = 0.0F;
    float fire_cooldown = 0.0F;
    float knockback_timer = 0.0F;
    float invincible_timer = 0.0F;
};
