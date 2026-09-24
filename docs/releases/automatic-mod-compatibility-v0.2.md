## About

**Josh's [GoldenEye 007 Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus) is now supported**, including mouse controls for its Map Maker and menus. GoldenEye Plus also includes **1–4-player local co-op**, developed by Josh and the mod's contributors.

This build automatically finds GoldenEye's relocated game segment and the correct firing-rate and Disable Head Roll patch points in ROM mods. The included **Mouse Injector v0.3** finds mouse-control, FOV and supported reload locations. **Murk's RandomEye-zer v1.1 remains supported**. Invalid or ambiguous matches are skipped, and the modern-build window background fix is retained.

## Changes

- Supports GoldenEye Plus detection, copied game code, FOV and player firing-rate patches.
- Adds Map Maker free-camera mouse look and mouse selection in the Basic/Advanced chooser and editor menu.
- Adds mouse navigation to GoldenEye watch, multiplayer and confirmation menus that use directional input.
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

Watch and other directional menus respond to mouse movements and left-click. Enter still closes the main watch. Orbit mode keeps its keyboard controls; test-preview mouse controls are not added.

## Compatibility

GoldenEye Plus operation has been confirmed by user testing. The Windows builds and automated tests passed, including retail GoldenEye, the supplied Plus build and Perfect Dark USA Rev 1 code. These checks do not establish that every co-op configuration or mod works.

**Previously tested:** GoldenEye 007 (USA), Perfect Dark (USA v1.1), Murk's RandomEye-zer v1.1, Bloodlust v1.06, King of the Hill v0.98, Cartridge Tilt, GE Stereo SFX, GE Compilation v1.1, GoldenEye Tower, Goldfinger 64, Netplay 60FPS LTK Cup Edition v1.1, Pheonaarx's Yet To Come, Project GoldenEye v2.1, RickRollEye 64, TND64 Expanded and TSWLM 64 Demo v1.

Automatic discovery supports recognized code layouts. Plus's rewritten reverse-pitch and HUD/aspect patches are skipped when unverified. Its reload key uses the mod's native interact/reload action, with priority over Fire while held. Perfect Dark's legacy cursor/reload patches and invasive combat timing changes still require their verified layouts.

Online co-op with a full-screen view for each player is a possible future project; it is not included in this release.

Please open an issue if a mod does not work or has glitches. Include its name/version, required base ROM and what happened. Do not upload ROM files. Mod-specific bugs should also be reported to the mod's developer.

## Setup

Install this over [Graslu's original 1964 GEPD package](https://github.com/Graslu/1964GEPD/releases/tag/latest). Close 1964, back up your existing files, then extract the ZIP into your 1964 folder and replace both `1964.exe` and `plugin/Mouse_Injector.dll`. **[Mouse Injector v0.3](https://github.com/Madcadden/mouse-injector/releases/tag/automatic-mod-compatibility-v0.3) is already included.** Keep your settings file and cold-boot the ROM after upgrading. No ROMs are included.

[Graslu's Setup guide](https://www.youtube.com/watch?v=8mL0I__VMec) · [Graslu's 4K 60 FPS video demo](https://www.youtube.com/watch?v=rxWkLdgdcPA&t=81s)

## Credits

Thanks to **Graslu** for 1964 GEPD and its releases/guides; **Stolen and Carnivorous** for the original Mouse Injector/GEPD work and rewrite; **Joel Middendorf (schibo) and Rice** for 1964 0.8.5; **Catherine Reprobate (NeonNyan), HackBond and Graslu** for later PD/decomp work; and **Ryan C. Gordon and the ManyMouse contributors**.

Thanks to **Josh (Joshua-1248)** and the contributors to **GoldenEye 007 Plus** for the mod, Map Maker and expanded co-op.

Auto-Mod Edition changes by **Jamie McCadden** ( ϓØŁØ ֆШΔǤǤΞƝŞ )

Thanks also to every mod, plugin and texture-pack creator!

## Files

`1964GEPD-Automatic-Mod-Compatibility-v0.2.zip` includes `1964.exe` and `plugin/Mouse_Injector.dll`.

**SHA-256**

- ZIP: `12F030215E4F19A2F843D69BE4DDDD896E7750D3BFD30AB1A4756DC94911461D`
- `1964.exe`: `53C156E514E88DEEE19DD78DB5A690DE9CF38EBDD69CF93ACF04C83900761BCC`
- `plugin/Mouse_Injector.dll`: `1AF45876E2254AB75406C46D97658F8A56E81892544EA70A077BE9FB93FEF0C9`

[Source](https://github.com/Madcadden/1964GEPD/tree/automatic-mod-compatibility) · [Release source](https://github.com/Madcadden/1964GEPD/tree/automatic-mod-compatibility-v0.2) · [Mouse Injector source](https://github.com/Madcadden/mouse-injector/tree/automatic-mod-compatibility)
