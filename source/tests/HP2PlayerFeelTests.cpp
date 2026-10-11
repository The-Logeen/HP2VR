#include "HP2PlayerFeel.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace HP2PlayerFeel;
    SceneEvidence scene{};

    auto free = decide(MovementMode::Free, scene);
    assert(free.presentation == Presentation::InBody);
    assert(free.translation == Authority::Player && free.rotation == Authority::Player);

    scene.nativeExternalCamera = true;
    scene.allowlistedCinematic = true;
    auto off = decide(MovementMode::Free, scene);
    assert(off.presentation == Presentation::InBody);
    assert(off.translation == Authority::Player && off.rotation == Authority::Player);

    scene.cinematicsEnabled = true;
    scene.allowlistedCinematic = false;
    auto unselected = decide(MovementMode::CinematicBody, scene);
    assert(unselected.presentation == Presentation::InBody);
    assert(unselected.translation == Authority::NativeScript);
    assert(unselected.rotation == Authority::Player);

    scene.allowlistedCinematic = true;
    auto selected = decide(MovementMode::Free, scene);
    assert(selected.presentation == Presentation::SelectedCinematic);
    assert(selected.translation == Authority::NativeScript);
    assert(selected.rotation == Authority::NativeScript && selected.showNativeHarry);

    scene = {};
    scene.stationaryPanel = true;
    auto panel = decide(MovementMode::Free, scene);
    assert(panel.presentation == Presentation::StationaryPanel);
    assert(panel.translation == Authority::Frozen && panel.rotation == Authority::Player);

    NudgeState nudge;
    NudgeDecision value{};
    for (int i = 0; i < 6; ++i)
        value = nudge.update(77, true, true, false, false, false, 0.1f);
    assert(!value.active);
    value = nudge.update(77, true, true, false, false, false, 0.1f);
    assert(value.active && !value.forced && value.strength > 0.0f);
    value = nudge.update(77, true, true, false, false, true, 0.01f);
    assert(!value.active);
    for (int i = 0; i < 20; ++i)
        value = nudge.update(77, true, true, false, false, false, 0.1f);
    assert(!value.active); // same gate does not repeatedly pull
    value = nudge.update(77, true, true, true, false, false, 0.1f);
    assert(value.active && value.forced);
    value = nudge.update(77, false, true, false, false, false, 0.1f);
    assert(!value.active && nudge.gateToken == 0);

    assert(intentionalStick(0.2f, 0.0f));
    assert(!intentionalStick(0.1f, 0.1f));
    assert(sweptArrival(0, 0, 0, 20, 0, 0, 10, 0, 0, 2, 4));
    assert(!sweptArrival(0, 0, 0, 400, 0, 0, 10, 0, 0, 2, 4));
    std::cout << "HP2PlayerFeelTests passed\n";
}
