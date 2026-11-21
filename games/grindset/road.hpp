#pragma once

#include <svanes/registry.hpp>

namespace grindset {

/**
 * The view out the windshield: a sunrise sky with the sun just above the
 * horizon. The sky is a stack of thin bands fading from deep blue at the top
 * to pale orange at the horizon, since the engine has no linear gradients.
 */
class Road final {
public:
    /**
     * Creates the sky bands, the sun, and its glow.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     */
    void Initialize(svanes::Registry &world);
};

} // namespace grindset
