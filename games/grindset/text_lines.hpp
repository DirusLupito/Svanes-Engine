#pragma once

#include <random>
#include <string>
#include <vector>

namespace grindset {

/**
 * The lines of an editable text file, one entry per line, to pick from at
 * random. Blank lines are skipped, and Windows line endings are accepted.
 */
class TextLines final {
public:
    /**
     * Reads every line of a text file.
     * @param path The file to read.
     * @throws std::runtime_error if the file cannot be opened or has no lines
     * that are not blank.
     */
    explicit TextLines(const std::string &path);

    /**
     * Picks one of the lines at random.
     * @param random The random number generator to pick with.
     * @return The line.
     */
    const std::string &Pick(std::mt19937 &random) const;

private:
    std::vector<std::string> lines;
};

} // namespace grindset
