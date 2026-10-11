#pragma once
#include <cmath>
namespace HP2RobeMotion {
inline float clamp(float x){return std::fmax(0.f,std::fmin(1.f,x));}
inline void apply(float& x,float& y,float& z,float restZ,float weight,float walking,float crouch) {
    // Render-only, modest backward flare around the upper skirt; no leg or
    // arm vertices are in the exported mask. Fade out before deep crouching.
    const float lower=clamp((.78f-restZ)/.25f);
    const float a=1.15f*clamp(weight)*clamp(walking)*lower*clamp(1.f-crouch/.10f);
    const float dz=z-.78f,c=std::cos(a),s=std::sin(a);
    const float nx=x*c-dz*s;
    z=.78f+x*s+dz*c;x=nx;
    y+=(y<0?-1.f:1.f)*.20f*(a/1.15f);
}
}
