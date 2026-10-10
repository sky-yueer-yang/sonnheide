#!/usr/bin/env python3
"""Original blue/red/white/ffaa00 32-cell arcade sprites; stdlib, Python3.9+."""
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
# A crossed wrench and hammer has a distinct silhouette from the settings gear.
SHAPES['tools']=([
    poly((21,92),(35,106),(82,59),(91,53),(105,44),(106,27),(94,36),(83,34),(81,23),(92,13),(77,13),(66,24),(63,40)),
    poly((38,43),(47,34),(106,94),(96,104)),
    poly((19,33),(39,13),(62,36),(42,56)),
],[])
SHAPES['star']=([poly((64,13),(75,50),(112,64),(75,77),(64,114),(51,77),(14,64),(51,50))],[])
SHAPES['sun']=([circle(64,64,28)]+[rect(59,9,10,17),rect(59,103,10,17),rect(9,59,17,10),rect(103,59,17,10)]+[poly((x,y),(x+10,y),(x+18,y+8),(x+8,y+8)) for x,y in ((24,24),(87,24),(24,94),(87,94))],[])
SHAPES['moon']=([circle(62,64,42)],[circle(80,48,34)])
SHAPES['exit']=([rect(27,23,52,83),rect(65,57,38,11),poly((91,39),(111,63),(91,86))],[rect(39,35,28,59)])
# Four shared pigment families. All highlights and shadows have an explicit
# base/shade/white-tint recipe; no purple or green pigment enters UI artwork.
BASE_COLORS={'blue':(57,125,222),'red':(227,75,63),'white':(255,255,255),'yellow':(255,170,0)}
PALETTE_ID='blue_red_white_ffaa00'
PIGMENT_RECIPES={}
def pigment(family,shade=1.0,tint=0.0):
    if family not in BASE_COLORS or not 0<=shade<=1 or not 0<=tint<=1:
        raise ValueError('bounded four-family pigment recipe required')
    value='#'+''.join('%02x'%int(math.floor(channel*shade+(255-channel*shade)*tint+.5)) for channel in BASE_COLORS[family])
    PIGMENT_RECIPES[value]={'base':family,'shade':shade,'white_tint':tint}
    return value
