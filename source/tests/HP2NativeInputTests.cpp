#include "HP2NativeInput.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    HP2VR::InputPayload input{};
    input.move[0] = -0.8f;
    input.move[1] = 0.9f;
    input.turn[0] = 0.61f;
    input.trigger[1] = 0.9f;
    input.buttons = HP2VR::Jump | HP2VR::SkipDialogue | HP2VR::Menu;
    const auto active = HP2NativeInput::map(input, true);
    assert(active.forward && !active.backward && active.left && !active.right);
    assert(active.cast && active.jump && active.skip && active.turn > 0.0f);
    assert(active.moveX < 0.0f && active.moveY > 0.0f);
    assert(std::sqrt(active.moveX * active.moveX + active.moveY * active.moveY) <= 1.001f);
    assert(active.menu);

    input.pointerUv[0] = 0.25f;
    input.pointerUv[1] = 0.75f;
    input.pointerValid = input.pointerPressed = 1;
    const auto menu = HP2NativeInput::map(input, true, true);
    assert(menu.menu && menu.pointerValid && menu.pointerClick);
    assert(menu.pointerU == 0.25f && menu.pointerV == 0.75f);
    assert(!menu.forward && !menu.cast && menu.turn == 0.0f);
    assert(!menu.backward && !menu.left && !menu.right && !menu.jump && !menu.skip);
    const auto lostWhileHeld = HP2NativeInput::map(input, false);
    assert(!lostWhileHeld.forward && !lostWhileHeld.backward && !lostWhileHeld.left && !lostWhileHeld.right);
    assert(!lostWhileHeld.cast && !lostWhileHeld.jump && !lostWhileHeld.skip && !lostWhileHeld.pointerClick);

    input.move[0] = 0.1f;
    input.move[1] = -0.21f;
    input.turn[0] = -0.2f;
    input.trigger[1] = 0.64f;
    input.buttons = HP2VR::Menu;
    const auto dead = HP2NativeInput::map(input, true);
    assert(!dead.forward && !dead.backward && !dead.left && !dead.right);
    assert(!dead.cast && !dead.jump && !dead.skip && dead.turn == 0.0f && dead.menu);

    const auto neutral = HP2NativeInput::map(input, false);
    assert(!neutral.forward && !neutral.cast && neutral.turn == 0.0f);

    HP2Locomotion::Turn turning;
    int accumulated{};
    for (int frame = 0; frame < 90; ++frame)
        accumulated += turning.step(1.0f, 1.0f / 90.0f, 90.0f);
    assert(std::abs(accumulated - 16384) <= 1);
    std::cout << "HP2NativeInputTests passed\n";
}
