#include <svanes/application.hpp>

int main()
{
    const svanes::Application application({
        .title = "Svanes Engine Poopbox",
        .width = 1920,
        .height = 1080,
    });

    return application.run();
}
