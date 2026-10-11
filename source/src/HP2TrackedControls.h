#pragma once
#include "v31-reference/WandInteraction.h"
#include "v31-reference/VRDraw.h"
#include "v31-reference/WandMesh.h"
#include "HP2Flick.h"
#include "HP2GnomeThrow.h"

namespace HP2Tracked {
static WandInteraction::State wand;
static uint32_t generation{};
static bool ready{};
static bool pressPending{};
static bool carryHeld{};
static HP2GnomeThrow::Gesture gnomeGesture;
static AActor* gestureGnome{};
static uint32_t gestureGeneration{};
static HP2VR::InputPayload throwSample{};
static bool haveThrowSample{};
static bool lessonMode(){return inputViewport&&inputViewport->Actor&&!appStricmp(*playerState(inputViewport->Actor),TEXT("SpellLearning"));}
static AActor* carriedGnome(){
    auto* a=inputViewport?Cast<AActor>(reflectedObject(inputViewport->Actor,TEXT("CarryingActor"))):nullptr;
    return a&&!a->bDeleteMe&&!appStricmp(a->GetClass()->GetName(),TEXT("Gnome"))?a:nullptr;
}
static void scalar(UObject* object,const TCHAR* name,float value) {
    auto* p=object?FindField<UFloatProperty>(object->GetClass(),name):nullptr;
    if(p && p->ElementSize==4)*reinterpret_cast<float*>(reinterpret_cast<BYTE*>(object)+p->Offset)=value;
}
static bool flag(UObject* object,const TCHAR* name) {
    auto* p=object?FindField<UBoolProperty>(object->GetClass(),name):nullptr;
    return p && (*reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(object)+p->Offset)&p->BitMask);
}
static void boolean(UObject* object,const TCHAR* name,bool value) {
    auto* p=object?FindField<UBoolProperty>(object->GetClass(),name):nullptr;
    if(!p)return;
    auto* word=reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(object)+p->Offset);
    if(value)*word|=p->BitMask;else *word&=~p->BitMask;
}

