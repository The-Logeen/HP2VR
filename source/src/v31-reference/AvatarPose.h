#pragma once
#include <cmath>
namespace AvatarPose {
// The prepared rig's eye anchor is 1.50 m above its soles and its nominal
// collar is 1.30 m. Scale the complete avatar from the comfort eye height so
// feet, torso, shoulders and arms retain one coherent human proportion.
constexpr float rigEyeHeight(){return 1.50f;}
constexpr float rigCollarHeight(){return 1.30f;}
inline float sanitizedEyeHeight(float eye){if(!std::isfinite(eye))eye=1.38f;return std::fmax(.8f,std::fmin(2.f,eye));}
inline float heightScale(float eye){return sanitizedEyeHeight(eye)/rigEyeHeight();}
inline float eyeAboveCollar(float eye){const float safe=sanitizedEyeHeight(eye);return safe-rigCollarHeight()*heightScale(safe);}
// Local +X is behind this mesh after its 180-degree forward correction. The
// eyes belong in front of the neck, so move the body backward from the tracked
// head; the chest then enters a natural downward gaze before the collar.
inline float eyesForwardOfNeck(){return .115f;}
// A 180-degree visual rotation puts the rig's named Left arm on player right.
// Keep action/controller indices physical: 0 left, 1 right.
inline int controllerForArm(int arm){return 1-arm;}
struct NeckFollow {
 float x=0,y=0;bool ready=false;
 void reset(){ready=false;}
 void update(float targetX,float targetY,float dt){
  if(!ready){x=targetX;y=targetY;ready=true;return;}
  float dx=targetX-x,dy=targetY-y,d=std::sqrt(dx*dx+dy*dy);
  if(d>.50f){x=targetX;y=targetY;return;} // teleport or new tracking origin
  if(dt<=0)return; // second eye uses the same body pose
  // 1 cm quiet zone, gentle follow, maximum 4 cm neck displacement.
  const float follow=d>.01f?(1.f-std::exp(-dt/.10f))*(d-.01f)/d:0.f;
  x+=dx*follow;y+=dy*follow;dx=targetX-x;dy=targetY-y;d=std::sqrt(dx*dx+dy*dy);
  if(d>.04f){x=targetX-dx*.04f/d;y=targetY-dy*.04f/d;}
 }
};
inline float armStretch(float distance,float length){
 if(!std::isfinite(distance)||!std::isfinite(length)||length<=.001f)return 1.f;
 return std::fmax(1.f,std::fmin(1.30f,(distance+.01f)/length));
}
inline float elbowSide(int hand){return hand?.28f:-.28f;}
inline float elbowDown(){return .25f;}
inline float elbowBack(){return .08f;}
}
