# Climb / flight / HUD iteration

## Implemented

- Climb attempt latches retry blocking immediately, including native completion. Rearm requires 150 ms of neutral forward input while grounded and outside a watched climb. Holding forward can continue ordinary movement but cannot repeatedly start the same climb.
- Recovery window shortened from up to 3 seconds to 1.2 seconds; early stalled/failed-exit checks retained. Cancel if the full-height-adjusted centre drops over 12 UU below the original attempt or leaves its bounded horizontal reach. No FarMoveActor call remains in recovery, including test mode; all recovery movement uses full-cylinder MoveActor collision sweeps. Obstructed sweeps abort rather than bypass geometry.
- MountFinish halves HP2's collision cylinder. Camera/controller anchor compensates that temporary half-height; this is not physical lift or a world teleport.
- BroomHarry PlayerWalking is controllable flight, independently of the cinematics preference. Native yaw is authoritative during flight; ordinary on-foot snap/smooth turning cannot overwrite flight steering.
- Corrected prior analog input magnitude error: HP2 uses fJoyBroomSensitivity=1/13312. Supply normalized stick *13312, not *2. Native course offsets, inversion, collision and steering remain in charge. No unrestricted free-flight replacement.
- Flight camera/controller anchor ignores negative walking eye-height offsets. No negative walking calibration dragging the view into a seated body.
- A during active flight requests native action only with a valid nearby KickTarget. A is not a snitch-catch shortcut. Outside active flight, retain original skip behavior.
- Right grip rising edge when native bReaching is set, target is a visible Snitch, and the tracked right hand is within 0.4 m submits the normal native action. Native director still decides success. No forced score/win/catch state. Real snitch encounter remains unverified; the 0.4 m reach may need tuning against HP2's animated target.
- Broom presentation now explicitly loads HP2's original HProps.skBroomQudditch script class, retaining inherited actor functions and original mesh/texture. A transient non-colliding instance is drawn only for the tracked body view, aligned with pawn yaw and 0.65 m below the eye anchor. The earlier custom native-only actor was removed: it lacked inherited Spawned metadata and crashed. The packaged-class replacement reaches the pitch and renders its handle in a simulated first-person gameplay capture without that failure. Existing avatar is retained; red-robed body remains outstanding.
- Further front/side robe flare: maximum 1.15 rad rearward and 0.20 avatar-unit lateral opening, still speed-smoothed, masked and crouch-faded. Visual clearance remains a headset acceptance check.
- Hidden-Harry fallback now also detects sustained (0.5 s) idle staging farther than 1200 UU from native camera with obstructed line of sight. Conservative heuristic, not perfect semantic scene membership. Climbing/trade/controllable flight keep their own camera rules.
- Scoped reflected Canvas inset for native ChallengeScoreManager.DrawScore and StatusManager.RenderHudItemManager. New fourth VR settings page adjusts HUD inset and vertical lowering; menu/trade selection handling unchanged. Underlying health, beans, scores and native visibility timing remain owned by HP2.

## Still outstanding

- Wrist-attached HUD; dedicated always-visible vs changes-only mode with tunable fade timing.
- Red-robed Quidditch body and seated animation/IK integration. Native broom handle is now visible; headset comfort/alignment still needs acceptance testing.
- Full live headset validation of problem ledges, end-challenge score placement, hidden-Harry scenes, sustained Quidditch steering/kicks and physical snitch catch.

## Verification

Build and included tests pass. Runtime confirms both HUD hooks and corrected positive/negative/neutral broom axis writes; remote idle fallback activates in the diagnostic intro. Short simulated map smoke tests do not validate completion of a Quidditch match or climbing every authored ledge. See runtime-progression-check logs for individual runs. Normal game saves/settings and HP1 sources must remain untouched.

Packaged-broom regression: 55-second isolated Quidditch run exited 0 and destroyed the engine cleanly. 1455 coherent stereo pairs and 1455 readback uploads; native A action requests occurred in active flight. Delayed gameplay screenshot shows the original broom handle over the pitch. Evidence preserved in `outputs/broom-packaged-20261004/`. These are simulated input/readback checks, not headset performance measurements or proof of a successful physical snitch catch. The simulation-only broom mode now looks down and pulses ordinary guarded A input; normal startup remains the main menu.

Subsequent 20-second on-foot smoke (BeanRewardRoom through EntryHall_Hub) exited 0, produced 432 coherent pairs, and exercised native wand charging/release and tracked effect endpoint. Normal runtime Gate 2/3 installation then completed; engine and companion hashes match the build. HP2VRSettings.ini and every file under the normal private Save directory retained their pre-install hashes. Prior owned binaries/configurations are backed up by the installers. No HP1 source was modified.

## Test order

1. Hold forward through the troublesome climb; verify no late return to top after a fall. Release forward briefly before a new climb. Report the ledge if progress fails rather than assuming recovery is complete.
2. Look down while walking/stopping/crouching for front robe clearance.
3. Fly with cinematics on: first-person, stick up/down/left/right; A near an opponent kicks. During a legitimate snitch reaching opportunity, reach with the right hand and newly squeeze right grip near it.
4. Right grip + Y opens VR settings; cycle OTHER SETTINGS to HUD inset/lower controls. Check health/beans and challenge results. Wrist mode is not yet available.
5. Revisit the parked-Harry scene; confirm fallback returns to body after native cutscene ends.
