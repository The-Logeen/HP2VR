#pragma once
// Bind-space axis through the curled middle/little finger loops, pointing out
// the thumb side. Controller mapping remains physical left=0, right=1.
namespace HandGripFrame {
static const float shaft[2][3]={{-.984f,.154f,.082f},{-.984f,-.154f,.082f}};
static const float centre[2][3]={{-.04684f,-.37639f,.68116f},{-.04684f,.37639f,.68116f}};
inline float dorsalSide(int controller){return controller==1?1.f:-1.f;}
}
