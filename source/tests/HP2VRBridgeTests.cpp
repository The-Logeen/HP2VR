#include "HP2VRBridge.h"
#include <cassert>
#include <cstring>
#include <iostream>

static HP2VR::InputPayload validInput(uint64_t when) {
    HP2VR::InputPayload value{};
    value.head.valid = 1;
    value.head.m[0] = value.head.m[5] = value.head.m[10] = 1.0f;
    value.head.sampleQpc = when;
    for (int eye = 0; eye < 2; ++eye) {
        value.eyeToHead[eye].valid = 1;
        value.eyeToHead[eye].sampleQpc = when;
        value.eyeToHead[eye].m[0] = value.eyeToHead[eye].m[5] =
            value.eyeToHead[eye].m[10] = 1.0f;
        value.eyeToHead[eye].m[3] = eye ? 0.032f : -0.032f;
        value.eyeTangents[eye][0] = -1.1f;
        value.eyeTangents[eye][1] = 1.1f;
        value.eyeTangents[eye][2] = -1.0f;
        value.eyeTangents[eye][3] = 1.0f;
    }
    value.sampleQpc = when;
    value.inputAvailable = 1;
    value.focusOwned = 1;
    value.stereoAvailable = 1;
    value.move[1] = 0.75f;
    value.trigger[1] = 1.0f;
    value.buttons = HP2VR::RightGrip;
    value.pointerUv[0] = 0.25f;
    value.pointerUv[1] = 0.75f;
    value.pointerValid = value.pointerPressed = 1;
    return value;
}

int main() {
    HP2VR::SharedState state{};
    state.qpcFrequency = 1000;
    assert(HP2VR::compatible(state));
    assert(std::wstring_view(HP2VR::MappingName).find(L"HP1") == std::wstring_view::npos);
    HP2VR::GpuSharedState gpu{};
    assert(HP2VR::compatible(gpu));
    assert(sizeof(gpu) == 64);
    assert(std::wstring_view(HP2VR::GpuMappingName).find(L"HP1") == std::wstring_view::npos);
    gpu.version++;
    assert(!HP2VR::compatible(gpu));

    auto sent = validInput(1000);
    HP2VR::publishInput(state, sent);
    HP2VR::InputPayload received{};
    assert(HP2VR::readInput(state, 1100, received));
    assert(received.move[1] == 0.75f && received.trigger[1] == 1.0f);
    assert(received.buttons == HP2VR::RightGrip && received.pointerPressed);
    assert(HP2VR::validStereo(received));

    HP2VR::FramePayload frame{};
    frame.renderedHead = sent.head;
    std::memcpy(frame.eyeTangents, sent.eyeTangents, sizeof(frame.eyeTangents));
    frame.captureSequence[0] = 7;
    frame.captureSequence[1] = 9;
    frame.width = 1024;
    frame.height = 1024;
    frame.pairNumber = 3;
    frame.stereoValid = 1;
    frame.presentation = HP2VR::Presentation::StationaryPanel;
    HP2VR::publishFrame(state, frame);
    HP2VR::FramePayload frameCopy{};
    assert(HP2VR::readFrame(state, frameCopy));
    assert(frameCopy.pairNumber == 3 && frameCopy.captureSequence[1] == 9);
    assert(frameCopy.presentation == HP2VR::Presentation::StationaryPanel);

    assert(!HP2VR::readInput(state, 1400, received));
    assert(received.move[0] == 0.0f && received.move[1] == 0.0f);
    assert(received.trigger[0] == 0.0f && received.trigger[1] == 0.0f);
    assert(received.buttons == 0 && received.inputAvailable == 0);
    assert(!received.pointerValid && !received.pointerPressed);

    sent = validInput(2000);
    sent.focusOwned = 0;
    HP2VR::publishInput(state, sent);
    assert(!HP2VR::readInput(state, 2001, received));
    assert(received.buttons == 0);

    sent = validInput(3000);
    sent.head.m[4] = NAN;
    HP2VR::publishInput(state, sent);
    assert(!HP2VR::readInput(state, 3001, received));

    state.version++;
    assert(!HP2VR::readInput(state, 3001, received));
    assert(!HP2VR::readFrame(state, frameCopy));
    std::cout << "HP2VRBridgeTests passed\n";
}

