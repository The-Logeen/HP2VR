#pragma once

#include <cmath>

namespace HP2Stereo {

struct Layout {
    float scale{};
    float centerX{};
    float centerY{};
    float uMin{};
    float vMin{};
    float uMax{1.0f};
    float vMax{1.0f};
    bool valid{};
};

struct Bounds {
    float uMin{};
    float vMin{};
    float uMax{1.0f};
    float vMax{1.0f};
    bool valid{};
};

inline Layout calculate(unsigned width, unsigned height, const float raw[4]) {
    Layout output{};
    const float spanX = raw[1] - raw[0];
    const float spanY = raw[3] - raw[2];
    if (!width || !height || !std::isfinite(spanX) || !std::isfinite(spanY) ||
        spanX <= 0.01f || spanY <= 0.01f) return output;
    const float horizontalScale = width / spanX;
    const float verticalScale = height / spanY;
    output.scale = horizontalScale < verticalScale ? horizontalScale : verticalScale;
    const float activeWidth = output.scale * spanX;
    const float activeHeight = output.scale * spanY;
    const float padX = (width - activeWidth) * 0.5f;
    const float padY = (height - activeHeight) * 0.5f;
    output.centerX = padX - raw[0] * output.scale;
    output.centerY = padY - raw[2] * output.scale;
    output.uMin = padX / width;
    output.uMax = (padX + activeWidth) / width;
    output.vMin = padY / height;
    output.vMax = (padY + activeHeight) / height;
    output.valid = std::isfinite(output.scale) && output.scale > 0.0f &&
        output.uMin >= 0.0f && output.vMin >= 0.0f && output.uMax <= 1.0f &&
        output.vMax <= 1.0f;
    return output;
}

inline float tangentExtent(const float eyes[2][4]) {
    float result{};
    for (int eye = 0; eye < 2; ++eye)
        for (float value : eyes[eye]) {
            const float magnitude = std::fabs(value);
            if (magnitude > result) result = magnitude;
        }
    return result;
}

// HP1VR V31's default projection strategy for this generation of Unreal.
// Render a common symmetric frustum and select each OpenVR asymmetric frustum
// with texture bounds.  Applying an off-axis centre in both UE1 and SteamVR
// makes the two images appear dramatically farther apart.
inline float horizontalExtent(const float eyes[2][4], unsigned width, unsigned height) {
    if (!width || !height) return 0.0f;
    const float aspect = float(width) / height;
    float extent = 0.0f;
    for (int eye = 0; eye < 2; ++eye)
        for (int axis = 0; axis < 4; ++axis) {
            const float required = std::fabs(eyes[eye][axis]) * (axis >= 2 ? aspect : 1.0f);
            if (required > extent) extent = required;
        }
    return extent;
}

inline Bounds centeredCrop(const float raw[4], float symmetricExtent, float aspect = 1.0f) {
    Bounds output{};
    if (!std::isfinite(symmetricExtent) || symmetricExtent <= 0.01f ||
        !std::isfinite(aspect) || aspect <= 0.0f) return output;
    const float verticalExtent = symmetricExtent / aspect;
    output.uMin = 0.5f + 0.5f * raw[0] / symmetricExtent;
    output.uMax = 0.5f + 0.5f * raw[1] / symmetricExtent;
    output.vMin = 0.5f - 0.5f * raw[3] / verticalExtent;
    output.vMax = 0.5f - 0.5f * raw[2] / verticalExtent;
    output.valid = std::isfinite(output.uMin) && std::isfinite(output.uMax) &&
        std::isfinite(output.vMin) && std::isfinite(output.vMax) &&
        output.uMin >= 0.0f && output.vMin >= 0.0f &&
        output.uMax <= 1.0f && output.vMax <= 1.0f &&
        output.uMax > output.uMin && output.vMax > output.vMin;
    return output;
}

} // namespace HP2Stereo