template<class T,class Property=UStructProperty> struct SavedField {
    T* field{}; T saved{};
    void set(UObject* object, const TCHAR* name, const T& value) {
        auto* p = object ? FindField<Property>(object->GetClass(), name) : nullptr;
        if (!p || p->ElementSize != sizeof(T)) return;
        field = reinterpret_cast<T*>(reinterpret_cast<BYTE*>(object) + p->Offset);
        saved = *field; *field = value;
    }
    ~SavedField() { if (field) *field = saved; }
};
struct SavedFlag {
    DWORD* field{}; DWORD mask{}; bool saved{};
    void clear(UObject* object, const TCHAR* name) {
        auto* p = object ? FindField<UBoolProperty>(object->GetClass(), name) : nullptr;
        if (!p) return;
        field = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(object) + p->Offset);
        mask = p->BitMask; saved = (*field & mask) != 0; *field &= ~mask;
    }
    ~SavedFlag() { if (field) { if (saved) *field |= mask; else *field &= ~mask; } }
};
static FVector anchor(APlayerPawn* pawn) {
    FVector point = pawn->Location;
    auto* p = FindField<UFloatProperty>(pawn->GetClass(), TEXT("BaseEyeHeight"));
    if (p && p->ElementSize == 4) point.Z += *reinterpret_cast<FLOAT*>(reinterpret_cast<BYTE*>(pawn) + p->Offset);
    point.Z += (broomPlayer(pawn)?Max(0.f,HP2Settings::eyeOffset):HP2Settings::eyeOffset)*.01f*WorldUnitsPerMetre + comfortOffset;
    if(HP2Climb::mounting(pawn))point.Z+=Max(0.f,HP2Climb::visualHalfHeight(pawn)-pawn->CollisionHeight);
    return point;
}
static bool carryPose(FVector& point,FVector& direction,const HP2VR::InputPayload& sample) {
    if(!inputViewport||!inputViewport->Actor||!haveOrigin||!HP2VR::finitePose(sample.hand[1]))return false;
    const auto& h=sample.hand[1];const FCoords basis=GMath.UnitCoords/FRotator(0,bodyYaw,0);
    point=anchor(inputViewport->Actor)+basisVector(rotateYaw(trackingVector(h.m[3]-originHead[0],h.m[7]-originHead[1],h.m[11]-originHead[2]),-originYaw),basis)*WorldUnitsPerMetre;
    direction=basisVector(rotateYaw(trackingVector(-h.m[2],-h.m[6],-h.m[10]),-originYaw),basis).SafeNormal();
    return true;
}
struct CarryPresentation {
    SavedField<FVector> location;SavedField<FRotator> rotation;
    CarryPresentation(){FVector p,d;auto* a=carriedGnome();if(a&&inputWasLive&&carryPose(p,d,pairInput)){location.set(a,TEXT("Location"),p-FVector(0,0,a->CollisionHeight*.65f));rotation.set(a,TEXT("Rotation"),d.Rotation());}}
};
static bool active() {
    return ready && inputWasLive && inputViewport && inputViewport->Actor &&
        !inputViewport->bShowWindowsMouse && !HP2Settings::open && walkingView(inputViewport->Actor);
}
static bool pose(const HP2VR::InputPayload& sample, FVector& grip, FVector& direction, FVector& up) {
    if (!ready || !inputViewport || !inputViewport->Actor || !haveOrigin) return false;
    const FCoords basis = GMath.UnitCoords / FRotator(0, bodyYaw, 0);
    auto p = wand.position;
    direction = FVector(0, 0, 1); up = basis.XAxis;
    if (wand.owner >= 0) {
        const auto& hand = sample.hand[wand.owner];
        if (!HP2VR::finitePose(hand)) return false;
        p = {hand.m[3], hand.m[7], hand.m[11]};
        direction = basisVector(rotateYaw(trackingVector(-hand.m[2], -hand.m[6], -hand.m[10]), -originYaw), basis).SafeNormal();
        up = basisVector(rotateYaw(trackingVector(hand.m[1], hand.m[5], hand.m[9]), -originYaw), basis);
    }
    grip = anchor(inputViewport->Actor) + basisVector(rotateYaw(
        trackingVector(p.x - originHead[0], p.y - originHead[1], p.z - originHead[2]), -originYaw), basis) * WorldUnitsPerMetre;
    return true;
}
static void update(float dt, bool live, bool menu) {
    const bool available = live && !menu && HP2VR::finitePose(latestInput.head) && !carriedGnome();
    if (available && generation != latestInput.head.trackingGeneration) {
        generation = latestInput.head.trackingGeneration;
        wand = WandInteraction::State{};
    }
    WandInteraction::Point head{latestInput.head.m[3], latestInput.head.m[7], latestInput.head.m[11]};
    const auto socket = available ? WandInteraction::holsterSocket(head, originYaw, .25f, .05f, .75f) : wand.position;
    WandInteraction::Point hands[2]{};
    bool valid[2]{}, grips[2]{}, triggers[2]{};
    for (int i = 0; i < 2; ++i) {
        const auto& hand = latestInput.hand[i];
        hands[i] = {hand.m[3], hand.m[7], hand.m[11]};
        valid[i] = HP2VR::finitePose(hand);
        grips[i] = (latestInput.buttons & (i ? HP2VR::RightGrip : HP2VR::LeftGrip)) != 0;
        triggers[i] = latestInput.trigger[i] > .65f;
    }
    const int previous = wand.owner;
    const bool wasCharging=wand.charging;
    wand.update(Clamp(dt, 0.f, .1f), socket, hands, valid, grips, triggers, available);
    if(wand.charging && !wasCharging)pressPending=true;
    if(!wand.charging)pressPending=false;
    ready = available;
    auto* gnome=carriedGnome();
    if(gnome!=gestureGnome||gestureGeneration!=latestInput.head.trackingGeneration){gnomeGesture.reset();haveThrowSample=false;gestureGnome=gnome;gestureGeneration=latestInput.head.trackingGeneration;}
    const bool motionValid=gnome&&live&&!menu&&HP2Settings::immersiveGnomes&&HP2VR::finitePose(latestInput.head)&&HP2VR::finitePose(latestInput.hand[1])&&walkingView(inputViewport->Actor);
    if(!live||menu||!HP2Settings::immersiveGnomes||!HP2VR::finitePose(latestInput.hand[1]))haveThrowSample=false;
    const float* h=latestInput.hand[1].m;const float* headPose=latestInput.head.m;
    const FVector motion=rotateYaw(trackingVector(h[3]-headPose[3]-.25f*h[2],h[7]-headPose[7]-.25f*h[6],h[11]-headPose[11]-.25f*h[10]),-originYaw);
    const bool gesture=gnomeGesture.update(motion.X,motion.Y,motion.Z,dt,motionValid);
    if(gesture){throwSample=latestInput;haveThrowSample=true;line("HP2 gnome forward gesture: native throw requested, no wand");}
    const bool held=live&&!menu&&gnome&&HP2VR::finitePose(latestInput.hand[1])&&(latestInput.trigger[1]>.65f||gesture);
    if(held&&!carryHeld)pressPending=true;
    carryHeld=held;
    if (previous != wand.owner) line("HP2 tracked wand owner=%d (hold grip; trigger armed=%d)", wand.owner, wand.triggerReady);
    if (wand.cancel && inputViewport) invokeNoParameters(inputViewport->Actor, TEXT("StopAiming"));
}

