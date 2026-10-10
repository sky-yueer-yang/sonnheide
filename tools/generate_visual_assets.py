#!/usr/bin/env python3
"""Original 24-cell pixel symbols, crisp nearest replication; stdlib, Python3.9+."""
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/generated/ui/icons'
# Positive filled solids and negative spaces. Coordinates are in a 128-square.
def rect(x,y,w,h): return ('rect',x,y,w,h)
def circle(x,y,r): return ('circle',x,y,r)
def poly(*p): return ('poly',p)
SHAPES={
'world':([circle(64,64,42)], [rect(20,58,88,6),rect(59,20,10,88)]),
'earth':([circle(64,64,43)], [poly((29,39),(50,29),(71,37),(63,53),(44,58),(38,78),(27,64)),poly((73,56),(101,60),(94,84),(79,98),(68,80))]),
'camera':([rect(24,43,80,51),poly((40,43),(47,29),(76,29),(83,43))],[circle(64,67,19)]),
'blank':([poly((29,20),(82,20),(102,40),(102,107),(29,107))],[poly((80,24),(80,43),(99,43)),rect(44,63,43,6),rect(44,79,30,6)]),
'people':([circle(49,39,14),circle(86,47,11),poly((22,93),(25,67),(37,56),(59,56),(73,68),(75,103),(23,103)),poly((76,68),(98,65),(108,77),(108,98),(82,98))],[]),
'buildings':([poly((19,60),(64,22),(109,60)),rect(28,59,73,45)], [rect(55,73,18,32),rect(38,67,9,12),rect(82,67,9,12)]),
'terrain':([poly((12,103),(47,28),(74,73),(91,43),(116,103))],[poly((42,42),(49,28),(60,51),(50,47))]),
'economy':([rect(23,45,82,61),poly((23,45),(38,25),(90,25),(105,45))],[rect(61,28,7,76),rect(37,72,16,6),rect(75,72,16,6)]),
'laws':([rect(30,100,68,6),rect(60,28,8,72),circle(64,26,9),rect(25,45,78,6),poly((17,76),(30,52),(43,76)),poly((85,76),(98,52),(111,76))],[rect(24,73,12,4),rect(92,73,12,4)]),
'settings':([circle(64,64,34)]+[poly((64+42*math.cos(a-.14),64+42*math.sin(a-.14)),(64+49*math.cos(a-.14),64+49*math.sin(a-.14)),(64+49*math.cos(a+.14),64+49*math.sin(a+.14)),(64+30*math.cos(a+.18),64+30*math.sin(a+.18)),(64+30*math.cos(a-.18),64+30*math.sin(a-.18))) for a in [i*math.pi/4 for i in range(8)]],[circle(64,64,16)]),
'pause':([rect(38,30,18,68),rect(72,30,18,68)],[]),
'play':([poly((39,27),(102,64),(39,101))],[]),
'save':([poly((27,23),(91,23),(105,37),(105,106),(27,106))],[rect(43,23,39,29),rect(43,70,45,36)]),
'load':([poly((19,43),(56,43),(65,55),(109,55),(104,101),(20,101)),rect(58,24,9,41),poly((44,51),(63,72),(83,51))],[rect(32,83,60,5)]),
'back':([poly((58,27),(22,63),(58,100),(64,93),(42,70),(108,70),(108,57),(42,57),(64,34))],[]),
'close':([poly((30,21),(107,98),(98,107),(21,30)),poly((98,21),(107,30),(30,107),(21,98))],[]),
'create':([rect(59,23,10,82),rect(23,59,82,10)],[]),
'check':([poly((20,65),(45,91),(108,35),(100,27),(46,77),(28,57))],[]),
'audio':([rect(24,51,22,27),poly((45,51),(74,28),(74,101),(45,78)),poly((84,44),(92,37),(104,50),(109,64),(104,78),(92,91),(84,84),(94,73),(98,64),(94,55))],[]),
'help':([circle(64,64,43)],[circle(64,90,5),poly((45,43),(54,34),(73,34),(85,45),(85,56),(70,69),(70,77),(60,77),(60,65),(75,53),(75,47),(68,43),(59,43),(53,50))]),
}
SHAPES['tools']=SHAPES['settings']

def inside(s,x,y):
    if s[0]=='rect': return s[1]<=x<s[1]+s[3] and s[2]<=y<s[2]+s[4]
    if s[0]=='circle': return (x-s[1])**2+(y-s[2])**2<=s[3]**2
    p=s[1];hit=False;j=len(p)-1
    for i in range(len(p)):
        a,b=p[i],p[j]
        if ((a[1]>y)!=(b[1]>y)) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]: hit=not hit
        j=i
    return hit

def chunk(kind,body): return struct.pack('>I',len(body))+kind+body+struct.pack('>I',zlib.crc32(kind+body)&0xffffffff)
def png(w,h,rgba):
    raw=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
GRID=24
PALETTES={
    'light':((255,255,247),(238,229,208),(192,174,141)),
    'dark':((255,255,247),(255,209,112),(255,170,0)),
}
def cells(solids,holes,palette):
    result=[]
    for y in range(GRID):
        row=[]
        for x in range(GRID):
            px,py=(x+.5)*128/GRID,(y+.5)*128/GRID
            filled=any(inside(s,px,py) for s in solids) and not any(inside(s,px,py) for s in holes)
            # Three discrete filled tones; never an edge stroke or antialiased mask.
            tone=0 if y<9 else 1 if y<17 else 2
            row.append((*palette[tone],255) if filled else (0,0,0,0))
        result.append(row)
    return result
def write_changed(path,body):
    if not path.exists() or path.read_bytes()!=body:path.write_bytes(body)
def generate():
    OUT.mkdir(parents=True,exist_ok=True)
    outputs=[]
    for name,(solids,holes) in SHAPES.items():
        for theme in ('light','dark','default'):
            grid=cells(solids,holes,PALETTES['light' if theme=='default' else theme])
            stem=name if theme=='default' else name+'-'+theme
            svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" shape-rendering="crispEdges">'
            svg+=''.join('<rect x="%d" y="%d" width="1" height="1" fill="#%02x%02x%02x"/>'%(x,y,*pixel[:3]) for y,row in enumerate(grid) for x,pixel in enumerate(row) if pixel[3])+'</svg>\n'
            path=OUT/(stem+'.svg');body=svg.encode('utf-8');write_changed(path,body)
            outputs.append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(body).hexdigest(),'theme':theme,'format':'svg'})
            for size in (96,192):
                scale=size//GRID
                data=bytes(v for y in range(size) for x in range(size) for v in grid[y//scale][x//scale])
                path=OUT/('%s-%d.png'%(stem,size));body=png(size,size,data);write_changed(path,body)
                outputs.append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(body).hexdigest(),'pixels':size,'theme':theme,'format':'png'})
    manifest={'format':'SonnOriginalIcons2','author':'Sonnheide project','license':'Original project artwork; no public license granted','recipe':'tools/generate_visual_assets.py','stroke':False,'logical_grid':GRID,'scaling':'nearest_integer','alpha':'binary','themes':['light','dark'],'outputs':outputs}
    write_changed(ROOT/'assets/manifests/original_ui_icons.json',(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print('Original pixel icon outputs:',len(outputs))
if __name__=='__main__':generate()
