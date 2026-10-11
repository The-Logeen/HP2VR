# Changelog

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
