#include <svanes/application.hpp>

int main()
{
    const svanes::Application application({
        .title = "Svanes Engine Sandbox",
        .width = 1280,
        .height = 720,
    });

    return application.run();
}
