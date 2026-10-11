# HP2 tracked wand and strafing — October 2

V31 reference remains untouched. Copied WandGeometry, WandMesh, VRDraw,
WandInteraction and its tests into src/v31-reference; source texture lives in
assets/WandAtlas.pcx and is staged by Build-Gate2 and installed with ownership hashes.

## Changes

- Ordinary walking disables HP2 screen-relative movement only while its original
  PlayerMove executes. Removed walking JoyX (rotation axis); A/D now produce native
  strafing. HP2 collision, target locks and tutorial restrictions remain native.
- Body yaw drives pawn/ViewRotation, controller-space transformation and V31 floor
  arrow; headset yaw remains independent. Removed duplicate turn deadzone.
- V31 hip socket, proximity pickup, hold-grip ownership and smooth return. Either
  hand can pick up. Preheld trigger must release before charging; tracking/focus
  loss cancels ownership. No grip recenter gesture.
- Native SpellCursor UpdateCursor/LookForTarget use scoped controller camera ray,
  restricted to this player's cursor. Original HP2 script runs unchanged.
- Native baseWand GetWandEndPoint returns the tracked mesh tip for this player's
  held wand. Trigger dispatch remains HP2's own input/casting path.
- Refresh viewport from the client before input and after native Tick instead of
  dereferencing Draw's potentially closed cached viewport.
- Camera projection and eye-height fixes preserved. Default launcher remains
  Startup.unr/main menu. Explicit test URLs are not persisted in launch defaults.

## Verification

Build-Gate3 and regression tests pass, including copied V31 interaction tests.
Retail hooks validated: PlayerMove script 1000 bytes/4-byte parameters;
UpdateCursor 1124/4; LookForTarget 45/4; GetWandEndPoint 67/12.
Native atlas import confirmed 563 vertices/491 triangles.
BeanRewardRoom simulated walking produced lateral velocities approximately
(209.95,-4.51,0) and (-209.95,4.51,0), perpendicular to the body heading.
Hip pickup/release logged. Native cursor scope and effect-tip overrides executed.
Viewed native eye BMP: textured held wand visibly renders in the world.
Final 14-second simulated run: 285 coherent stereo pairs, exit 0, clean teardown.
Earlier timed close during travel raised UEngine::InputEvent; viewport refresh
guard added and the same bounded test then exited cleanly. This is not exhaustive
proof of every shutdown/travel sequence.

## Remaining headset verification

Floor arrow visibility when looking down, controller model alignment, hip reach,
casting against valid targets and moving effects during charge need user testing.
Scripted cinematic/lesson/broom states are not established by ordinary-walking
tests. No claim of universal forward synchronization across those states yet.
The simulation-only controls and BMP capture require explicit environment flags;
normal launch does not enable them.

Launch tools/Start-HP2VR.ps1 with no diagnostic arguments. Load a normal save;
look down at right hip, reach close and hold grip, raise wand, hold/release trigger,
then release grip to return it. Test left-stick sideways and right-stick turning,
then look sideways without turning the body. Use SteamVR recenter only.
