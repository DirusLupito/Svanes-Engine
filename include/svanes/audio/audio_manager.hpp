/**
 * Header file for the public interface of the AudioManager class.
 * Anything here can be used by external game code to manage audio.
 * @file audio_manager.hpp
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

struct MIX_Mixer;
struct MIX_Audio;
struct MIX_Track;

namespace svanes {

namespace internal {

class AudioManagerInternal;

}

/**
 * Lookup key for a game sound
 * 
 * - id: This game sounds unique ID
 */
struct SoundHandle {
    std::uint32_t id = 0;
};

/**
 * Lookup key for game music
 * 
 * - id: This musics unique ID
 */
struct MusicHandle {
    std::uint32_t id = 0;
};

class AudioManager final {
public:
    ~AudioManager();

    /**
     * Loads a game sound from a file and stores it in the manager
     * Throws an exception if the path is empty or if the audio cannot be loaded.
     * @param path The file path to the sound audio.
     * @return A SoundHandle that can be used to reference the loaded sound.
     * @throws std::invalid_argument if the path is empty.
     * @throws std::runtime_error if the audio cannot be loaded.
     */
    SoundHandle LoadSound(std::string_view path);

     /**
     * Loads game music from a file and stores it in the manager
     * Throws an exception if the path is empty or if the audio cannot be loaded.
     * @param path The file path to the music audio.
     * @return A MusicHandle that can be used to reference the loaded music.
     * @throws std::invalid_argument if the path is empty.
     * @throws std::runtime_error if the audio cannot be loaded.
     */
    MusicHandle LoadMusic(std::string_view path);

    /**
     *
     * @param sound
     */
    void PlaySound(SoundHandle sound);

    /**
     *
     * @param music
     * @param loop
     */
    void PlayMusic(MusicHandle music, bool loop = true);

    void StopMusic();

    /**
     *
     * @param volume
     */
    void SetMusicVolume(float volume);

private:
    struct MixerDeleter {
        void operator()(MIX_Mixer* mixer) const;
    };
    struct AudioDeleter {
        void operator()(MIX_Audio* audio) const;
    };
    struct TrackDeleter {
        void operator()(MIX_Track* track) const;
    };

    using MixerPointer = std::unique_ptr<MIX_Mixer, MixerDeleter>;
    using AudioPointer = std::unique_ptr<MIX_Audio, AudioDeleter>;
    using TrackPointer = std::unique_ptr<MIX_Track, TrackDeleter>;

    AudioManager();

    SoundHandle StoreSound(AudioPointer audio);
    MusicHandle StoreMusic(AudioPointer audio);

    MixerPointer mixer;
    TrackPointer music_track;
    std::uint32_t next_sound_id = 1;
    std::uint32_t next_music_id = 1;
    std::unordered_map<std::uint32_t, AudioPointer> sounds;
    std::unordered_map<std::uint32_t, AudioPointer> music_tracks;

    friend class internal::AudioManagerInternal;
};

}
