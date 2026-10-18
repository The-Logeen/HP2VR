#include "HP2AimAngles.h"
#include <cassert>
#include <cstdio>
int main(){
 const float m[]{1,0,0,4,0,1,0,5,0,0,1,6};float f[3],u[3];
 HP2AimAngles::axes(m,0,0,f,u);assert(f[0]==0&&f[1]==0&&f[2]==-1&&u[1]==1);
 HP2AimAngles::axes(m,90,0,f,u);assert(std::fabs(f[1]-1)<.00001f);
 HP2AimAngles::axes(m,0,90,f,u);assert(std::fabs(f[0]-1)<.00001f);
 for(int p=-90;p<=90;p+=2)for(int y=-90;y<=90;y+=2){HP2AimAngles::axes(m,float(p),float(y),f,u);float ff=0,uu=0,d=0;for(int i=0;i<3;++i){ff+=f[i]*f[i];uu+=u[i]*u[i];d+=f[i]*u[i];}assert(std::fabs(ff-1)<.00001f&&std::fabs(uu-1)<.00001f&&std::fabs(d)<.00001f);}
 assert(m[3]==4&&m[7]==5&&m[11]==6);puts("PASS zero defaults, pitch/yaw signs, 8281 orthonormal angle pairs, unchanged translation");
}
