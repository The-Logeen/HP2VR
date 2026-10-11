# HP2VR player-feel contract

This is the owner-approved HP1VR V31 behavior translated into HP2 ownership
terms. It is an implementation contract, not a claim that a scene has passed
headset testing.

## Three independent decisions

Every tick decides these separately:

1. **Presentation**: Harry's body camera, a stationary UI panel, or one audited
   external cinematic shot.
2. **Translation authority**: player, HP2 native script, one bounded assisted
   nudge, or frozen for UI/cinematic safety.
3. **Rotation authority**: player, one active assisted nudge, or an audited
   cinematic.

A generic camera change, `BaseCam` state, scripted destination, or timer is not
enough by itself to take control.

## Cinematics

- Cinematics Off never grants cinematic translation or rotation authority.
- Cinematics On grants it only when HP2 is using an external authored camera
  **and** the current interval is on the audited allowlist.
- During that selected interval, native HP2 Harry remains visible and performs
  the original movement/animation. Translation and rotation are frozen.
- Every other scene stays in the selected movement mode. Returning from a shot
  restores presentation and input authority atomically.

## Movement and rotation

- Free Move owns translation continuously outside stationary UI and selected
  cinematics. Normal scripted walks, facing changes, and repeated corrections
  do not retake it. Level travel, required teleports, deaths, and hard plot
  transitions remain native.
- Rotation is manual in every movement mode outside a selected cinematic and a
  currently active nudge.
- Cinematic Body may retain HP2's native body translation, but does not imply
  native rotation or external-camera presentation.
- Assisted mode is player-owned by default. It waits 0.65 seconds of quiet,
  makes at most one 0.65-second bounded attempt per identified staging gate,
  and yields immediately to move or turn input. It does not repeatedly pull
  for the same gate. A continuous correction is permitted only when an
  observer separately proves the plot is hard-blocked, and manual input still
  interrupts it.

## Body, head, and avatar

- Body-forward yaw drives locomotion, the avatar, and any floor direction aid.
- Head tracking changes view only. Looking sideways must not redirect walking.
- The prepared HP1VR avatar was already based on HP2 Harry assets, so its
  procedural rig can be reused after HP2 bone/material attachment points are
  verified. No HP1 address, offset, hook, or package name is portable.

## Menus and story panels

- Native HP2 UWindow/FEBook remains the source of truth.
- UI is shown on a crisp, world-stationary SteamVR panel, placed 1.8 m ahead on
  entry and not head-locked.
- A 4:3 source is 1.6 x 1.2 m. Height stays 1.2 m and width follows the actual
  source aspect.
- One panel transform supplies the visible overlay, ray intersection, pointer
  endpoint, and native Windows mouse UV. Trigger is the native left click.
- While the panel owns input, locomotion, spells, jump, and dialogue actions
  are neutral. Head look and physical turning remain available.
- Overlay, pointer, and eye resources are persistent across menu transitions;
  routine open/close must not create/destroy them.

## Visual stability

World and UI use persistent output eye resources so a menu transition does not
change the submitted texture identity/format. UI receives no world sharpening.
The prior HP1 window/glass shimmer fix must be located and evidenced before its
renderer state is ported; it is not assumed from the regressed V31 output.

## HP2-specific ownership

HP2 spell lessons and Quidditch keep their own native state machines, scoring,
failure, retry, and progression. VR adapts physical input to their verified
input owners. Optional HP1-style variants may alter input presentation only;
they never replace completion logic.
