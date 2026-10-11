# Mod-only packaging checks — 2026-10-05

## Guided setup revision

- WinForms layout constructed successfully under Windows PowerShell with 32 controls.
  Interactive clicking and clean-machine/headset acceptance remain unverified.
- Dependency status reports the matching game, DX11 renderer, OpenVR and local
  prerequisites. A missing OpenVR input returned Ready=false.
- Fresh installation completed outside the release folder with no optional DDS
  files selected. Localized AllDialog.USA_uax now copies correctly; empty game
  directories are recreated. No files in the working prototype were changed.
- Side-by-side update completed. Synthetic files in both save directories and
  VR/input settings matched the source byte-for-byte. These were isolated test
  files, not the user's saves; actual save loading still needs acceptance testing.
- A 12-second simulated normal-menu launch exited 0 and captured 725 native-menu
  frames. Zero stereo gameplay pairs are expected for this menu-only test; this
  does not establish headset interaction, gameplay or performance.
- Updates refresh Game.ini/renderer defaults while preserving User.ini and
  HP2VRSettings.ini. Existing install directories are never overwritten.

## Earlier packaging checks

- Successful dependency preflight using the user's existing HP2 renderer and
  separate x64 OpenVR library; exact file hashes checked.
- Missing renderer test failed before creating any destination directory.
- Fresh mod-only install imported dependencies and optional DDS textures from
  outside the package. No third-party downloads were performed by the installer.
- A 12-second simulated Quidditch_Intro smoke test exited 0, produced 44 coherent
  1920x1440 stereo frame pairs, and logged clean engine teardown. This is a
  startup/transport check, not a headset performance benchmark or full match test.
- Verify-Package.ps1 checks the file manifest and rejects bundled renderer,
  OpenVR, shaders, DDS replacements, prerequisite installers or base-game data.

The test installation is outside this package. Do not distribute that installed
copy. This package's normal installed launcher still starts at the main menu.
The download/installation guide uses short attributed excerpts and summaries;
HP2VR-specific steps are not represented as upstream author instructions.
The older all-in-one staging folder and working prototype were not modified.
