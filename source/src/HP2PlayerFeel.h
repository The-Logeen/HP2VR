#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HP2PlayerFeel {

// Camera presentation, translation authority, and rotation authority are
// intentionally independent. HP2 can move a camera, Harry, or both; treating
// those signals as one generic "cutscene" flag caused the behavior the owner
// rejected in HP1VR.
enum class MovementMode : uint32_t { CinematicBody, Free, Assisted };
enum class Presentation : uint32_t { InBody, StationaryPanel, SelectedCinematic };
enum class Authority : uint32_t { Player, NativeScript, Assistance, Frozen };

struct SceneEvidence {
    bool stationaryPanel{};
    bool cinematicsEnabled{};
    bool nativeExternalCamera{};
    bool allowlistedCinematic{};
};

struct NudgeDecision {
    bool active{};
    bool forced{};
    float strength{};
};

// One bounded attempt per authored gate. A new gate token permits a new nudge.
// Manual input always wins. Continuous correction is reserved for a separately
// evidenced hard plot block and still yields immediately to manual input.
struct NudgeState {
    static constexpr float QuietDelaySeconds = 0.65f;
    static constexpr float RampSeconds = 0.20f;
    static constexpr float MaximumNudgeSeconds = 0.65f;

    uint64_t gateToken{};
    float quietSeconds{};
    float activeSeconds{};
    bool attempted{};

    void reset() { gateToken = 0; quietSeconds = activeSeconds = 0.0f; attempted = false; }

    NudgeDecision update(uint64_t token, bool pending, bool reachable,
                         bool hardPlotBlock, bool manualMove, bool manualTurn,
                         float deltaSeconds) {
        if (!pending || !reachable || token == 0) {
            reset();
            return {};
        }
        if (gateToken != token) {
            gateToken = token;
            quietSeconds = activeSeconds = 0.0f;
            attempted = false;
        }
        const float dt = std::isfinite(deltaSeconds) && deltaSeconds > 0.0f
            ? (std::min)(deltaSeconds, 0.1f) : 0.0f;
        if (manualMove || manualTurn) {
            // Interrupting an active nudge consumes this gate's normal attempt.
            if (activeSeconds > 0.0f) attempted = true;
            quietSeconds = activeSeconds = 0.0f;
            return {};
        }
        quietSeconds += dt;
        if (quietSeconds < QuietDelaySeconds) return {};

        if (hardPlotBlock) return {true, true, 1.0f};
        if (attempted) return {};

        activeSeconds += dt;
        if (activeSeconds > MaximumNudgeSeconds) {
            attempted = true;
            activeSeconds = 0.0f;
            return {};
        }
        const float strength = (std::min)(1.0f,
            (std::max)(0.05f, activeSeconds / RampSeconds));
        return {true, false, strength};
    }
};

struct Decision {
    Presentation presentation{Presentation::InBody};
    Authority translation{Authority::Player};
    Authority rotation{Authority::Player};
    bool showNativeHarry{};
};

inline bool selectedCinematic(const SceneEvidence& scene) {
    return scene.cinematicsEnabled && scene.nativeExternalCamera &&
        scene.allowlistedCinematic;
}

inline Decision decide(MovementMode mode, const SceneEvidence& scene,
                       const NudgeDecision& nudge = {}) {
    // A menu owns actions and translation, but head look and manual body yaw
    // remain free. The SteamVR layer presents it as a stationary panel.
    if (scene.stationaryPanel)
        return {Presentation::StationaryPanel, Authority::Frozen,
                Authority::Player, false};

    // Cinematics Off, a non-allowlisted shot, or a camera change without an
    // external authored shot can never seize player movement or rotation.
    if (selectedCinematic(scene))
        return {Presentation::SelectedCinematic, Authority::NativeScript,
                Authority::NativeScript, true};

    Decision result{};
    if (mode == MovementMode::CinematicBody)
        result.translation = Authority::NativeScript;
    else if (mode == MovementMode::Assisted && nudge.active)
        result.translation = Authority::Assistance;
    else
        result.translation = Authority::Player;

    // Rotation is always manual outside a selected cinematic, except for the
    // short active part of Assisted mode. Manual turn cancels that upstream.
    result.rotation = mode == MovementMode::Assisted && nudge.active
        ? Authority::Assistance : Authority::Player;
    return result;
}

inline bool intentionalStick(float x, float y, float threshold = 0.18f) {
    return std::isfinite(x) && std::isfinite(y) &&
        x * x + y * y > threshold * threshold;
}

// Swept arrival preserves HP1 V31's protection against walking through a
// staging mark between ticks. Long jumps/loads are not treated as a sweep.
inline bool sweptArrival(float ax, float ay, float az, float bx, float by, float bz,
                         float tx, float ty, float tz, float radius, float height) {
    const float values[]{ax, ay, az, bx, by, bz, tx, ty, tz, radius, height};
    for (float value : values) if (!std::isfinite(value)) return false;
    if (radius <= 0.0f || height <= 0.0f) return false;
    const float dx = bx - ax, dy = by - ay, dz = bz - az;
    const float denominator = dx * dx + dy * dy;
    const float t = denominator > 0.0001f && denominator <= 160.0f * 160.0f &&
        std::fabs(dz) <= height
        ? (std::max)(0.0f, (std::min)(1.0f,
            ((tx - ax) * dx + (ty - ay) * dy) / denominator)) : 1.0f;
    const float x = ax + t * dx - tx;
    const float y = ay + t * dy - ty;
    const float z = az + t * dz - tz;
    return x * x + y * y <= radius * radius && std::fabs(z) <= height;
}

} // namespace HP2PlayerFeel
