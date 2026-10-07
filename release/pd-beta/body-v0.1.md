## Current Auto-Mod Edition — Perfect Dark beta support

This download now contains the working Perfect Dark beta build: **1964 GEPD Auto-Mod Edition v0.2.2 with Mouse Injector v0.3.2**. Both the v0.2.2 release and the older v0.1 release URL serve the same current binaries. The v0.1 asset keeps its old filename so existing download links remain useful.

### What's included

- Supports the **Perfect Dark NTSC 6.4 and PAL 28.7 debug/beta layouts**, including normal and cursor mouse aiming, EyeSpy pitch, and separate interaction/reload controls.
- Adds the matching controller-polling speed patches and PAL Perfect Dark task handling. **PAL keeps its native timing.**
- Repairs the recognized original **EC development-cart header and two development-board reads in the loaded copy**. The ROM file on disk stays unchanged.
- Applies the correct Perfect Dark EEPROM capacity to PAL and preserves ROM options when EC repair changes the loaded identity.
- **GoldenEye 007 Plus 2.4 Map Maker free-camera mouse look**, including the confirmed free-fly fix.
- Separate **E = interact/open doors** and **R = reload** with the usual gameplay bindings, including the supported Plus and Perfect Dark beta layouts.
- Plus Map Maker texture-scrolling fixes, including backwards scrolling with J and affected IA4 textures, and the Native Test Mode exit fix.
- Automatic discovery of supported GoldenEye controls, FOV, reload and relocated code; Perfect Dark settings, FOV, 60 FPS and head-roll discovery.
- Configurable D-pad and L/R shoulder bindings, existing INI settings, and normal W+S input.

Recognized GoldenEye engine mods use GLideN64's existing **GOLDENEYE** graphics profile and texture-pack/cache name. The split-screen correction for the recognized **GLideN64_2020** build is retained; keep framebuffer emulation enabled. Texture checksums must still match the chosen texture pack. The depth adapter checks the graphics DLL before applying its fix; other plugin builds are not assumed to have the same layout.

### Install

Start with [Graslu's original 1964 GEPD package](https://github.com/Graslu/1964GEPD/releases/tag/latest). Close 1964, back up your current files, then extract this update over that folder. Replace **both `1964.exe` and `plugin/Mouse_Injector.dll`**. Select **Mouse Injector** as the input plugin and cold-boot the ROM after upgrading; an old save state can restore old code.

Keep your existing INI files, other plugins and saves. This is an update package, not the complete graphics/audio plugin distribution. No ROMs, game assets or user settings are included.

[Graslu's setup guide](https://www.youtube.com/watch?v=8mL0I__VMec) · [4K 60 FPS video demo](https://www.youtube.com/watch?v=rxWkLdgdcPA&t=81s)

### Map Maker controls

Defaults with the WASD input profile, for Josh's **[GoldenEye 007 Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus)**:

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

The active tool determines the D-pad action, including module, layer or texture selection. Click either side of Material, Music and Grid Size values to adjust them. Bindings can be changed in Input Settings. Keyboard **R is reload during gameplay**, separate from the N64 R shoulder.

Mouse navigation while paused applies to Map Maker editing menus. Regular GoldenEye gameplay watch menus, including Plus's Native Test Mode, use keyboard/controller navigation. Front-end, multiplayer and confirmation menus retain their existing mouse controls. Orbit mode keeps its keyboard controls. Free-camera mouse look pauses while the editor menu is open.

GoldenEye Plus itself provides expanded 1–4-player local co-op. Online co-op is not included here.

### Compatibility

The Perfect Dark beta build and Plus free-fly fix have been confirmed working in gameplay. Automated checks also cover ROM signatures, input routing, patch ownership and source/build correspondence. Automatic discovery applies to recognized code layouts; missing or ambiguous matches are skipped. PD debug menus use keyboard/controller navigation.

The source ZIPs are the exact build snapshots. `Mouse-Injector-FreeFly-F3-Source.zip` is retained as a legacy filename for the current injector source, also provided as `Mouse-Injector-PD-Beta-Source.zip`. GitHub's automatically generated source archives below still refer to the historical tags; use the attached build-source ZIPs or the exact source links below.

### Files and source

The update contains `1964.exe` and `plugin/Mouse_Injector.dll`. The standalone matching injector is also available from the [Mouse Injector release](https://github.com/Madcadden/mouse-injector/releases/tag/automatic-mod-compatibility-v0.3.2).

SHA-256:

- Update ZIP: `c57d9ecacb5bc1c3ef86770cf291a79aba470e5d721f0d308fad512597351ab5`
- `1964.exe`: `3daaef3916c74d48f1de0dff1606ca3c67d316400c70ecca0723b16575cc9aad`
- `plugin/Mouse_Injector.dll`: `968c4b9ae71f98733ef9a6523086df0890d2267c33795747eaa2bdb175ca0929`

[Emulator build source](https://github.com/Madcadden/1964GEPD/tree/bf6b09853378a121719179128f4cc8dcface9957) · [Injector build source](https://github.com/Madcadden/mouse-injector/tree/eaa00c1da6e3e2923d6599a16579db53af7db5a7)

### Credits

Thanks to **Graslu** for 1964 GEPD and its releases/guides; **Stolen and Carnivorous** for the original Mouse Injector/GEPD work and rewrite; **Joel Middendorf (schibo) and Rice** for 1964 0.8.5; **Catherine Reprobate (NeonNyan), HackBond and Graslu** for later Perfect Dark/decomp work; **Ryan Dwyer** and the Perfect Dark decompilation contributors; and **Ryan C. Gordon and the ManyMouse contributors**.

Thanks to **Josh (Joshua-1248)** and the **GoldenEye 007 Plus** contributors for the mod, Map Maker and expanded local co-op.

Auto-Mod Edition changes by **Jamie McCadden** ( ϓØŁØ ֆШΔǤǤΞƝŞ ). Thanks also to the mod, plugin and texture-pack creators.
