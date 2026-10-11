# October 3 comfort / interaction test build

HP1 V31 reference remains read-only. Player saves are not used by automated tests. Source assets copied into HP2: AvatarGeometry, AvatarPose, HandGripFrame, AvatarAtlas.

Texture audit clarification: the runtime already inherited Akven's 2022-01-04 4x ESRGAN pack from the graphics-modded base, documented in the workspace MODS_INSTALLED.md and baseline/BASELINE.md. All 3,126 external DDS files are present in runtime/Textures. "No new pack installed" below refers only to this update, not absence of the existing pack. Individual asset replacement use has not been GPU-traced, and the separately imported VR avatar uses the V31 atlas rather than these DDS replacements.

## Implemented

- Resolution presets 1280x960 / 1600x1200 (new default) / 1920x1440, selected in VR settings page 2, applied on restart. Requested 4x AA, alpha-to-coverage, anisotropy16, full mesh LOD. No new HD texture pack was installed; higher output resolution cannot invent detail absent from original textures. Final captured render target is single-sample, which alone does not establish the internal AA state.
- Grounded stair-height smoothing, not HMD rotation smoothing. Jump/fall, large discontinuities and moving bases reset it. Both eyes and tracked wand share the offset. Small native climb vertical movement and checked recovery height changes are eased visually. Native collision placement remains checked and discrete; this is not yet a fully swept smooth climb trajectory.
- Failed climb attempts are latched until forward is released, avoiding repeated mount animations from a held stick. A valid supported top still uses collision-tested recovery. It cannot guarantee getting up where no safe landing is found.
- Gnome carrying uses right-controller presentation and right-trigger native AltFire independently of wand ownership. ThrowCarryingActor keeps native throw states, trajectory/target assistance and collision behavior, with tracked hand origin and aim. Presentation is scoped to draw; throw origin is checked against world/movers. Native pickup remains unchanged. Real gnome pickup/throw still requires gameplay verification.
- Optional wand-flick lessons: hold the physical wand, flick up/down/left/right, briefly settle between flicks. Native SpellLessonTrigger.PlayGame.PlayerInput still decides timing, misses, score and progression. Stick axes/buttons are scoped out for that call only when the option is enabled. No automatic correct answers or skipped lesson rounds. Hook signatures validated in retail runtime; actual lesson timing remains unverified.
- Experimental body avatar toggle: V31 geometry/atlas, tracked arms/fingers, first-person head removed, hip lowering and knee bend from tracked vertical crouch with planted feet. Render-only; no crouched gameplay collision. No animated walking foot planting/terrain foot IK yet; deep crouch is limited to avoid collapse. Optional and off by default pending headset assessment.
- Extra aim beam is off by default; original native effects remain. Forward arrow can be toggled for artifact isolation.
- HP2 CutSceneManager advances its sliding subtitle border in SetCurrBorderHeight during render. Suppress only the duplicate right-eye step, retaining native text/timers/dialogue and mono menu rendering. This addresses a specific binocular border mismatch, not proof that every lower-screen artifact is gone.
- SteamVR companion fades a stale world after 250ms without a new frame (150ms fade out, 200ms return). Input/head tracking continues; focus loss/shutdown clears the fade. Does not shorten loading or interpolate arbitrary native camera cuts. HP2VR_NO_TRANSITION_FADE=1 disables the diagnostic comfort behavior. Actual compositor fade requires headset verification.

## Tests

Build-Gate3 and new comfort/flick unit tests passed, including cardinal directions, one-event/rearm, diagonal/tracking-jump rejection, stairs/jumps/travel. Isolated 1600x1200 stereo run passed and previous native Lumos/Flipendo launch self-tests still passed. Avatar-on standing and simulated 45cm crouch/downward-look captures were inspected. Retail Mount, carried-object throw and PlayGame.PlayerInput hooks installed. These checks do not substitute for headset visual comfort, actual carried-gnome release, or timed lesson scoring.

Final PrivetDr native dialogue smoke test: exit 0, both native subtitle function signatures validated (77/60 script bytes, Canvas parameter 4 bytes), runtime confirmed suppression of the right-eye duplicate border animation step. Evidence: SUBTITLE-SMOKE-20261003.log. All map overrides were confined to diagnostic invocations; normal launcher remains unchanged.

## Test instructions

Normal tools/Start-HP2VR.ps1 still opens the native main menu, with no hardcoded test map. Existing EyeOffsetCm=-26, UnitsPerMetre=40, SmoothSpeed=120, snap preference and BodyOnly are preserved.

Right grip + Y opens VR settings; bottom-left OTHER SETTINGS switches pages. Start with avatar OFF and test dialogue/subtitles, stairs, the problematic ledge and normal spell casting. If low-edge artifacts persist, toggle Forward arrow OFF and compare. Right-trigger throws a carried gnome without grabbing a wand. Enable Body avatar separately, look down, crouch and stand. Enable Lessons: Wand flicks only when ready to test a lesson. Resolution changes require restarting HP2; use 1280x960 if higher resolution costs too much frame rate.

Native behavior reference: https://github.com/metallicafan212/HP2UScriptDecompile (harry.uc, Triggers/SpellLessonTrigger.uc, Cutscene/CutSceneManager.uc). Renderer triangle API checked against https://github.com/metallicafan212/ICBINDx11Drv/blob/master/Src/UnTriangles.cpp; no renderer DLL replacement performed.
