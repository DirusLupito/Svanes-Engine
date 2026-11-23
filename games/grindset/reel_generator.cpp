#include "reel_generator.hpp"

#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>

namespace grindset {

namespace {

constexpr float kHueShift = 50.0F;
constexpr float kTopSaturation = 0.65F;
constexpr float kTopValue = 0.75F;
constexpr float kBottomSaturation = 0.7F;
constexpr float kBottomValue = 0.4F;

constexpr svanes::Color kFactTop{.red = 18, .green = 22, .blue = 46};
constexpr svanes::Color kFactBottom{.red = 44, .green = 18, .blue = 62};
constexpr svanes::Color kFirstChoice{.red = 196, .green = 48, .blue = 64};
constexpr svanes::Color kSecondChoice{.red = 40, .green = 92, .blue = 196};
constexpr svanes::Color kSunsetTop{.red = 248, .green = 150, .blue = 70};
constexpr svanes::Color kSunsetBottom{.red = 92, .green = 40, .blue = 112};

/**
 * Turns a hue, saturation, and value into a color.
 *
 * @param hue The hue, in degrees. Any value is wrapped into 0 to 360.
 * @param saturation The saturation, from 0 to 1.
 * @param value The brightness, from 0 to 1.
 * @return The color, fully opaque.
 */
svanes::Color FromHsv(float hue, float saturation, float value) {
    const float wrapped = std::fmod(std::fmod(hue, 360.0F) + 360.0F, 360.0F);
    const float chroma = value * saturation;
    const float sector = wrapped / 60.0F;
    const float rising =
        chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    if (sector < 1.0F) {
        red = chroma;
        green = rising;
    } else if (sector < 2.0F) {
        red = rising;
        green = chroma;
    } else if (sector < 3.0F) {
        green = chroma;
        blue = rising;
    } else if (sector < 4.0F) {
        green = rising;
        blue = chroma;
    } else if (sector < 5.0F) {
        red = rising;
        blue = chroma;
    } else {
        red = chroma;
        blue = rising;
    }
    const float base = value - chroma;
    const auto channel = [base](float amount) {
        return static_cast<std::uint8_t>(std::lround((amount + base) * 255.0F));
    };
    return {
        .red = channel(red), .green = channel(green), .blue = channel(blue)};
}

/**
 * Splits a line at its " | " separator.
 *
 * @param line The line.
 * @return The text before and after the separator.
 * @throws std::runtime_error if the line has no separator.
 */
std::pair<std::string, std::string> SplitPair(const std::string &line) {
    const std::string separator = " | ";
    const std::size_t at = line.find(separator);
    if (at == std::string::npos) {
        throw std::runtime_error("The reel line \"" + line +
                                 "\" needs two parts separated by \" | \".");
    }
    return {line.substr(0, at), line.substr(at + separator.size())};
}

} // namespace

ReelGenerator::ReelGenerator(const std::string &text_directory)
    : pov(text_directory + "/pov.txt"), facts(text_directory + "/facts.txt"),
      would_you_rather(text_directory + "/would_you_rather.txt"),
      brainrot(text_directory + "/brainrot.txt"),
      emoji_stories(text_directory + "/emoji_stories.txt"),
      guru(text_directory + "/guru.txt"), seed(std::random_device{}()) {}

Reel ReelGenerator::Generate(std::uint32_t reel) const {
    std::seed_seq sequence{seed, reel};
    std::mt19937 random(sequence);
    std::uniform_int_distribution<std::uint32_t> pick_format(
        0, static_cast<std::uint32_t>(ReelFormat::Count) - 1);
    std::uniform_real_distribution<float> pick_hue(0.0F, 360.0F);
    const auto format = static_cast<ReelFormat>(pick_format(random));
    const float hue = pick_hue(random);
    const svanes::Color top = FromHsv(hue, kTopSaturation, kTopValue);
    const svanes::Color bottom =
        FromHsv(hue + kHueShift, kBottomSaturation, kBottomValue);

    switch (format) {
    case ReelFormat::Pov:
        return {.format = format,
                .text = pov.Pick(random),
                .top_color = top,
                .bottom_color = bottom};
    case ReelFormat::FakeFact:
        return {.format = format,
                .text = facts.Pick(random),
                .top_color = kFactTop,
                .bottom_color = kFactBottom};
    case ReelFormat::WouldYouRather: {
        auto [first, second] = SplitPair(would_you_rather.Pick(random));
        return {.format = format,
                .text = std::move(first),
                .second_text = std::move(second),
                .top_color = kFirstChoice,
                .bottom_color = kSecondChoice};
    }
    case ReelFormat::Brainrot:
        return {.format = format,
                .text = brainrot.Pick(random),
                .top_color = top,
                .bottom_color = bottom};
    case ReelFormat::EmojiStory: {
        auto [emoji, caption] = SplitPair(emoji_stories.Pick(random));
        return {.format = format,
                .text = std::move(emoji),
                .second_text = std::move(caption),
                .top_color = top,
                .bottom_color = bottom};
    }
    case ReelFormat::Guru:
        return {.format = format,
                .text = guru.Pick(random),
                .top_color = kSunsetTop,
                .bottom_color = kSunsetBottom};
    case ReelFormat::Count:
        break;
    }
    throw std::logic_error("Picked a reel format that does not exist.");
}

} // namespace grindset