// Scope the native cursor query to the controller ray. HP2 still performs its
// own traces, target checks, spell selection and game-state restrictions.
struct AimScope {
    SavedField<FVector> location, forward, target;
    SavedField<FRotator> rotation;
    AimScope(UObject* cursor) {
        if (!active() || wand.owner < 0 || cursor != reflectedObject(inputViewport->Actor, TEXT("SpellCursor"))) return;
        FVector grip, dir, up;
        if (!pose(latestInput, grip, dir, up)) return;
        UObject* camera = reflectedObject(inputViewport->Actor, TEXT("Cam"));
        UObject* camTarget = reflectedObject(camera, TEXT("CamTarget"));
        location.set(camera, TEXT("Location"), grip);
        forward.set(camera, TEXT("vForward"), dir);
        rotation.set(camera, TEXT("Rotation"), dir.Rotation());
        target.set(camTarget, TEXT("Location"), grip + dir * (wandLengthMetres * WorldUnitsPerMetre));
        static bool reported{};
        if (!reported && location.field && forward.field && rotation.field && target.field) {
            line("HP2 native cursor controller-ray scope verified"); reported = true;
        }
    }
};
using Native = void(UObject::*)(FFrame&, void* const);
struct Hook { UFunction* fn{}; Native original{}; DWORD flags{}; };
static Hook moveHook, cursorHook, targetHook, tipHook, inputHook, fovHook, projectileHook, lessonHook, throwHook, mountHook, subtitleInHook, subtitleOutHook;
static Hook vendorInputHook,vendorYesHook,vendorNoHook,vendorDrawHook;
static Hook mountTickHook;
static Hook scoreHudHook,statusHudHook;
static bool projectileLayout(UFunction* fn) {
    if(!fn || fn->Script.Num()!=761 || fn->ParmsSize!=24)return false;
    const int starts[]={307,386,434,471};
    for(int i=0;i<4;++i) {
        auto* p=FindField<UStructProperty>(fn,i?TEXT("R"):TEXT("vStart"));
        UProperty* encoded{};
        appMemcpy(&encoded,&fn->Script(starts[i]+2),sizeof(encoded));
        if(!p || p->ElementSize!=12 || fn->Script(starts[i])!=EX_Let || fn->Script(starts[i]+1)!=EX_LocalVariable || encoded!=p)return false;
    }
    return fn->Script(377)==EX_JumpIfNot && fn->Script(411)==EX_Jump && fn->Script(468)==EX_Jump && fn->Script(496)==EX_Let;
}
class Adapter : public UObject {
    void call(Hook& hook, Native replacement, FFrame& stack, void* const result) {
        hook.fn->Func = hook.original; hook.fn->FunctionFlags = hook.flags;
        struct Restore { Hook& h; Native n; ~Restore() { h.fn->Func = n; h.fn->FunctionFlags = h.flags | FUNC_Native; } } restore{hook, replacement};
        if (stack.Node == hook.fn) (this->*hook.original)(stack, result);
        else CallFunction(stack, result, hook.fn);
    }
public:
    void insetHud(Hook& h,Native replacement,FFrame& stack,void* const result){
        UObject* canvas{};
        if(stack.Node==h.fn)canvas=*reinterpret_cast<UObject**>(stack.Locals);
        else {BYTE* code=stack.Code;stack.Step(this,&canvas);stack.Code=code;}
        SavedField<INT,UIntProperty> sx,sy;SavedField<FLOAT,UFloatProperty> ox,oy,cx,cy;
        if(canvas&&pairTracked&&currentEye>=0&&inputViewport&&!inputViewport->bShowWindowsMouse&&!HP2Settings::open){
            auto* px=FindField<UIntProperty>(canvas->GetClass(),TEXT("SizeX"));auto* py=FindField<UIntProperty>(canvas->GetClass(),TEXT("SizeY"));
            if(px&&py){const INT w=*reinterpret_cast<INT*>(reinterpret_cast<BYTE*>(canvas)+px->Offset),height=*reinterpret_cast<INT*>(reinterpret_cast<BYTE*>(canvas)+py->Offset);
                const float inset=HP2Settings::hudInset*.01f,lower=HP2Settings::hudLower*.01f;
                sx.set(canvas,TEXT("SizeX"),INT(w*(1-2*inset)));sy.set(canvas,TEXT("SizeY"),INT(height*(1-2*inset-Max(0.f,lower))));
                ox.set(canvas,TEXT("OrgX"),w*inset);oy.set(canvas,TEXT("OrgY"),height*(inset+lower));
                cx.set(canvas,TEXT("ClipX"),w*(1-2*inset));cy.set(canvas,TEXT("ClipY"),height*(1-2*inset-Max(0.f,lower)));
            }
        }
        call(h,replacement,stack,result);
    }
    void scoreHud(FFrame& s,void* const r){insetHud(scoreHudHook,static_cast<Native>(&Adapter::scoreHud),s,r);}
    void statusHud(FFrame& s,void* const r){insetHud(statusHudHook,static_cast<Native>(&Adapter::statusHud),s,r);}
    void mountTick(FFrame& stack,void* const result) {
        auto* pawn=inputViewport?inputViewport->Actor:nullptr;
        if(inputWasLive && this==static_cast<UObject*>(pawn)) {
            auto* p=FindField<UStructProperty>(GetClass(),TEXT("MountDelta"));
            if(p && p->ElementSize==sizeof(FVector)) {
                auto* delta=reinterpret_cast<FVector*>(reinterpret_cast<BYTE*>(this)+p->Offset);const FVector saved=*delta;
                // Native negative residual assumes skeletal root lift. Do not
                // apply a downward pull when that lift is absent in VR.
                delta->Z=Max(0.f,delta->Z);
                call(mountTickHook,static_cast<Native>(&Adapter::mountTick),stack,result);*delta=saved;return;
            }
        }
        call(mountTickHook,static_cast<Native>(&Adapter::mountTick),stack,result);
    }
    void vendorInput(FFrame& stack,void* const result) {
        if(HP2Vendor::active() && this==HP2Vendor::manager()) {
            auto* pawn=inputViewport->Actor;auto* field=FindField<UByteProperty>(pawn->GetClass(),TEXT("bVendorReply"));
            if(field && field->ElementSize==1){auto* value=reinterpret_cast<BYTE*>(pawn)+field->Offset;const BYTE saved=*value;*value=HP2Vendor::choice()>=0?1:0;
                const int selected=HP2Vendor::choice();
                call(vendorInputHook,static_cast<Native>(&Adapter::vendorInput),stack,result);*value=saved;
                if(selected>=0){line("HP2 native vendor consumed choice=%d bar=%d",selected,flag(inputViewport->Console,TEXT("bVendorBar")));HP2Vendor::consumed();}return;}
        }
        call(vendorInputHook,static_cast<Native>(&Adapter::vendorInput),stack,result);
    }
    void vendorHover(Hook& hook,Native replacement,FFrame& stack,void* const result,int button) {
        if(HP2Vendor::active() && this==HP2Vendor::manager()) {
            if(stack.Node!=hook.fn && *stack.Code==EX_EndFunctionParms)++stack.Code;
            if(result)*static_cast<UBOOL*>(result)=HP2Vendor::choice()==button;return;
        }
        call(hook,replacement,stack,result);
    }
    void vendorYes(FFrame& stack,void* const result){vendorHover(vendorYesHook,static_cast<Native>(&Adapter::vendorYes),stack,result,0);}
    void vendorNo(FFrame& stack,void* const result){vendorHover(vendorNoHook,static_cast<Native>(&Adapter::vendorNo),stack,result,1);}
    void vendorDraw(FFrame& stack,void* const result) {
        if(HP2Vendor::active() && this==HP2Vendor::manager()) {if(stack.Node!=vendorDrawHook.fn){UObject* canvas{};stack.Step(this,&canvas);if(*stack.Code==EX_EndFunctionParms)++stack.Code;}return;}
        call(vendorDrawHook,static_cast<Native>(&Adapter::vendorDraw),stack,result);
    }
    void subtitle(Hook& hook,Native replacement,FFrame& stack,void* const result) {
        if(currentEye==1 && pairTracked) {
            // HP2 advances the border IN DRAW, not Tick. Left eye already
            // advanced it. Right eye must draw that identical height.
            if(stack.Node!=hook.fn){UObject* canvas{};stack.Step(this,&canvas);stack.Code++;}
            static bool reported{};if(!reported){reported=true;line("HP2 subtitle border: right-eye duplicate animation step suppressed");}
            return;
        }
        call(hook,replacement,stack,result);
    }
    void subtitleIn(FFrame& s,void* const r){subtitle(subtitleInHook,static_cast<Native>(&Adapter::subtitleIn),s,r);}
    void subtitleOut(FFrame& s,void* const r){subtitle(subtitleOutHook,static_cast<Native>(&Adapter::subtitleOut),s,r);}
    void mount(FFrame& stack,void* const result) {
        if(inputWasLive && inputViewport && this==static_cast<UObject*>(inputViewport->Actor) && HP2Climb::retryBlocked) {
            // Consume arguments without entering another animation while the
            // same failed approach is held. Native checked recovery ran first.
            if(stack.Node!=mountHook.fn){FVector ignored;stack.Step(this,&ignored);stack.Code++;}
            return;
        }
        call(mountHook,static_cast<Native>(&Adapter::mount),stack,result);
    }
    void throwing(FFrame& stack,void* const result) {
        SavedField<FVector> forward;
        FVector hand,dir;auto* a=carriedGnome();
        if(a && inputWasLive && this==static_cast<UObject*>(inputViewport->Actor) && carryPose(hand,dir,(haveThrowSample&&a==gestureGnome)?throwSample:latestInput)) {
            auto* pawn=inputViewport->Actor;
            // Native release keeps its accuracy targeting, collision flags,
            // throw speed and stateBeingThrown. Only hand origin/aim change.
            const FVector origin=pawn->Location+FVector(0,0,pawn->CollisionHeight*.5f);
            FCheckResult hit(1.f);pawn->GetLevel()->SingleLineCheck(hit,pawn,hand,origin,TRACE_VisBlocking,FVector(a->CollisionRadius,a->CollisionRadius,a->CollisionHeight));
            if(hit.Time<1.f)hand=origin+(hand-origin)*Max(0.f,hit.Time-.05f);
            pawn->GetLevel()->FarMoveActor(a,hand,0,0);
            forward.set(reflectedObject(pawn,TEXT("Cam")),TEXT("vForward"),dir);
            line("HP2 native gnome throw from tracked right hand (no wand required)");
        }
        call(throwHook,static_cast<Native>(&Adapter::throwing),stack,result);
    }
    void lesson(FFrame& stack,void* const result) {
        static HP2Flick::Recognizer flick;static UObject* prior{};static DWORD last{};
        auto* pawn=inputViewport?inputViewport->Actor:nullptr;
        if(!HP2Settings::flickLessons || !lessonMode() || this!=reflectedObject(pawn,TEXT("CurrSpellLesson"))) {
            flick.reset();call(lessonHook,static_cast<Native>(&Adapter::lesson),stack,result);return;
        }
        const DWORD now=GetTickCount();float dt=last?(now-last)*.001f:0.f;last=now;
        if(prior!=this){flick.reset();prior=this;}
        const bool valid=inputWasLive&&!HP2Settings::open&&!inputViewport->bShowWindowsMouse&&wand.owner>=0&&HP2VR::finitePose(latestInput.hand[wand.owner]);
        float x=0,y=0;
        if(valid){const float* m=latestInput.hand[wand.owner].m;
            const FVector plane=rotateYaw(trackingVector(m[3]-latestInput.head.m[3]-.35f*m[2],m[7]-latestInput.head.m[7]-.35f*m[6],m[11]-latestInput.head.m[11]-.35f*m[10]),-originYaw);
            x=plane.Y;y=plane.Z;
        }
        const int key=flick.update(x,y,dt,valid);
        struct RestoreInput {BYTE* ptr[4]{};BYTE saved[4]{};~RestoreInput(){for(int i=0;i<4;++i)if(ptr[i])*ptr[i]=saved[i];}} saved;
        const TCHAR* names[]{TEXT("bSpellLessonUp"),TEXT("bSpellLessonDown"),TEXT("bSpellLessonLeft"),TEXT("bSpellLessonRight")};
        for(int i=0;i<4;++i){auto* p=FindField<UByteProperty>(pawn->GetClass(),names[i]);if(p&&p->ElementSize==1){saved.ptr[i]=reinterpret_cast<BYTE*>(pawn)+p->Offset;saved.saved[i]=*saved.ptr[i];*saved.ptr[i]=key==i?1:0;}}
        auto* px=FindField<UFloatProperty>(pawn->GetClass(),TEXT("aSpellLessonX"));auto* py=FindField<UFloatProperty>(pawn->GetClass(),TEXT("aSpellLessonY"));
        float oldx=px?*reinterpret_cast<float*>(reinterpret_cast<BYTE*>(pawn)+px->Offset):0,oldy=py?*reinterpret_cast<float*>(reinterpret_cast<BYTE*>(pawn)+py->Offset):0;
        scalar(pawn,TEXT("aSpellLessonX"),0);scalar(pawn,TEXT("aSpellLessonY"),0);
        call(lessonHook,static_cast<Native>(&Adapter::lesson),stack,result);
        scalar(pawn,TEXT("aSpellLessonX"),oldx);scalar(pawn,TEXT("aSpellLessonY"),oldy);
        if(key>=0)line("HP2 lesson physical flick direction=%d; native round judges timing",key);
    }
    void projectile(FFrame& stack, void* const result) {
        // HP2 does NOT launch through GetWandEndPoint. Its own ProjectileFire2
        // separately reads mesh WeaponLoc and third-person Cam.Rotation.
        // Scope only the verified non-sword start/rotation expressions, like
        // V31. Keep native Spawn, failure fallback, boss tuning and hit logic.
        struct ScriptRestore {
            UFunction* fn{}; BYTE saved[761]{};
            ~ScriptRestore(){if(fn)appMemcpy(fn->Script.GetData(),saved,sizeof(saved));}
        } restore;
        FVector grip,dir,up,tip;bool tracked=false;
        if(active() && wand.owner>=0 && this==reflectedObject(inputViewport->Actor,TEXT("Weapon")) && !flag(this,TEXT("bUsingSword")) && pose(latestInput,grip,dir,up) && projectileLayout(projectileHook.fn)) {
            tip=grip+dir*(wandLengthMetres*WorldUnitsPerMetre);
            const FRotator rotation=dir.Rotation();
            restore.fn=projectileHook.fn;appMemcpy(restore.saved,restore.fn->Script.GetData(),sizeof(restore.saved));
            auto constant=[&](int begin,int end,BYTE token,const void* value) {
                BYTE* bytes=static_cast<BYTE*>(restore.fn->Script.GetData());
                appMemset(bytes+begin,EX_Nothing,end-begin);bytes[begin]=token;appMemcpy(bytes+begin+1,value,12);
            };
            constant(313,377,EX_VectorConst,&tip);
            constant(392,411,EX_RotationConst,&rotation);
            constant(440,468,EX_RotationConst,&rotation);
            constant(477,496,EX_RotationConst,&rotation);
            tracked=true;
        }
        call(projectileHook,static_cast<Native>(&Adapter::projectile),stack,result);
        if(tracked) {
            auto* shot=result?Cast<AActor>(*static_cast<UObject**>(result)):nullptr;
            line("HP2 tracked projectile=%ls tipError=%.3f forwardDot=%.4f",shot?shot->GetClass()->GetName():TEXT("none"),shot?(shot->Location-tip).Size():-1.f,shot?(shot->Rotation.Vector()|dir):0.f);
        }
    }
    void input(FFrame& stack, void* const result) {
        call(inputHook,static_cast<Native>(&Adapter::input),stack,result);
        if(inputViewport&&this==static_cast<UObject*>(inputViewport->Actor)&&broomPlayer(inputViewport->Actor)) {
            const auto control=HP2NativeInput::map(latestInput,inputWasLive,inputViewport->bShowWindowsMouse||HP2Settings::open);
            // BroomHarry reads these after Super.PlayerInput returns.
            // Match HP2's Joy bindings; its native invert/sensitivity remain in charge.
            scalar(this,TEXT("aJoyBroomYaw"),control.moveX*13312.f);
            scalar(this,TEXT("aJoyBroomPitch"),-control.moveY*13312.f);
            static bool aHeld{},gripHeld{};
            const bool flight=activeFlight(inputViewport->Actor)&&inputWasLive&&!inputViewport->bShowWindowsMouse&&!HP2Settings::open;
            const bool a=flight&&(latestInput.buttons&HP2VR::SkipDialogue);
            const bool grip=flight&&(latestInput.buttons&HP2VR::RightGrip);
            bool action=false;
            if(a&&!aHeld){
                invokeNoParameters(this,TEXT("UpdateKickTarget"));
                auto* target=Cast<AActor>(reflectedObject(this,TEXT("KickTarget")));
                auto* range=FindField<UFloatProperty>(GetClass(),TEXT("TrackingOffsetRange_Horz"));
                const float reach=range?*reinterpret_cast<float*>(reinterpret_cast<BYTE*>(this)+range->Offset)*2.f:0.f;
                action=target&&!target->bHidden&&(target->Location-inputViewport->Actor->Location).Size()<reach;
            }
            if(grip&&!gripHeld&&flag(this,TEXT("bReaching"))&&!flag(this,TEXT("bCaughtTarget"))){
                auto* target=Cast<AActor>(reflectedObject(this,TEXT("LookForTarget")));
                FVector hand,dir;
                if(target&&!target->bHidden&&!appStricmp(target->GetClass()->GetName(),TEXT("Snitch"))&&carryPose(hand,dir,latestInput))
                    action=action||(target->Location-hand).Size()<.4f*WorldUnitsPerMetre;
            }
            auto* button=FindField<UByteProperty>(GetClass(),TEXT("bBroomAction"));
            if(flight&&button&&button->ElementSize==1)*reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(this)+button->Offset)=action?1:0;
            if(action)line("HP2 Quidditch native action from %s",a&&!aHeld?"A kick":"near-snitch grip");
            aHeld=a;gripHeld=grip;
            static bool reported{};if(!reported){reported=true;line("HP2 native broom axes active: left stick XY -> yaw/pitch");}
            static unsigned seen{};const unsigned direction=control.moveY>.3f?1:control.moveY<-.3f?2:4;
            if(!(seen&direction)){seen|=direction;line("HP2 broom pitch input sample=%.3f native=%.3f",control.moveY,-control.moveY*13312.f);}
        }
        if(inputViewport && this==static_cast<UObject*>(inputViewport->Actor) && ((active() && wand.owner>=0)||carriedGnome())) {
            const bool held=carriedGnome()?carryHeld:wand.charging;
            boolean(inputViewport->Console,TEXT("bSpaceReleased"),!held);
            auto* alt=FindField<UByteProperty>(GetClass(),TEXT("bAltFire"));
            if(alt && alt->ElementSize==1)*reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(this)+alt->Offset)=held?1:0;
            if(pressPending) {
                pressPending=false;
                if(!flag(this,TEXT("bIsAiming")) && !(carriedGnome()&&flag(this,TEXT("bThrow")))) {
                    auto* fn=FindFunction(TEXT("AltFire"));
                    auto* p=fn?FindField<UFloatProperty>(fn,TEXT("f")):nullptr;
                    if(fn && fn->ParmsSize==4 && p && p->Offset==0){FLOAT value=0;ProcessEvent(fn,&value);}
                }
            }
        }
    }
    void move(FFrame& stack, void* const result) {
        SavedFlag screen, autoCenter;
        if (active() && this == static_cast<UObject*>(inputViewport->Actor)) {
            if(wand.owner>=0) {
                // WinDrv polls the physical mouse before PlayerMove and may
                // synthesize a release for our virtual press. Reassert the
                // tracked button at the native gameplay entry, after that poll.
                boolean(inputViewport->Console,TEXT("bSpaceReleased"),!wand.charging);
                auto* alt=FindField<UByteProperty>(GetClass(),TEXT("bAltFire"));
                if(alt && alt->ElementSize==1)*reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(this)+alt->Offset)=wand.charging?1:0;
            }
            screen.clear(this, TEXT("bScreenRelativeMovement"));
            autoCenter.clear(this,TEXT("bAutoCenterCamera"));
            // Set one analog source at the native movement entry, including
            // zero every neutral frame. Do not mix keyboard hold and Joy axes.
            const auto input=HP2NativeInput::map(latestInput,true,false);
            scalar(this,TEXT("aForward"),HP2Climb::moving?0.f:input.moveY*6000.f);
            scalar(this,TEXT("aStrafe"),HP2Climb::moving?0.f:input.moveX*6000.f);
            scalar(this,TEXT("aSideMove"),0);
            scalar(this,TEXT("aTurn"),0);
            auto* camera=Cast<AActor>(reflectedObject(this,TEXT("Cam")));
            if(camera && !flag(this,TEXT("bInDuelingMode"))) {
                camera->Rotation.Yaw=bodyYaw;
                camera->DesiredRotation.Yaw=bodyYaw;
            }
            synchronizeBodyForward(inputViewport->Actor, bodyTrackingGeneration, true);
        }
        call(moveHook, static_cast<Native>(&Adapter::move), stack, result);
        if (active()) synchronizeBodyForward(inputViewport->Actor, bodyTrackingGeneration, true);
    }
    void cursor(FFrame& stack, void* const result) {
        AimScope aim(this);
        call(cursorHook, static_cast<Native>(&Adapter::cursor), stack, result);
    }
    void target(FFrame& stack, void* const result) {
        AimScope aim(this);
        call(targetHook, static_cast<Native>(&Adapter::target), stack, result);
    }
    void fov(FFrame& stack, void* const result) {
        AimScope aim(this);
        call(fovHook,static_cast<Native>(&Adapter::fov),stack,result);
    }
    void tip(FFrame& stack, void* const result) {
        call(tipHook, static_cast<Native>(&Adapter::tip), stack, result);
        if (active() && wand.owner >= 0 && result &&
            this == reflectedObject(inputViewport->Actor, TEXT("Weapon"))) {
            FVector grip, dir, up;
            if (pose(latestInput, grip, dir, up)) {
                *static_cast<FVector*>(result) = grip + dir * (wandLengthMetres * WorldUnitsPerMetre);
                static bool reported{};
                if (!reported) { line("HP2 native wand effect endpoint follows tracked tip"); reported = true; }
            }
        }
    }
};
static void installOne(Hook& hook, const TCHAR* path, Native replacement, int bytes, const TCHAR* field, const TCHAR* type) {
    if (hook.fn) return;
    auto* fn = FindObject<UFunction>(nullptr, path);
    if (!fn || !fn->Script.Num() || (fn->FunctionFlags & FUNC_Native) || fn->ParmsSize != bytes) return;
    auto* p = FindField<UProperty>(fn, field);
    if (!p || p->Offset != 0 || appStricmp(p->GetClass()->GetName(), type)) return;
    hook = {fn, fn->Func, fn->FunctionFlags}; fn->AddToRoot();
    fn->Func = replacement; fn->FunctionFlags |= FUNC_Native;
    line("HP2 validated native adapter %ls script=%d parameters=%d", path, fn->Script.Num(), fn->ParmsSize);
}
static void install() {
    installOne(scoreHudHook,TEXT("HGame.ChallengeScoreManager.DrawScore"),static_cast<Native>(&Adapter::scoreHud),8,TEXT("Canvas"),TEXT("ObjectProperty"));
    installOne(statusHudHook,TEXT("HGame.StatusManager.RenderHudItemManager"),static_cast<Native>(&Adapter::statusHud),16,TEXT("Canvas"),TEXT("ObjectProperty"));
    installOne(mountTickHook,TEXT("HGame.harry.MountFinish.PlayerTick"),static_cast<Native>(&Adapter::mountTick),4,TEXT("DeltaTime"),TEXT("FloatProperty"));
    installOne(vendorInputHook,TEXT("HGame.VendorManager.VendorTransaction.PlayerInput"),static_cast<Native>(&Adapter::vendorInput),4,TEXT("fDeltaTime"),TEXT("FloatProperty"));
    installOne(vendorYesHook,TEXT("HGame.VendorManager.IsMouseOverVendorYes"),static_cast<Native>(&Adapter::vendorYes),4,TEXT("ReturnValue"),TEXT("BoolProperty"));
    installOne(vendorNoHook,TEXT("HGame.VendorManager.IsMouseOverVendorNo"),static_cast<Native>(&Adapter::vendorNo),4,TEXT("ReturnValue"),TEXT("BoolProperty"));
    installOne(vendorDrawHook,TEXT("HGame.VendorManager.DrawVendorBar"),static_cast<Native>(&Adapter::vendorDraw),4,TEXT("canvas"),TEXT("ObjectProperty"));
    installOne(subtitleInHook,TEXT("HGame.CutSceneManager.SlideIn.SetCurrBorderHeight"),static_cast<Native>(&Adapter::subtitleIn),4,TEXT("Canvas"),TEXT("ObjectProperty"));
    installOne(subtitleOutHook,TEXT("HGame.CutSceneManager.SlideOut.SetCurrBorderHeight"),static_cast<Native>(&Adapter::subtitleOut),4,TEXT("Canvas"),TEXT("ObjectProperty"));
    installOne(mountHook,TEXT("HGame.harry.Mount"),static_cast<Native>(&Adapter::mount),12,TEXT("Delta"),TEXT("StructProperty"));
    if(!throwHook.fn){auto* fn=FindObject<UFunction>(nullptr,TEXT("HGame.harry.ThrowCarryingActor"));
        if(fn&&fn->Script.Num()>0&&fn->ParmsSize==0&&!(fn->FunctionFlags&FUNC_Native)) {
            throwHook={fn,fn->Func,fn->FunctionFlags};fn->AddToRoot();fn->Func=static_cast<Native>(&Adapter::throwing);fn->FunctionFlags|=FUNC_Native;
            line("HP2 native carried-object throw adapter installed");
        }
    }
    installOne(lessonHook,TEXT("HGame.SpellLessonTrigger.PlayGame.PlayerInput"),static_cast<Native>(&Adapter::lesson),4,TEXT("fDeltaTime"),TEXT("FloatProperty"));
    auto* launch=FindObject<UFunction>(nullptr,TEXT("HGame.baseWand.ProjectileFire2"));
    if(projectileLayout(launch))installOne(projectileHook,TEXT("HGame.baseWand.ProjectileFire2"),static_cast<Native>(&Adapter::projectile),24,TEXT("ProjClass"),TEXT("ClassProperty"));
    static bool inspected{};
    if(!inspected && GetEnvironmentVariableA("HP2VR_SPELL_INSPECT",nullptr,0)) {
        auto* weapon=inputViewport?reflectedObject(inputViewport->Actor,TEXT("Weapon")):nullptr;
        auto* fn=weapon?weapon->FindFunction(TEXT("ProjectileFire2")):nullptr;
        if(fn) {
            inspected=true;
            line("ProjectileFire2 path=%ls script=%d params=%d flags=%x",fn->GetPathName(),fn->Script.Num(),fn->ParmsSize,fn->FunctionFlags);
            for(TFieldIterator<UProperty> p(fn);p;++p)line("Projectile field %ls type=%ls offset=%d size=%d ptr=%p",p->GetName(),p->GetClass()->GetName(),p->Offset,p->ElementSize,*p);
            for(int i=0;i<fn->Script.Num();i+=16) {
                char bytes[64]{};int n=0;
                for(int j=i;j<Min(i+16,fn->Script.Num());++j)n+=sprintf_s(bytes+n,sizeof(bytes)-n,"%02x ",fn->Script(j));
                line("Projectile bytecode %d %s",i,bytes);
            }
        }
    }
    installOne(inputHook,TEXT("HGame.harry.PlayerInput"),static_cast<Native>(&Adapter::input),4,TEXT("DeltaTime"),TEXT("FloatProperty"));
    installOne(moveHook, TEXT("HGame.harry.PlayerWalking.PlayerMove"), static_cast<Native>(&Adapter::move), 4, TEXT("DeltaTime"), TEXT("FloatProperty"));
    installOne(cursorHook, TEXT("HGame.SpellCursor.UpdateCursor"), static_cast<Native>(&Adapter::cursor), 4, TEXT("bJustStopAtClosestPawnOrWall"), TEXT("BoolProperty"));
    installOne(targetHook, TEXT("HGame.SpellCursor.LookForTarget"), static_cast<Native>(&Adapter::target), 4, TEXT("ReturnValue"), TEXT("BoolProperty"));
    installOne(fovHook,TEXT("HGame.SpellCursor.CanCameraSeeYouInFOV"),static_cast<Native>(&Adapter::fov),20,TEXT("rOutsideFOV"),TEXT("IntProperty"));
    installOne(tipHook, TEXT("HGame.baseWand.GetWandEndPoint"), static_cast<Native>(&Adapter::tip), sizeof(FVector), TEXT("ReturnValue"), TEXT("StructProperty"));
}
static void remove() {
    for (auto* h : {&moveHook, &cursorHook, &targetHook, &tipHook,&inputHook,&fovHook,&projectileHook,&lessonHook,&throwHook,&mountHook,&subtitleInHook,&subtitleOutHook}) if (h->fn) {
        h->fn->Func = h->original; h->fn->FunctionFlags = h->flags; h->fn->RemoveFromRoot(); *h = Hook{};
    }
}
static void projectileSelfTest() {
    static bool done{};
    if(done || !GetEnvironmentVariableA("HP2VR_PROJECTILE_TEST",nullptr,0) || !active() || wand.owner<0 || !wand.charging || !projectileHook.fn)return;
    done=true;
    auto* weapon=reflectedObject(inputViewport->Actor,TEXT("Weapon"));
    if(!weapon)return;
    BYTE saved[761];appMemcpy(saved,projectileHook.fn->Script.GetData(),sizeof(saved));
    for(const TCHAR* name:{TEXT("HGame.spellLumos"),TEXT("HGame.spellFlipendo")}) {
        auto* cls=FindObject<UClass>(nullptr,name);
        if(!cls){line("HP2 projectile SELFTEST missing %ls",name);continue;}
        struct Params {UClass* cls;FLOAT speed;DWORD warn,weaponRot;AActor* target;AActor* result;} params{cls,400,0,0,nullptr,nullptr};
        static_assert(sizeof(Params)==24);
        weapon->ProcessEvent(projectileHook.fn,&params);
        FVector grip,dir,up;pose(latestInput,grip,dir,up);
        const FVector tip=grip+dir*(wandLengthMetres*WorldUnitsPerMetre);
        const bool restored=!appMemcmp(saved,projectileHook.fn->Script.GetData(),sizeof(saved));
        const bool ok=params.result && (params.result->Location-tip).Size()<.1f && (params.result->Rotation.Vector()|dir)>.999f && restored;
        line("HP2 projectile SELFTEST %ls %s scriptRestored=%d",name,ok?"PASS":"FAIL",restored);
        if(params.result)params.result->GetLevel()->DestroyActor(params.result);
    }
}
struct RunePresentation {
    SavedField<FVector> location;
    SavedField<FRotator> rotation;
    RunePresentation() {
    if(!active() || wand.owner<0)return;
    auto* cursor=reflectedObject(inputViewport->Actor,TEXT("SpellCursor"));
    auto* target=Cast<AActor>(reflectedObject(cursor,TEXT("aCurrentTarget")));
    auto* rune=Cast<AActor>(reflectedObject(cursor,TEXT("SpellGesture")));
    if(!target || !rune || rune->bHidden)return;
    const FVector eye=anchor(inputViewport->Actor);
    FVector center=target->Location;
    auto* offset=FindField<UStructProperty>(target->GetClass(),TEXT("CentreOffset"));
    if(offset && offset->ElementSize==sizeof(FVector))center+=*reinterpret_cast<FVector*>(reinterpret_cast<BYTE*>(target)+offset->Offset);
    const FVector toward=(eye-center).SafeNormal();
    // Presentation only: retain native target selection and spell eligibility.
    const FVector displayLocation=center+toward*Min((eye-center).Size()*.75f,Max(target->CollisionRadius,target->CollisionHeight)+6.f);
    location.set(rune,TEXT("Location"),displayLocation);
    rotation.set(rune,TEXT("Rotation"),(displayLocation-eye).Rotation());
    }
};
static void draw(FSceneNode* frame) {
    if ((!active() && !(lessonMode()&&ready)) || currentEye < 0 || !pairTracked) return;
    FVector grip, dir, up;
    if (pose(pairInput, grip, dir, up))
        WandMesh::draw(frame, grip, dir, up, wandLengthMetres * WorldUnitsPerMetre, WorldUnitsPerMetre);
    // V31's filled body-forward floor arrow; never derived from headset yaw.
    auto* pawn = frame->Viewport->Actor;
    auto* texture = pawn->Level ? pawn->Level->DefaultTexture : nullptr;
    if (!texture) return;
    const float yaw = bodyYaw * TwoPi / 65536.f;
    const FVector forward(std::cos(yaw), std::sin(yaw), 0), right(-std::sin(yaw), std::cos(yaw), 0);
    FVector base = pawn->Location; base.Z -= pawn->CollisionHeight - 2.f; base += forward * 1.5f;
    FVector points[3]{base + forward * 6.f - right * 4.f, base + forward * 6.f + right * 4.f, base + forward * 18.f};
    FTextureInfo info{}; texture->Lock(info, frame->Viewport->CurrentTime, 0, frame->Viewport->RenDev);
    if(HP2Settings::beam && wand.owner>=0 && wand.charging && pose(pairInput,grip,dir,up)) {
        const FVector start=grip+dir*(wandLengthMetres*WorldUnitsPerMetre);
        FVector end=start+dir*(12.f*WorldUnitsPerMetre);
        auto* cursor=reflectedObject(pawn,TEXT("SpellCursor"));
        auto* p=cursor?FindField<UStructProperty>(cursor->GetClass(),TEXT("vLOS_End")):nullptr;
        if(p && p->ElementSize==sizeof(FVector))end=*reinterpret_cast<FVector*>(reinterpret_cast<BYTE*>(cursor)+p->Offset);
        const FVector side=((end-start)^(anchor(pawn)-start)).SafeNormal()*(.003f*WorldUnitsPerMetre);
        FVector corners[4]{start-side,start+side,end+side,end-side};
        FTransTexture beam[4]{};FTransTexture* refs[4]{};
        for(int i=0;i<4;++i){beam[i].Point=corners[i].TransformPointBy(frame->Coords);beam[i].U=info.USize*.5f;beam[i].V=info.VSize*.5f;beam[i].Light=FPlane(.5f,.7f,1.f,1.f);refs[i]=&beam[i];}
        VRDraw::polygon(frame,info,refs,4,PF_TwoSided|PF_Translucent);
    }
    FTransTexture vertices[3]{}; FTransTexture* pointers[3]{};
    for (int i = 0; i < 3; ++i) {
        vertices[i].Point = points[i].TransformPointBy(frame->Coords);
        vertices[i].U = info.USize * .5f; vertices[i].V = info.VSize * .5f;
        vertices[i].Light = FPlane(.05f, .85f, 1.f, 1.f); pointers[i] = &vertices[i];
    }
    // A solid marker, not an additive translucent layer competing with floor
    // and spell particles. Keep native depth occlusion for walls/steps.
    if(HP2Settings::arrow)VRDraw::polygon(frame, info, pointers, 3, PF_TwoSided | PF_Occlude);
    texture->Unlock(info);
}
}
