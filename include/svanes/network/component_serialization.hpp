#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/timeline_system.hpp>

#include <optional>

namespace svanes {

/**
 * Writes a Transform's position and rotation.
 * @param writer The message to append to.
 * @param transform The transform to write.
 */
void WriteTransform(MessageWriter &writer, const Transform &transform);

/**
 * @param reader The message positioned at a transform written by
 * WriteTransform.
 * @return The transform.
 */
Transform ReadTransform(MessageReader &reader);

/**
 * Writes every Kinematic2D field, including which limits are set.
 * @param writer The message to append to.
 * @param motion The motion to write.
 */
void WriteKinematic(MessageWriter &writer, const Kinematic2D &motion);

/**
 * @param reader The message positioned at motion written by WriteKinematic.
 * @return The motion.
 * @throws std::invalid_argument for malformed data.
 */
Kinematic2D ReadKinematic(MessageReader &reader);

/**
 * Writes every SpriteAnimation field, including playback progress.
 * @param writer The message to append to.
 * @param animation The animation to write.
 */
void WriteSpriteAnimation(MessageWriter &writer,
                          const SpriteAnimation &animation);

/**
 * @param reader The message positioned at an animation written by
 * WriteSpriteAnimation.
 * @return The animation.
 */
SpriteAnimation ReadSpriteAnimation(MessageReader &reader);

/**
 * Writes a color's four channels.
 * @param writer The message to append to.
 * @param color The color to write.
 */
void WriteColor(MessageWriter &writer, const Color &color);

/**
 * @param reader The message positioned at a color written by WriteColor.
 * @return The color.
 */
Color ReadColor(MessageReader &reader);

/**
 * Writes a rectangle's center and size.
 * @param writer The message to append to.
 * @param rectangle The rectangle to write.
 */
void WriteRectangle(MessageWriter &writer, const Rectangle2D &rectangle);

/**
 * @param reader The message positioned at a rectangle written by
 * WriteRectangle.
 * @return The rectangle.
 */
Rectangle2D ReadRectangle(MessageReader &reader);

/**
 * Writes a Timeline's tic size, pause state, and exact elapsed time. The
 * parent is a local entity, so it is not written. A caller with parented
 * timelines writes its own identity for the parent and supplies the local
 * entity to ReadTimeline.
 * @param writer The message to append to.
 * @param timeline The timeline to write.
 */
void WriteTimeline(MessageWriter &writer, const Timeline &timeline);

/**
 * @param reader The message positioned at a timeline written by WriteTimeline.
 * @param parent The local entity whose Timeline the result follows, if any.
 * @return A timeline that advances exactly as the written one would.
 * @throws std::invalid_argument for malformed data.
 */
Timeline ReadTimeline(MessageReader &reader,
                      std::optional<Entity> parent = std::nullopt);

} // namespace svanes
