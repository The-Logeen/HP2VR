#pragma once
namespace HP2ClimbPolicy {
inline bool recover(float walking,float elapsed,float idle) {
    return walking>=.04f || (elapsed>=.45f&&idle>=.12f) || elapsed>=1.2f;
}
inline bool nearAttempt(float startZ,float currentZ,float horizontal,float radius){return currentZ>=startZ-12.f&&horizontal<=radius*2.f+58.f;}
inline bool top(float z,float start,float current,float maxRise,float normal) {
    return normal>=.7f && z>start+5.f && z>=current-1.f && z<=start+maxRise;
}
}
