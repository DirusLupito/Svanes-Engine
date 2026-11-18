#include <svanes/input/input_manager.hpp>

#include <svanes/input/input_events.hpp>

#include <algorithm>

namespace svanes {

void InputManager::SetChordWindow(TicCount tics) {
    chordWindow = tics;
    if (tics == 0) {
        chordHistory.clear();
        return;
    }

    TrimChordHistory();
    if (chordHistory.empty()) {
        chordHistory.push_back(CurrentChordSample());
    }
}

Chord::Sample InputManager::CurrentChordSample() const {
    Chord::Sample sample{sampleTics, {}, {}};
    for (std::size_t i = 0; i < kKeyCount; ++i) {
        sample.before[i] = previous[i];
        sample.down[i] = current[i];
    }
    return sample;
}

void InputManager::TrimChordHistory() {
    if (sampleTics > chordWindow) {
        const auto oldest = sampleTics - chordWindow;
        const auto keep = std::ranges::lower_bound(chordHistory, oldest, {},
                                                   &Chord::Sample::tics);
        chordHistory.erase(chordHistory.begin(), keep);
    }
}

bool InputManager::WasTriggered(const Chord &chord) const {
    if (current == previous) {
        return false;
    }
    if (chordHistory.empty()) {
        const auto currentPoll = CurrentChordSample();
        return chord.TriggeredBy({&currentPoll, 1});
    }
    return chord.TriggeredBy(chordHistory);
}

// To test if something is down, we can just look at the current state.
// If it's down, it's down :)

// To test if something was just pressed, we can add in the previous state.
// If it's down now, but was not down in the previous frame, then it was just
// pressed.

// Symmetrically, to test if something was just released, we can see if
// it's up now, but was down in the previous frame. If so, it was just released.

bool InputManager::IsDown(Key key) const {
    return current.at(static_cast<std::size_t>(key));
}

bool InputManager::WasPressed(Key key) const {
    const std::size_t index = static_cast<std::size_t>(key);
    return current.at(index) && !previous.at(index);
}

bool InputManager::WasReleased(Key key) const {
    const std::size_t index = static_cast<std::size_t>(key);
    return !current.at(index) && previous.at(index);
}

bool InputManager::IsMouseButtonDown(MouseButton button) const {
    return mouse_current.at(static_cast<std::size_t>(button));
}

bool InputManager::WasMouseButtonPressed(MouseButton button) const {
    const std::size_t index = static_cast<std::size_t>(button);
    return mouse_current.at(index) && !mouse_previous.at(index);
}

bool InputManager::WasMouseButtonReleased(MouseButton button) const {
    const std::size_t index = static_cast<std::size_t>(button);
    return !mouse_current.at(index) && mouse_previous.at(index);
}

// POJO slop... but in C++

Vector2D InputManager::MousePosition() const { return mouse_position; }

Vector2D InputManager::MouseDeltaThisFrame() const {
    return mouse_delta_this_frame;
}

void InputManager::HandleKeyInput(const KeyInputEvent &event) {
    const auto index = static_cast<std::size_t>(event.key);
    current.at(index) = event.down;
    if (!chordHistory.empty()) {
        chordHistory.back().down.set(index, event.down);
    }
}

void InputManager::HandleMouseButtonInput(const MouseButtonInputEvent &event) {
    mouse_current.at(static_cast<std::size_t>(event.button)) = event.down;
}

void InputManager::HandleMouseMotionInput(const MouseMotionInputEvent &event) {
    mouse_position = event.position;
    mouse_delta_this_frame += event.delta;
}

} // namespace svanes
