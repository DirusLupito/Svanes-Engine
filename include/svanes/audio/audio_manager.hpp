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

    /**
     * Reports how far into the currently assigned music track playback has progressed.
     * A stopped or paused track reports the position it halted at. For a looping track,
     * this is the position within the current loop, not the cumulative time played.
     * @return The playback position, in milliseconds.
     * @throws std::runtime_error if the position cannot be determined.
     */
    std::int64_t MusicPositionMilliseconds() const;

    /**
     * Reports the length of the currently assigned music track.
     * @return The track's duration, in milliseconds.
     * @throws std::runtime_error if no music is assigned, or its duration cannot be determined.
     */
    std::int64_t MusicDurationMilliseconds() const;

    /**
     * Seeks the currently assigned music track to the given position. Requires an audio
     * format that supports seeking; not all formats do.
     * @param position_milliseconds The position to seek to, in milliseconds.
     * @throws std::runtime_error if the seek fails.
     */
    void SeekMusic(std::int64_t position_milliseconds);

    /**
     * Sets the playback speed of the currently assigned music track as a ratio against its
     * normal speed. Values above/below 1.0 speed the track up/down and raise/lower its pitch
     * to match, the same way changing a record player's speed would.
     * @param ratio The playback speed ratio. Must be between 0.01 and 100.
     * @throws std::runtime_error if the ratio cannot be set.
     */
    void SetMusicPlaybackRate(float ratio);

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
