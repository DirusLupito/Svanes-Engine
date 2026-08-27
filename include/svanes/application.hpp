#pragma once

#include <string>

namespace svanes {

struct ApplicationSettings {
    std::string title = "Svanes Engine";
    int width = 1280;
    int height = 720;
};

class Application final {
public:
    explicit Application(ApplicationSettings settings = {});

    [[nodiscard]] int run() const;

private:
    ApplicationSettings settings_;
};

} // namespace svanes
