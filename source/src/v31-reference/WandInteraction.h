#pragma once
#include <cmath>
// V30 shadow of the pure tracking-space wand interaction. The unowned wand can
// preview a moving holster and a held grip may enter the pickup volume.
namespace WandInteraction {
struct Point { float x=0,y=0,z=0; };
inline float distance(Point a,Point b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}
inline Point holsterSocket(Point head,float yaw,float side,float forward,float drop){
 const float c=std::cos(yaw),s=std::sin(yaw);
 return {head.x+side*c-forward*s,head.y-drop,head.z+side*s+forward*c};
}
struct State {
    int owner=-1;
    bool returning=false,charging=false,cancel=false;
    bool previousGrip[2]{},previousNear[2]{},triggerReady=false;
    float elapsed=0;
    Point position{},returnStart{};
    void retarget(Point socket){if(owner<0){returning=false;elapsed=0;position=returnStart=socket;}}
    void update(float dt,Point socket,const Point hands[2],const bool valid[2],const bool grip[2],const bool trigger[2],bool available) {
        cancel=false;
        if(owner>=0 && (!available || !valid[owner] || !grip[owner])) {
            cancel=charging;charging=false;triggerReady=false;
            owner=-1;returning=true;elapsed=0;returnStart=position;
        }
        if(owner<0) {
            if(returning){elapsed+=dt>0?dt:0;float t=elapsed/.28f;if(t>=1){t=1;returning=false;}t=t*t*(3-2*t);position={returnStart.x+(socket.x-returnStart.x)*t,returnStart.y+(socket.y-returnStart.y)*t,returnStart.z+(socket.z-returnStart.z)*t};}
            else position=socket;
            for(int h=0;h<2;h++){const bool withinGrab=valid[h]&&distance(hands[h],position)<=.20f;if(available&&grip[h]&&withinGrab&&(!previousGrip[h]||!previousNear[h])){owner=h;returning=false;triggerReady=!trigger[h];break;}}
        }
        if(owner>=0){position=hands[owner];if(!trigger[owner])triggerReady=true;charging=triggerReady&&trigger[owner];}
        for(int h=0;h<2;h++){previousGrip[h]=grip[h];previousNear[h]=valid[h]&&distance(hands[h],position)<=.20f;}
    }
};
}
