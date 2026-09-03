#pragma once

#include <string>

namespace svanes {

class IGame;

struct ApplicationSettings {
    std::string title = "Svanes Engine";
    int32_t width = 1920;
    int32_t height = 1080;
};

class Application final {
public:
    explicit Application(ApplicationSettings settings = {});

    int32_t run(IGame& game) const;

private:
    ApplicationSettings settings;
};

} // namespace svanes
