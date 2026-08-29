# 1964 GEPD Edition

* https://github.com/Graslu/1964GEPD/releases

1964 0.8.5, modified for use with the Mouse Injector for GE/PD

## Random-Eye-zer compatibility fork

This fork adds support for Murk-17's
[Random-Eye-zer: The True Randomizer](https://www.moddb.com/mods/random-eye-the-true-randomizer)
while preserving the original retail GoldenEye and Perfect Dark behavior.

Changes in this fork:

- Locates a recompiled GoldenEye game segment dynamically instead of assuming
  the retail ROM offset. This allows RandomEye's relocated executable to boot.
- Maps `GE Firing Rate Hack` and `GE Disable Head Roll` to RandomEye's verified
  addresses when ROM CRC `B72EDF71/C22234D1` is loaded.
- Validates the original instructions before applying those optional hacks.
- Keeps the stock GEPD overclock system; no experimental 120-FPS timing patch
  is included.
- Includes `1964-modern.vcxproj` for Visual Studio 2022 Win32 builds.

No ROMs, ROM patches, game assets, or save files are included.

### Building with Visual Studio 2022

Install the Desktop development with C++ workload. Open a Visual Studio
Developer PowerShell, change to this repository, and run:

```powershell
msbuild .\1964-modern.vcxproj /t:Rebuild /m `
  /p:Configuration=Release `
  /p:Platform=Win32
```

The executable will be written to:

```text
build-vs2022\Release\1964.exe
```

Copy the normal GEPD plugin folder and configuration files beside that
executable. For RandomEye mouse support, use the matching RandomEye-compatible
Mouse Injector fork.

Please note: The only games that benefit from this branch are GoldenEye 007 and Perfect Dark for Nintendo 64. Any other game will have negative or no impact at all. ROM files not included.

# Copyright
1964 is Copyright (c) 1999-2002 Joel Middendorf

Unmodified 1964 0.8.5 source code can be found at https://sourceforge.net/projects/schibo/files/1964%200.8.5/1964-2002-0922.zip/1964-2002-0922.zip
