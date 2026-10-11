#pragma once
#include <cmath>
namespace HP2Comfort {
inline float crouchDrop(float standing,float current,float units,float bodyScale) {
    if(!std::isfinite(standing)||!std::isfinite(current)||bodyScale<=0)return 0;
    const float value=(standing-current-.05f)*units/bodyScale;
    return std::fmax(0.f,std::fmin(.58f,value));
}
struct Height {
    float current{},previous{},airTime{};bool ready{};
    void reset(){ready=false;}
    float update(float target,float dt,bool grounded,bool climb=false,bool descendingStep=false) {
        if(!std::isfinite(target))return current;
        airTime=grounded?0.f:airTime+dt;
        const bool shortDescent=descendingStep && airTime<=.15f && target<=previous;
        if(!ready || dt<=0 || dt>.25f || (!grounded&&!climb&&!shortDescent) || std::fabs(target-previous)>(climb?180.f:40.f)) {
            current=previous=target;ready=true;return current;
        }
        previous=target;
        current+=(target-current)*(1.f-std::exp(-dt/(climb?.18f:.20f)));
        return current;
    }
};
}
