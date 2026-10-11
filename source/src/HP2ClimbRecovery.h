#pragma once
#include "HP2ClimbPolicy.h"

// Native-first, like V31, but HP2 Destination is an absolute authored target.
// Never use HP1's MountDelta heuristic: HP2 modifies it for root animation.
namespace HP2Climb {
static APlayerPawn* owner{};
static ULevel* level{};
static bool watching{};
static bool recoveredFrame{},retryBlocked{};
static bool moving{};
static FVector moveTarget;
static float moveElapsed{};
static float releaseTime{};
static FVector start, last, destination;
static float elapsed{}, idle{}, walking{}, height{}, radius{};
static bool mounting(APlayerPawn* p) {
    const auto s=playerState(p);
    return !appStricmp(*s,TEXT("Mounting")) || !appStricmp(*s,TEXT("MountFinish"));
}
static bool finite(const FVector& v) {
    return std::isfinite(v.X)&&std::isfinite(v.Y)&&std::isfinite(v.Z);
}
// MountFinish temporarily halves HP2's collision cylinder. This must not
// raise the rendered body's floor into the camera by half a cylinder.
static float visualHalfHeight(APlayerPawn* p) {
    if(!p)return 0;
    if(mounting(p)) {
        if(owner==p && watching && height>0)return height;
        if(!appStricmp(*playerState(p),TEXT("MountFinish")))return p->CollisionHeight*2.f;
    }
    return p->CollisionHeight;
}
static void update(APlayerPawn* p,float seconds) {
    recoveredFrame=false;
    if(!p || p!=owner || p->GetLevel()!=level) {
        owner=p;level=p?p->GetLevel():nullptr;watching=false;retryBlocked=false;moving=false;releaseTime=0;
    }
    if(!p || !level)return;
    if(moving) {
        if(appStricmp(*playerState(p),TEXT("PlayerWalking"))){moving=false;return;}
        moveElapsed+=Clamp(seconds,0.f,.1f);p->Physics=PHYS_None;p->Velocity=p->Acceleration=FVector(0,0,0);
        FVector waypoint=moveTarget;
        // Sweep up alongside the ledge before advancing over its top.
        if(p->Location.Z<moveTarget.Z-1.f){waypoint.X=p->Location.X;waypoint.Y=p->Location.Y;}
        FVector delta=waypoint-p->Location;const float distance=delta.Size();const float step=150.f*Clamp(seconds,0.f,.05f);
        if(distance>step&&distance>0)delta*=step/distance;
        FCheckResult hit(1.f);level->MoveActor(p,delta,p->Rotation,hit);
        recoveredFrame=true;
        if((p->Location-moveTarget).SizeSquared()<2.f){moving=false;p->Physics=PHYS_Walking;retryBlocked=true;line("HP2 climb swept top-out complete");}
        else if(hit.Time<.9f||moveElapsed>2.f){moving=false;p->Physics=PHYS_Falling;retryBlocked=true;line("HP2 climb sweep blocked; no teleport performed");}
        return;
    }
    const bool climb=mounting(p);
    const float dt=std::isfinite(seconds)?Clamp(seconds,0.f,.1f):0.f;
    if(!climb&&!watching&&p->Physics==PHYS_Walking&&std::fabs(latestInput.move[1])<.2f)releaseTime+=dt;else releaseTime=0;
    if(releaseTime>=.15f)retryBlocked=false;
    if(climb && !watching) {
        auto* field=FindField<UStructProperty>(p->GetClass(),TEXT("Destination"));
        if(!field || field->ElementSize!=sizeof(FVector))return;
        destination=*reinterpret_cast<FVector*>(reinterpret_cast<BYTE*>(p)+field->Offset);
        if(!finite(destination))return;
        start=last=p->Location;
        // BeginState has already halved the cylinder when first seen here.
        const bool half=!appStricmp(*playerState(p),TEXT("MountFinish"));
        height=p->CollisionHeight*(half?2.f:1.f);
        radius=p->CollisionRadius*(half?2.f:1.f);
        elapsed=idle=walking=0;watching=true;retryBlocked=true;releaseTime=0;
        line("HP2 climb observing native attempt destination=%.2f,%.2f,%.2f",destination.X,destination.Y,destination.Z);
    }
    if(!watching)return;
    FVector offset=p->Location-start;offset.Z=0;
    const float centreZ=p->Location.Z+(climb?Max(0.f,height-p->CollisionHeight):0.f);
    if(!HP2ClimbPolicy::nearAttempt(start.Z,centreZ,offset.Size(),radius)){
        watching=false;retryBlocked=true;line("HP2 climb recovery cancelled: left original ledge; no return to top");return;
    }
    const bool normal=!appStricmp(*playerState(p),TEXT("PlayerWalking"));
    // Travel/cinematic/death states own the pawn; never interrupt them.
    if(!climb&&!normal){watching=false;return;}
    elapsed+=dt;
    if((p->Location-last).SizeSquared()>.25f){last=p->Location;idle=0;}else idle+=dt;
    if(normal && p->Physics==PHYS_Walking) {
        if(p->CollisionHeight>=height-.5f && p->Location.Z-start.Z>=Min(height*.7f,30.f)) {
            watching=false;retryBlocked=true;releaseTime=0;line("HP2 climb completed natively; release forward to rearm");return;
        }
        walking+=dt;
    }else walking=0;
    if(!HP2ClimbPolicy::recover(walking,elapsed,idle))return;
    watching=false;
    retryBlocked=true;
    // Run HP2 EndState to restore cylinder and PrePivot before testing fit.
    p->GotoState(FName(TEXT("PlayerWalking")));
    p->Velocity=FVector(0,0,0);p->Acceleration=FVector(0,0,0);
    p->Physics=PHYS_Falling; // failure must never leave PHYS_None frozen
    if(std::fabs(p->CollisionHeight-height)>.5f || std::fabs(p->CollisionRadius-radius)>.5f)return;
    FVector direction=destination-start;direction.Z=0;
    const float reach=radius*2.f+58.f;
    if(direction.SizeSquared()<1.f || direction.Size()>reach)return;
    direction=direction.SafeNormal();
    // Search only the authored approach, not unrelated platforms behind Harry.
    const float maxRise=Max(height*2.f,destination.Z-start.Z+height);
    if(maxRise>height*4.f)return;
    for(int i=0;i<5;++i) {
        FVector column=destination+direction*(i*radius*.5f);
        FVector horizontal=column-start;horizontal.Z=0;
        if(horizontal.Size()>reach)continue;
        FCheckResult hit(1.f);
        level->SingleLineCheck(hit,p,FVector(column.X,column.Y,start.Z-height+5.f),
            FVector(column.X,column.Y,start.Z+maxRise),TRACE_AllColliding);
        if(hit.Time>=1.f || hit.Normal.Z<.7f)continue;
        FVector target(column.X,column.Y,hit.Location.Z+height+1.f);
        if(!finite(target)||!HP2ClimbPolicy::top(target.Z,start.Z,p->Location.Z,maxRise,hit.Normal.Z))continue;
        // The restored full cylinder is checked by every MoveActor sweep.
        // Do not call FarMoveActor, even in test mode, for climb recovery.
        {
            p->Physics=PHYS_None;moveTarget=target;moving=true;moveElapsed=0;
            recoveredFrame=true;retryBlocked=true;
            line("HP2 climb starting checked swept top-out after %.2fs",elapsed);
            return;
        }
        break;
    }
    line("HP2 climb recovery rejected: no supported full-cylinder destination; falling locally");
}
}
