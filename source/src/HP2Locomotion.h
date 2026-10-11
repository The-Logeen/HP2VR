#pragma once

#include <cmath>

namespace HP2Locomotion {

inline float axis(float value, float deadzone = 0.22f) {
    if (!std::isfinite(value)) return 0.0f;
    const float magnitude = std::fabs(value);
    if (magnitude <= deadzone) return 0.0f;
    const float clamped = magnitude > 1.0f ? 1.0f : magnitude;
    return std::copysign((clamped - deadzone) / (1.0f - deadzone), value);
}

// Same frame-rate-independent smooth-turn accumulator used by HP1VR V31.
// UE1 represents a full revolution as 65536 integer rotation units.
struct Turn {
    float fraction{};

    int step(float input, float seconds, float degreesPerSecond = 90.0f) {
        if (!std::isfinite(input) || !std::isfinite(seconds) ||
            !std::isfinite(degreesPerSecond) || seconds < 0.0f ||
            degreesPerSecond < 0.0f) return 0;
        if (seconds > 0.1f) seconds = 0.1f;
        fraction += axis(input) * degreesPerSecond * seconds * (65536.0f / 360.0f);
        const int amount = static_cast<int>(fraction);
        fraction -= static_cast<float>(amount);
        return amount;
    }
};

} // namespace HP2Locomotion
