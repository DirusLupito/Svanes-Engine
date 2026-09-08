/**
 * Header file for the internal interface of the AudioManager class.
 * This is not intended to be used by external game code.
 * @file audio_manager_internal.hpp
 */


#pragma once

#include <svanes/audio/audio_manager.hpp>

namespace svanes::internal {

/**
 * Internal interface for the AudioManager class.
 * This class provides methods for creating an AudioManager.
 * As it deals with raw MIX_ pointers it should not be exposed to game code.
 * The AudioManager class itself is responsible for playing sound files, including music and sound effects.
 * 
 */
class AudioManagerInternal {
public:
    /**
     * Creates an AudioManager instance using the provided SDL_Renderer.
     * @return A AudioManager instance.
     */
    static AudioManager Create();
};

}
