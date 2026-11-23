#include "text_lines.hpp"

#include <fstream>
#include <stdexcept>

namespace grindset {

TextLines::TextLines(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open the text file " + path + ".");
    }
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t") != std::string::npos) {
            lines.push_back(line);
        }
    }
    if (lines.empty()) {
        throw std::runtime_error("The text file " + path + " has no lines.");
    }
}

const std::string &TextLines::Pick(std::mt19937 &random) const {
    std::uniform_int_distribution<std::size_t> pick(0, lines.size() - 1);
    return lines[pick(random)];
}

} // namespace grindset
