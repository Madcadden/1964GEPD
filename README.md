# 1964 GEPD 0.8.5 - Auto-Mod Edition

**[Download Auto-Mod Edition v0.2.2](https://github.com/Madcadden/1964GEPD/releases/tag/automatic-mod-compatibility-v0.2.2)** — includes Mouse Injector v0.3.2.

1964 0.8.5 GEPD Edition with automatic compatibility support for GoldenEye 007 and Perfect Dark ROM mods.

This branch automatically locates relocated GoldenEye game segments and the firing-rate and head-roll modification points instead of relying on fixed retail-ROM addresses. It also includes the corrected main-window and toolbar appearance.

The included **[Mouse Injector v0.3.2](https://github.com/Madcadden/mouse-injector/releases/tag/automatic-mod-compatibility-v0.3.2)** adds FOV and input discovery, Map Maker mouse controls, and configurable N64 D-pad and shoulder buttons.

## Map Maker pause-menu mouse navigation

Pause-menu mouse navigation is enabled **only while editing in GoldenEye 007 Plus's Map Maker**. Regular gameplay watch menus use keyboard/controller navigation in every GoldenEye ROM, including Plus's Native Test Mode. Front-end, multiplayer and confirmation menu mouse controls are unchanged.

The v0.2.2 download has been updated in place. This update fixes the black screen after confirming **Exit** in GoldenEye 007 Plus’s **Native Test Mode**. Exit now returns to Map Maker with the current map intact. Replace `1964.exe` with this updated build and cold-boot the ROM. The included Mouse Injector v0.3.2 DLL is unchanged.

## GoldenEye 007 Plus support

This release supports Josh's **[GoldenEye 007 Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus)**, including mouse look in the Map Maker free camera and mouse navigation in its editor menus. The mod itself includes expanded **1–4-player local co-op**. Thanks to Josh and the GoldenEye Plus contributors for their work.

Online co-op with a full-screen view for each player is a possible future project; it is not included in this release.

### Map Maker controls

Defaults with the WASD input profile:

| Action | Mouse / keyboard |
|---|---|
| Look / move in free camera | Mouse / WASD |
| Place or draw | Left-click / hold left-click |
| Delete | E |
| Rotate (N64 L shoulder) | U |
| 2× movement speed (N64 R shoulder) | Hold O or right-click |
| D-pad Up / Down / Left / Right | I / K / J / L |
| Open editor menu | Enter |
| Editor menus | Hover and left-click; right-click goes back |

The active tool determines the D-pad action, including module, layer or texture selection. Click either side of Material, Music and Grid Size values to adjust them. All added buttons can be remapped in Input Settings. Keyboard **R remains reload** during gameplay; it is separate from the N64 R shoulder.

The Map Maker's editing pause menus support mouse navigation. Regular gameplay watch menus, including the watch in Plus's Native Test Mode, retain keyboard/controller navigation. Other directional menus retain their existing mouse controls. Orbit mode keeps its keyboard controls. Mouse look in First Person View and Native Test Mode is unchanged.

## Install

Start with [Graslu's original 1964 GEPD package](https://github.com/Graslu/1964GEPD/releases/tag/latest). Close 1964 and back up your current files, then extract the v0.2.2 ZIP over that folder. Replace **both** `1964.exe` and `plugin/Mouse_Injector.dll`; the matching injector is already included. Keep your existing settings, other plugins and save files, and cold-boot the ROM after upgrading. This ZIP is an update package; it does not include the complete graphics/audio plugin distribution.

## GoldenEye Plus and Perfect Dark discovery

This source adds content-based GoldenEye detection, independent player/AI/drone
patch validation, and support for code copied into expansion RAM. ROM patches
are prepared at the post-IPL game-entry boundary. Later code patches run on
the emulation thread and invalidate affected compiled blocks.

Perfect Dark's 60 FPS switch, head-roll option getter and pause flag are found
from unique instruction patterns. The head-roll menu option now covers both
games. The more invasive PD combat-boost/guard timing shim remains restricted
to the verified USA Rev 1 layout, with complete original-code checks.

Unrecognized or ambiguous instructions are skipped. This does not guarantee
support for every rewritten mod. Old save states can retain old code and do not
validate the new boot patches. GoldenEye Plus operation has been confirmed by
user testing; automated tests also cover the supplied retail GoldenEye, Plus and
Perfect Dark USA Rev 1 code. This is not a claim that every co-op configuration
or third-party mod has been tested.

See the [v0.2.2 release notes](docs/releases/automatic-mod-compatibility-v0.2.2.md) for changes and file hashes.

## Credits and upstream project

This is an unofficial modification of [Graslu's 1964 GEPD Edition](https://github.com/Graslu/1964GEPD). The original GEPD release and documentation are available from [Graslu's releases page](https://github.com/Graslu/1964GEPD/releases).

1964 is Copyright (c) 1999-2002 Joel Middendorf.

The unmodified 1964 0.8.5 source code is available from [SourceForge](https://sourceforge.net/projects/schibo/files/1964%200.8.5/1964-2002-0922.zip/1964-2002-0922.zip).

> **Notice:** This emulator is intended specifically for GoldenEye 007 and Perfect Dark. Other Nintendo 64 games may receive no benefit or behave incorrectly. ROM files are not included.
