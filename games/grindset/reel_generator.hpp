#pragma once

#include "text_lines.hpp"

#include <svanes/render/basic_render_types.hpp>

#include <cstdint>
#include <string>

namespace grindset {

/**
 * The kinds of reel there are.
 *
 * VALUES:
 * - Pov: A "POV:" line written large over a color fade.
 * - FakeFact: A made-up fact under a "Did you know?" heading.
 * - WouldYouRather: Two choices, one on each half of the screen.
 * - Brainrot: A caption over the top half, with a fake game playing itself
 * on the bottom half.
 * - EmojiStory: A row of bouncing emoji with a caption under it.
 * - Guru: A motivational quote over a sunset, with a speaker's silhouette.
 * - Count: Not a kind of reel; the number of kinds.
 */
enum class ReelFormat : std::uint8_t {
    Pov,
    FakeFact,
    WouldYouRather,
    Brainrot,
    EmojiStory,
    Guru,
    Count,
};

/**
 * What one reel shows.
 *
 * FIELDS:
 * - format: The kind of reel.
 * - text: The reel's main text: the POV line, the fact, the first choice, the
 * caption, the emoji, or the quote.
 * - second_text: The second choice for a would you rather, the caption under
 * an emoji story, and empty otherwise.
 * - top_color: The background's color at the top.
 * - bottom_color: The background's color at the bottom, or the second
 * choice's half for a would you rather.
 */
struct Reel {
    ReelFormat format;
    std::string text;
    std::string second_text;
    svanes::Color top_color;
    svanes::Color bottom_color;
};

/**
 * Makes up reels from the lines in the game's text files. A reel is made from
 * its number, so the same number always gives the same reel while the game
 * runs, and a new game gives new reels.
 *
 * Each kind of reel reads its own file in the text folder: pov.txt, facts.txt,
 * would_you_rather.txt, brainrot.txt, emoji_stories.txt, and guru.txt. A would
 * you rather line holds its two choices, and an emoji story line holds its
 * emoji and its caption, separated by " | ".
 */
class ReelGenerator final {
public:
    /**
     * Reads the text files.
     * @param text_directory The folder holding them.
     * @throws std::runtime_error if a file cannot be read or has no lines.
     */
    explicit ReelGenerator(const std::string &text_directory);

    /**
     * Makes up a reel.
     * @param reel The reel's number.
     * @return What the reel shows.
     * @throws std::runtime_error if a would you rather or emoji story line
     * has no " | " separator.
     */
    Reel Generate(std::uint32_t reel) const;

private:
    TextLines pov;
    TextLines facts;
    TextLines would_you_rather;
    TextLines brainrot;
    TextLines emoji_stories;
    TextLines guru;
    std::uint32_t seed;
};

} // namespace grindset
