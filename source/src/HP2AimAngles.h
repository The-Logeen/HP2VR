#pragma once
#include <cmath>
namespace HP2AimAngles {
inline void axes(const float* m,float pitch,float yaw,float* forward,float* up){
 if(pitch==0.f&&yaw==0.f){for(int i=0;i<3;++i){forward[i]=-m[i*4+2];up[i]=m[i*4+1];}return;}
 const float p=pitch*.0174532925199433f,y=yaw*.0174532925199433f;
 const float cp=std::cos(p),sp=std::sin(p),cy=std::cos(y),sy=std::sin(y);
 const float f[]{sy*cp,sp,-cy*cp},u[]{-sy*sp,cp,cy*sp};
 for(int i=0;i<3;++i){forward[i]=m[i*4]*f[0]+m[i*4+1]*f[1]+m[i*4+2]*f[2];up[i]=m[i*4]*u[0]+m[i*4+1]*u[1]+m[i*4+2]*u[2];}
}
}
