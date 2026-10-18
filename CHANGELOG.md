# Changelog

## 0.1.5 (preview), 2026-10-18

- Experimental Steam Frame controller bindings (`frame_controller`), picked by
  SteamVR as the default profile "HP2VR - Steam Frame (experimental)". Left
  View opens the game menu, right grip + left View opens VR settings. Not yet
  tested on Frame hardware. See [FRAME-CONTROLS.md](FRAME-CONTROLS.md), which
  is also copied into each installation.
- New VR settings page (OTHER SETTINGS four times) to adjust wand and menu
  pointer pitch and yaw independently, from -90 to +90 degrees in 2-degree
  steps. Defaults are zero, so existing controllers behave as before.
- Engine and companion rebuilt for the angle settings; Touch, Index and Vive
  bindings are unchanged
- Launchers pass `-ExecutionPolicy Bypass` for their own PowerShell process
  only, so scripts run even when Windows blocks them by default. No saved
  policy or antivirus setting is changed.

## 0.1.4 (preview), 2026-10-15

- Setup finds more folder layouts: for the renderer you can pick the folder
  holding `Common` and `Harry Potter 2`, or either of those folders; for
  OpenVR you can pick the SDK root, `bin` or `win64`, or paste the DLL path.
  Quotes around pasted paths are ignored.
- Double-click a row in the requirements list for what was searched and how
  to fix it. A missing file now names the path that was checked, and a found
  renderer or OpenVR shows the path it resolved to.
- New offline [SETUP-GUIDE.html](SETUP-GUIDE.html) from download to first
  launch, opened by the **Full setup guide** button and copied into each
  installation
- Game and mod binaries are unchanged

## 0.1.3 (preview), 2026-10-13

- Installer diagnostics: each Check requirements or Install / update run saves
  its own log folder (request, transcript, raw output, result report) in your
  Windows temp folder, with an **Open diagnostic logs** button. Failures name
  the phase, the file, expected and actual hashes, and a next step. See
  [INSTALLER-DIAGNOSTICS.md](INSTALLER-DIAGNOSTICS.md).
- Setup window errors are saved to `HP2VR-Setup-GUI-error-*.txt`
- Game and mod binaries are unchanged

## 0.1.2 (preview), 2026-10-11

- Fixes the in-headset main menu: the startup panel now fills correctly and
  the right trigger selects Load Game / New Game. The companion now uses the
  game window's Windows DPI setting when finding, capturing and clicking the
  game's native menu, and clears a held trigger when the dialog changes.
- New native menu DPI regression test (`source/tests/NativeMenuDpiTests.cpp`)
- Package check also blocks `.usa`, `.bmp`, `.log` and `*_uax` files

## 0.1.1 (preview), 2026-10-11

- README: exact, step-by-step install guide with a requirements table,
  which box to pick in the installer, and fixes for common setup errors
- Release workflow can be started by hand from GitHub Actions

## 0.1.0 (preview), 2026-10-11

First public mod-only release.

- MIT license; README with requirements, controls and uninstall; build guide;
  troubleshooting (including antivirus); bug-report template; GitHub Actions
  package check and tagged releases
- Build scripts no longer depend on the original developer's folders

- Stereo rendering for HP2 through SteamVR via the DirectX 11 renderer 1.6.2
- Tracked wand spellcasting using the game's own spell system
- Touch, Index and Vive controller bindings
- In-game VR settings panel (right grip + Y)
- Comfort options: smooth or snap turning, in-body camera, avatar body
- Guided installer with exact dependency hash checks and side-by-side updates
- Isolated install: original game and saves are never modified

Known limitations: not yet verified end-to-end in a headset; the native
Windows launcher menu is not shown in VR; saves aren't imported from the
original game.
