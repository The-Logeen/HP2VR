# Native controls and VR settings follow-up

## Implemented

- HP2 PlayerInput adapter reasserts tracked bAltFire and HPConsole.bSpaceReleased
  after physical input polling, before native PlayerTick decides whether to fire.
  A pending trigger edge can invoke the original AltFire once if polling canceled
  the initial press. No synthetic spell spawning or automatic target bypass.
- PlayerMove receives one normalized analog forward/strafe source, including zero
  at rest. Walking no longer receives duplicate JoyY. Native camera yaw and Harry's
  DesiredRotation agree with authoritative body yaw; native collision and scripted
  restrictions remain. Head looking does not steer that body heading.
- Visible thin wand-tip ray uses native cursor endpoint while charging. Native rune
  presentation moves toward the player, bounded so it cannot move past the player;
  its orientation follows the player-to-rune direction. Target eligibility remains
  HP2's. Cursor FOV query is also scoped to the controller ray when ABI validates.
- A during HUD cutscene mode invokes native HPConsole.StartFastForward once per
  press, the path used by base-game Enter. It does not rewrite cues/timers/rewards.
  This supersedes SAFE-SKIP-20261002's temporary disabled-A gate. It is HP2 native
  skipping, not HP1's incompatible spoken-line timer algorithm.
- Right grip + Y opens the dedicated stationary VR settings panel; Y alone retains
  the native game menu. Pointer + trigger adjusts rows, Y or Close dismisses.
- Persisted private System/HP2VRSettings.ini: eye offset, units/metre, smooth/snap,
  smooth speed, snap angle, always-in-body camera vs native cinematic camera.
  The body option changes viewpoint, not native script movement/progression or a
  full tracked avatar. The settings panel blocks player controls but does not pause
  native timers/world simulation. Native cinematic selection filtering remains future work.

## Verification

Build and regression tests pass. Bounded simulated native runs exit 0.
Native charge telemetry holds aiming=1/charged=1/release=0 for the held interval,
then aiming=0/charged=0/release=1 on release. No eligible locked target was present:
this proves hold/release state, not a spell hit. Viewed an eye capture with wand ray.
Strafe moves perpendicular to body heading; jump from rest has velocity X=Y=0.
Native Privet intro StartFastForward/FinishedFastForward logged and subsequent
scripted movement continued. This is not complete campaign progression validation.
Settings open/click/close simulation passed and saved speed 105; restored baseline
90 afterward. Captured settings panel viewed at 1280x960 with all rows visible.

## Headset test

Normal Start-HP2VR.ps1 (no diagnostic URL). Load an ordinary save with an available
spell target. Hold grip at right hip; raise wand, hold trigger while aiming at the
target and release trigger while retaining grip. Check native effect/rune/hit.
Test strafe after turning, then neutral-stick jump. Test A in a skippable cutscene.
Open right-grip+Y settings, check each adjustment, restore scale 40/offset 0 if needed.
Try always-in-body camera in a short familiar scene; native scripted actions remain.
