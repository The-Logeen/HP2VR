# HP1 V31 to HP2 porting notes

## Confirmed reusable architecture

- Separate x86 game/engine side and x64 SteamVR side.
- A versioned shared-memory contract with unique HP2 names.
- Timestamped snapshots that become neutral when stale or invalid.
- One coherent pose per submitted stereo pair.
- Flat menu/story presentation on a readable panel, separate from world stereo.
- Body-forward locomotion independent from head look.
- Transactional multi-binary builds and exact supported-build hashes.
- Native gameplay remains authoritative for spells, lessons, scores, saves, and
  scene progression.

## Corrected V31 behavior to preserve for HP2VR

The current HP1 V31 output is a reference, not an unquestioned behavioral or
visual baseline.  HP2VR must preserve these owner-confirmed corrections, and a
later HP1 V31 repair should use the same rules:

- **Window/material stability:** V31 previously contained a successful fix for
  shimmering/flickering window surfaces, but that regression is visible again.
  Before inheriting renderer state, texture filtering, mip/LOD, depth, or
  temporal behavior, identify the last non-regressed implementation and port
  the evidenced fix.  HP2VR acceptance includes close, distant, moving-head,
  and stereo checks of window/glass surfaces; a stable world with shimmering
  windows is not a visual pass.
- **Selected cinematics only:** `Cinematics On` does not enable every native or
  scripted camera change.  Only an explicit, audited allowlist of cinematic
  intervals may enter cinematic presentation.  During one of those selected
  intervals, locomotion and rotation are frozen and HP2's native Harry remains
  visible, performing the authored base-game movement.  All non-selected
  scenes remain in the active movement policy even when cinematics are on.
  With `Cinematics Off`, no interval may take cinematic movement or rotation
  authority.
- **Free Move:** player translation is continuously authoritative in every
  non-selected-cinematic scene.  Scripted ordinary walks, turns, facing, and
  repeated position corrections must not retake control.  Required teleports,
  level travel, and hard plot transitions remain native.  `Free Move` combined
  with `Cinematics Off` therefore means continuously free translation and
  rotation.
- **Free rotation:** manual rotation is authoritative in every movement mode,
  including scene/body modes.  The only exceptions are an allowlisted
  cinematic while `Cinematics On`, and a currently active assisted nudge.  In
  nudge mode, deliberate manual turning immediately retakes rotation authority;
  assistance may try again only after the configured quiet delay and only if a
  still-pending plot/staging condition justifies it.
- **Nudge/Assisted mode:** manual locomotion remains authoritative by default.
  Assistance may begin only after a quiet delay, issues a bounded nudge toward
  a still-pending reachable staging area/facing, then yields again.  Player
  movement or rotation immediately retakes the corresponding authority.  It
  must not pull continuously or repeatedly.  A stronger forced correction is
  allowed only after evidence that the plot cannot advance without reaching
  the required authored position; it must stop immediately once that gate
  clears.
- **Cinematic visibility:** native Harry is visible and follows native authored
  animation/movement while an external cinematic camera owns presentation.
  On return to gameplay, first-person/avatar visibility and the selected
  movement authority are restored atomically.

Implementation must distinguish camera presentation, translation authority,
and rotation authority.  `BaseCam`, `CamTarget`, view target, player state,
pending scripted destination, and progress events should feed an explicit
policy state machine; neither a generic cutscene flag nor a timer alone is
sufficient to justify freezing or forcing the player.  Cinematic authority
requires both `Cinematics On` and membership in the audited interval allowlist.

## Not portable without HP2 evidence

- HP1 package/class/function names, script bytecode, native addresses, offsets,
  hook lengths, and bridge version numbers.
- HP1 renderer proxy assumptions, camera/FOV hooks, lesson gestures, wand cast
  paths, broom controls, menu dimensions, HUD calls, or texture crash guards.
- HP1 runtime/save/config namespaces and any V30/V31 binary.

## HP2 facts observed in the selected installation

- `Game.exe` is x86 and imports the retail `Core.dll`, `Engine.dll`, and
  `Window.dll` path.
- `Version.txt` reports `HPCos_021009_1205-1` / `021009_1205`.
- The main script package is `hgame.u`; HP1's `HPBase.u`, `HarryPotter.u`, and
  `HPMenu.u` package split does not exist here.
- Startup uses `startup.unr`; `PrivetDr.unr` is available as a bounded first
  flat-run/first-room candidate.
- The selected installation is configured for
  `D3D11Drv.D3D11RenderDevice`, HP2 build 1.6.2. The first private live world
  run instead bound `SoftDrv.dll` after the native first-run detector rewrote
  `GameRenderDevice`. The detector also promoted `FirstRun` from `0` to engine
  version `433`, so Gate 1 now pins that value before reasserting D3D11; this
  is an explicit live verification item.
- The installation includes 3,126 external DDS replacements and is therefore a
  modified graphics baseline, even though its retail engine/game binaries
  currently match the recorded identity set.
- Static package-table inspection now identifies the HP2 gameplay owners in
  `HP2-OWNERSHIP-MAP.md`. In particular, `HGame.harry`, `BaseCam`, `baseWand`,
  `SpellCursor`, `SpellLessonTrigger`, `BroomHarry`, and `HPHud` replace the
  assumptions that an HP1 port might otherwise copy.
