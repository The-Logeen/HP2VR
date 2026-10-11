# Standing calibration, stair comfort, and transition presentation

## Changes

- Standing reference is sampled when the avatar first renders in gameplay, rather than assuming the earlier SteamVR origin pose was the player's standing pose. SteamVR recenter or the new third-page calibration button samples again. A 1.5 cm quiet zone avoids bending knees from small tracking fluctuations. This is a relative standing reference, not automatic floor detection.
- Body scale is cached at calibration. Previously changing eye offset resized the body continuously, making camera/body adjustment self-cancelling. Eye offset now moves the camera relative to the calibrated body until the next calibration. The existing -26 setting was preserved.
- Avatar receives the same visual stair-height offset as the camera and tracked hands. Native collision and HMD motion remain unfiltered.
- Stair smoothing runs immediately before the eye pair rather than after the engine tick. Easing increased from 0.10 to 0.20 seconds, step-discontinuity threshold from 26 to 40 units. Brief downward falling steps receive up to 0.15 seconds of smoothing; upward jumps and larger drops/teleports bypass it. Saved SmoothSteps was 0; restored to 1 for testing.
- Companion now resubmits its persistent local eye textures if game textures cannot be acquired during loading. It retains their original render pose, never pretends stale imagery was newly rendered. Keyed-mutex acquisition is nonblocking; a partial acquisition returns key 1 rather than prematurely permitting one eye to advance.
- Last real headset log contained repeated fade/recovery oscillations. Stale-world threshold increased from 250 to 750 ms; return from black now requires at least eight new pairs and 350 ms of continuous recovery. Stationary panels bypass world-stall fades. Long loads can still fade to black for comfort; no promise of uninterrupted live world rendering while HP2 loads.
- Added optional third-page tessellation experiment, restart required: Tesselation=True, TessDefaultFactor=0.15. Default remains OFF after comparison below. Existing requested MSAA4, alpha coverage, full mesh LOD and anisotropy16 unchanged; user Quality2 preserved.

## Verification

- Gate3 builds, transport/projection/input/player-feel/panel/lesson-policy/gnome/wand/climb/comfort tests passed. Added tests for repeated steps, short descending steps, jump bypass, arbitrary standing height and crouch quiet zone.
- Isolated runtime, no user saves and no SteamVR: 18-second BeanRewardRoom run with simulated avatar/crouch/trigger, tessellation on: 397 coherent pairs, clean exit. Off comparison: 420 coherent pairs, clean exit. Native transition to EntryHall_Hub observed. Different frame counts are not a controlled performance benchmark.
- Captures TESSELLATION-ON-20261003.bmp and TESSELLATION-OFF-20261003.bmp show stretched/spiky avatar geometry with tessellation on. Exact captured animation instants are not identical; this is a rejection of the visual result, not proof of a particular shader root cause. Kept tessellation off. Renderer compiled shaders do not provide inspectable tessellation source here.
- Logs confirm standing calibration head=1.700, origin=1.700 and requested tessellation state/factor. Unit tests cover non-1.7 starting heights.
- Initial sandboxed launch returned host error 2 without updating game logs; the scoped elevated isolated runs above succeeded.
- No headset verification of the new SteamVR resubmission/fade behavior, actual stair comfort, or calibration button yet. Remaining edge/subtitle/mesh glitches are not declared fixed.

## Headset test

Launch tools/Start-HP2VR.ps1 normally: main menu, then load a save. Stand naturally as gameplay starts. Open right grip + Y, OTHER SETTINGS twice, select Calibrate standing body height, and close the panel while remaining standing. Look down, crouch, then stand. If needed adjust eye offset afterward without recalibrating immediately (recalibration fits body to the new eye height).

Compare the same stairs up/down with smoothing on/off. Check ordinary jumping. Trigger a known scene transition and report any SteamVR room/application-loading exposure versus a deliberate smooth dark fade. Keep tessellation off for this first test; toggle it only for a separate restart comparison. For remaining lower-view glitches, compare avatar on/off and record whether subtitles are present.
