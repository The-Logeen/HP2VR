#include "HP2StereoProjection.h"
#include <cassert>
#include <cmath>
#include <iostream>

static bool close(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    const float eyes[2][4]{{-1.16f, 1.30f, -1.22f, 1.18f},
                           {-1.30f, 1.16f, -1.22f, 1.18f}};
    const auto left = HP2Stereo::calculate(768, 768, eyes[0]);
    const auto right = HP2Stereo::calculate(768, 768, eyes[1]);
    assert(left.valid && right.valid && close(left.scale, right.scale));
    assert(left.centerX < right.centerX);
    assert(close(left.centerX + eyes[0][0] * left.scale, left.uMin * 768));
    assert(close(left.centerY + eyes[0][3] * left.scale, left.vMax * 768));
    assert(close(HP2Stereo::tangentExtent(eyes), 1.30f));
    const float runtimeEyes[2][4]{{-1.376382f, 0.839100f, -1.428148f, 0.965689f},
                                  {-0.839100f, 1.376382f, -1.428148f, 0.965689f}};
    const float extent = HP2Stereo::tangentExtent(runtimeEyes);
    const auto centeredLeft = HP2Stereo::centeredCrop(runtimeEyes[0], extent);
    const auto centeredRight = HP2Stereo::centeredCrop(runtimeEyes[1], extent);
    assert(centeredLeft.valid && centeredRight.valid);
    assert(close(centeredLeft.uMin, 1.0f - centeredRight.uMax));
    assert(close(centeredLeft.uMax, 1.0f - centeredRight.uMin));
    assert(close(centeredLeft.vMin, centeredRight.vMin));
    assert(close(centeredLeft.vMax, centeredRight.vMax));
    // A rectangular UE viewport has a narrower vertical frustum. Both the
    // engine FOV and compositor crop must use the same aspect-aware extents.
    for (const float aspect : {1.0f, 4.0f / 3.0f, 16.0f / 9.0f}) {
        const unsigned width = unsigned(900 * aspect);
        const float actualAspect = float(width) / 900;
        const float horizontal = HP2Stereo::horizontalExtent(runtimeEyes, width, 900);
        for (const auto& eye : runtimeEyes) {
            const auto crop = HP2Stereo::centeredCrop(eye, horizontal, actualAspect);
            assert(crop.valid);
            assert(close((crop.uMin - 0.5f) * 2 * horizontal, eye[0]));
            assert(close((0.5f - crop.vMax) * 2 * horizontal / actualAspect, eye[2]));
            assert(close((0.5f - crop.vMin) * 2 * horizontal / actualAspect, eye[3]));
        }
    }
    const float bad[4]{1.0f, 1.0f, -1.0f, 1.0f};
    assert(!HP2Stereo::calculate(768, 768, bad).valid);
    std::cout << "HP2StereoProjectionTests passed\n";
}
