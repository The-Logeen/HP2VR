# Troubleshooting

## My antivirus flagged `d3d11.dll` (e.g. "Trojan:Win32/Wacatac")

HP2VR's `d3d11.dll` is a *proxy*: the game loads it instead of Windows'
Direct3D 11 library, it forwards every call to the real one in `System32`,
and it captures the rendered frames for VR. That load-in-place pattern is what
machine-learning detections (the `!ml` suffix) react to, and the file is not
code-signed. Windows Defender has flagged it this way during development.

What to do:

1. Check the file is exactly the one released. In PowerShell:
   `Get-FileHash .\runtime\System\d3d11.dll` and compare with the hash for
   `payload/System/d3d11.dll` in `FILE-MANIFEST.csv` of the release.
2. If it matches, you can report it as a false positive at
   <https://www.microsoft.com/wdsi/filesubmission>.
3. If you aren't comfortable, don't use the mod. The source is in this repo
   (`source/src/HP2D3D11Proxy.cpp`) if you want to build it yourself.

Don't turn off your antivirus or add broad folder exclusions for this mod.

## The installer says a file doesn't match / version not supported

HP2VR only runs on the exact game build and dependency versions it was tested
with. Use the English retail game, renderer **1.6.2** (not a newer one) and
OpenVR SDK **2.15.6** `win64` DLL (not `win32`).

## Game starts in software rendering or shows a black window

The game's first-run detector can switch the renderer back to software. Reinstall
to a new destination; the installer re-applies the DirectX 11 settings. Check the
Visual C++ 2010 **x86** and DirectX June 2010 runtimes are installed.

## Nothing shows in the headset

Start SteamVR and confirm your headset is tracking *before* running
Start-HP2VR.cmd. The game's own menu appears on a panel; gameplay is in stereo.

## Controllers don't respond

Open SteamVR > Settings > Controllers > Manage controller bindings, choose
HP2VR and select the default binding for your controllers.

## Motion sickness

Open VR settings (right grip + Y) and switch to snap turning, lower the move
speed, or choose the in-body camera.

## Reporting a bug

Open an issue and include your headset, controllers, GPU, which step failed,
and the `.log` files under `runtime\VR` and `runtime\Home` in your installed folder.