- Compile-only x86 SDK layout sentinels establish `UGameEngine=224`,
  `URender=280`, `UViewport=240`, and `APlayerPawn=2344`. Retail runtime sizes
  measured by the read-only live probe are `UGameEngine=276`, `URender=280`,
  `UViewport=260`, and `APlayerPawn=2400`. The HP2 engine subclass therefore
  carries an evidenced 52-byte retail tail; viewport/player subclasses remain
  forbidden because their SDK layouts differ from retail.
- Live reflection measured `harry=5304`, `BaseCam=2408`, `BroomHarry=5680`,
  `SpellCursor=1184`, `baseWand=1232`, `SpellLessonTrigger=1192`, and
  `HPHud=960`. Important `harry` offsets include `Cam=4448`,
  `SpellCursor=2664`, `AimRotOffset=2668`, `focusActor=4392`, `Weapon=880`,
  broom axes `2612..2624`, and lesson axes `2628/2632`. These values are
  diagnostic evidence, not permission to copy HP1 behavior or write raw state.
- `HP2VREngine.dll` now loads through the retail package manager with
  `UGameEngine` and `URender` subclasses. The required native package export was
  `GPackage`; omitting `IMPLEMENT_PACKAGE(HP2VREngine)` caused the earlier
  pre-constructor access violation.
- An end-to-end synthetic run (no SteamVR startup) rendered and captured 303
  coherent stereo pairs at 512x384, published the exact render pose, and closed
  both processes with exit code zero. This proves the native stereo and GPU
  transport path, not headset presentation.
- HP2's spell lessons and Quidditch have now been traced through their native
  script owners. `docs/HP2-LESSONS-QUIDDITCH-DESIGN.md` records the behavioral
  differences from HP1 V31 and the two-mode port contract. A new pure
  `HP2LessonQuidditch` input layer recognizes physical-trace cardinal segments,
  shapes target-space broom commands, preserves the exact native catch gate,
  and fail-neutrals action edges. Its unit tests pass; it is intentionally not
  connected to live retail state before the observe-only reflection gate.

## Gate status and next work

1. Gates 0-3—baseline, private boot, D3D11/live ABI, native subclasses, and
   synthetic same-pose stereo—are complete for this exact build.
2. Run the x64 companion against a real SteamVR compositor and headset; verify
   eye order, scale, projection, latency, color space, focus loss, and recenter.
3. Verify the installed Touch/Quest, Index/Knuckles, or Vive action manifest.
   The bridge and HP2-native W/S/A/D, MouseX, LeftMouse, RightMouse, and Enter
   adapter are built and neutral-path proven; physical controls are not yet
   headset-observed.
4. Separate flat menus/story presentation from world stereo, then tune the
   native movement/turn adapter and switch authority explicitly for
   `BroomHarry`.
5. Add the observe-only spell-lesson/Quidditch state probe described in
   `HP2-LESSONS-QUIDDITCH-DESIGN.md`, then connect the tested pure adapter only
   through the verified HP2 input owners.

## Online cross-checks

- The Harry Potter Unreal Wiki documents HP game command-line switches,
  including `-nofrontend`, and identifies HP2's `HGame/Game.ini` configuration
  and available HP2 editor/decompile resources:
  <https://github.com/metallicafan212/HarryPotterUnrealWiki/wiki>
- The HP2 script decompile confirms `harry`, `BaseCam`, `SpellCursor`, broom
  axes, spell-lesson axes, and aiming fields used to define the native ownership
  boundary: <https://github.com/metallicafan212/HP2UScriptDecompile>
- OldUnreal's Unreal Tournament/UE1 development forum records the same native
  compatibility constraints encountered here: exact headers/libs, Unicode
  engine differences, `/Zc:wchar_t-`, and legacy `for` scoping. It also contains
  HP2-specific discussion of the 433 lineage and unstable stock mouse smoothing:
  <https://www.oldunreal.com/phpBB3/viewforum.php?f=19>

## Open compatibility questions

- The local `sdk_hp2` direct-host experiment compiled but exited with
  `0xC0000005` before its first log line. Its ABI/runtime initialization path is
  not compatible enough to use as a drop-in host without further diagnosis.
- A separate `HP2UCC.exe` commandlet-host experiment also proved ABI-incompatible
  and produced an invalid-memory-read dialog. It has been removed from the
  private runtime and is not part of either Gate build.
- HP2 resolves its Documents home through dynamically loaded `shfolder.dll` and
  appends `Harry Potter II`. Gate 1 uses an x86 proxy in the private runtime to
  redirect only `CSIDL_PERSONAL`; all other folder requests forward to the real
  system DLL. The first failed run created `HP2VRGate1.log` in the normal
  Documents folder, proving command-line INI selection alone is insufficient.
- Real SteamVR compositor/headset behavior. Synthetic stereo is proven, but no
  result is labeled headset-observed yet.
- Menu/story panel detection and presentation rules across HP2's full campaign.
- HP2-specific input invocation paths for walking, aiming, spell lessons, and
  `BroomHarry`; raw offsets will not be used as a substitute for ownership.
- The correct isolated save/config namespace; the active desktop game currently
  also has configuration under OneDrive Documents.
- Headset/runtime/GPU details for performance and comfort targets.

