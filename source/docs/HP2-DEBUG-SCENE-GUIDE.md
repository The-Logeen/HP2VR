# HP2 private-runtime debug and scene guide

This guide applies only to the isolated `hp2-vr-prototype` runtime.  Start a
normal game from its main menu before jumping so Harry, the save slot, and
campaign globals exist.

## Native shortcut window

HP2 implements the selector directly in `HGame.HPConsole`:

1. Debug mode must already be enabled in the private `Game.ini`.
2. Once Harry is controllable, press **F4 by itself**.  On a laptop whose
   function row controls brightness/volume, use **Fn+F4** only to produce the
   actual F4 key.  Do not use Alt+F4; Windows will close the game.
3. The native window is titled `Shortcut Window`.  Select a level and activate
   its launch control.  Pressing F4 while the window is active closes it.

Do not bind `F4=ShortCut` in `User.ini`.  HP2 already handles F4 internally;
adding a second command can open and immediately close the same window.

F7 calls HP2's debug-mode toggle.  Avoid it during analysis because turning
debug mode off disables the F4 selector.

## Console travel fallback

The console key is key code 192, normally the grave/tilde key below Escape.
When a console input line is visible, the standard travel form is:

```text
open Ch1Rictusempra.unr
```

Direct travel is for inspection.  It can omit campaign flags, inventory, or a
director setup that normal progression supplies, so record whether a test used
normal play, the F4 window, or a console `open` command.

## Useful native levels

| Purpose | Map/console argument |
| --- | --- |
| Rictusempra lesson | `Ch1Rictusempra.unr` |
| Skurge lesson | `Ch2Skurge.unr` |
| Diffindo lesson | `Ch3Diffindo.unr` |
| Spongify lesson | `Ch4Spongify.unr` |
| Quidditch lesson | `Quidditch_Intro.unr` |
| Quidditch match | `Quidditch.unr` |
| Dueling | `Arena.unr` |
| Whomping Willow | `Adv1Willow.unr` |
| Dungeon quest | `Adv3DungeonQuest.unr` |
| Greenhouse | `Adv4Greenhouse.unr` |
| Polyjuice/Goyle | `Adv6Goyle.unr` |
| Slytherin common room | `Adv7SlythComRoom.unr` |
| Forbidden Forest | `Adv8Forest.unr` |
| Aragog | `Adv9Aragog.unr` |
| Corridor of Secrets A | `Adv11aCorridor.unr` |
| Corridor of Secrets B | `Adv11bSecrets.unr` |
| Chamber of Secrets | `Adv12Chamber.unr` |
| Entry Hall hub | `Entryhall_hub.unr` |
| Grand Staircase hub | `Grandstaircase_hub.unr` |
| Grounds hub | `Grounds_Hub.unr` |
| Grounds at night | `Grounds_Night.unr` |
| Gryffindor challenge | `Ch7Gryffindor.unr` |

The native selector also includes Privet Drive, a test level, Bean Reward Room,
Gold Wizard Card, and other hub entries.  Prefer normal campaign entry when
validating rewards, story transitions, or save progression.
