# World-space vendors and swept climb recovery

## Changes

- VendorManager.VendorTransaction retains HP2's native purchase/refusal handler. The VR presentation replaces only its flat bar and mouse hit tests with a world-space price/item display and right-controller Yes/No selection. A trigger edge invokes the validated native PlayerInput handler directly because dialogue can capture Harry's normal input. It does not require holding the wand.
- The world continues rendering in stereo. Native actor capture, dialogue, stock, affordability, bean deduction, item delivery, and release remain owned by HP2. Native vendor actors are not converted to a frozen screenshot. The player remains stationary during HP2's transaction.
- Added pointer ray and hover underline. Preheld trigger does not select a button when the prompt opens. Missing required native hooks leave the native interface unchanged.
- Climbing ignores continued special-state movement-axis injection. MountFinish's negative vertical residual is clamped while live VR owns the local player; native animation/state completion is retained.
- The old recovery destination teleport is replaced with bounded, collision-swept up-then-over movement. Full-cylinder support and fit checks still apply. A blocked sweep stops and permits local falling rather than teleporting through geometry.
- Existing forced in-body climbing, camera/body separation, automatic standing calibration, renderer transfer optimizations, settings, and normal main-menu startup remain unchanged.

## Verification scope

Gate 3 builds and all included policy/projection/input/comfort/flick/wand tests pass. World-space text was visually inspected from the isolated renderer capture. These automated tests do not establish headset pointer comfort or successful climbing at the user's particular ledge. Native vendor result checks are recorded separately below.

Diagnostic vendor spawning requires HP2VR_VENDOR_TEST and a runtime path containing runtime-progression-check. It is not enabled by the normal launcher. Diagnostic maps and test saves are isolated from normal gameplay.

Isolated decline result: `HP2 native vendor consumed choice=1 bar=0`, followed by `state=PlayerWalking menu=0` and clean shutdown. The fixture uses a spawned default vendor rather than the authored outdoor Fred/George offer. Direct map travel produces unrelated missing-story-actor CutErrors; it is not a valid full-story regression. Do not claim a real inventory/bean purchase was verified by this fixture.

Isolated Yes result: `HP2 native vendor consumed choice=0 bar=0`, followed by `state=PlayerWalking menu=0` and clean shutdown. This exercises the default vendor's no-stock response, not a paid item delivery. Both tests used simulated tracking and actual cross-GPU readback, without SteamVR/headset submission.

## Headset checks

1. Launch tools/Start-HP2VR.ps1 normally and load the existing game.
2. Approach Fred or George. Aim the right-controller ray at No, then press/release the trigger. Confirm the prompt closes and dialogue returns control.
3. Re-engage and choose Yes with enough beans. Confirm the normal price, bean deduction, and delivered item. Check an insufficient-beans response if convenient.
4. Hold forward throughout a climb. Confirm no downward fall/scream or endpoint teleport, and that climbing stays in-body even with cinematics enabled.

Do not treat a failed collision sweep as permission to bypass a wall or move to an unsupported destination. Record the map and exact ledge for any remaining failure.
