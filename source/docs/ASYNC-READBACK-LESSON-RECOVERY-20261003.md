# Frame stalls, lesson recovery strokes, and avatar walking

## Evidence and transport changes

The last real headset run started with tessellation OFF and 1920x1440 eyes. Its companion log contained 28 automatic stale-frame blackout events. This was not a tessellation-on run.

The old companion performed blocking CPU Map immediately after each eye CopyResource, holding the game's keyed eye locks throughout CPU readback, destination upload, and SteamVR Submit. The two-device path is necessary with the observed Intel game adapter and NVIDIA compositor adapter, but those waits and lock lifetimes were unnecessary.

HP1 V31 D3D11ProxyV31.cpp queues readback before fetching it, so work can overlap. HP2 now queues BOTH eyes into private staging, releases producer locks immediately, and maps with DO_NOT_WAIT. Both maps must succeed before either destination eye is updated. Pending pairs retain their own frame metadata and original rendered head pose. When pending, the companion submits its last complete pair instead of waiting or pretending the old image has a new head pose. No automatic FadeToColor remains.

This removes an explicit GPU-readback wait and producer-lock coupling. It does NOT eliminate CPU upload bandwidth, all driver waits, or HP2's native loading time. Timing diagnostics every five seconds separate acquisition failures, incoherent source pairs, GPU-readback deferrals, transfer time, submission time, and frame age. GetTickCount timing is coarse, not a precision GPU benchmark.

Added HP2VR_READBACK_TEST for isolated simulation. It runs the same staging/map/upload code on real Intel and NVIDIA devices without starting SteamVR. At 1600x1200: 427 complete uploaded pairs, zero deferrals; at 1920x1440 with tessellation off: 479 complete uploaded pairs, one nonblocking deferral, clean game exit. Both reported max poll/upload 16 ms using coarse timing. Save-free BeanRewardRoom fixture naturally transitioned to EntryHall_Hub; native out-of-sequence cutscene errors are fixture limitations, not graphics failures.

Actual SteamVR Submit timing, headset comfort, and stalls in a user's save remain to be measured on the next headset run. The readback smoke test is not a substitute for that.

## Lesson gesture

The previous recognizer rearmed after a pause anywhere, including the end of a flick. Returning from that endpoint could therefore create an opposite-direction input. New recovery state retains the stroke's starting centre and suppresses all motion until the wand returns within 2.5 cm and settles below 0.20 m/s for 40 ms. Holding the endpoint does not rearm it. A subsequent deliberate opposite flick works normally.

HP2's PlayGame PlayerInput hook still sends one native directional edge. Native spline-distance hit windows, misses, awards and progression remain authoritative; no matching-arrow filtering, queued late hits or auto-pass. The technique is flick, return, flick on the next beat. It deliberately does not count a continuous back-and-forth sweep as two intentional strokes.

Tests cover endpoint pauses, reverse recovery suppression, deliberate opposite beats at 10/16/33 ms sample intervals, cardinal directions and discontinuity rejection. A live full lesson and human gesture feel are not verified yet.

## Avatar

Preserved standing calibration from first gameplay draw, manual standing recalibration, and SteamVR-origin reset, with a 1.5 cm crouch quiet zone. Eye offset stays -26; body scale is held after calibration. Existing arbitrary-height/no-crouch unit tests pass. Stand naturally and use the manual calibration if the initial sample occurred while adjusting the headset.

Copied the authored 24-frame AvatarWalk data from read-only HP1 V31. Compile-time vertex-count check ensures it matches the HP2 VR avatar mesh. Blends leg deltas by native pawn horizontal speed while walking, advances once per stereo pair, fades out at rest/in air/deep crouch, and leaves controller arm IK in control. This is the reference walking cycle, not terrain foot IK or a new strafe/backward animation set. HP1 sources were not modified.

## Test

Start the normal launcher and load a save. Keep tessellation OFF and existing resolution unchanged for comparison. Check a scene transition, walking/stairs, and a lesson. Calibrate standing body under right grip + Y, Other Settings twice. In lessons use flick-return-neutral-next flick, including an opposite arrow after returning. Stop if visuals are uncomfortable. Inspect runtime/VR/HP2VRCompanion.log for HP2 frame timing records after the run.
