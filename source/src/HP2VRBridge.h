#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace HP2VR {

inline constexpr uint32_t BridgeMagic = 0x52563248u; // H2VR in little endian.
inline constexpr uint32_t BridgeVersion = 3;
inline constexpr wchar_t MappingName[] = L"Local\\HP2VR_Bridge_v3";
inline constexpr wchar_t FrameEventName[] = L"Local\\HP2VR_Frame_v3";
inline constexpr wchar_t ShutdownEventName[] = L"Local\\HP2VR_Shutdown_v3";
inline constexpr uint32_t GpuBridgeMagic = 0x55473248u; // H2GU in little endian.
inline constexpr uint32_t GpuBridgeVersion = 1;
inline constexpr wchar_t GpuMappingName[] = L"Local\\HP2VR_D3D11_GPU_v1";

enum Button : uint64_t {
    Jump = 1ull << 0,
    Menu = 1ull << 1,
    LeftGrip = 1ull << 2,
    RightGrip = 1ull << 3,
    SkipDialogue = 1ull << 4,
    Recenter = 1ull << 5,
};

enum class Presentation : uint32_t {
    WorldStereo = 0,
    StationaryPanel = 1,
    SelectedCinematic = 2,
};

struct Pose34 {
    float m[12]{};
    uint64_t sampleQpc{};
    uint32_t valid{};
    uint32_t trackingGeneration{};
};

struct InputPayload {
    Pose34 head{};
    Pose34 hand[2]{};
    Pose34 eyeToHead[2]{};
    float eyeTangents[2][4]{}; // OpenVR left, right, top, bottom.
    float move[2]{};
    float turn[2]{};
    float trigger[2]{};
    uint64_t buttons{};
    uint64_t sampleQpc{};
    uint32_t inputAvailable{};
    uint32_t focusOwned{};
    uint32_t stereoAvailable{};
    uint32_t reserved{};
    float pointerUv[2]{};
    uint32_t pointerValid{};
    uint32_t pointerPressed{};
};

// Published only after the x86 engine has rendered and captured both eyes.
// The x64 companion uses this exact render pose with Submit_TextureWithPose,
// so a later tracking sample is never mislabeled as the pose used by the game.
struct FramePayload {
    Pose34 renderedHead{};
    float eyeTangents[2][4]{};
    int32_t captureSequence[2]{};
    uint32_t width{};
    uint32_t height{};
    uint32_t pairNumber{};
    uint32_t stereoValid{};
    Presentation presentation{Presentation::WorldStereo};
    uint32_t reserved{};
};

struct alignas(64) SharedState {
    uint32_t magic{BridgeMagic};
    uint32_t version{BridgeVersion};
    uint32_t bytes{sizeof(SharedState)};
    uint32_t reserved{};
    uint64_t qpcFrequency{};
    uint32_t gamePid{};
    uint32_t companionPid{};
    uint64_t gameHeartbeatQpc{};
    uint64_t companionHeartbeatQpc{};
    uint32_t inputSequence{};
    uint32_t frameSequence{};
    InputPayload input{};
    FramePayload frame{};
};

// Keep this transport struct pointer-free and layout-identical in the x86 game
// proxy and x64 SteamVR companion. Shared handles are stored as uint64_t.
struct alignas(8) GpuSharedState {
    uint32_t magic{GpuBridgeMagic};
    uint32_t version{GpuBridgeVersion};
    int32_t generation{};
    int32_t captureSequence[2]{};
    uint64_t sharedHandle[2]{};
    uint32_t width{};
    uint32_t height{};
    uint32_t format{};
    int32_t producerError{};
    int32_t consumerReady{};
    int32_t consumerError{};
};

static_assert(std::is_trivially_copyable_v<Pose34>);
static_assert(std::is_trivially_copyable_v<InputPayload>);
static_assert(std::is_trivially_copyable_v<FramePayload>);
static_assert(std::is_trivially_copyable_v<SharedState>);
static_assert(std::is_trivially_copyable_v<GpuSharedState>);
static_assert(sizeof(InputPayload) == 424);
static_assert(sizeof(FramePayload) == 128);
static_assert(sizeof(SharedState) == 640);
static_assert(sizeof(GpuSharedState) == 64);

