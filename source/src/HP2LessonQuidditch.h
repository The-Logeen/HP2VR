#pragma once

#include <algorithm>
#include <cmath>

namespace HP2LessonQuidditch {

// HP2's SpellLessonTrigger accepts only these four discrete events.  A VR
// trace may recognize the event, but SpellLessonTrigger remains responsible
// for deciding whether its moving wand is inside the native hit window.
enum class LessonDirection {
    None,
    Up,
    Down,
    Left,
    Right,
};

// Converts motion on a locked two-dimensional lesson plane into one discrete
// HP2 direction at a time.  Coordinates are plane-local metres.  This class
// deliberately knows nothing about score, interpolation points, or lessons.
struct LessonTraceRecognizer {
    float anchorX{};
    float anchorY{};
    float minimumTravel{0.035f};
    float axisDominance{1.15f};
    bool anchored{};

    void reset() { anchored = false; }

    LessonDirection update(float x, float y, bool drawing, bool live = true) {
        if (!live || !drawing || !std::isfinite(x) || !std::isfinite(y)) {
            reset();
            return LessonDirection::None;
        }
        if (!anchored) {
            anchorX = x;
            anchorY = y;
            anchored = true;
            return LessonDirection::None;
        }

        const float dx = x - anchorX;
        const float dy = y - anchorY;
        const float ax = std::fabs(dx);
        const float ay = std::fabs(dy);
        if (std::max(ax, ay) < minimumTravel) return LessonDirection::None;

        LessonDirection direction = LessonDirection::None;
        if (ax >= ay * axisDominance)
            direction = dx < 0.0f ? LessonDirection::Left : LessonDirection::Right;
        else if (ay >= ax * axisDominance)
            direction = dy < 0.0f ? LessonDirection::Down : LessonDirection::Up;

        // Do not let a diagonal or tracking jump accumulate forever.  A new
        // cardinal segment starts from the latest sampled point.
        if (direction != LessonDirection::None ||
            std::max(ax, ay) >= minimumTravel * 2.0f) {
            anchorX = x;
            anchorY = y;
        }
        return direction;
    }
};

inline float shapedAxis(float value, float deadzone = 0.22f) {
    if (!std::isfinite(value)) return 0.0f;
    const float magnitude = std::fabs(value);
    if (magnitude <= deadzone) return 0.0f;
    const float denominator = std::max(0.001f, 1.0f - deadzone);
    const float shaped = std::min(1.0f, (magnitude - deadzone) / denominator);
    return value < 0.0f ? -shaped : shaped;
}

// These are semantic target-space rates, not a second flight heading.  In an
// HP2 match BroomHarry.PlayerTrack consumes them as horizontal/vertical
// offsets around the Snitch.  In free flight BroomHarry.PlayerMove consumes
// the same native inputs as yaw/pitch controls.
struct BroomCommand {
    float right{};
    float up{};
    bool nativeAction{};
};

inline BroomCommand broomFromStick(float x, float y, bool actionHeld,
                                   bool live = true) {
    if (!live) return {};
    return {shapedAxis(x), shapedAxis(y), actionHeld};
}

// Mirrors the live QuidditchDirector.GamePlay catch condition.  This is an
// input eligibility check only: a successful physical reach must still emit
// BroomHarry's normal action edge so the director calls CatchTarget itself.
inline bool nativeCatchEligible(bool canReachForSnitch, float progressPercent,
                                bool finalMatch, bool inTrench) {
    return canReachForSnitch && std::isfinite(progressPercent) &&
           progressPercent > 98.0f && (!finalMatch || inTrench);
}

struct ActionEdge {
    bool held{};

    void reset() { held = false; }

    bool update(bool pressed, bool live = true) {
        if (!live) {
            reset();
            return false;
        }
        const bool rising = pressed && !held;
        held = pressed;
        return rising;
    }
};

} // namespace HP2LessonQuidditch
