# 1964 GEPD 0.8.5 - Auto-Mod Edition

**[Download the latest Auto-Mod Edition release](https://github.com/Madcadden/1964GEPD/releases/tag/automatic-mod-compatibility-v0.1)**

1964 0.8.5 GEPD Edition with automatic compatibility support for GoldenEye 007 ROM hacks.

This branch automatically locates relocated GoldenEye game segments and the firing-rate and head-roll modification points instead of relying on fixed retail-ROM addresses. It also includes the corrected main-window and toolbar appearance.

Use it with **[Mouse Injector Automatic Mod Compatibility v0.2](https://github.com/Madcadden/mouse-injector/releases/tag/automatic-mod-compatibility-v0.2)** for automatic mouse-control and manual-reload injection-point detection.

## Development candidate: GoldenEye Plus and Perfect Dark discovery

This source adds content-based GoldenEye detection, independent player/AI/drone
patch validation, and support for code copied into expansion RAM. ROM patches
are prepared at the post-IPL game-entry boundary. Later code patches run on
the emulation thread and invalidate affected compiled blocks.

Perfect Dark's 60 FPS switch, head-roll option getter and pause flag are found
from unique instruction patterns. The head-roll menu option now covers both
games. The more invasive PD combat-boost/guard timing shim remains restricted
to the verified USA Rev 1 layout, with complete original-code checks.

Unrecognized or ambiguous instructions are skipped. This does not guarantee
support for every rewritten mod. Cold-boot the ROM after replacing both the
emulator and injector. Old save states cannot validate the new boot patches.
Source and synthetic-memory tests do not replace Windows gameplay testing.

## Credits and upstream project

This is an unofficial modification of [Graslu's 1964 GEPD Edition](https://github.com/Graslu/1964GEPD). The original GEPD release and documentation are available from [Graslu's releases page](https://github.com/Graslu/1964GEPD/releases).

1964 is Copyright (c) 1999-2002 Joel Middendorf.

The unmodified 1964 0.8.5 source code is available from [SourceForge](https://sourceforge.net/projects/schibo/files/1964%200.8.5/1964-2002-0922.zip/1964-2002-0922.zip).

> **Notice:** This emulator is intended specifically for GoldenEye 007 and Perfect Dark. Other Nintendo 64 games may receive no benefit or behave incorrectly. ROM files are not included.
