# HP2 VR spell-lesson and Quidditch design

## Decision

HP2 should keep its own lesson and Quidditch state machines.  The VR mod should
offer two input styles around those state machines:

1. **HP2 Native VR** translates controller input into HP2's original
   directional lesson and target-tracking broom inputs.
2. **HP1-style VR** adds physical rune tracing, two-hand/one-hand broom
   steering, and an eligible-only physical Snitch reach, but converts each of
   those actions back into HP2's native inputs.  It does not replace HP2's
   timing, scoring, pursuit, rival, trench, catch, reward, or story code.

This is a compatibility layer, not an HP1 minigame transplanted into HP2.

## Why HP1's implementation cannot simply be copied

| System | HP1 V31 | HP2 retail code | Consequence for HP2 VR |
| --- | --- | --- | --- |
| Spell lesson | Continuous 2D mouse trace graded against `SpellGesture.Points` | A scripted wand follows a spline while the player enters timed Up/Down/Left/Right events at `SpellLessonInterpolationPoint` actors | Physical motion must become timed cardinal events; HP1's gesture arrays and judge do not exist here |
| Quidditch steering | V31 resolved VR commands into one manual flight-heading owner | During matches `BroomHarry.PlayerTrack` automatically pursues the Snitch and input changes horizontal/vertical offsets around it | VR steering must command target-space offsets, not overwrite a world heading |
| Match progress | HP1 referee-specific progress/catch transition | `QuidditchDirector` derives progress from pursuit time, proximity bands, rival position, collisions, damage, match score, and final-match trench state | Do not fabricate progress or catch readiness |
| Catch | V31 released an HP1 referee's verified native catch transition | HP2's active `GamePlay.OnActionKeyPressed` directly calls `BroomHarry.CatchTarget` only after its own reach/progress/trench checks | A physical reach may emit the normal action edge only while HP2 says it is eligible |
| Action button | Context-specific HP1 action | HP2 first tries to kick a nearby `QuidditchPlayer`; otherwise it calls the director, which may catch or miss | Never call `DoKick` or `CatchTarget` from generic VR input |

The reusable V31 principles are still valuable: lock interaction planes in
world space, keep head look separate from vehicle control, allow one-hand
continuation, reject stale tracking, release held input on mode changes, retain
native scoring/progression, and validate every reflected owner after travel.

## HP2 spell lessons: native implementation

The four lesson maps are `Ch1Rictusempra.unr`, `Ch2Skurge.unr`,
`Ch3Diffindo.unr`, and `Ch4Spongify.unr`.  All use
`HGame.SpellLessonTrigger`:

1. `Activate` finds the shape, camera, spline endpoints, professor, and Harry;
   calls `harry.StartSpellLearning`; selects the cutscene camera; registers
   with the HUD; and freezes characters/props.
2. `GameIntro` shows the shape, moves the camera, spawns
   `SpellLessonWand`, explains the rules, resets the hit points, and enters the
   first round.
3. In `PlayGame`, `SpellLessonWand` follows the authored spline.  Each
   `SpellLessonInterpolationPoint` has a direction for each of the three
   rounds.  The trigger accepts four edge-gated lesson inputs or a dominant
   joystick axis.
4. `DoArrowKeyPressed` searches the nearest still-active point immediately
   before/after the moving wand.  A direction is a hit only when it matches
   that point and lands inside the native distance window.  Wrong or badly
   timed input marks the point missed.  Passed active points are also marked
   missed.
5. A round can loop 5, 4, and 3 times respectively.  Perfect completion of a
   round awards 5, 10, then 15 House Points.  Running out of attempts ends the
   lesson without those later awards, but `EndLesson` still grants the spell
   and resumes the authored event chain.

The hit window expands by the wand's movement since the previous frame.  That
detail matters at uneven frame rates and is another reason not to reproduce
the score in the VR layer.

### Lesson control modes

**HP2 Native VR**

- While Harry's native state is `SpellLearning` and the current lesson is in
  `PlayGame`, map a controller axis/directional control to the existing Up,
  Down, Left, and Right bindings.
- Use press/release edges, matching `nKeyNeedsReset`; do not leave a direction
  held across an arrow.
