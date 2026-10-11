#pragma once
#include "WandGeometry.h"
// V30: the real Harry wand, in place of two coloured prisms.
//
// WHERE THIS CAME FROM -------------------------------------------------------
// Logan's Unity prefab (HarrysWand.prefab) and its FBX. The prefab is a
// CONTRACT, not an asset: no mesh, no texture, no spell code -- the visual is a
// GUID reference and HPWandController belongs to the Unity rebuild, where the
// wand casts. Here HP1 owns casting through SpellHooks, so the only thing worth
// importing is the geometry and the grip/tip agreement, and that part the
// prefab pins down exactly.
//
// The FBX carries three empties parented to the mesh -- Grip, Tip and
// Spell Effect. Composing Grip and Tip through the prefab's visual transform
// lands them on the prefab's own GripAttach and SpellMuzzle to within a
// FLOAT, which settles the question the prefab alone could not answer: the
// 15-degree visual tilt is not an error, it is the wand's own axis, and
// SpellMuzzle really is the tip of the model.
//
// SIZE ----------------------------------------------------------------------
// The one thing that did NOT carry over is scale. Unity imports that FBX at
// exactly 30x, which makes the wand 49.3 cm from fist to tip and 27 mm thick --
// nearly twice a film wand, and against a 28 cm assumption baked into this
// wrapper's own wand ray and into LessonVR's cursor lever. Export-Wand.py
// rescales to 28 cm on the way out, so the mesh below already matches what the
// rest of the build believes. Re-run it with a different length to change that.
namespace WandMesh {

static UTexture* atlas=nullptr;
static bool tried=false;

// Same import route AvatarRender uses: hand UE1's own texture factory a PCX and
// let it build a native indexed texture. The exporter writes a 256-colour PCX
// because that is what this software renderer can actually sample.
static void initialize(){
 if(tried)return;tried=true;
 auto factoryClass=FindObject<UClass>(nullptr,TEXT("Engine.EngineTextureFactory"));
 if(!factoryClass)return;
 auto factory=ConstructObject<UFactory>(factoryClass);
 atlas=ImportObject<UTexture>(UObject::GetTransientPackage(),TEXT("HPVRWandAtlas"),RF_Transient,TEXT("WandAtlas.pcx"),nullptr,factory);
 if(atlas)atlas->AddToRoot();
 if(trace){std::fprintf(trace,"Wand atlas import: %s (%d vertices, %d triangles)\n",atlas?"OK":"FAILED - WandAtlas.pcx must sit beside the game executable; falling back to prism geometry",
  static_cast<int>(sizeof(wandVertices)/sizeof(wandVertices[0])),static_cast<int>(sizeof(wandTriangles)/sizeof(wandTriangles[0])));std::fflush(trace);}
}

// grip   : where the fist is, in world units
// shaft  : unit vector along the wand
// roll   : any vector not parallel to shaft; fixes the cross-section's rotation
// reach  : grip-to-tip distance in world units. During a lesson the wand
//          stretches to the cursor, so Z is scaled to meet it while X and Y
//          keep true thickness -- a wand that got fatter as it reached would
//          look like chewing gum.
static bool draw(FSceneNode* frame,const FVector& grip,const FVector& shaft,const FVector& roll,float reach,float worldScale){
 initialize();
 if(!atlas||reach<=1.f||worldScale<=0.f)return false;

 FVector right=(roll^shaft).SafeNormal();
 if(right.SizeSquared()<.25f)right=(FVector(0,0,1)^shaft).SafeNormal();
 if(right.SizeSquared()<.25f)right=(FVector(0,1,0)^shaft).SafeNormal();
 if(right.SizeSquared()<.25f)return false;
 const FVector up=(shaft^right).SafeNormal();

 const float lateral=worldScale;                        // metres -> world units
 const float along=reach/Max(.0001f,wandLengthMetres);  // stretch to the tip

 const int count=static_cast<int>(sizeof(wandVertices)/sizeof(wandVertices[0]));
 // 563 vertices. Transforming them per eye is nothing next to the scene.
 static FVector view[sizeof(wandVertices)/sizeof(wandVertices[0])];
 static FPlane light[sizeof(wandVertices)/sizeof(wandVertices[0])];
 for(int i=0;i<count;i++){
  const WandVertex& v=wandVertices[i];
  const FVector world=grip+right*(v.p[0]*lateral)+up*(v.p[1]*lateral)+shaft*(v.p[2]*along);
  view[i]=world.TransformPointBy(frame->Coords);
  // Cheap wrap lighting off the wand's own normal. HP1's lighting never sees
  // this mesh, and flat-lit wood in a dim corridor reads as a paper cutout.
  const FVector n=right*v.n[0]+up*v.n[1]+shaft*v.n[2];
  const float key=Clamp(.62f+.38f*static_cast<float>((n|FVector(.35f,.22f,.91f))),.35f,1.f);
  light[i]=FPlane(key,key,key,1.f);
 }

 FTextureInfo info{};atlas->Lock(info,frame->Viewport->CurrentTime,0,frame->Viewport->RenDev);
 struct Unlock {FTextureInfo& i;~Unlock(){atlas->Unlock(i);}} unlock{info};
 unsigned drawn=0;
 for(const auto& t:wandTriangles){
  FTransTexture p[3]{};FTransTexture* ptr[3]={&p[0],&p[1],&p[2]};bool clipped=false;
  for(int k=0;k<3;k++){
   const int index=t.v[k];
   p[k].Point=view[index];
   if(p[k].Point.Z<1.f){clipped=true;break;}
   p[k].Project(frame);p[k].ComputeOutcode(frame);
   p[k].U=wandVertices[index].uv[0]*info.USize;
   p[k].V=wandVertices[index].uv[1]*info.VSize;
   p[k].Light=light[index];
  }
  if(clipped||(p[0].Flags&p[1].Flags&p[2].Flags))continue;
  VRDraw::polygon(frame,info,ptr,3,PF_TwoSided|PF_Occlude);++drawn;
 }
 static bool reported=false;
 if(!reported&&drawn&&trace){reported=true;std::fprintf(trace,"Harry's wand mesh rendered: %u triangles, %.1f cm at %.0f UU/m\n",drawn,wandLengthMetres*100.f,worldScale);std::fflush(trace);}
 return drawn>0;
}
}
