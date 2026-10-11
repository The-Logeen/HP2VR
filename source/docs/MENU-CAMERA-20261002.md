# HP2 menu / camera correction, October 2

## Evidence

- Retail package inspection confirms FEBook.EscFromConsole and CloseBook.
- The community HP2 decompile's FEBook implementation selects InGamePage through
  EscFromConsole. MAIN is not the in-game pause page. We invoke the installed
  retail function, not a replacement script or the modified community package.
  Reference: https://github.com/metallicafan212/HP2UScriptDecompile/blob/master/HGame/Classes/Menu/FEBook.uc
- A 14-second, explicitly simulated PrivetDr run captured the actual native book
  at 1280x960. It did not launch SteamVR, load the user's save, or change the
  normal main-menu startup URL. The run ended with exit 0 and 249 coherent pairs.
- Simulated forward input produced approximately 210 UU/s native walking.
- Runtime BaseEyeHeight is 40.75 at offset 940; CollisionHeight is 42.
  ViewRotation offset 916 matches this SDK. The overall SDK class-size mismatch
  warranted reflected access, but does not establish the cause of black objects.

## Changes

- Y invokes native EscFromConsole; closing invokes native CloseBook.
- Direct render-target menu capture retained; diagnostic BMP no longer swaps
  already-BGRA pixels a second time.
- Stereo horizontal FOV accounts for viewport aspect, and compositor vertical
  cropping uses the corresponding vertical extent. Added rectangular viewport
  projection regression tests (square, 4:3, 16:9).
- Eye height and ViewRotation use validated runtime property offsets.
- During ordinary first-person draws, temporarily hide the original Harry mesh
  to prevent self-occlusion. Native actor visibility is restored before gameplay
  updates; scripted external camera states remain native. This is not the final
  tracked avatar implementation.

## Unverified / outstanding

Headset comfort, eye height in the user's save, jump-related black surfaces,
and in-engine pointer clicks still require testing. The menu image and native
walking were verified in simulation, not in a headset. Tracked wand presentation
and native cursor-ray adaptation remain unfinished: ordinary HP2 SpellCursor
uses BaseCam.vForward, not simply Harry.AimRotOffset. Do not advertise a complete
motion-controlled wand or a completed V31-style avatar.
