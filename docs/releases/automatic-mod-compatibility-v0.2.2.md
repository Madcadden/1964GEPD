## Changes in v0.2.2

Fixes the black screen after confirming **Exit** in GoldenEye 007 Plus’s **Native Test Mode**. Exit now returns to Map Maker with the current map intact. This correction requires the updated `1964.exe`; the included Mouse Injector v0.3.2 DLL is unchanged.

**Josh's [GoldenEye 007 Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus) is now supported**, including mouse controls for its Map Maker and menus. GoldenEye Plus also includes **1–4-player local co-op**, developed by Josh and the mod's contributors.

This build automatically finds GoldenEye's relocated game segment and the correct firing-rate and Disable Head Roll patch points in ROM mods. The included **Mouse Injector v0.3.2** finds mouse-control, FOV and supported reload locations. **Murk's RandomEye-zer v1.1 remains supported**. Invalid or ambiguous matches are skipped, and the modern-build window background fix is retained.

## Features retained from v0.2

- Supports GoldenEye Plus detection, copied game code, FOV and player firing-rate patches.
- Adds Map Maker free-camera mouse look and mouse selection in the Basic/Advanced chooser and editor menu.
- Adds mouse navigation to GoldenEye 007 Plus's Map Maker editing pause menus, plus multiplayer and confirmation menus that use directional input.
- Adds configurable D-pad and L/R shoulder bindings. Existing settings migrate without resetting your controls or FOV.
- Makes R usable for Plus's native interact/reload action during gameplay; E retains its native action.
- Improves Perfect Dark FOV/settings discovery and automatically locates its 60 FPS and head-roll controls. The menu now labels Disable Head Roll for GE/PD.
- Prepares relocated ROM code before the mod loader, applies later code updates on the emulation thread and restores owned patches across Stop/Play.

## Map Maker controls

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

The active tool determines the D-pad action, including module, layer or texture selection. Click either side of Material, Music and Grid Size values to adjust them. Buttons can be remapped in Input Settings. **Keyboard R remains reload** during gameplay, separate from the N64 R shoulder.

The Map Maker's editing pause menus support mouse navigation. Regular gameplay watch menus, including the watch in Plus's Native Test Mode, retain keyboard/controller navigation. Other directional menus retain their existing mouse controls. Orbit mode keeps its keyboard controls. Mouse look in First Person View and Native Test Mode is unchanged.

## Compatibility

GoldenEye 007 Plus! 

**Previously tested:** GoldenEye 007 (USA), Perfect Dark (USA v1.1), Murk's RandomEye-zer v1.1, Bloodlust v1.06, King of the Hill v0.98, Cartridge Tilt, GE Stereo SFX, GE Compilation v1.1, GoldenEye Tower, Goldfinger 64, Netplay 60FPS LTK Cup Edition v1.1, Pheonaarx's Yet To Come, Project GoldenEye v2.1, RickRollEye 64, TND64 Expanded and TSWLM 64 Demo v1.

Automatic discovery supports recognized code layouts. 

Please open an issue if a mod does not work or has glitches. Include its name/version, required base ROM and what happened. Do not upload ROM files. Mod-specific bugs should also be reported to the mod's developer.

## Setup

Install this over [Graslu's original 1964 GEPD package](https://github.com/Graslu/1964GEPD/releases/tag/latest). Close 1964, back up your existing files, then extract the ZIP into your 1964 folder and replace both `1964.exe` and `plugin/Mouse_Injector.dll`. **[Mouse Injector v0.3.2](https://github.com/Madcadden/mouse-injector/releases/tag/automatic-mod-compatibility-v0.3.2) is already included.** Keep your existing settings, other plugins and save files, and cold-boot the ROM after upgrading. This ZIP is an update package for an existing installation; it does not include the complete graphics/audio plugin distribution. No ROMs are included.

[Graslu's Setup guide](https://www.youtube.com/watch?v=8mL0I__VMec) · [Graslu's 4K 60 FPS video demo](https://www.youtube.com/watch?v=rxWkLdgdcPA&t=81s)

## Credits

Thanks to **Graslu** for 1964 GEPD and its releases/guides; **Stolen and Carnivorous** for the original Mouse Injector/GEPD work and rewrite; **Joel Middendorf (schibo) and Rice** for 1964 0.8.5; **Catherine Reprobate (NeonNyan), HackBond and Graslu** for later PD/decomp work; and **Ryan C. Gordon and the ManyMouse contributors**.

Thanks to **Josh (Joshua-1248)** and the contributors to **GoldenEye 007 Plus** for the mod, Map Maker and expanded co-op.

Auto-Mod Edition changes by **Jamie McCadden** ( ϓØŁØ ֆШΔǤǤΞƝŞ )

Thanks also to every mod, plugin and texture-pack creator!

## Files

The download has been updated in place; the version remains v0.2.2.

`1964GEPD-Automatic-Mod-Compatibility-v0.2.2.zip` includes `1964.exe`, `plugin/Mouse_Injector.dll`, installation notes and checksums.

**SHA-256**

- ZIP: `4e32c16672fdc3db12469dba6be26cebbd81123f75818f48fad4cc6d4ef6da00`
- `1964.exe`: `b7369abecb1c65f994e1a3e907ec5f1ffdbf3013e92d4d4613d0632b02b7c700`
- `plugin/Mouse_Injector.dll`: `9d1c3df81c70601ff9bb7e7dd97c8cfa641ef3104f844d2c4d6e727468c798fd`

[Source](https://github.com/Madcadden/1964GEPD/tree/automatic-mod-compatibility) · [Emulator build source](https://github.com/Madcadden/1964GEPD/tree/1f75fec4c66a8348f67763c47696c5d375c5d993) · [Updated injector source](https://github.com/Madcadden/mouse-injector/tree/fecf88b0059531c42c73965514f9f7907a17d0a8)

