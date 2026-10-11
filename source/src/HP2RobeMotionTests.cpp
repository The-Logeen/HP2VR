#include "HP2RobeMotion.h"
#include "HP2RobeWeights.h"
#include <cassert>
#include <cstdio>
int main(){
    for(int reason=0;reason<4;++reason){float x=-.15f,y=.15f,z=.25f;
        HP2RobeMotion::apply(x,y,z,reason==3?1.f:.25f,reason==0?0.f:1.f,reason==1?0.f:1.f,reason==2?.2f:0.f);
        assert(std::fabs(x+.15f)<1e-6f&&std::fabs(y-.15f)<1e-6f&&std::fabs(z-.25f)<1e-6f);
    }
    float x=-.15f,y=.15f,z=.25f;HP2RobeMotion::apply(x,y,z,.25f,1,1,0);
    assert(x>-.15f&&y>.15f&&std::isfinite(z));
    float a=-.15f,b=-.15f,c=.25f;HP2RobeMotion::apply(a,b,c,.25f,1,1,0);
    assert(std::fabs(x-a)<1e-6f&&std::fabs(y+b)<1e-6f&&std::fabs(z-c)<1e-6f);
    int count=0;for(float w:hp2FrontRobeWeights){assert(w>=0&&w<=1);count+=w>.001f;}assert(count>10&&count<180);
    std::printf("Robe motion passed: %d masked vertices; rest/crouch/torso exclusions, rearward flare and symmetry\n",count);
}
