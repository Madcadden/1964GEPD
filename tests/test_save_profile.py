#!/usr/bin/env python3
"""Run the production save detector/policy with bounded, generated ROMs.
Optional real ROM paths are read locally; no ROM or save data is published.
"""
from pathlib import Path
import ctypes, hashlib, json, re, subprocess, sys, tempfile
root = Path(__file__).resolve().parents[1]
header = (root/'ge_save_profile.h').read_text()
source = (root/'1964ini.c').read_text()
policy = source.split('/* BEGIN VERIFIED PLUS SAVE POLICY',1)[1].split('/* END VERIFIED PLUS SAVE POLICY */',1)[0]
policy = '/* BEGIN VERIFIED PLUS SAVE POLICY' + policy
ini = (root/'1964ini.h').read_text()
enums = '\n'.join(re.findall(r'enum (?:SAVE_TYPE|GAMESAVETYPE|SAVETYPE|EEPROMSIZE)\s*\{.*?\};',ini,re.S))
assert len(enums) > 100
patterns = {}
for name,body in re.findall(r'static const GE_SAVE_WORD geSave(\w+)\[\] = \{(.*?)\};',header,re.S):
    patterns[name] = [(int(a,16),int(b,16)) for a,b in re.findall(r'\{0x([0-9A-F]+)U,0x([0-9A-F]+)U\}',body)]
def put(b,at,v): b[at:at+4] = (v & 0xffffffff).to_bytes(4,'little')
def call(ram): return 0x0c000000 | ((ram & 0x0fffffff) >> 2)
def fixture(driver=0x12000, resident=0x10000, osprobe=0x16000):
    b=bytearray(max(driver+204,resident+0x158,osprobe+152,0x20000))
    put(b,0,0x80371240)
    places={'Probe':driver,'Transfer':driver+36,'JoyProbe':resident,'JoyLong':resident+0xc0,'OSProbe':osprobe}
    for name,at in list(places.items())+[('JoyLong',resident+0x10c)]:
        for i,(word,mask) in enumerate(patterns[name]):put(b,at+4*i,word)
    ram = lambda at: 0x80000000 + at - 0x8000
    put(b,driver+8,call(ram(resident)))
    put(b,driver+36+108,call(ram(resident+0xc0)))
    put(b,driver+36+132,call(ram(resident+0x10c)))
    put(b,resident+8,call(0x80001000));put(b,resident+28,call(0x80002000))
    put(b,resident+16,0x3c048006);put(b,resident+24,0x24840070)
    put(b,resident+20,call(ram(osprobe)))
    for i,at in enumerate((resident+0xc0,resident+0x10c)):
        put(b,at+16,call(0x80001000));put(b,at+48,call(0x80002000))
        put(b,at+24,0x3c048006);put(b,at+28,0x24840070)
        put(b,at+40,call(0x80003000+i*0x100))
    return b,places
with tempfile.TemporaryDirectory() as td:
    td=Path(td)
    c=td/'test.c'
    c.write_text('#include "'+str(root/'ge_save_profile.h')+'"\n'+enums+'''
struct { const unsigned char *ROM_Image; } gMemoryState;
unsigned int gAllocationLength;
struct { int Save_Type, Eeprom_size; } currentromoptions;
int detect(const unsigned char *r, unsigned int n) { return GEPlusHas16KSaveDriver(r,n); }
int apply(const unsigned char *r, unsigned int n, int save, int eep) {
gMemoryState.ROM_Image=r;gAllocationLength=n;
currentromoptions.Save_Type=save;currentromoptions.Eeprom_size=eep;
'''+policy+'''
return currentromoptions.Save_Type*4 + currentromoptions.Eeprom_size;
}
''')
    subprocess.run(['cc','-std=c89','-pedantic','-Wall','-Wextra','-Werror','-O2','-shared','-fPIC',str(c),'-o',str(td/'test.so')],check=True)
    lib=ctypes.CDLL(str(td/'test.so'))
    lib.detect.argtypes=[ctypes.c_void_p,ctypes.c_uint];lib.detect.restype=ctypes.c_int
    lib.apply.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_int];lib.apply.restype=ctypes.c_int
    checks=0
    def detect(b,expect):
        global checks
        buf=ctypes.create_string_buffer(bytes(b)); actual=lib.detect(buf,len(b));checks+=1
        assert actual==expect,(checks,actual,expect)
    good,places=fixture();detect(good,1)
    for save in range(7):
        for eep in range(4):
            buf=ctypes.create_string_buffer(bytes(good)); assert lib.apply(buf,len(good),save,eep)==6*4+3;checks+=1
            bad=bytearray(good);put(bad,places['Probe']+20,0x384e0001)
            buf=ctypes.create_string_buffer(bytes(bad));assert lib.apply(buf,len(bad),save,eep)==save*4+eep;checks+=1
    for name,at in list(places.items())+[('JoyLong',places['JoyLong']+0x4c)]:
        for i,(word,mask) in enumerate(patterns[name]):
            b=bytearray(good);v=int.from_bytes(b[at+4*i:at+4*i+4],'little')
            bit=mask & -mask;put(b,at+4*i,v^bit);detect(b,0)
    for at in (places['Probe']+8,places['Transfer']+108,places['Transfer']+132,places['JoyProbe']+20,
               places['JoyLong']+16,places['JoyLong']+28,places['JoyLong']+48):
        b=bytearray(good);put(b,at,int.from_bytes(b[at:at+4],'little')+1);detect(b,0)
    b=bytearray(good);b[0x10:0x40]=bytes([0xa5])*48;detect(b,1)
    b,unused=fixture(0x310000,0x510000,0x516000);detect(b,1)
    b=bytearray(good);b.extend(bytes(204));b[-204:]=good[places['Probe']:places['Probe']+204];detect(b,0)
    b=bytearray(good);delta=0x20000;b.extend(bytes(delta));b[0x10000+delta:0x18000+delta]=good[0x10000:0x18000];detect(b,0)
    for n in (0,1,3,4,63,0x1000,0x10ff,len(good)-1):detect(good[:n],0)
    assert lib.detect(None,0x20000)==0;checks+=1
    assert lib.detect(ctypes.create_string_buffer(bytes(good)),0x4000004)==0;checks+=1
    for at in (places['Probe']+8,places['Transfer']+108,places['JoyProbe']+20):
        for badtarget in (0x807ffffc,0x8ffffffc,0x80000000):
            b=bytearray(good);put(b,at,call(badtarget));detect(b,0)
    actual=[]
    for path in sys.argv[1:]:
        raw=Path(path).read_bytes();b=bytearray(raw)
        if b[:4]==b'\x80\x37\x12\x40':
            for k in range(4):b[k::4]=raw[3-k::4]
        elif b[:4]==b'\x37\x80\x40\x12':
            for k in range(4):b[k::4]=raw[k^2::4]
        assert b[:4]==b'\x40\x12\x37\x80'
        buf=ctypes.create_string_buffer(bytes(b));matched=lib.detect(buf,len(b))
        actual.append({'file':Path(path).name,'sha256':hashlib.sha256(raw).hexdigest(),'eeprom16_detected':bool(matched)})
    print(json.dumps({'synthetic_checks_passed':checks,'actual_roms':actual,'scope':'Production C detector and actual integrated options block. No interactive save-device round trip.'},indent=2))