inline bool compatible(const SharedState& state) {
    return state.magic == BridgeMagic && state.version == BridgeVersion && state.bytes == sizeof(SharedState);
}

inline bool compatible(const GpuSharedState& state) {
    return state.magic == GpuBridgeMagic && state.version == GpuBridgeVersion;
}

inline bool finitePose(const Pose34& pose) {
    if (!pose.valid) return false;
    for (float value : pose.m) if (!std::isfinite(value) || std::fabs(value) > 100000.0f) return false;
    return true;
}

inline bool finiteTangents(const float tangents[4]) {
    for (int i = 0; i < 4; ++i)
        if (!std::isfinite(tangents[i]) || std::fabs(tangents[i]) > 10.0f) return false;
    return tangents[1] - tangents[0] > 0.01f && tangents[3] - tangents[2] > 0.01f;
}

inline bool validStereo(const InputPayload& input) {
    return input.stereoAvailable && finitePose(input.eyeToHead[0]) &&
        finitePose(input.eyeToHead[1]) && finiteTangents(input.eyeTangents[0]) &&
        finiteTangents(input.eyeTangents[1]);
}

inline bool fresh(uint64_t nowQpc, uint64_t sampleQpc, uint64_t frequency, uint32_t maxAgeMs = 250) {
    if (!frequency || !sampleQpc || nowQpc < sampleQpc) return false;
    const uint64_t maxTicks = (frequency * maxAgeMs) / 1000;
    return nowQpc - sampleQpc <= maxTicks;
}

inline void neutralize(InputPayload& input) {
    input.move[0] = input.move[1] = 0.0f;
    input.turn[0] = input.turn[1] = 0.0f;
    input.trigger[0] = input.trigger[1] = 0.0f;
    input.buttons = 0;
    input.pointerUv[0] = input.pointerUv[1] = 0.0f;
    input.pointerValid = input.pointerPressed = 0;
    input.inputAvailable = 0;
    input.focusOwned = 0;
}

inline void publishInput(SharedState& state, const InputPayload& input) {
    std::atomic_ref<uint32_t> sequence(state.inputSequence);
    sequence.fetch_add(1, std::memory_order_acq_rel); // odd: writer owns it
    std::memcpy(&state.input, &input, sizeof(input));
    sequence.fetch_add(1, std::memory_order_release); // even: stable snapshot
}

inline bool readInput(SharedState& state, uint64_t nowQpc, InputPayload& output,
                      uint32_t* stableSequence = nullptr) {
    if (!compatible(state)) { neutralize(output); return false; }
    std::atomic_ref<uint32_t> sequence(state.inputSequence);
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = sequence.load(std::memory_order_acquire);
        if (before & 1u) continue;
        std::memcpy(&output, &state.input, sizeof(output));
        const uint32_t after = sequence.load(std::memory_order_acquire);
        if (before == after && !(after & 1u)) {
            if (stableSequence) *stableSequence = after;
            const bool valid = output.inputAvailable && output.focusOwned && finitePose(output.head) &&
                               fresh(nowQpc, output.sampleQpc, state.qpcFrequency);
            if (!valid) neutralize(output);
            return valid;
        }
    }
    neutralize(output);
    return false;
}


inline void publishFrame(SharedState& state, const FramePayload& frame) {
    std::atomic_ref<uint32_t> sequence(state.frameSequence);
    sequence.fetch_add(1, std::memory_order_acq_rel);
    std::memcpy(&state.frame, &frame, sizeof(frame));
    sequence.fetch_add(1, std::memory_order_release);
}

inline bool readFrame(SharedState& state, FramePayload& output) {
    if (!compatible(state)) return false;
    std::atomic_ref<uint32_t> sequence(state.frameSequence);
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = sequence.load(std::memory_order_acquire);
        if (before & 1u) continue;
        std::memcpy(&output, &state.frame, sizeof(output));
        const uint32_t after = sequence.load(std::memory_order_acquire);
        if (before == after && !(after & 1u))
            return output.stereoValid && finitePose(output.renderedHead) &&
                finiteTangents(output.eyeTangents[0]) && finiteTangents(output.eyeTangents[1]);
    }
    return false;
}

} // namespace HP2VR