def blue(shade=1.0,tint=0.0):return pigment('blue',shade,tint)
def red(shade=1.0,tint=0.0):return pigment('red',shade,tint)
def gold(shade=1.0,tint=0.0):return pigment('yellow',shade,tint)
def white():return pigment('white')
BLUES=(blue(tint=.5),blue(),blue(.7),blue(.32))
REDS=(red(tint=.5),red(),red(.65),red(.32))
WHITES=(white(),blue(tint=.88),blue(tint=.65),blue(.32))
GOLDS=(gold(),gold(.8),gold(.6),blue(.32))
PIGMENTS={
'world':BLUES,'earth':BLUES,'camera':BLUES,'blank':WHITES,
'people':WHITES,'buildings':WHITES,'terrain':BLUES,
'economy':GOLDS,'laws':GOLDS,'settings':BLUES,'pause':GOLDS,
'play':BLUES,'save':BLUES,'load':GOLDS,'back':BLUES,'close':REDS,
'create':GOLDS,'check':WHITES,'audio':BLUES,'help':BLUES,
'tools':BLUES,'star':GOLDS,'sun':GOLDS,'moon':WHITES,'exit':REDS,
}
DETAILS={
'world':[(poly((34,36),(51,29),(66,38),(61,51),(45,56),(43,75),(31,63)),white()),(poly((72,56),(99,62),(89,85),(78,97),(68,80)),blue(tint=.8))],
'earth':[(poly((29,39),(50,29),(71,37),(63,53),(44,58),(38,78),(27,64)),white()),(poly((73,56),(101,60),(94,84),(79,98),(68,80)),blue(tint=.8))],
'camera':[(rect(32,48,63,5),red()),(circle(64,69,21),blue(.32)),(circle(64,69,16),white()),(circle(64,69,9),blue()),(rect(59,60,7,6),white()),(rect(86,50,7,5),gold())],
'blank':[(rect(41,60,48,5),blue()),(rect(41,74,42,5),blue()),(rect(41,88,29,5),red()),(poly((80,23),(80,42),(98,42)),blue(tint=.5))],
'people':[(rect(26,66,31,31),blue()),(rect(60,70,33,30),red()),(rect(31,26,31,11),blue(.55)),(rect(75,36,23,8),red(.65)),(rect(39,42,4,4),blue(.32)),(rect(53,42,4,4),blue(.32)),(rect(37,71,8,7),white()),(rect(78,77,7,6),gold())],
'buildings':[(poly((22,61),(64,26),(106,61)),red()),(poly((32,58),(63,35),(96,58)),red(tint=.4)),(rect(39,67,9,13),blue()),(rect(81,67,9,13),blue()),(rect(56,75,16,28),blue(.55)),(rect(58,75,3,28),blue(tint=.5)),(rect(66,87,4,4),gold())],
'terrain':[(poly((36,54),(47,28),(61,52),(49,45)),white()),(poly((69,80),(91,43),(109,85)),blue(tint=.5)),(poly((76,68),(91,43),(100,66),(90,61)),white()),(rect(14,99,99,5),blue(.55))],
'economy':[(rect(25,44,78,6),gold(tint=.2)),(rect(58,53,13,28),gold()),(rect(62,62,5,8),blue(.32)),(rect(26,91,75,5),red(.65))],
'laws':[(rect(60,29,7,68),gold(tint=.2)),(rect(22,78,19,7),white()),(rect(88,78,19,7),white()),(rect(36,98,55,7),blue())],
'settings':[(circle(64,64,16),blue(.32)),(circle(64,64,10),white()),(rect(58,53,5,4),blue(tint=.5))],
'save':[(rect(43,25,38,27),white()),(rect(68,25,8,19),red()),(rect(41,69,49,35),blue(tint=.88)),(rect(46,77,36,5),blue(.7)),(rect(46,88,36,5),blue(.7))],
'load':[(rect(58,24,9,41),white()),(poly((44,51),(63,72),(83,51)),white()),(rect(24,80,79,17),gold(.8))],
'help':[(circle(64,90,5),red()),(poly((45,43),(54,34),(73,34),(85,45),(85,56),(70,69),(70,77),(60,77),(60,65),(75,53),(75,47),(68,43),(59,43),(53,50)),white())],
'exit':[(rect(39,35,28,59),blue(.32)),(rect(65,57,38,11),white()),(poly((91,39),(111,63),(91,86)),white()),(rect(61,67,4,5),gold())],
'tools':[(poly((38,43),(47,34),(106,94),(96,104)),blue(tint=.8)),(poly((19,33),(39,13),(62,36),(42,56)),red()),(poly((24,32),(39,18),(56,36),(42,49)),red(tint=.5)),(rect(26,89,6,6),white())],
}

def rgb(value):return tuple(int(value[i:i+2],16) for i in (1,3,5))

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
GRID=32
def cells(name,solids,holes):
    palette=tuple(rgb(value) for value in PIGMENTS[name])
    mask=[]
    for y in range(GRID):
        row=[]
        for x in range(GRID):
            px,py=(x+.5)*128/GRID,(y+.5)*128/GRID
            filled=any(inside(shape,px,py) for shape in solids)
            if name=='moon':filled=filled and not any(inside(shape,px,py) for shape in holes)
            row.append(filled)
        mask.append(row)
    result=[]
    for y in range(GRID):
        row=[]
        for x in range(GRID):
            if not mask[y][x]:row.append((0,0,0,0));continue
            px,py=(x+.5)*128/GRID,(y+.5)*128/GRID
            edge=any(not(0<=xx<GRID and 0<=yy<GRID) or not mask[yy][xx] for xx,yy in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)))
            colour=palette[3] if edge else palette[0 if y<12 else 1 if y<22 else 2]
            if not edge:
                if name!='moon' and any(inside(shape,px,py) for shape in holes):colour=palette[3]
                for shape,value in DETAILS.get(name,[]):
                    if inside(shape,px,py):colour=rgb(value)
            row.append((*colour,255))
        result.append(row)
    return result
