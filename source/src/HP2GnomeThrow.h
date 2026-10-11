#pragma once
#include <cmath>
namespace HP2GnomeThrow {
// Head-relative controller-tip motion, in metres, in a fixed body-forward frame.
struct Gesture {
    float x{},y{},z{},ax{},ay{},az{},quiet{},window{};
    bool ready{},armed{},fired{};
    void reset(){*this=Gesture{};}
    bool update(float nx,float ny,float nz,float dt,bool valid) {
        if(!valid||!std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(nz)||dt<=0||dt>.1f){ready=armed=false;quiet=window=0;return false;}
        if(fired)return false;
        if(!ready){x=ax=nx;y=ay=ny;z=az=nz;ready=true;return false;}
        const float dx=nx-x,dy=ny-y,dz=nz-z;
        const float distance=std::sqrt(dx*dx+dy*dy+dz*dz),speed=distance/dt;
        x=nx;y=ny;z=nz;
        if(distance>.30f||speed>6.f){ready=armed=false;quiet=window=0;return false;}
        if(speed<.18f){quiet+=dt;if(quiet>=.15f){armed=true;ax=x;ay=y;az=z;window=0;}}
        else quiet=0;
        if(!armed)return false;
        window+=dt;
        const float forward=x-ax,side=y-ay,up=z-az;
        const float travel=std::sqrt(forward*forward+side*side+up*up);
        if(forward>=.06f&&speed>=.9f&&dx/dt>=.65f&&forward>=travel*.70f&&window<=.30f){fired=true;return true;}
        if(window>.30f){armed=false;quiet=0;}
        return false;
    }
};
}
