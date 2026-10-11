#pragma once
namespace VRDraw {
// Native software rasterization expects pre-clipped polygons. GPU backends can
// tolerate outside vertices, which concealed the missing clip stage in testing.
static void polygon(FSceneNode* f,FTextureInfo& texture,FTransTexture** input,int count,DWORD flags){
 if(count<3||count>8||f->Proj.Z<=0)return;
 FTransTexture a[24]{},b[24]{};for(int i=0;i<count;i++)a[i]=*input[i];
 const float left=f->PrjXM,right=f->PrjXP,top=f->PrjYM,bottom=f->PrjYP;
 for(int plane=0;plane<5;plane++){
  auto distance=[&](const FVector& p){switch(plane){case 0:return p.Z-1.f;case 1:return p.X+left*p.Z;case 2:return right*p.Z-p.X;case 3:return p.Y+top*p.Z;default:return bottom*p.Z-p.Y;}};
  int n=0;for(int i=0;i<count;i++){
   const auto& p=a[i];const auto& q=a[(i+1)%count];float dp=distance(p.Point),dq=distance(q.Point);bool inside=dp>=0,next=dq>=0;
   if(inside)b[n++]=p;
   if(inside!=next){float t=dp/(dp-dq);auto& v=b[n++];v=FTransTexture{};v.Point=p.Point+(q.Point-p.Point)*t;v.U=p.U+(q.U-p.U)*t;v.V=p.V+(q.V-p.V)*t;v.Light=p.Light+(q.Light-p.Light)*t;v.Fog=p.Fog+(q.Fog-p.Fog)*t;}
  }
  count=n;if(count<3)return;for(int i=0;i<count;i++)a[i]=b[i];
 }
 FTransTexture* points[24]{};USHORT indices[66]{};for(int i=0;i<count;i++){a[i].Project(f);a[i].ComputeOutcode(f);points[i]=&a[i];}
 for(int i=0;i<count-2;i++){indices[i*3]=0;indices[i*3+1]=i+1;indices[i*3+2]=i+2;}
 f->Viewport->RenDev->DrawTriangles(f,texture,points,count,indices,(count-2)*3,flags,nullptr);
}
}