def write_changed(path,body):
    if not path.exists() or path.read_bytes()!=body:path.write_bytes(body)
def validate_grid(name,grid):
    if len(grid)!=GRID or any(len(row)!=GRID for row in grid):
        raise ValueError(name+': logical dimensions')
    allowed={rgb(value) for value in PIGMENT_RECIPES}
    for y,row in enumerate(grid):
        for x,pixel in enumerate(row):
            if pixel[3] not in (0,255) or (pixel[3] and pixel[:3] not in allowed):
                raise ValueError(name+': nonbinary alpha or unregistered pigment')
            if (x in (0,GRID-1) or y in (0,GRID-1)) and pixel[3]:
                raise ValueError(name+': symbol touches external frame')
    if not any(pixel[3] for row in grid for pixel in row):
        raise ValueError(name+': empty symbol')
def generate():
    if set(PIGMENTS)!=set(SHAPES) or len(SHAPES)!=25:
        raise ValueError('Only the25 consumed original UI symbols are generated')
    OUT.mkdir(parents=True,exist_ok=True)
    outputs=[]
    expected=set()
    for name,(solids,holes) in SHAPES.items():
        grid=cells(name,solids,holes)
        validate_grid(name,grid)
        svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" shape-rendering="crispEdges">'
        svg+=''.join('<rect x="%d" y="%d" width="1" height="1" fill="#%02x%02x%02x"/>'%(x,y,*pixel[:3]) for y,row in enumerate(grid) for x,pixel in enumerate(row) if pixel[3])+'</svg>\n'
        path=OUT/(name+'.svg');body=svg.encode('utf-8');write_changed(path,body);expected.add(path.name)
        outputs.append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(body).hexdigest(),'palette':PALETTE_ID,'format':'svg'})
        for size in (96,192):
            scale=size//GRID
            data=bytes(value for y in range(size) for x in range(size) for value in grid[y//scale][x//scale])
            path=OUT/('%s-%d.png'%(name,size));body=png(size,size,data);write_changed(path,body);expected.add(path.name)
            outputs.append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(body).hexdigest(),'pixels':size,'palette':PALETTE_ID,'format':'png'})
    # Age no longer recolours artwork. Remove only this recipe's obsolete files.
    for path in OUT.iterdir():
        if path.is_file() and path.suffix in ('.svg','.png') and path.name not in expected:path.unlink()
    manifest={
        'format':'SonnOriginalIcons4','author':'Sonnheide project',
        'license':'Original project artwork; no public license granted',
        'recipe':'tools/generate_visual_assets.py','recipe_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'stroke':True,'external_frame':False,'logical_grid':GRID,'scaling':'nearest_integer','alpha':'binary',
        'palette':PALETTE_ID,'primary_colors':{name:'#'+''.join('%02x'%c for c in colour) for name,colour in BASE_COLORS.items()},
        'pigment_recipe':'base_times_shade_then_linear_white_tint_round_half_up',
        'pigment_derivatives':dict(sorted(PIGMENT_RECIPES.items())),
        'yellow_base':'#ffaa00','yellow_recipe':'shade_then_linear_white_tint_round_half_up',
        'yellow_derivatives':{value:{'shade':recipe['shade'],'tint':recipe['white_tint']} for value,recipe in sorted(PIGMENT_RECIPES.items()) if recipe['base']=='yellow'},
        'themes':[],'outputs':outputs,
    }
    write_changed(ROOT/'assets/manifests/original_ui_icons.json',(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
    print('Original pixel icon outputs:',len(outputs))
if __name__=='__main__':generate()
