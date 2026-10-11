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

## What you need

Get every item marked **required** before you start. All of them are free
except the game itself.

| # | Item | Required? | Exact version | Where |
| --- | --- | --- | --- | --- |
| 1 | Windows | required | 10 or 11, **64-bit** | |
| 2 | *Harry Potter and the Chamber of Secrets* for PC | required | 2002 English retail release (build `021009_1205`). Other versions and the GBA/console games won't work. | your own copy |
| 3 | A SteamVR headset | required | Quest via Link/Air Link/Virtual Desktop, Index, Vive, Rift, Pico and similar | |
| 4 | Steam + SteamVR | required | latest | [SteamVR on Steam](https://store.steampowered.com/app/250820/SteamVR/) |
| 5 | HP2 DirectX 11 Renderer | required | **1.6.2** exactly | [gamepressure download](https://www.gamepressure.com/download/harry-potter-and-the-chamber-of-secrets-directx11-renderer-v162/zb12b06) |
| 6 | Valve OpenVR SDK | required | **2.15.6** exactly | [GitHub release v2.15.6](https://github.com/ValveSoftware/openvr/releases/tag/v2.15.6) |
| 7 | Visual C++ 2010 SP1 redistributable | required if not already installed | **x86** (`vcredist_x86.exe`), even on 64-bit Windows | [Microsoft download](https://www.microsoft.com/en-us/download/details.aspx?id=26999) |
| 8 | DirectX End-User Runtimes | required if not already installed | June 2010 | [Microsoft download](https://www.microsoft.com/en-us/download/details.aspx?id=8109) |
| 9 | ESRGAN 4x HD texture pack | optional | | [ModDB](https://www.moddb.com/mods/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack/addons/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack) |

You also need free disk space about the size of your game folder (plus the
texture pack, if used). HP2VR installs into its **own copy** of the game, so
your original game and saves are never touched.

## Install, step by step

### Step 1: Get HP2VR

1. Go to [Releases](../../releases) and download `HP2VR-v0.1.x.zip` (the newest).
2. **Before extracting**, right-click the zip > **Properties** > tick
   **Unblock** at the bottom > **OK**. If there's no Unblock box, skip this.
   (Without it, Windows may refuse to run the installer's scripts.)
3. Right-click the zip > **Extract All** and extract to a simple folder,
   for example `C:\Mods\HP2VR`. Don't run it from inside the zip.

### Step 2: Find your game folder

Find the folder where HP2 is installed. It's the folder that contains a
`System` folder with `Game.exe` inside, for example
`C:\Program Files (x86)\EA Games\Harry Potter and the Chamber of Secrets`.
Write this path down.

### Step 3: Download and extract the renderer

1. Download the **DirectX11 Renderer v1.6.2** archive from the gamepressure link.
2. Extract it to its own folder, for example `C:\Mods\HP2-DX11-1.6.2`.
   Inside you should see a `Common` folder and a `Harry Potter 2` folder.
3. Don't copy anything into your game. The HP2VR installer takes the files
   it needs from this folder.

Already have renderer 1.6.2 installed in your game? Skip this step and leave
the renderer box blank in Step 7.

### Step 4: Download and extract OpenVR

1. On the OpenVR v2.15.6 release page, under **Assets**, download
   **Source code (zip)**.
2. Extract it, for example to `C:\Mods\openvr-2.15.6`. Check that
   `bin\win64\openvr_api.dll` exists inside it.
3. You don't install anything from it. HP2VR copies only that one DLL.

### Step 5: Install the Microsoft runtimes (only if missing)

Not sure? Do Step 7 first: *Check requirements* tells you if they're missing.

1. **Visual C++ 2010**: on the Microsoft page click Download, choose
   `vcredist_x86.exe` (not x64), run it and follow the prompts.
2. **DirectX June 2010**: download `directx_Jun2010_redist.exe`, run it,
   extract it to an empty temporary folder, then run `DXSETUP.exe` from that
   folder. This adds old DirectX libraries; it doesn't downgrade Windows.

### Step 6 (optional): HD textures

Download the ESRGAN pack and extract it to its own folder, for example
`C:\Mods\HP2-ESRGAN`. Skip this for original textures.

### Step 7: Run the installer

1. In your extracted HP2VR folder, double-click **Install-HP2VR.cmd**.
   A window called *HP2VR guided setup* opens. (If Windows SmartScreen
   appears, click **More info** > **Run anyway**.)
2. Fill in the boxes with **Browse**:

   | Box | What to pick |
   | --- | --- |
   | HP2 game folder | your game folder from Step 2 |
   | Renderer folder | the extracted renderer folder from Step 3 (blank if the game already has 1.6.2) |
   | OpenVR SDK folder or DLL | the extracted OpenVR folder from Step 4 |
   | Optional HD texture folder | the extracted texture pack from Step 6, or blank |
   | Update from | leave blank for a first install |
   | New installation folder | a folder that **does not exist yet**, for example `C:\Games\HP2VR` (not inside the game folder or the HP2VR download) |

3. Click **1. Check requirements**. A list shows each component and its
   status. Fix any required row marked *Missing* or *Error* (the Details column
   says why), then click it again. **2. Install / update** stays greyed out
   until everything required is found. SteamVR may show *Check manually*;
   that's fine if SteamVR runs on your PC.
4. Click **2. Install / update**. Copying can take several minutes; don't
   close the window.
5. When it finishes, click **Open installed folder**.

### Step 8: Play

1. Start SteamVR and make sure your headset and both controllers are tracking.
2. In the installed folder, double-click **Start-HP2VR.cmd**.
3. The game opens at its normal menu. Choose **New Game** (saves from your
   original game aren't imported).
4. Use **Start-HP2VR.cmd** every time you play. `FIRST-RUN.html` in the same
   folder has a short first-play guide.

### Updating to a new version

Download and extract the new release (Step 1), run its Install-HP2VR.cmd,
pick your current HP2VR install in **Update from**, and pick a new
**New installation folder**. Game, renderer and OpenVR paths are taken from
the old install; saves and settings are copied over. The old copy is kept so
you can go back. Use the new folder's Start-HP2VR.cmd from then on.

### Uninstall

Close the game. If you want your saves, copy
`runtime\Home\Harry Potter II\Save` out of the installed folder. Then delete
the installed folder. Nothing else on your PC was changed.

### If something goes wrong

- **"running scripts is disabled on this system"**: you skipped Unblock in
  Step 1. Delete the extracted folder, unblock the zip, and extract again.
- **A file "doesn't match" or a version is "not supported"**: you have a
  different game, renderer or OpenVR version. Recheck the exact versions in
  the table above (renderer 1.6.2, OpenVR 2.15.6 `win64`).
- More fixes, including antivirus warnings, are in
  [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

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
