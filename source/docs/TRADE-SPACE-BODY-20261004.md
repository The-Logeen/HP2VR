# Trade space and body presentation

## Implemented

- Vendor prompt anchored beside the native Vendor actor, 0.65 m lateral and 0.35 m on the player's side, at the player's eye height minus 0.25 m. It rotates to face the player while its world position stays fixed.
- On prompt opening, a close player backs away gently for at most 0.8 seconds at 0.6 m/s; stick movement immediately takes precedence. Walls and unsupported ground stop movement.
- While the native transaction awaits a choice, stick translation and configured smooth/snap rotation are available without releasing HP2's capture or changing its story state. Ground-supported translation uses collision sweeps, not teleportation. This is not unrestricted jump/climb movement inside a transaction.
- Pawn distance above 3 m from the initial vendor position submits native No. Choice input, stock, payment, native item delivery and dialogue remain HP2-owned. Headset-only roomscale distance is not yet included in this threshold.
- In-body cinematic exception is deliberately conservative: active native cutscene plus Harry hidden, mesh missing, or in zone zero more than 1000 UU from the native camera. The check runs outside render-time body hiding. Trades/climbs cannot trigger this fallback. Distant camera alone is insufficient.
- Restored full body-weighted authored walk deltas rather than fading them out above the knees. This removes the artificial upper-robe freeze. Existing calibrated crouch, planted feet, head omission, controller arms and finger poses are retained.

## Not yet fulfilled

The avatar is still the prepared V31 geometry/VR_Walk, not HP2's live skharryMesh skeleton and full native animation set. Removing the walk height mask does not establish identical native robe flow. Native skeleton/topology mapping and hybrid controller overrides still need implementation and validation. Do not describe this build as a completed native-avatar replacement.

The cinematic heuristic requires headset confirmation at the specific dark exterior scene; a visible Harry parked in a nonzero zone will intentionally not be classified absent without better scene evidence.

## Validation

Gate 3 compilation and existing projection/input/comfort/flick/climb/wand tests pass. Isolated native No regression closes the prompt and returns PlayerWalking. Movement/walk-away regression results are added below after execution. Normal startup remains Startup/main menu; diagnostic travel is confined to runtime-progression-check. User settings and HP1 reference files must remain unchanged.

Completed movement regression: simulated backward stick while vendor UI was open produced `HP2 vendor walk-away sends native No`, `HP2 native vendor consumed choice=1 bar=0`, and `PlayerWalking menu=0`. Pawn moved approximately 75 UU from its transaction start before control returned. Over 900 stereo pairs were published; clean shutdown. This verifies scripted movement/cancellation, not headset button ergonomics or every terrain slope. Installed engine hash matches build; user settings hash unchanged.

## Headset test

Run tools/Start-HP2VR.ps1 normally. Load the trade save, let Fred's prompt open, check spacing, then move sideways/back and rotate. Walk over 3 m away and verify refusal returns control. Re-engage and try Yes/No. Check standing straight, crouching, hand grips/trigger poses and robe motion while walking. Revisit the absent-Harry cinematic and report the map/scene if it remains dark.
