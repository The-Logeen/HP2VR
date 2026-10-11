# Broom pitch and robe opening revision

HP2 BroomHarry extends harry and flies in PlayerWalking; QuidditchPlayer is a separate HChar, not the controlled pawn. The previous state-only walking predicate suppressed joystick special-state injection during flight. BroomHarry is now excluded from ground-walking adapters. After native harry.PlayerInput, write aJoyBroomYaw=2*stickX and aJoyBroomPitch=-2*stickY, matching HP2's default joystick bindings. BroomHarry then consumes these in its native input logic; native sensitivity/inversion and track bounds remain authoritative. Zero axes are written for neutral/tracking loss/menu ownership. No flight mechanics, rewards or course script replaced.

Robe revision: whole existing RR/RL front/side panel bone weights, 84 vertices rather than 78 position-feathered vertices. Increase maximum backward rotation from .45 to .85 radians, lateral spread from .055 to .15 avatar units. Existing speed smoothing, crouch fadeout, slower walk cadence and head/hand rig unchanged. This remains a procedural approximation requiring visual testing, not proven native cloth matching.

Trade implementation untouched. Build and all included tests pass, including 84-vertex robe symmetry/exclusion checks. Diagnostic Quidditch launch uses separate runtime and saves; normal launcher still begins at the menu.

Runtime smoke: BroomHarry1 identified correctly; native input adapter observed pitch +1.487, -1.487 and zero for corresponding simulated stick inputs. 596 coherent stereo pairs, clean exit. The test entered the intro cutscene and does NOT prove complete course steering/altitude response. Headset flight remains the acceptance test. Installed engine verified against built hash; VR settings unchanged.

Source checked: https://github.com/metallicafan212/HP2UScriptDecompile/blob/master/HGame/Classes/Quidditch/BroomHarry.uc

Headset: load gameplay; look down walking forward, stop, crouch. Then load Quidditch; left stick forward/back should supply climb/descent according to native invert preference; left/right supplies lateral steering. Check return to neutral. Report if the robe still occludes the trousers, with a screenshot if possible, or whether pitch responds but hits native track limits.
