#pragma once

#include <cmath>
#include "HP2VRBridge.h"
#include "HP2Locomotion.h"

namespace HP2NativeInput {

struct Desired {
    bool forward{};
    bool backward{};
    bool left{};
    bool right{};
    bool cast{};
    bool jump{};
    bool skip{};
    bool menu{};
    bool pointerClick{};
    bool pointerValid{};
    float pointerU{};
    float pointerV{};
    float turn{};
    float moveX{};
    float moveY{};
};

inline float deadzone(float value, float threshold = 0.22f) {
    const float magnitude = std::fabs(value);
    if (magnitude <= threshold) return 0.0f;
    const float scaled = (magnitude - threshold) / (1.0f - threshold);
    return value < 0.0f ? -scaled : scaled;
}

inline Desired map(const HP2VR::InputPayload& input, bool live, bool menuOwned = false) {
    Desired output{};
    if (!live) return output;
    output.menu = (input.buttons & HP2VR::Menu) != 0;
    if (menuOwned) {
        output.pointerValid = input.pointerValid &&
            std::isfinite(input.pointerUv[0]) && std::isfinite(input.pointerUv[1]) &&
            input.pointerUv[0] >= 0.0f && input.pointerUv[0] <= 1.0f &&
            input.pointerUv[1] >= 0.0f && input.pointerUv[1] <= 1.0f;
        output.pointerU = output.pointerValid ? input.pointerUv[0] : 0.0f;
        output.pointerV = output.pointerValid ? input.pointerUv[1] : 0.0f;
        output.pointerClick = output.pointerValid && input.pointerPressed;
        return output;
    }
    float x = deadzone(input.move[0]);
    float y = deadzone(input.move[1]);
    const float length = std::sqrt(x * x + y * y);
    if (length > 1.0f) { x /= length; y /= length; }
    output.moveX = x;
    output.moveY = y;
    output.forward = y > 0.0f;
    output.backward = y < 0.0f;
    output.left = x < 0.0f;
    output.right = x > 0.0f;
    output.turn = deadzone(input.turn[0]);
    output.cast = input.trigger[1] > 0.65f;
    output.jump = (input.buttons & HP2VR::Jump) != 0;
    output.skip = (input.buttons & HP2VR::SkipDialogue) != 0;
    return output;
}

} // namespace HP2NativeInput
