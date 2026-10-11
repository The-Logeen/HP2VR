# External downloads and installation

Checked 2026-10-05. Short quotations below are from the linked pages; longer
directions are attributed summaries, not verbatim reproductions. The HP2VR
integration steps are explicitly distinguished from the authors' instructions.
No third-party downloads are included or downloaded automatically.

## Required: HP2 DirectX11 Renderer 1.6.2 — darknovismc / Marcin Nowicki

[Author's project](https://www.moddb.com/mods/unreal-directx11-renderer-for-ut-ug-rune-and-dx)
and [the tested version's download and instructions](https://www.gamepressure.com/download/harry-potter-and-the-chamber-of-secrets-directx11-renderer-v162/zb12b06).

The version-specific download page says:
> If you are launching the game for the fisrt time then select D3D11 Renderer.

Its installation directions, summarized:

1. From Common, place D3D11Drv.int, Effects11.dll and the d3d11drv folder under
   the game's System directory. It instructs replacing the previous shader
   folder when updating; back it up first instead of merging different versions.
2. Use d3d11drv.dll from the Harry Potter 2 folder, and include tessellation.cfg.
3. Select DirectX11 in the renderer preferences, or configure
   GameRenderDevice=D3D11Drv.D3D11RenderDevice under Engine.Engine.
4. It also points to normal/height maps when available and VC++ 2010 x86 for
   initialization failures.

HP2VR integration: use the exact HP2 1.6.2 build, not the latest unrelated
renderer or another game's DLL. The installer verifies renderer/library/shader
hashes. It configures VR rendering itself and supplies its VR tessellation.cfg.
To avoid modifying your original game, prepare the same System layout in a
separate folder and pass -DependencyRoot to the installer.

## Optional: ESRGAN 4x texture pack — Akven

[Download](https://www.moddb.com/mods/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack/addons/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack)
and [author's installation/compatibility page](https://www.moddb.com/mods/harry-potter-and-the-chamber-of-secrets-esrgan-upscale-pack).

The download page says:
> Just unpack into main game folder. NEED DirectX11 renderer to work. Follow the installation procedure from the mod description.

The author's main page directs users to install DX11 first, then unpack the
texture archive into the game directory. It suggests disabling tessellation
if polygons move or look jagged. The download-page author comment also says
to merge the extracted contents without deleting existing textures.

HP2VR integration: the game root for an already installed VR copy is
Installed-HP2VR/runtime. Merge there, preserving the Textures subfolder layout.
Alternatively select the extracted pack's Textures folder in guided setup's
optional HD texture field (-TextureRoot on the command line). Leaving it blank
uses original textures on a fresh install; updates retain existing DDS files. Never remove the
original UTX packages. Tessellation remains off in our supplied VR settings.
The author's optional ReShade/widescreen/music suggestions are not required
or validated for HP2VR; don't add them to diagnose a VR problem.

## Required: Valve OpenVR runtime, x64

[Official OpenVR SDK](https://github.com/ValveSoftware/openvr) and
[tested SDK 2.15.6 download](https://github.com/ValveSoftware/openvr/releases/tag/v2.15.6).

HP2VR-specific integration directions (not claimed to be Valve's installer
instructions): obtain SDK 2.15.6 and extract bin/win64/openvr_api.dll. Select
the extracted SDK root in guided setup, or pass that file with -OpenVRDll on
the command line. Do not choose the win32 DLL. The tested SHA-256 is
BAB8AC6EF64E68A9CA53315B0014D131088584B2EFDFA6DB511D67EC03CFCB4A.
The full SDK need not be installed just to play. This API DLL is NOT SteamVR.
Install SteamVR separately through Steam and connect your headset normally.

## Microsoft prerequisites, if missing

- [Visual C++ 2010 SP1 MFC security update](https://www.microsoft.com/en-us/download/details.aspx?id=26999):
  choose vcredist_x86.exe for the 32-bit HP2 renderer, even on 64-bit Windows.
  Microsoft's page offers saving the installer and then running it. Follow its
  prompts; a restart may be requested.
- [DirectX End-User Runtimes, June 2010](https://www.microsoft.com/en-us/download/details.aspx?id=8109):
  download the offline redist. Extract it to a temporary folder and run DXSETUP.
  The page identifies these as legacy side-by-side libraries, including D3DX11;
  this does not replace the operating system's DirectX version.

These installers are not bundled or run silently. Use Microsoft downloads,
not DLL-download sites. Headset software, SteamVR and GPU drivers remain
separate installations. Do not disable security protection to install HP2VR.
