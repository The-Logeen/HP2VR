#pragma once
#include <cmath>
namespace HP2Flick {
struct Recognizer {
    float x{},y{},ax{},ay{},quiet{};bool ready{},armed=true;
    void reset(){ready=false;armed=true;quiet=0;}
    int update(float nx,float ny,float dt,bool valid) {
        if(!valid||!std::isfinite(nx)||!std::isfinite(ny)||dt<=0||dt>.1f){reset();return -1;}
        if(!ready){x=ax=nx;y=ay=ny;ready=true;return -1;}
        float speed=std::hypot(nx-x,ny-y)/dt;x=nx;y=ny;
        if(speed>5.f){reset();return -1;} // tracking discontinuity, not a flick
        // A pause at the tip of a stroke is NOT the next neutral pose. Keep
        // the original centre until the hand returns; consume the recovery
        // stroke even when it points in the next arrow's direction.
        if(!armed) {
            if(std::hypot(x-ax,y-ay)<.025f && speed<.20f)quiet+=dt;else quiet=0;
            if(quiet>=.04f){armed=true;quiet=0;ax=x;ay=y;}
            return -1;
        }
        if(speed<.12f){quiet+=dt;if(quiet>=.30f){ax=x;ay=y;}}else quiet=0;
        float dx=x-ax,dy=y-ay,xx=std::fabs(dx),yy=std::fabs(dy);
        if(speed<.35f || (xx<.045f&&yy<.045f))return -1;
        int direction=-1;
        if(xx>yy*1.25f)direction=dx<0?2:3;
        else if(yy>xx*1.25f)direction=dy>0?0:1;
        if(direction>=0){armed=false;quiet=0;}
        return direction;
    }
};
}
