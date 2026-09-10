"""Generate deterministic item metadata from the pinned upstream source snapshot."""
from pathlib import Path
import xml.etree.ElementTree as ET
import csv, io, json
root=Path(__file__).resolve().parent
source=root/'provenance/Resources.resx'
data={n.attrib['name']:n.findtext('value','') for n in ET.parse(source).getroot().findall('data')}
rows=[]
weapon_content={}
for group in json.loads((root/'provenance/weapon-content.json').read_text())['files']:
    for identifier in group['ids']:
        if identifier in weapon_content and weapon_content[identifier]!=group['content']:
            raise ValueError(f'Conflicting weapon content: {identifier}')
        weapon_content[identifier]=group['content']
for category in ('Weapons','Arrows','Talismans','Armor','CraftingMaterials','UpgradeMaterials','Consumables','Sorceries','Incantations','SpiritAshes'):
    for row in csv.reader(io.StringIO(data[category])):
        if not row: continue
        if category=='Weapons':
            identifier,name=int(row[0]),row[1]
            stack,content=1,weapon_content.get(identifier,2)
            grantable=content==0
        else:
            content,identifier,name,stack=int(row[0]),int(row[1],16),row[2],int(row[3])
            grantable=category in ('Arrows','Talismans','Armor','CraftingMaterials','UpgradeMaterials','Sorceries','Incantations') and content==0
        if not 0<stack<=9999 or not 0<=identifier<=0xffffffff: raise ValueError(row)
        rows.append((identifier,name,category,stack,content,grantable))
rows.sort(key=lambda row:(row[2],row[1]))
seen={}
for row in rows:
    if row[0] in seen:
        prior=seen[row[0]]
        if row==prior: continue
        if prior[1]!=row[1]: raise ValueError(f'Conflicting names: {prior} / {row}')
        # Some weapon tables also list ammunition. Retain the explicitly
        # classified ammunition entry rather than an unknown-content weapon.
        if prior[2]=='Weapons': seen[row[0]]=row
        elif row[2]!='Weapons': raise ValueError(f'Conflicting category: {prior} / {row}')
    else: seen[row[0]]=row
rows=sorted(seen.values(),key=lambda row:(row[2],row[1]))
lines=['// Generated from TarnishedTool 2a7a76939d3dd21c233ee4f1e5df3b2dddf0e49c (MIT).']
for identifier,name,category,stack,content,grantable in rows:
    lines.append(f'{{0x{identifier:08x}u,{json.dumps(name,ensure_ascii=True)},{json.dumps(category)},{stack},{content},{str(grantable).lower()}}},')
(root/'src/catalog.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'Generated {len(rows)} catalog records; {sum(r[-1] for r in rows)} eligible base-game records.')
