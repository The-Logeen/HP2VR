# Steam Frame bindings and aim calibration

This is a Windows PCVR/OpenVR mod running through SteamVR, not a native headset app.
Frame support is a release candidate; hardware acceptance remains pending.

## Setup

1. Extract the entire ZIP. Run Install-HP2VR.cmd; use Full setup guide for dependencies.
2. Select a NEW destination. Optionally select a supported previous installation under Update from to migrate saves/settings without overwriting it.
3. Start PC SteamVR with both controllers connected, then the NEW installed Start-HP2VR.cmd.
4. In SteamVR's VR controller-binding settings select the running HP2VR app and Frame controllers. Select **HP2VR - Steam Frame (experimental)**. SteamVR selects defaults by controller type, but a custom binding can override them. Do not use a flat-game Steam Input gamepad layout.
5. After leaving the SteamVR dashboard, release all triggers/grips/buttons and center sticks before testing; the mod requires neutral inputs to rearm after focus changes.

## Included bindings

| Frame control | Action |
| --- | --- |
| Left stick | Movement |
| Right stick | Turn using configured smooth/snap mode |
| Right trigger | Menu selection; equipped wand: hold to charge/aim, release to cast |
| Left trigger | Jump |
| Left/right grip click | Corresponding hand grab; use at hip to draw wand |
| Left View | Game menu |
| Hold right grip, press left View | VR settings |
| Right A | Skip-dialog action, subject to existing contextual gameplay behavior |
| Left/right raw tracked pose | Corresponding hand pose |
| Left/right haptic output | Corresponding hand vibration |

Profile: `payload/VR/bindings_hp2_frame_controller.json`, installed as
`runtime/VR/bindings_hp2_frame_controller.json`, registered as `frame_controller`.
Unlisted buttons have no additional binding in this profile. No D-pad center click
is assumed. Cast and jump are analog vector1 actions driven by trigger pull;
a digital click remap alone is not equivalent. Grabs use click, not just touch.
Pose bindings are required in addition to buttons.

## Independent pointer and wand calibration

Open VR settings, then OTHER SETTINGS four times from the first page. Touch's
shortcut remains right grip + Y. Pointer pitch/yaw adjusts the menu ray; wand
pitch/yaw adjusts visible wand and spell aim independently. Positive pitch raises
the direction; positive yaw turns toward controller-local right. Left half of a
setting decreases, right half increases, in 2-degree steps from -90 to +90.
Reset all aim angles restores zero.

Start at zero and adjust one axis at a time in small increments. No fixed +25 or
-40 degree correction is established for Frame. Off-panel aiming can hide a
pointer without tracking being broken. Angles cannot repair missing pose/actions.
These settings are per installation, NOT per controller model: reset/recalibrate
when switching controllers. Movement, hip position and raw hand tracking are not rotated.

### Recovery when the pointer cannot reach settings

Close HP2VR and back up `runtime/System/HP2VRSettings.ini` in the NEW installation.
In Notepad update these keys under the existing `[VR]` section, without duplicate
keys/sections. Zero restores old angle behavior; save and restart.

```ini
WandPitchDegrees=0
WandYawDegrees=0
PointerPitchDegrees=0
PointerYawDegrees=0
```

If calibrating through the file, change only one pointer angle a few degrees per attempt.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No tracked hands | SteamVR tracking, selected binding and both pose actions; angles cannot fix this |
| Hands track, pointer appears at odd angles | Pointer calibration and active profile; test both menus |
| Pointer visible but cannot click | Right trigger pull action and neutral-input rearming after dashboard |
| Sound but no spell effect | Equip wand, hold on valid target, fully release; verify analog trigger returns to zero |
| Wand differs from pointer | Calibrate separately; independence is intentional |
| Settings appear ineffective | Confirm launcher and settings belong to the NEW installation |

Report SteamVR version, headset/controller models, active binding name/exported
JSON, whether both hands track, trigger activity/return to zero in SteamVR's
input test, all four angles and `runtime/VR/HP2VRCompanion.log` if present.
Describe charging versus release separately. Review personal paths before sharing.
Do not disable antivirus or change machine-wide execution policy.

## Acceptance test and rollback

1. Full stationary main menu, pointer and right-trigger New/Load selection.
2. View and grip+View open the intended menus; both can be selected and closed.
3. Draw wand from hip; verify visual alignment, held charge/rune and release spell effect on a known valid target.
4. Move, strafe, turn and jump; no unintended movement while using menus.
5. Adjust each angle independently; relaunch for persistence and test reset.
6. If available, test previously working controllers with zero offsets.

Keep the old installation. Roll back by closing this copy and using the old
launcher; restore previous SteamVR binding if necessary. Saves do not synchronize
between copies. Package/build tests are not substitutes for this headset test.

## References

- [Valve Frame input](https://partner.steamgames.com/doc/steamhardware/steamframe/input)
- [Valve OpenVR input profiles](https://github.com/ValveSoftware/openvr/wiki/Input-Profiles)
- [Microsoft execution policies](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies/)
