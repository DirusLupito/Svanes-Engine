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
    bool jump = false;
};

enum class GooseState : std::uint8_t {
    Idle,
    Walking,
};

class Goose final {
public:
    void Spawn(svanes::GameContext& context, float x, float y);

    void Update(const svanes::FrameContext& frame, const GooseIntent& intent);

    void SetState(svanes::Registry& world, GooseState next);

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
};
