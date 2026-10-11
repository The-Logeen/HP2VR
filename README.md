# HP2VR: Harry Potter and the Chamber of Secrets in PC VR

An unofficial, experimental PC VR mod for the original 2002 PC release of
*Harry Potter and the Chamber of Secrets*. It renders the game in stereo
through SteamVR and maps tracked motion controllers onto the game's own
controls, including a tracked wand for spellcasting.

> **Status: preview (0.1.0).** Installation, startup and stereo rendering are
> tested in simulation. Full headset play-through has **not** been verified yet.
> Expect rough edges and please report what you find.

This repository is the complete, ready-to-install mod: prebuilt mod files,
installer, controller bindings and full source. **You must own the game.** No game files, third-party renderer,
OpenVR DLL, HD textures or Microsoft installers are included.

## Requirements

- 64-bit Windows 10 or 11
- A PC VR headset that works with SteamVR (Quest via Link/Air Link/Virtual
  Desktop, Index, Vive, Rift and similar). Not a standalone Quest app.
- Harry Potter and the Chamber of Secrets (PC, English retail build). The
  installer checks exact file hashes and refuses other versions.
- Free downloads, linked with directions in
  [DOWNLOADS-AND-INSTALL.md](DOWNLOADS-AND-INSTALL.md):
  - HP2 DirectX 11 Renderer **1.6.2** (required)
  - Valve OpenVR SDK **2.15.6** `bin/win64/openvr_api.dll` (required)
  - Visual C++ 2010 x86 and DirectX June 2010 runtimes (if missing)
  - ESRGAN 4x HD texture pack (optional)

## Install

1. Download the latest `HP2VR-*.zip` from the [Releases](../../releases)
   page (or use **Code > Download ZIP** on this page) and extract it anywhere.
2. Get the required downloads above.
3. Double-click **Install-HP2VR.cmd**. Pick your game folder, the extracted
   renderer folder and the extracted OpenVR SDK folder, then a **new, empty**
   destination folder. Click *Check requirements*, then *Install / update*.
4. Start SteamVR, then run **Start-HP2VR.cmd** in the installed folder.

Your original game and saves are never modified. The mod runs from its own
isolated copy with its own save folder (saves are not imported; start a new game).

### Updating

Run the new release's installer, choose your old install under *Update from*,
and pick a new destination. Saves and settings are carried over and the old
copy is kept for rollback.

### Uninstall

Close the game, back up `runtime\Home\Harry Potter II\Save` if you want your
saves, then delete the installed folder. Nothing else on your PC is changed.

## Controls

| Action | Control |
| --- | --- |
| Move | Left stick |
| Turn (smooth or snap) | Right stick |
| Jump | Left trigger |
| Cast a spell | Hold right grip to draw the wand, aim, hold right trigger, release it |
| Skip cutscene | A |
| Game menu | Y |
| VR settings | Right grip + Y |
| Recenter | SteamVR recenter, or the button on the settings panel's calibration page |

Bindings are provided for Oculus/Meta Touch, Valve Index and HTC Vive
controllers and can be remapped in SteamVR's controller bindings.

VR settings (eye height offset, world scale, smooth/snap turning, speed,
in-body vs cinematic camera) are saved in `runtime\System\HP2VRSettings.ini`.

## Help

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md), including what to do if your
antivirus flags `d3d11.dll`. Report bugs through
[Issues](../../issues) with the template filled in.

## Building from source

See [BUILDING.md](BUILDING.md).

## Credits and license

- HP2VR mod, wand and avatar: The-Logeen. MIT License, see [LICENSE](LICENSE).
- HP2 DirectX 11 Renderer: darknovismc / Marcin Nowicki (not bundled).
- ESRGAN texture pack: Akven (not bundled).
- OpenVR: Valve Corporation, BSD-3-Clause, see [OPENVR-NOTICE.txt](OPENVR-NOTICE.txt).

Harry Potter is a trademark of Warner Bros. Entertainment Inc. This is a
non-commercial fan project and is not affiliated with or endorsed by Warner
Bros., Electronic Arts, KnowWonder or Valve.
