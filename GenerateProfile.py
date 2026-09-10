from pathlib import Path
import struct,hashlib
root=Path(__file__).resolve().parent
data=(root.parent/'Game/eldenring.exe').read_bytes()
assert hashlib.sha256(data).hexdigest() in {
    'd1a84083c6c7c7902162ff098f7d86812839aa6b3575959398857e539c488134',
    '1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891',
}
# Pin every RVA used by runtime.cpp to the explicit shared 2.7.0/2.7.1 arms.
import re
source=(root/'provenance/Offsets-2.7.1.cs').read_text(encoding='utf-8-sig')
required={'WorldChrMan.Base':0x3D69FF8,'MenuMan.Base':0x3D6F820,'ChrDbgFlags.Base':0x3D6A210,
          'GameDataMan.Base':0x3D61F98,'CSFlipperImp.Base':0x458DB58,'MapItemManImpl.Base':0x3D6BAC0,
          'Functions.ItemSpawn':0x561400,'Functions.GetPlayerItemQuantityById':0x785E50,'Functions.GiveRunes':0x25E0E0}
for symbol,rva in required.items():
    block=re.search(re.escape(symbol)+r'\s*=\s*moduleBase\s*\+\s*Version\s*switch\s*\{(.*?)\};',source,re.S)
    assert block and re.search(r'Version2_7_0 or Version2_7_1\s*=>\s*0x'+format(rva,'X')+r'\s*,',block[1]),symbol
pe=struct.unpack_from('<I',data,0x3c)[0]
num=struct.unpack_from('<H',data,pe+6)[0]
opt=struct.unpack_from('<H',data,pe+20)[0]
sections=pe+24+opt
def read(rva,n):
    for i in range(num):
        off=sections+40*i
        virtual_size,virtual_address,raw_size,raw_ptr=struct.unpack_from('<IIII',data,off+8)
        if virtual_address<=rva<virtual_address+raw_size:
            return data[raw_ptr+rva-virtual_address:raw_ptr+rva-virtual_address+n]
    raise ValueError(rva)
lines=['// Entry bytes read from the exact locally fingerprinted executable.']
for name,rva in [('itemSpawn',0x561400),('itemQuantity',0x785e50),('giveRunes',0x25e0e0)]:
    b=read(rva,16)
    lines.append(f'constexpr std::array<unsigned char,16> {name}Header{{'+','.join(hex(v) for v in b)+'};')
(root/'src/engine_headers.inc').write_text('\n'.join(lines)+'\n')
print('Recorded three entry-point guards for the pinned executable.')
