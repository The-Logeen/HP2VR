# Existing-avatar front robe adjustment

No replacement avatar, texture, hand rig or animation asset. The read-only source mesh exposes separate `bone_RR*` and `bone_RL*` robe weights. Export-RobeWeights.py derives a feathered front mask (78/1530 vertices); trousers, hands and rear robe groups are excluded. The source blend and HP1 files are never saved or modified.

Follow-up: visual walk-cycle cadence reduced by 15% (phase multiplier 0.85). Actual pawn speed, movement input, robe activation and crouch calibration are unchanged. Headset judgment is still needed for cadence and foot sliding.

HP2RobeMotion adds a bounded backward rotation around the upper skirt (maximum 0.45 radians), plus at most 0.055 avatar-space lateral spread. The lower front panels receive most of the adjustment, feathered at their attachment and side seam. It is applied after the prepared walking deltas and before the existing crouch/world transform.

Forward pawn speed drives a smoothed blend; stationary, reverse and pure sideways motion do not request flare. Existing walking eligibility excludes airborne/climbing motion. Crouching fades the added effect to zero by 0.10 avatar-space drop, preserving the existing crouch solution. Both eyes share one updated blend per pair.

This is a procedural visual approximation, not imported native HP2 robe animation or cloth simulation. It may need headset tuning for trousers/robe clearance. No claim of exact base-game animation matching.

Build includes HP2RobeMotionTests: exclusions at rest, deep crouch, torso height and zero mask; symmetrical flare in the expected backward direction; finite output; mask bounds/count. All existing build tests also pass.

Isolated simulated avatar/crouch smoke: clean exit, 446 coherent stereo pairs and cross-GPU uploads, zero deferred readbacks. This validates runtime integration, not the visual quality of forward-walking flare in the headset. Installed engine hash matches build; settings hash unchanged.

Headset test: launch normally, look down while walking forward, stop, walk backward/sideways, then crouch and stand. Confirm front hems expose more trousers without tearing, excessive spreading or snapping; confirm hands and standing calibration are unchanged.
