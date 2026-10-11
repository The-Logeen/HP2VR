# Harry Potter 2 VR prototype

This is the isolated development workspace for a PC VR adaptation of *Harry
Potter and the Chamber of Secrets*. It uses the HP1 V31 project as an
architectural reference, but it does not load, modify, or install HP1 binaries.

## Current milestone

Gates 0 through 3 are complete for the selected HP2 installation. The project
records the exact executable, engine, renderer, package, map, and configuration
baseline in `baseline/`. The selected installation is an English (`usa`) x86
retail build with the HP2 DX11 renderer 1.6.2 and an external ESRGAN texture
pack; it is intentionally treated as a modified, build-locked baseline.

A private new-game run reached `PrivetDr`, `FlyingFordCutScene`, and
`Adv1Willow`, possessed `HGame.harry`, created an isolated Slot 1 save, and
shut down cleanly. D3D11, the live ABI probe, the HP2 engine/render subclasses,
and same-tick stereo capture are now proven in retail `Game.exe`. The source
tree contains:

- an HP2-only, versioned shared bridge with fail-neutral input snapshots;
- a read-only build inspector that refuses unknown core binaries;
- a standalone safety launcher for bounded flat runs from a marked private
  runtime;
- unit tests for bridge compatibility, stale-input handling, projection math,
  proxy exports, and build gates;
- scripts to reproduce the baseline and stage a separate private runtime.

The first live world run exposed an HP2 first-run behavior: with
`FirstRun=0`, the native detector rewrote `GameRenderDevice` to
`SoftDrv.SoftwareRenderDevice` even though the selected installation is
configured for the installed DX11 1.6.2 renderer. The private installer now
transactionally sets `FirstRun=433` and reasserts all three D3D11 renderer keys
in the private System and redirected-home configurations. `-NOFRONTEND` bypasses
the native launcher that performed the rewrite. Live logs now prove the D3D11
proxy captures the real device and loads the ABI probe.

Gate 3 supplies HP2-specific `UGameEngine` and `URender` subclasses. One fresh
tracking pose is held for a complete pair; HP2's own camera is transformed by
value for the left and right views; its player, `BaseCam`, targeting, spell,
broom, and save state are not modified. Per-eye OpenVR projection tangents are
written to `FSceneNode`, D3D11 keyed textures are captured, and the exact pose
used to render is published for `Submit_TextureWithPose`. A no-SteamVR
simulation produced 303 coherent 512x384 pairs on the local Intel GPU, then
both processes shut down normally. Proof logs are under `runtime/VR/`.

This is not yet labeled headset-proven or generally playable. Touch/Quest,
Index/Knuckles, and Vive action manifests now feed tracked hands, two sticks,
two triggers, grips, menu, dialogue-skip, and recenter into the neutral bridge.
The first native adapter reuses HP2's own W/S/A/D, MouseX, LeftMouse,
RightMouse, and Enter bindings; it never writes player fields. Its mapping is
unit-tested and the absent-controller path is live-proven, but physical
controller input still needs headset observation. Escape is intentionally not
mapped because this HP2 configuration binds it to quit. The next gate is a real
SteamVR/headset presentation and controller test, followed by panel/menu
presentation, comfort behavior, wand/lesson translation, and broom tuning. See
`docs/HP2-OWNERSHIP-MAP.md` for the native authority boundary and
`docs/HP2-LESSONS-QUIDDITCH-DESIGN.md` for the HP2-native plus optional
HP1-style spell-lesson/Quidditch plan. The source tree now includes a tested,
pure HP2 lesson/Quidditch input foundation; it is not connected to live retail
state until the observe-only reflection gate proves the named owners.

The standalone launcher checks the `.hp2vr-private-runtime` marker and exact
known hashes before starting the runtime's own `Game.exe`. It also requires the
tested private `shfolder.dll` redirect so HP2's log/config/save home lives under
`runtime/Home`, not the user's Documents folder. A direct process built against
the local `sdk_hp2` compiled but faulted with `0xC0000005` before its first log
line. A later `HP2UCC.exe` experiment produced the user-observed invalid-memory-
read dialog as well. Both standalone SDK host routes are abandoned; neither is
installed in the runtime or used by a normal build.

## Build

Run `Build-Gate1.cmd`. It builds and runs CPU tests, then creates the inspector,
home-folder proxy, and safety launcher in `build/`. It never installs into a
game directory. Run `tools\Install-Gate1.ps1` afterward to install the tested
proxy into this project's private runtime only.

Run `Build-Gate2.cmd` to rebuild Gate 1 plus the D3D11 transport, compile-only
HP2 SDK ABI sentinels, package inspector, and live read-only probe. Then run
`tools\Install-Gate2.ps1` to install only the owned proxy/probe files into this
project's marked private runtime. The installer refuses unowned or externally
changed destinations and preserves replaced owned versions under
`runtime\VR\gate2-backups`.

Run `Build-Gate3.cmd` to rebuild all earlier gates plus the HP2 engine package.
After closing the private game and companion, install the matched build with:

```powershell
.\tools\Install-Gate1.ps1
.\tools\Install-Gate2.ps1
.\tools\Install-Gate3.ps1 -Mode Both
```

`RenderOnly` and `EngineOnly` remain available as diagnostic Gate 3 modes.
Every installer is limited to this project's marked private runtime and refuses
unowned or externally changed destinations.

For the first real headset test, start SteamVR and the headset, then run:

```powershell
.\tools\Start-HP2VR.ps1
```

The launcher waits until the game is closed and asks the game to shut down the
companion before D3D teardown. `-Seconds 20 -Simulate` runs a bounded no-SteamVR
diagnostic. The default starts `Startup.unr` with HP2's native front end enabled
and leaves New Game selection to the player. The test-only `-NOFRONTEND` default
and forced `FEBook.OpenBook` startup have been removed. A specific
map remains available as an explicit diagnostic override with `-UnrealUrl`; no
gameplay scene is hard-coded as the normal start.

The normal-start check on 2026-09-29 reached the native Windows Main Menu
(New Game, Load Game, Options) and closed cleanly. This front end runs before
the Unreal engine and is not yet captured by the VR panel. It also rewrites
System/Game.ini's GameRenderDevice to SoftDrv even with FirstRun=433; Gate 1
was reapplied after the check. Capturing this front end and preserving D3D11
through its game handoff are still required before this path is headset-ready.

The private WinDrv test window is pinned to 1280x960. This is large enough to
inspect menus and stereo captures on the desktop without changing the original
HP2 installation; final headset render sizing remains a separate measured VR
setting.

To refresh the read-only inventory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\Inventory-HP2.ps1 `
  -GameRoot "C:\Games\Harry Potter II"
```

To create a private runtime later, choose a new empty destination and run
`tools\Stage-PrivateRuntime.ps1`. The script refuses an existing non-empty
folder, a destination nested in the source, reparse points, and hash-mismatched
core binaries.

To prove the redirected run did not touch the existing desktop game's files,
run `tools\Verify-ExternalIsolation.ps1`. It compares the captured external
snapshot by path, length, and SHA-256 and rejects additions or removals.

## Evidence labels

Project notes distinguish four levels: observed in installed files, observed in
a live flat run, observed in a headset, and inferred from reference code. A
successful compile is never reported as a playable VR build.

