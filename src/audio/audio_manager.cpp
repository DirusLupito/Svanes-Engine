#include <svanes/audio/audio_manager.hpp>

#include "audio_manager_internal.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace svanes {

void AudioManager::MixerDeleter::operator()(MIX_Mixer* mixer) const
{
    MIX_DestroyMixer(mixer);
}

void AudioManager::AudioDeleter::operator()(MIX_Audio* audio) const
{
    MIX_DestroyAudio(audio);
}

void AudioManager::TrackDeleter::operator()(MIX_Track* track) const
{
    MIX_DestroyTrack(track);
}

AudioManager::AudioManager()
{
    if (!MIX_Init()) {
        throw std::runtime_error("Could not initialize SDL_mixer: " + std::string{SDL_GetError()});
    }

    MIX_Mixer* raw_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (raw_mixer == nullptr) {
        throw std::runtime_error("Could not create audio mixer: " + std::string{SDL_GetError()});
    }
    mixer = MixerPointer{raw_mixer};

    MIX_Track* raw_track = MIX_CreateTrack(mixer.get());
    if (raw_track == nullptr) {
        throw std::runtime_error("Could not create music track: " + std::string{SDL_GetError()});
    }
    music_track = TrackPointer{raw_track};
}

AudioManager::~AudioManager()
{
    music_tracks.clear();
    sounds.clear();
    music_track.reset();
    mixer.reset();
    MIX_Quit();
}

SoundHandle AudioManager::StoreSound(AudioPointer audio)
{
    if (next_sound_id == 0) {
        throw std::runtime_error("Sound handle space is exhausted.");
    }

    const SoundHandle handle{next_sound_id++};
    sounds[handle.id] = std::move(audio);
    return handle;
}

MusicHandle AudioManager::StoreMusic(AudioPointer audio)
{
    if (next_music_id == 0) {
        throw std::runtime_error("Music handle space is exhausted.");
    }

    const MusicHandle handle{next_music_id++};
    music_tracks[handle.id] = std::move(audio);
    return handle;
}

SoundHandle AudioManager::LoadSound(std::string_view path)
{
    if (path.empty()) {
        throw std::invalid_argument("Sound path cannot be empty.");
    }

    const std::string path_string{path};
    AudioPointer audio{MIX_LoadAudio(mixer.get(), path_string.c_str(), true)};
    if (audio == nullptr) {
        throw std::runtime_error("Could not load sound '" + path_string + "': " + SDL_GetError());
    }

    return StoreSound(std::move(audio));
}

MusicHandle AudioManager::LoadMusic(std::string_view path)
{
    if (path.empty()) {
        throw std::invalid_argument("Music path cannot be empty.");
    }

    const std::string path_string{path};
    AudioPointer audio{MIX_LoadAudio(mixer.get(), path_string.c_str(), false)};
    if (audio == nullptr) {
        throw std::runtime_error("Could not load music '" + path_string + "': " + SDL_GetError());
    }

    return StoreMusic(std::move(audio));
}

void AudioManager::PlaySound(SoundHandle sound)
{
    const auto entry = sounds.find(sound.id);
    if (entry == sounds.end()) {
        throw std::invalid_argument("Sound handle does not exist.");
    }

    if (!MIX_PlayAudio(mixer.get(), entry->second.get())) {
        throw std::runtime_error("Could not play sound: " + std::string{SDL_GetError()});
    }
}

void AudioManager::PlayMusic(MusicHandle music, bool loop)
{
    const auto entry = music_tracks.find(music.id);
    if (entry == music_tracks.end()) {
        throw std::invalid_argument("Music handle does not exist.");
    }

    if (!MIX_SetTrackAudio(music_track.get(), entry->second.get())) {
        throw std::runtime_error("Could not assign music track: " + std::string{SDL_GetError()});
    }

    const SDL_PropertiesID options = SDL_CreateProperties();
    if (options == 0) {
        throw std::runtime_error("Could not create music playback options: " + std::string{SDL_GetError()});
    }

    SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0);
    const bool played = MIX_PlayTrack(music_track.get(), options);
    SDL_DestroyProperties(options);

    if (!played) {
        throw std::runtime_error("Could not play music: " + std::string{SDL_GetError()});
    }
}

void AudioManager::StopMusic()
{
    if (!MIX_StopTrack(music_track.get(), 0)) {
        throw std::runtime_error("Could not stop music: " + std::string{SDL_GetError()});
    }
}

void AudioManager::SetMusicVolume(float volume)
{
    if (!MIX_SetTrackGain(music_track.get(), volume)) {
        throw std::runtime_error("Could not set music volume: " + std::string{SDL_GetError()});
    }
}

AudioManager internal::AudioManagerInternal::Create()
{
    return AudioManager{};
}

}
