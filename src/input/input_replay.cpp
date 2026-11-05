#include <svanes/events/replay_session.hpp>

#include <svanes/input/input_events.hpp>

#include <cmath>

namespace svanes {

void ReplaySession::RegisterInputEvents() {

    // Keyboard input
    RegisterEvent<KeyInputEvent>(
        [](MessageWriter &writer, const KeyInputEvent &event) {
            writer.WriteUint8(static_cast<std::uint8_t>(event.key));
            writer.WriteBool(event.down);
        },
        [](MessageReader &reader) {
            const auto key = reader.ReadUint8();
            const auto down = reader.ReadBool();
            if (key >= static_cast<std::uint8_t>(Key::Count)) {
                throw std::invalid_argument("Invalid replay key.");
            }
            return std::make_unique<KeyInputEvent>(static_cast<Key>(key), down);
        });

    // Mouse button input
    RegisterEvent<MouseButtonInputEvent>(
        [](MessageWriter &writer, const MouseButtonInputEvent &event) {
            writer.WriteUint8(static_cast<std::uint8_t>(event.button));
            writer.WriteBool(event.down);
        },
        [](MessageReader &reader) {
            const auto button = reader.ReadUint8();
            const auto down = reader.ReadBool();
            if (button >= static_cast<std::uint8_t>(MouseButton::Count)) {
                throw std::invalid_argument("Invalid replay mouse button.");
            }
            return std::make_unique<MouseButtonInputEvent>(
                static_cast<MouseButton>(button), down);
        });

    // Mouse motion input
    RegisterEvent<MouseMotionInputEvent>(
        [](MessageWriter &writer, const MouseMotionInputEvent &event) {
            writer.WriteFloat32(event.position.x);
            writer.WriteFloat32(event.position.y);
            writer.WriteFloat32(event.delta.x);
            writer.WriteFloat32(event.delta.y);
        },
        [](MessageReader &reader) {
            const auto x = reader.ReadFloat32();
            const auto y = reader.ReadFloat32();
            const auto dx = reader.ReadFloat32();
            const auto dy = reader.ReadFloat32();
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx) ||
                !std::isfinite(dy)) {
                throw std::invalid_argument("Invalid replay mouse motion.");
            }
            return std::make_unique<MouseMotionInputEvent>(Vector2D{x, y},
                                                           Vector2D{dx, dy});
        });
}

} // namespace svanes
