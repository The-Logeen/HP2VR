#pragma once

#include <cmath>

namespace HP2ScreenPanel {

// A single stationary-space model is shared by the compositor overlay, the
// controller ray, its visible endpoint, and HP2's native Windows mouse.
struct Panel {
    static constexpr float DefaultWidthMetres = 1.6f;
    static constexpr float DefaultHeightMetres = 1.2f;

    float halfWidth{DefaultWidthMetres * 0.5f};
    float halfHeight{DefaultHeightMetres * 0.5f};
    float center[3]{};
    float right[3]{};
    float normal[3]{};
    bool ready{};

    bool place(const float* head, float distance = 1.8f, float sourceAspect = 4.0f / 3.0f) {
        if (!head || !std::isfinite(distance) || distance < 0.2f || distance > 8.0f)
            return false;
        const float horizontal = std::sqrt(head[0] * head[0] + head[8] * head[8]);
        if (horizontal < 0.01f || !std::isfinite(horizontal)) return false;
        halfHeight = DefaultHeightMetres * 0.5f;
        halfWidth = sourceAspect > 0.05f && sourceAspect < 20.0f
            ? halfHeight * sourceAspect : DefaultWidthMetres * 0.5f;
        right[0] = head[0] / horizontal;
        right[1] = 0.0f;
        right[2] = head[8] / horizontal;
        normal[0] = -right[2];
        normal[1] = 0.0f;
        normal[2] = right[0];
        for (int index = 0; index < 3; ++index)
            center[index] = head[index * 4 + 3] - normal[index] * distance;
        ready = true;
        return true;
    }

    bool pick(const float* origin, const float* direction, float& u, float& v,
              float* hit = nullptr) const {
        if (!ready || !origin || !direction) return false;
        const float denominator = direction[0] * normal[0] + direction[2] * normal[2];
        if (denominator >= -0.0001f) return false;
        const float distance = ((center[0] - origin[0]) * normal[0] +
            (center[2] - origin[2]) * normal[2]) / denominator;
        if (distance <= 0.0f || distance > 8.0f || !std::isfinite(distance)) return false;
        const float x = origin[0] + distance * direction[0] - center[0];
        const float y = origin[1] + distance * direction[1] - center[1];
        const float z = origin[2] + distance * direction[2] - center[2];
        u = 0.5f + (x * right[0] + z * right[2]) / (halfWidth * 2.0f);
        v = 0.5f - y / (halfHeight * 2.0f);
        const bool inside = u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
        if (inside && hit) {
            hit[0] = origin[0] + distance * direction[0];
            hit[1] = origin[1] + distance * direction[1];
            hit[2] = origin[2] + distance * direction[2];
        }
        return inside;
    }
};

inline void controllerRay(const float* controller, float* origin, float* direction) {
    origin[0] = controller[3];
    origin[1] = controller[7];
    origin[2] = controller[11];
    direction[0] = -controller[2];
    direction[1] = -controller[6];
    direction[2] = -controller[10];
}

} // namespace HP2ScreenPanel