- Suspend and release every direction in dialogue, `GameIntro`,
  `DeclareNewRound`, `LevelOver`, pause, lost focus, stale tracking, travel, or
  lesson exit.

**HP1-style Physical Trace**

- On entry to the playable round, lock a comfortable two-dimensional plane in
  world space, as V31 did.  Do not attach it continuously to head motion.
- Project the selected wand/controller tip onto that plane while the drawing
  trigger is held.  Accumulate movement in metres and recognize one dominant
  cardinal segment at a time.
- Convert each recognized segment into the same native direction edge used by
  the direct mode.  HP2's moving wand position still decides hit or miss.
- Render the original HP2 shape/arrows and a reachable VR trace overlay.  Do
  not substitute an HP1 rune or infer a different route from the spell name.
- Break the visible trail and reset the segment anchor on pause, tracking loss,
  round reset, or release.  Re-entry begins with an anchor-only sample so it
  cannot create a false direction.

This mode should feel closer to HP1, but it cannot be exactly the same game:
HP2 grades *when and which direction* the player moves, not geometric distance
to a complete rune.  Keeping that distinction visible is preferable to a
hidden auto-success system.

## HP2 Quidditch: native implementation

### Control and pursuit

`BroomHarry` has two materially different flight paths inside the same
`PlayerWalking` state:

- With no `LookForTarget`, `PlayerMove` applies yaw/pitch control, forward
  acceleration, collision avoidance, boost/brake, animation, and sound.
- With a `LookForTarget`, `PlayerTrack` is used instead.  Directional, mouse,
  or joystick input adjusts bounded horizontal and vertical offsets around the
  target.  Harry automatically turns toward that target, accelerates, and
  chooses speed from his tracking distance.

`QuidditchDirector.GamePlay.BeginState` sets Harry's target to the Snitch, so
normal tutorial/match play is the second mode.  Treating it as HP1-style free
flight would fight the native pursuit code and make progress/collision tuning
unreliable.

### Lesson and match phases

- `QuidditchLessonDirector` begins in a cutscene state.  Authored cutscene
  commands start a 90-second Snitch lesson and then a 120-second mock game.
- The Snitch lesson uses the normal `GamePlay` pursuit/progress logic but no
  opponent seeker.  The mock game enables the seeker, kicking, damage, and
  match win/loss events.
- Full matches support three authored mechanics: hoops, proximity, or
  proximity with hoops.  The default is the combined mechanic.
- In proximity play, pursuit time reduces `fSnitchTrackDist`, while Harry's
  actual distance from a moving proximity point adds gain/loss.  The acceptable
  region tightens as progress rises.  Bumps, hits, kicks, and Bludgers apply
  native penalties.
- The rival seeker pursues the same Snitch, can kick Harry, can block catch
  readiness, and can be kicked through the same action button.
- The scoreboard simulates goals during the chase.  Catching the Snitch adds
  150 points and the match result/House Point path remains director-owned.
- In the final Slytherin match, a timer triggers the authored entrance to the
  trench.  The cutscene's `Director ContinueGame` command switches the Snitch
  to `IPSnitchTrench`, sets `bInTrench`, restores pursuit, and only then permits
  the final catch.

### The real action/catch contract

`BroomHarry.PlayerInput` turns `bBroomAction` into a rising edge.  During
manual flight it first finds a nearby kick target.  If one is close enough, it
calls native `DoKick`; otherwise it calls `Director.OnActionKeyPressed`.

The active director catch condition is:

```text
bCanReachForSnitch
and fProgressPercent > 98
and (not final match or bInTrench)
```

When true, the director disables reaching/kicking, calls Harry's native
`CatchTarget` (with the authored post-catch path outside the trench), redirects
the camera, releases the seeker and Bludgers, and enters the win sequence.
`BroomHarry.Catching` creates the native catch proxy, attaches it to the right
hand, plays the animation/sound, and resumes the authored path.

Although `QuidditchDirector` contains a `GameCatch` state, the decompiled HP2
scripts contain no transition to it.  The live design must therefore target
the action path above, not assume HP1's catch-state transition applies.

### Quidditch control modes

**HP2 Native VR**

- Left stick commands target-space right/left and up/down using HP2's existing
  broom axes while `BroomHarry` is in native manual `PlayerWalking`.
