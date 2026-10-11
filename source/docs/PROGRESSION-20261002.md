# Lumos and native climb recovery

HP1 v31 remains read-only. Changes apply to the HP2 private prototype only.

## Lumos

The previous persistent rune relocation could contaminate native SpellCursor.vTargetOffset. HP2 computes that offset from SpellGesture.Location, and the released spell seeks TargetActor.Location + TargetOffset. A visible rune and incantation are not proof of a gargoyle hit.

RunePresentation now scopes location/rotation changes to DrawWorld and restores both before gameplay resumes. Native targeting, hit dispatch, gargoyle HandleSpellLumos, light broadcasts and door events remain unchanged. Stay-in-body is not supposed to cancel those events. This is a candidate cause/fix, not a verified statue completion.

## Climbing

V31 ClimbRecovery/ClimbGuard/ClimbSafety informed the native-first policy. HP2's own MountFinish halves collision size and restores it in EndState. HP2ClimbRecovery observes the native attempt, then handles a stalled attempt or walking return without meaningful height gain. It exits the native state before checking the full cylinder, uses HP2's absolute Destination (not HP1's MountDelta heuristic), searches the authored approach, checks a supporting surface and collision-test/checked-commit/post-test placement. It does not implement V31's general free-climb or saved-ground teleport.

Other scripted states cancel observation. Failure retains falling physics, not a frozen PHYS_None. No quest flags or door events are forced. Live logs report native completion, checked recovery, or rejection.

## Validation and headset retest

Build-Gate3 succeeded, including the new climb-policy tests and existing input, player-feel, projection, panel, wand and transport tests. Policy tests do not prove runtime geometry behavior. The affected statue and ledge require gameplay verification.

Isolated smoke test: first launch failed because the test-copy setup omitted System/d3d11drv shader resources (states.fxh). After copying the missing System resource directories, the 15-second simulated BeanRewardRoom run exited 0, produced 325 coherent stereo pairs at 1280x960 and shut down cleanly. SteamVR was not started. The test used runtime-progression-check/Home, not the player's save directory. This did not exercise the statue or climbing. Normal launch remains the native main menu; no diagnostic map was hardcoded.

Launch tools/Start-HP2VR.ps1 normally (no map argument). Keep Stay in body enabled, load the existing game, grab the wand, aim at the Lumos statue until locked/charged, then release trigger. Check statue reaction and wand illumination, then approach the passage. Revisit the repeating climb and hold forward through the native animation. Report whether it tops out, falls, or loops; HP2VREngine.log records the recovery decision.

Source inspected: https://raw.githubusercontent.com/metallicafan212/HP2UScriptDecompile/master/HGame/Classes/harry.uc . This community decompile is supporting evidence; the implementation builds against the HP2 SDK and uses reflected Destination and native collision APIs rather than HP1 offsets.
