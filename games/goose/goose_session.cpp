#include "goose_session.hpp"

#include <stdexcept>

svanes::RationalNumber TicSize(GooseSpeed speed)
{
    switch (speed) {
    case GooseSpeed::Half:
        return svanes::RationalNumber{2};
    case GooseSpeed::Normal:
        return svanes::RationalNumber{1};
    case GooseSpeed::Double:
        return svanes::RationalNumber{1, 2};
    }
    throw std::invalid_argument("TicSize received an unknown GooseSpeed.");
}

svanes::TicCount AdvanceWorldClock(
    svanes::Timeline& clock, svanes::TicCount real_tics, ClockRequest request, bool alone
)
{
    clock.SetTicSize(TicSize(alone ? request.speed : GooseSpeed::Normal));
    if (alone && request.paused) {
        clock.Pause();
    } else {
        clock.Unpause();
    }
    clock.Advance(real_tics);
    return clock.GetDeltaTics();
}
