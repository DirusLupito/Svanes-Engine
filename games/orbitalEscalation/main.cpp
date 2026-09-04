#include "orbital_escalation_game.hpp"

#include <svanes/application.hpp>

int32_t main()
{
    svanes::Application application({
        .title = "Orbital Escalation",
        .width = 1920,
        .height = 1080,
    });

    OrbitalEscalationGame game;
    return application.run(game);
}
