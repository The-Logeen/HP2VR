# Building HP2VR from source

Players don't need this. Download a release instead (see README).

## Prerequisites

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++** (the scripts call
  `vcvars32.bat` for the game-side x86 modules and `vcvars64.bat` for the
  x64 companion)
- [OpenVR SDK 2.15.6](https://github.com/ValveSoftware/openvr/releases/tag/v2.15.6)
  extracted somewhere (headers + `lib/win64/openvr_api.lib`)
- The HP2 Unreal Engine 1 headers (`sdk_hp2` from the UE1 SDKs collection),
  needed for Gate 2 and Gate 3 only
- Python + Blender only for the robe-weight export tools

## Where to point the build

Visual Studio is found automatically (`source\tools\vcvars.cmd` uses
`vswhere`, so any 2022+ edition works). Set these environment variables for
the rest; a script that needs one stops with a message if it's missing.

| Variable | Points to | Needed by |
| --- | --- | --- |
| `HP2VR_OPENVR_SDK` | extracted OpenVR SDK root (contains `headers` and `lib`) | Companion, Gate 2, presentation tests, release staging |
| `HP2VR_HP2_SDK` | the `sdk_hp2` folder | Gate 2, Gate 3, `tools\Build-HP2UCC.cmd` |
| `HP2VR_AVATAR_BLEND` | `HarryModel1_VR_Rigged.blend` | robe-weight Blender tools |

Example: `set HP2VR_OPENVR_SDK=C:\SDKs\openvr-2.15.6`

## Build order

Run from `source\` in a normal command prompt:

| Script | Builds | Needs |
| --- | --- | --- |
| `Build-Gate1.cmd` | CPU unit tests, inspector, `shfolder.dll` home redirect, `HP2VRHost.exe` launcher | VS only |
| `Build-Companion.cmd` | `HP2VRCompanion.exe` (x64 SteamVR side) | OpenVR SDK |
| `Build-Gate2.cmd` | Gate 1 + `d3d11.dll` proxy, probe, package inspector | OpenVR SDK, HP2 SDK |
| `Build-Gate3.cmd` | everything + `HP2VREngine.dll` | OpenVR SDK, HP2 SDK |

Output goes to `source\build\` (ignored by git). Each build runs its unit
tests and stops on the first failure. Nothing is installed into a game folder.

## Testing without a headset

`tools\Start-HP2VR.ps1 -Seconds 20 -Simulate` runs a bounded session with no
SteamVR. `Test-Presentation.cmd` runs the presentation integration tests.

## Packaging a release

`tools\New-ReleaseStaging.ps1` then `tools\Complete-ReleaseStaging.ps1` assemble
the mod-only package and `FILE-MANIFEST.csv`. Run `Verify-Package.ps1` on the
result: it fails if any game, renderer, OpenVR or Microsoft file slipped in.
After rebuilding, copy the new binaries into `payload\` and `utilities\`,
then regenerate `FILE-MANIFEST.csv` (the installer refuses any file whose
hash doesn't match it). Pushing a tag such as `v0.1.1` makes GitHub Actions
attach a zip of the repo to a Release automatically.

Line endings: `.gitattributes` turns off Git's conversion so checked-out files
keep the exact bytes the manifest expects. Don't change that.

## Notes

`source/docs/` holds dated development notes. Some describe earlier
experiments that no longer match the shipped package.
