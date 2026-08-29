#pragma once

#include <string>

namespace svanes {

class IGame;

struct ApplicationSettings {
    std::string title = "Svanes Engine";
    int width = 1920;
    int height = 1080;
};

class Application final {
public:
    explicit Application(ApplicationSettings settings = {});

    [[nodiscard]] int run(IGame& game) const;

private:
    ApplicationSettings settings_;
};

} // namespace svanes
