#include <svanes/network/component_serialization.hpp>

namespace svanes {

/**
 * Writes an optional float as a presence flag followed by the value if set.
 * @param writer The message to append to.
 * @param value The value to write.
 */
static void WriteOptionalFloat(MessageWriter &writer,
                               const std::optional<float> &value) {
    writer.WriteBool(value.has_value());
    if (value) {
        writer.WriteFloat32(*value);
    }
}

/**
 * @param reader The message positioned at a value written by
 * WriteOptionalFloat.
 * @return The value, or std::nullopt if it was not set.
 */
static std::optional<float> ReadOptionalFloat(MessageReader &reader) {
    if (reader.ReadBool()) {
        return reader.ReadFloat32();
    }
    return std::nullopt;
}

/**
 * Writes a rational number's numerator and denominator.
 * @param writer The message to append to.
 * @param value The value to write.
 */
static void WriteRational(MessageWriter &writer, RationalNumber value) {
    writer.WriteUint64(value.GetNumerator());
    writer.WriteUint64(value.GetDenominator());
}

/**
 * @param reader The message positioned at a value written by WriteRational.
 * @return The value.
 * @throws std::invalid_argument for a zero denominator.
 */
static RationalNumber ReadRational(MessageReader &reader) {
    const auto numerator = reader.ReadUint64();
    return RationalNumber{numerator, reader.ReadUint64()};
}

void WriteTransform(MessageWriter &writer, const Transform &transform) {
    writer.WriteFloat32(transform.x);
    writer.WriteFloat32(transform.y);
    writer.WriteFloat32(transform.rotation);
}

Transform ReadTransform(MessageReader &reader) {
    Transform transform;
    transform.x = reader.ReadFloat32();
    transform.y = reader.ReadFloat32();
    transform.rotation = reader.ReadFloat32();
    return transform;
}

void WriteKinematic(MessageWriter &writer, const Kinematic2D &motion) {
    writer.WriteFloat32(motion.velocity_x);
    writer.WriteFloat32(motion.velocity_y);
    writer.WriteFloat32(motion.acceleration_x);
    writer.WriteFloat32(motion.acceleration_y);
    writer.WriteFloat32(motion.angular_velocity);
    writer.WriteFloat32(motion.angular_acceleration);
    WriteOptionalFloat(writer, motion.max_speed);
    WriteOptionalFloat(writer, motion.max_acceleration);
    WriteOptionalFloat(writer, motion.max_angular_speed);
    WriteOptionalFloat(writer, motion.max_angular_acceleration);
}

Kinematic2D ReadKinematic(MessageReader &reader) {
    Kinematic2D motion;
    motion.velocity_x = reader.ReadFloat32();
    motion.velocity_y = reader.ReadFloat32();
    motion.acceleration_x = reader.ReadFloat32();
    motion.acceleration_y = reader.ReadFloat32();
    motion.angular_velocity = reader.ReadFloat32();
    motion.angular_acceleration = reader.ReadFloat32();
    motion.max_speed = ReadOptionalFloat(reader);
    motion.max_acceleration = ReadOptionalFloat(reader);
    motion.max_angular_speed = ReadOptionalFloat(reader);
    motion.max_angular_acceleration = ReadOptionalFloat(reader);
    return motion;
}

void WriteSpriteAnimation(MessageWriter &writer,
                          const SpriteAnimation &animation) {
    writer.WriteInt32(animation.frame_width);
    writer.WriteInt32(animation.frame_height);
    writer.WriteInt32(animation.frame_count);
    writer.WriteInt32(animation.current_frame);
    writer.WriteUint64(animation.tics_per_frame);
    writer.WriteUint64(animation.elapsed_tics);
}

SpriteAnimation ReadSpriteAnimation(MessageReader &reader) {
    SpriteAnimation animation;
    animation.frame_width = reader.ReadInt32();
    animation.frame_height = reader.ReadInt32();
    animation.frame_count = reader.ReadInt32();
    animation.current_frame = reader.ReadInt32();
    animation.tics_per_frame = reader.ReadUint64();
    animation.elapsed_tics = reader.ReadUint64();
    return animation;
}

void WriteColor(MessageWriter &writer, const Color &color) {
    writer.WriteUint8(color.red);
    writer.WriteUint8(color.green);
    writer.WriteUint8(color.blue);
    writer.WriteUint8(color.alpha);
}

Color ReadColor(MessageReader &reader) {
    Color color;
    color.red = reader.ReadUint8();
    color.green = reader.ReadUint8();
    color.blue = reader.ReadUint8();
    color.alpha = reader.ReadUint8();
    return color;
}

void WriteRectangle(MessageWriter &writer, const Rectangle2D &rectangle) {
    writer.WriteFloat32(rectangle.x);
    writer.WriteFloat32(rectangle.y);
    writer.WriteFloat32(rectangle.width);
    writer.WriteFloat32(rectangle.height);
}

Rectangle2D ReadRectangle(MessageReader &reader) {
    Rectangle2D rectangle;
    rectangle.x = reader.ReadFloat32();
    rectangle.y = reader.ReadFloat32();
    rectangle.width = reader.ReadFloat32();
    rectangle.height = reader.ReadFloat32();
    return rectangle;
}

void WriteTimeline(MessageWriter &writer, const Timeline &timeline) {
    WriteRational(writer, timeline.GetTicSize());
    writer.WriteBool(timeline.IsPaused());
    writer.WriteUint64(timeline.GetTotalTics());
    writer.WriteUint64(timeline.GetDeltaTics());
    WriteRational(writer, timeline.GetIncompleteProgress());
}

Timeline ReadTimeline(MessageReader &reader, std::optional<Entity> parent) {
    Timeline timeline(parent, ReadRational(reader));
    if (reader.ReadBool()) {
        timeline.Pause();
    }
    const auto total = reader.ReadUint64();
    const auto delta = reader.ReadUint64();
    timeline.SetProgress(total, delta, ReadRational(reader));
    return timeline;
}

} // namespace svanes
