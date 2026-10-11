#include "HP2LessonQuidditch.h"

#include <cassert>
#include <iostream>

using namespace HP2LessonQuidditch;

int main() {
    LessonTraceRecognizer trace{};
    assert(trace.update(0.0f, 0.0f, true) == LessonDirection::None);
    assert(trace.update(0.040f, 0.002f, true) == LessonDirection::Right);
    assert(trace.update(0.041f, 0.041f, true) == LessonDirection::Up);
    assert(trace.update(0.000f, 0.043f, true) == LessonDirection::Left);
    assert(trace.update(-0.002f, 0.000f, true) == LessonDirection::Down);

    trace.reset();
    assert(trace.update(0.0f, 0.0f, true) == LessonDirection::None);
    assert(trace.update(0.040f, 0.040f, true) == LessonDirection::None);
    assert(trace.update(0.080f, 0.040f, true) == LessonDirection::Right);
    assert(trace.update(0.10f, 0.10f, false) == LessonDirection::None);
    assert(trace.update(0.10f, 0.10f, true, false) == LessonDirection::None);

    const auto broom = broomFromStick(0.75f, -0.60f, true);
    assert(broom.right > 0.0f && broom.up < 0.0f && broom.nativeAction);
    const auto neutral = broomFromStick(0.9f, 0.9f, true, false);
    assert(neutral.right == 0.0f && neutral.up == 0.0f && !neutral.nativeAction);

    assert(nativeCatchEligible(true, 98.01f, false, false));
    assert(!nativeCatchEligible(true, 98.0f, false, false));
    assert(!nativeCatchEligible(false, 100.0f, false, false));
    assert(!nativeCatchEligible(true, 100.0f, true, false));
    assert(nativeCatchEligible(true, 100.0f, true, true));

    ActionEdge edge{};
    assert(edge.update(true));
    assert(!edge.update(true));
    assert(!edge.update(false));
    assert(edge.update(true));
    assert(!edge.update(true, false));
    assert(edge.update(true));

    std::cout << "HP2LessonQuidditchTests passed\n";
}
