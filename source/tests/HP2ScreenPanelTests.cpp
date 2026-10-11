#include "HP2ScreenPanel.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    HP2ScreenPanel::Panel panel;
    float head[12]{1, 0, 0, 0, 0, 1, 0, 1.7f, 0, 0, 1, 0};
    assert(panel.place(head, 1.8f, 4.0f / 3.0f));
    assert(std::fabs(panel.halfWidth * 2.0f - 1.6f) < 0.0001f);
    assert(std::fabs(panel.halfHeight * 2.0f - 1.2f) < 0.0001f);

    float origin[3]{0, 1.7f, 0};
    float direction[3]{0, 0, -1};
    float u{}, v{}, hit[3]{};
    assert(panel.pick(origin, direction, u, v, hit));
    assert(std::fabs(u - 0.5f) < 0.0001f && std::fabs(v - 0.5f) < 0.0001f);
    assert(std::fabs(hit[2] + 1.8f) < 0.0001f);

    direction[0] = 0.6f;
    direction[2] = -1.0f;
    assert(!panel.pick(origin, direction, u, v));

    HP2ScreenPanel::Panel square;
    assert(square.place(head, 1.8f, 1.0f));
    assert(std::fabs(square.halfWidth - square.halfHeight) < 0.0001f);

    float controller[12]{1, 0, 0, 0, 0, 1, 0, 1.7f, 0, 0, 1, 0};
    float rayOrigin[3]{}, rayDirection[3]{};
    HP2ScreenPanel::controllerRay(controller, rayOrigin, rayDirection);
    assert(rayDirection[0] == 0 && rayDirection[1] == 0 && rayDirection[2] == -1);
    std::cout << "HP2ScreenPanelTests passed\n";
}
