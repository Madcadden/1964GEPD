"""Integrate the save policy without changing other emulator options."""
from pathlib import Path
import hashlib

p = Path('1964ini.c')
text = p.read_text(encoding='utf-8')
block = '''\t/* BEGIN VERIFIED PLUS SAVE POLICY
\t * Runtime-only: correct even an old explicit 4Kb/SRAM ROM setting without
\t * rewriting the INI or converting/deleting any save. Keep Controller Pak
\t * available alongside EEPROM; the prior SRAM driver override stays above.
\t */
\tif(GEPlusHas16KSaveDriver(gMemoryState.ROM_Image, gAllocationLength))
\t{
\t\tcurrentromoptions.Save_Type = ANYUSED_SAVETYPE;
\t\tcurrentromoptions.Eeprom_size = EEPROMSIZE_16KB;
\t}
\t/* END VERIFIED PLUS SAVE POLICY */

'''
if '#include "ge_save_profile.h"' not in text:
    assert hashlib.sha256(text.encode()).hexdigest() == 'f91956fb055e711ad043def03cf06b8665d6afdeaef047e5f25f2db47502ef60', 'Base options source changed'
    anchor = '#include "win32/registry.h"\n'
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '#include "memory.h"\n#include "ge_save_profile.h"\n')
    anchor = '\tif(RomListSelectedEntry()->pinientry->countrycode == 0x45) // if USA ROM\n'
    assert text.count(anchor) == 1
    text = text.replace(anchor, block + anchor)
    p.write_text(text, encoding='utf-8', newline='\n')
assert text.count(block) == 1, 'Unexpected integrated policy'
assert text.index('0x90B1D709') < text.index(block), 'Older SRAM override must be preserved'
assert 'defaultoptions.Eeprom_size = EEPROMSIZE_4KB;' in text, 'General default changed'
print('Verified save policy is integrated; other options and defaults retained.')
