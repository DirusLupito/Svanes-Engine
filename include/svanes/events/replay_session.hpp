#pragma once

#include <svanes/events/event.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/timeline_system.hpp>

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace svanes {

class EventManager;

class StartRecordingEvent final : public Event {
public:
    explicit StartRecordingEvent(std::filesystem::path path);
    static std::string GetStaticEventType();
    const std::string GetEventType() const override;
    std::filesystem::path path;
};

class StopRecordingEvent final : public Event {
public:
    static std::string GetStaticEventType();
    const std::string GetEventType() const override;
};

class PlayReplayEvent final : public Event {
public:
    explicit PlayReplayEvent(std::filesystem::path path);
    static std::string GetStaticEventType();
    const std::string GetEventType() const override;
    std::filesystem::path path;
};

class PauseReplayEvent final : public Event {
public:
    static std::string GetStaticEventType();
    const std::string GetEventType() const override;
};

class ResumeReplayEvent final : public Event {
public:
    static std::string GetStaticEventType();
    const std::string GetEventType() const override;
};

class StopReplayEvent final : public Event {
public:
    static std::string GetStaticEventType();
    const std::string GetEventType() const override;
};

class ReplaySession final {
public:
    enum class State { Idle, Recording, Playing, Paused };

    struct Callbacks {
        std::function<NetworkMessage()> save;
        std::function<void(std::span<const std::byte>)> restore;
        std::function<void(TicCount)> step;
    };

    ReplaySession(EventManager &manager, TicCount step_tics,
                  Callbacks callbacks);
    ~ReplaySession();
    ReplaySession(const ReplaySession &) = delete;
    ReplaySession &operator=(const ReplaySession &) = delete;
    ReplaySession(ReplaySession &&) = delete;
    ReplaySession &operator=(ReplaySession &&) = delete;

    template <std::derived_from<Event> T, typename Encode, typename Decode>
    void RegisterEvent(Encode encode, Decode decode) {
        RegisterCodec(
            T::GetStaticEventType(),
            Codec{[encode = std::move(encode)](MessageWriter &writer,
                                               const Event &event) {
                      encode(writer, dynamic_cast<const T &>(event));
                  },
                  [decode = std::move(decode)](MessageReader &reader)
                      -> std::unique_ptr<Event> { return decode(reader); }});
    }

    void RegisterInputEvents();
    void ProcessControls();
    void BeginStep();
    void EndStep();
    void Update(TicCount elapsed_tics);
    State GetState() const;
    bool IsReplaying() const;

private:
    friend class EventManager;

    enum class Action { Record, Save, Play, Pause, Resume, Stop };

    struct Control {
        Action action;
        std::filesystem::path path;
    };

    struct Codec {
        std::function<void(MessageWriter &, const Event &)> encode;
        std::function<std::unique_ptr<Event>(MessageReader &)> decode;
    };

    struct RecordedEvent {
        std::string type;
        EventPriority priority;
        NetworkMessage payload;
        std::unique_ptr<Event> decoded;
    };

    using Step = std::vector<RecordedEvent>;

    void RegisterCodec(std::string type, Codec codec);
    bool Observe(const Event &event);
    void Reset();
    void Save() const;
    void Load(const std::filesystem::path &path);
    void CheckBoundary() const;

    EventManager *manager;
    TicCount step_tics;
    Callbacks callbacks;
    State state = State::Idle;
    std::unordered_map<std::string, Codec> codecs;
    std::vector<Control> controls;
    std::filesystem::path recording_path;
    NetworkMessage initial_state;
    std::vector<Step> steps;
    Step pending_events;
    std::size_t playback_step = 0;
    TicCount accumulated_tics = 0;
    bool step_open = false;
    bool updating = false;
    bool injecting = false;
};

}