- Right trigger produces the existing broom-action edge.  HP2 decides whether
  that means kick, miss, or catch.
- Tracked head yaw/pitch is view-only.  It must not alter the Snitch tracking
  offsets or the authored `QuidditchCameraTarget`.
- Scripted `FlyingOnPath`, `Catching`, `Hit`, `BroomDying`, cutscene, and
  pending-event states receive no VR flight command.

**HP1-style Broom VR**

- Reuse the V31 *control feel*, not its HP1 hook: stick, point, and physical
  shaft modes all resolve to one normalized HP2 right/up command.
- In physical shaft mode, both grips establish the shaft and neutral.  One
  remaining gripped hand keeps useful steering while the other reaches or
  performs an action.  Releasing both hands clears command and requires a fresh
  neutral capture.
- During target tracking, shaft/controller angles are rate controls for HP2's
  lateral/vertical offsets.  During a genuinely target-free broom segment the
  same native axes may control yaw/pitch.  There is never a second world-heading
  owner.
- A physical kick gesture, if added, may emit only the normal action edge.  The
  native proximity/target selection still decides whether a kick happens.
- A physical catch opportunity appears only while the reflected director
  satisfies the exact catch condition.  A free tracked hand must enter a
  reachable cue with continuous samples; then the adapter emits the same action
  edge as the button fallback.  Loss of eligibility immediately withdraws it.

HP2 declares `bBroomBoost` and `bBroomBrake`, but the retail keyboard file does
not bind them.  Its combined hoop mechanic also supplies automatic
`bAuxBoost`.  HP1 V31's trigger boost/brake mapping should not be copied until
an HP2 controller/native-input run proves the intended retail route.

## Implementation boundary

The first HP2-owned pure code is in `src/HP2LessonQuidditch.h`.  It contains:

- a plane-motion-to-cardinal recognizer for the optional trace mode;
- normalized target-space broom command shaping;
- the exact director catch-eligibility predicate; and
- a fail-neutral action-edge latch.

Its tests do not claim live gameplay.  They establish semantics before hooks
or reflection are introduced.

The live adapter should be added in these proof gates:

1. **Observe only:** log player class/state, current `CurrSpellLesson` state,
   `BroomHarry.LookForTarget`, director class/state, reach flag, progress,
   final-match flag, and trench flag.  Resolve fields/functions by name and
   validate type/size; do not import HP1 offsets.
2. **Native lesson input:** route controller directions only during the exact
   HP2 lesson play state.  Verify correct, wrong, early, late, retry, all three
   rounds, House Points, spell grant, pause, and exit in each of the four maps.
3. **Physical lesson input:** add the locked plane, trace rendering, dominant
   segment recognition, hand selection, haptics, and direct-mode fallback.
4. **Native broom input:** route both axes and the action edge only during
   eligible BroomHarry control.  Complete the 90-second lesson, mock match, all
   three play mechanics, rival combat, damage, win/loss, and post-match exit.
5. **Physical broom and catch:** add point/shaft modes and eligible-only hand
   reach.  Verify one-hand steering, action fallback, seeker blocking, progress
   loss while reaching, final trench entry/exit, native catch proxy, score,
   rewards, pause, death, reload, and tracking loss.

## Evidence and open questions

- Local `hgame.u` package inspection confirms all named owners/functions and
  the selected retail input fields.
- The readable script reference is the public
  [HP2UScriptDecompile](https://github.com/metallicafan212/HP2UScriptDecompile/tree/Scaling-Patch/HGame/Classes),
  especially `SpellLessonTrigger`, `BroomHarry`, `QuidditchDirector`, and
  `QuidditchLessonDirector`.
- OldUnreal identifies HP2 as a Unicode UE1 433-lineage game and records the
  editor/UCC compatibility limitations; this supports the current runtime
  package/reflection approach rather than treating stock UT tooling as binary
  compatible.
- The exact lesson-board comfort placement, controller signs, broom response,
  and reach cue remain headset questions.
- The source decompile is strong behavioral evidence but is not a substitute
  for observing the selected retail package at runtime.  Every live field and
  state transition still needs validation against this project's hashed build.

