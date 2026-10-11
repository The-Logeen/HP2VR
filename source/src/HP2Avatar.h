#pragma once
#include "v31-reference/AvatarGeometry.h"
#include "v31-reference/AvatarWalk.h"
#include "v31-reference/AvatarPose.h"
#include "v31-reference/HandGripFrame.h"
#include "HP2RobeWeights.h"
#include "HP2RobeMotion.h"
namespace HP2Avatar {
static UTexture* atlas{};
static bool calibrated{};
static unsigned calibratedGeneration{},calibrationRequest{};
static float standingHead{},standingEye{};
static_assert(sizeof(avatarWalk[0])/sizeof(avatarWalk[0][0])==sizeof(avatarVertices)/sizeof(avatarVertices[0]),"V31 walk/mesh vertex correspondence");
static FVector vec(const float* p){return FVector(p[0],p[1],p[2]);}
static FVector rotate(FVector v,FVector from,FVector to) {
    from=from.SafeNormal();to=to.SafeNormal();const float dot=from|to;const FVector cross=from^to;
    if(dot<-.999f)return -v;
    return v+(cross^v)+(cross^(cross^v))*(1.f/(1.f+dot));
}
// Render-only deformation: ankle stays fixed, knee bends forward, hip lowers.
// No game collision, pawn physics or camera crouch is synthesized here.
static FVector crouch(FVector p,float drop) {
    const float ankle=.08f,hip=.82f,half=(hip-ankle)*.5f;
    const float dz=Clamp(drop,0.f,.58f);
    if(p.Z>=hip)return p+FVector(dz*.16f,0,-dz);
    if(p.Z<=ankle)return p;
    const float kneeZ=(hip-dz+ankle)*.5f;
    const float bend=std::sqrt(Max(0.f,half*half-(kneeZ-ankle)*(kneeZ-ankle)));
    const FVector a(0,p.Y,ankle),k(-bend,p.Y,kneeZ),h(dz*.16f,p.Y,hip-dz);
    const float t=Clamp((p.Z-ankle)/(hip-ankle),0.f,1.f);
    FVector out=t<.5f?a+(k-a)*(t*2.f):k+(h-k)*((t-.5f)*2.f);
    out.X+=p.X;return out;
}
static void draw(FSceneNode* frame) {
    if(!HP2Settings::avatar || currentEye<0 || !pairTracked || !inputViewport || inputViewport->bShowWindowsMouse || HP2Settings::open)return;
    auto* pawn=frame->Viewport->Actor;if(!bodyView(pawn))return;
    if(!atlas){auto* cls=FindObject<UClass>(nullptr,TEXT("Engine.EngineTextureFactory"));if(!cls)return;
        auto* factory=ConstructObject<UFactory>(cls);atlas=ImportObject<UTexture>(UObject::GetTransientPackage(),TEXT("HP2VRAvatarAtlas"),RF_Transient,TEXT("AvatarAtlas.pcx"),nullptr,factory);if(!atlas)return;atlas->AddToRoot();line("HP2 V31 avatar atlas loaded");}
    const float units=WorldUnitsPerMetre;
    if(!calibrated || calibratedGeneration!=originGeneration || calibrationRequest!=HP2Settings::bodyCalibration) {
        // Intro/cinematic body draws are not proof the player has control.
        if(!inputWasLive || appStricmp(*playerState(pawn),TEXT("PlayerWalking")) ||
            frontEnd(inputViewport) || HP2Tracked::flag(reflectedObject(pawn,TEXT("myHUD")),TEXT("bCutSceneMode")))return;
        standingHead=pairInput.head.m[7];calibratedGeneration=originGeneration;
        standingEye=HP2Tracked::anchor(pawn).Z-comfortOffset-(pawn->Location.Z-HP2Climb::visualHalfHeight(pawn))+(standingHead-originHead[1]+.03f)*units;
        calibrationRequest=HP2Settings::bodyCalibration;calibrated=true;
        line("HP2 avatar control-ready calibration head=%.3f origin=%.3f bodyLift=3cm crouchDeadzone=5cm",standingHead,originHead[1]);
    }
    // Match the eyes at the player's calibrated standing pose, not the pose
    // sampled while loading or adjusting the headset. Recenter recalibrates.
    const float fullEye=Max(20.f,standingEye);
    const float scale=fullEye/1.5f;
    const float drop=HP2Comfort::crouchDrop(standingHead,pairInput.head.m[7],units,scale);
    static DWORD lastWalk{};static float walkPhase{},walkBlend{},robeBlend{};
    if(currentEye==0) {
        const DWORD now=GetTickCount();const float dt=lastWalk?Min(.1f,(now-lastWalk)*.001f):0.f;lastWalk=now;
        const float speed=std::sqrt(pawn->Velocity.X*pawn->Velocity.X+pawn->Velocity.Y*pawn->Velocity.Y)/units;
        const float wanted=walkingView(pawn)&&pawn->Physics==PHYS_Walking&&drop<.1f?Clamp(speed/.65f,0.f,1.f):0.f;
        walkBlend+=(wanted-walkBlend)*(1.f-std::exp(-10.f*dt));
        const float yaw=bodyYaw*TwoPi/65536.f;
        const float forward=(pawn->Velocity.X*std::cos(yaw)+pawn->Velocity.Y*std::sin(yaw))/units;
        const float robeWanted=wanted*HP2RobeMotion::clamp(forward/.8f);
        robeBlend+=(robeWanted-robeBlend)*(1.f-std::exp(-8.f*dt));
        if(robeWanted==0&&robeBlend<.001f)robeBlend=0;
        if(wanted==0 && walkBlend<.01f)walkBlend=0;
        // Visual cadence only: native pawn speed and controller input stay unchanged.
        walkPhase=std::fmod(walkPhase+.85f*speed*dt/(.75f*(fullEye/units/1.5f)),1.f);
    }
    const float walkFrame=walkPhase*24.f;const int walkA=int(walkFrame)%24,walkB=(walkA+1)%24;const float walkT=walkFrame-walkA;
    const FCoords body=GMath.UnitCoords/FRotator(0,bodyYaw+32768,0),tracking=GMath.UnitCoords/FRotator(0,bodyYaw,0);
    FVector origin=pawn->Location-FVector(0,0,HP2Climb::visualHalfHeight(pawn));
    origin.Z+=comfortOffset;
    const FVector headShift=basisVector(rotateYaw(trackingVector(pairInput.head.m[3]-originHead[0],0,pairInput.head.m[11]-originHead[2]),-originYaw),tracking)*units;
    // Feet stay planted during roomscale crouching. Torso lean follows head XY.
    const FVector lean=FVector(headShift.X,headShift.Y,0);
    origin+=body.XAxis*(AvatarPose::eyesForwardOfNeck()*units);
    auto world=[&](FVector p){const float torso=Clamp((p.Z-.08f)/.74f,0.f,1.f);return origin+basisVector(crouch(p,drop),body)*scale+lean*torso;};
    FVector starts[6],ends[6],handDir[2],handNormal[2];bool valid[2]{};
    for(int i=0;i<6;++i){starts[i]=world(vec(avatarJoints[i]));ends[i]=world(vec(avatarTips[i]));}
    auto orient=[&](FVector v,int arm){FVector f=vec(HandGripFrame::shaft[arm]).SafeNormal(),n=vec(avatarPalmNormal[arm]);FVector r=(n^f).SafeNormal();n=(f^r).SafeNormal();FVector tr=(handNormal[arm]^handDir[arm]).SafeNormal(),tu=(handDir[arm]^tr).SafeNormal();return handDir[arm]*(v|f)+tr*(v|r)+tu*(v|n);};
    for(int arm=0;arm<2;++arm) {
        const int hand=AvatarPose::controllerForArm(arm),i=arm*3;const auto& pose=pairInput.hand[hand];valid[arm]=HP2VR::finitePose(pose);if(!valid[arm])continue;
        const float* m=pose.m;
        FVector target=HP2Tracked::anchor(pawn)+basisVector(rotateYaw(trackingVector(m[3]-originHead[0],m[7]-originHead[1],m[11]-originHead[2]),-originYaw),tracking)*units;
        handDir[arm]=basisVector(rotateYaw(trackingVector(-m[2],-m[6],-m[10]),-originYaw),tracking).SafeNormal();
        handNormal[arm]=basisVector(rotateYaw(trackingVector(m[0],m[4],m[8]),-originYaw),tracking).SafeNormal()*HandGripFrame::dorsalSide(hand);
        target-=orient(vec(HandGripFrame::centre[arm])-vec(avatarJoints[i+2]),arm)*scale;
        FVector shoulder=starts[i],dir=(target-shoulder).SafeNormal();float upper=(ends[i]-shoulder).Size(),lower=(ends[i+1]-starts[i+1]).Size();
        const float stretch=AvatarPose::armStretch((target-shoulder).Size(),upper+lower);upper*=stretch;lower*=stretch;
        const float distance=Clamp((target-shoulder).Size(),Abs(upper-lower)+.01f,upper+lower-.01f);target=shoulder+dir*distance;
        const float along=(upper*upper-lower*lower+distance*distance)/(2*distance),height=std::sqrt(Max(0.f,upper*upper-along*along));
        FVector pole=body.YAxis*AvatarPose::elbowSide(arm)-body.ZAxis*.25f+body.XAxis*.08f;pole=(pole-dir*(pole|dir)).SafeNormal();
        const FVector elbow=shoulder+dir*along+pole*height;ends[i]=elbow;starts[i+1]=elbow;ends[i+1]=target;starts[i+2]=target;ends[i+2]=target+handDir[arm]*(vec(avatarTips[i+2])-vec(avatarJoints[i+2])).Size()*scale;
    }
    static FVector transformed[sizeof(avatarVertices)/sizeof(avatarVertices[0])];
    for(unsigned i=0;i<sizeof(avatarVertices)/sizeof(avatarVertices[0]);++i) {
        const auto& v=avatarVertices[i];const int arm=(v.w[4]+v.w[5]+v.w[6])>(v.w[1]+v.w[2]+v.w[3])?1:0,hand=AvatarPose::controllerForArm(arm);
        const float grip=(pairInput.buttons&(hand?HP2VR::RightGrip:HP2VR::LeftGrip))?1.f:0.f,trigger=Clamp(pairInput.trigger[hand],0.f,1.f);
        FVector p=vec(v.p)*((1-grip)*(1-trigger))+vec(v.poses[0])*(grip*(1-trigger))+vec(v.poses[1])*(grip*trigger)+vec(v.poses[2])*((1-grip)*trigger);
        // Keep the authored robe/hip displacement, not just the bottom legs.
        // The previous height mask froze the robe above the knees while the
        // trousers walked through it. Controller-arm weights stay independent.
        p+=(vec(avatarWalk[walkA][i])*(1-walkT)+vec(avatarWalk[walkB][i])*walkT)*(walkBlend*v.w[0]);
        HP2RobeMotion::apply(p.X,p.Y,p.Z,v.p[2],hp2FrontRobeWeights[i],robeBlend,drop);
        FVector out=world(p)*(v.w[0]+v.w[7]);
        for(int s=0;s<6;++s)if(v.w[s+1]>0) {
            FVector rel=basisVector(p-vec(avatarJoints[s]),body)*scale,rest=basisVector(vec(avatarTips[s])-vec(avatarJoints[s]),body);
            if(s%3==2&&valid[s/3])out+=(starts[s]+orient(p-vec(avatarJoints[s]),s/3)*scale)*v.w[s+1];
            else out+=(starts[s]+rotate(rel,rest,ends[s]-starts[s]))*v.w[s+1];
        }
        transformed[i]=out.TransformPointBy(frame->Coords);
    }
    FTextureInfo info{};atlas->Lock(info,frame->Viewport->CurrentTime,0,frame->Viewport->RenDev);
    for(const auto& t:avatarTriangles) {
        // First-person head/glasses are omitted, not allowed to enclose the HMD.
        if(avatarVertices[t.v[0]].w[7]+avatarVertices[t.v[1]].w[7]+avatarVertices[t.v[2]].w[7]>1.5f)continue;
        FTransTexture v[3]{};FTransTexture* ptr[3]{&v[0],&v[1],&v[2]};
        for(int k=0;k<3;++k){v[k].Point=transformed[t.v[k]];v[k].U=t.uv[k][0]*info.USize;v[k].V=(1-t.uv[k][1])*info.VSize;v[k].Light=FPlane(.8f,.8f,.8f,1);}
        VRDraw::polygon(frame,info,ptr,3,PF_TwoSided|PF_Occlude|(t.material==2?PF_Masked:0));
    }
    atlas->Unlock(info);
}
}
