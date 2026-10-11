# HP2 native gameplay ownership map

This map is derived from the selected HP2 `hgame.u` package (package version
79; 10,249 names, 1,937 imports, and 16,746 exports). It is HP2 evidence, not a
renaming of HP1 classes. The package was read as data with
`HP2PackageInspector.exe`; no retail code was executed to obtain these facts.

## Authority boundary

The VR layer must leave `HGame.harry` and its native subclasses authoritative
for movement, spell validity, targets, damage, lessons, broom rules, story
states, scoring, and saves. VR should supply view poses and translated input,
then observe the native result. This preserves the key HP1 V31 design while
replacing every HP1-specific class and field assumption with HP2 evidence.

## Walking player and camera

- `HGame.harry` extends `PlayerPawn`. It owns `PlayerCalcView`, `PlayerInput`,
  `PlayerTick`, `Fire`, `AltFire`, `Cast`, `StartAiming`, `StopAiming`,
  `AdjustAim`, `TurnOnCastingVars`, `TurnOffCastingVars`, `DoJump`,
  `ProcessAccel`, and `MovementMode`.
- Relevant states include `PlayerWalking`, `Mounting`, `MountFinish`,
  `SpellLearning`, `stateCutIdle`, and `wingspell`.
- Runtime-reflection candidates include `Cam`, `SpellCursor`, `Weapon`,
  `focusActor`, `AimRotOffset`, `bIsAiming`, `bIsAimingWithCharge`,
  `bLockedOnTarget`, `bMoveWhileCasting`, `bAutoCenterCamera`, and the four
  directional lockout flags.
- `BaseCam` extends `HPawn`; `BaseCamTarget` extends `HiddenHPawn`. `BaseCam`
  owns the native camera policy through `UpdatePosition`, `UpdateRotation`,
  `UpdateRotationUsingVectors`, `SetPosition`, `SetFinalRotation`,
  `SetCameraMode`, `TransitionToCameraMode`, collision checks, and target/FOV/
  offset setters. Important fields include `CamTarget`, `CurrentSet`,
  `CameraMode`, current/destination positions and rotations, and mouse deltas.

Port rule: a VR view offset belongs at the renderer/frame boundary, while the
native `BaseCam` remains the source for cutscene, target, and special-camera
intent. Never write a copied HP1 camera offset into HP2.

## Wand, aiming, and spells

- `baseWand` extends `HWeapon`. It owns current/last spell, charge, cast lists,
  wand endpoint, glow/charge behavior, and sword mode.
- `baseSpell` extends `Projectile` and owns `SetSpellCharge`,
  `SetSpellDirection`, `InitSpell`, `ProcessTouch`, `HitWall`,
  `CreateHitEffects`, and seeking updates.
- `SpellCursor` extends `ParticleFX` and owns target search/lock policy through
  `UpdateCursor`, `LookForTarget`, `LockOn`, `UnLock`, `SetLOSDistance`, and
  `CanCameraSeeYouInFOV`. Its observed fields include `PlayerHarry`, current and
  possible targets, gesture offset, LOS endpoint, and LOS distance.
- `harry.Cast` is the native gameplay authority. A physical VR wand should
  influence aim/gesture inputs and invoke the same native cast path, not spawn
  projectiles or award hits itself.

## Spell lessons

- `SpellLessonTrigger` owns the lesson loop. Its functions include
  `PlayerInput`, `WandAtInterpolationPoint`, `DoArrowKeyPressed`,
  `KeyMatchesIPArrow`, `CheckForMissedArrows`, `SetupCamera`, `Activate`, and
  `EndLesson`; states include `GameIntro`, `PlayGame`, and `LevelOver`.
- `harry` exposes lesson axes `aSpellLessonX` and `aSpellLessonY`.
- `harry.StartSpellLearning` stores `CurrSpellLesson` and enters native
  `SpellLearning`; that state's `PlayerInput` forwards to the current trigger.
- The trigger's active input is four edge-gated cardinal events. The moving
  `SpellLessonWand` follows an authored spline and each
  `SpellLessonInterpolationPoint` supplies a different direction per round.
  Native proximity/timing decides hits and misses. Three perfect rounds award
  5, 10, and 15 House Points; their maximum loop counts are 5, 4, and 3.

Port rule: direct controller mode may use the existing lesson bindings. The
optional physical mode may translate motion on a locked plane into those same
cardinal edges. Let HP2 decide interpolation hits, misses, retries, dialog,
House Points, spell grant, and event progression. Never write `nHitPointHits`,
`nLevel`, or copied HP1 gesture data.

## Broom and flight

- `BroomHarry` extends `harry` and overrides `Possess`, `PlayerInput`, and
  player states. It owns `PlayerTrack`, `FlyOnPath`, `GetOnPath`, `DoKick`,
  `DoCapture`, `CatchTarget`, `UpdateWatchTarget`, collision/damage behavior,
  and broom animation/audio.
- Observed control fields include `fPitchControl`, `fYawControl`, mouse pitch/
  yaw, control-device selectors, watch/kick targets, and boost/brake state.
- `BroomHarry.PlayerTick` calls `PlayerTrack` whenever `LookForTarget` is set
  and `PlayerMove` otherwise. During Quidditch the director sets the Snitch as
  the target, so input changes bounded lateral/vertical target offsets rather
  than freely owning the broom's world heading.
- `BroomHarry.PlayerInput` owns the action edge. It kicks a nearby native
  target first and otherwise calls `QuidditchDirector.OnActionKeyPressed`.
- `QuidditchDirector` owns pursuit progress, proximity bands, hoop stages,
  collision/damage penalties, seeker behavior, score simulation, catch
  eligibility, the final-match trench gate, win/loss, rewards, and event
  progression. `QuidditchLessonDirector` layers the timed Snitch lesson and
  mock match over that same gameplay state.

Port rule: broom controls are a separate native authority channel. Walking
locomotion injection must switch off while `BroomHarry` owns the pawn, as HP1
V31 eventually required, but using these HP2 classes and reflected fields.
All VR broom styles must resolve to one HP2 axis owner. Physical kick/catch may
emit only the normal action edge after native eligibility; never call `DoKick`,
`CatchTarget`, alter progress, or skip the trench/event sequence.

The detailed behavior and two-mode plan are in
`HP2-LESSONS-QUIDDITCH-DESIGN.md`.

## HUD and presentation

- `baseHUD` extends `HUD`; `HPHud` extends `baseHUD`.
- `HPHud` owns `PostRender`, `DrawSpellIcon`, `DrawHoops`,
  `DrawCutStyleText`, cutscene/subtitle handling, and registration of spell
  lesson, spell selector, pickup, countdown, challenge, enemy-health,
  Quidditch, and vendor managers.

Port rule: menus, cutscenes, and mouse-oriented interfaces should be presented
on the stationary VR panel. World stereo and physical input must not bypass
native HUD/cutscene state.

## ABI evidence and remaining proof

With HP2 SDK headers, x86 packing 4, and HP2's compiler compatibility flags,
the compile-time layouts are `UGameEngine=224`, `URender=280`,
`UViewport=240`, and `APlayerPawn=2344`. The read-only live probe measured the
retail layouts as `UGameEngine=276`, `URender=280`, `UViewport=260`, and
`APlayerPawn=2400`. Gate 3 uses only the evidenced 52-byte `UGameEngine` tail;
it does not subclass the mismatched viewport or player. Reflected field offsets
remain diagnostic and are never copied from HP1.
