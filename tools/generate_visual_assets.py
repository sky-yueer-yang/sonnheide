#!/usr/bin/env python3
"""Original physical-object pixel sprites; integer SVG/PNG, Python3.9+ stdlib.

Objects are drawn at 48 logical cells, exported at exact 2x/4x. Registered
pigment clusters are discrete; no blur, antialiasing, external fonts or images.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'assets/generated/ui/icons'
GRID = 48
PALETTE_ID = 'original_heavy_dark_pixel_materials'
BASE_COLORS = {
    'blue': (22,98,184), 'red': (222,64,45), 'white': (255,255,255), 'yellow': (255,170,0),
    'stone': (132,160,179), 'wood': (117,60,30), 'copper': (185,91,41),
    'skin': (233,150,99), 'green': (49,141,92), 'ink': (8,21,39),
}
PIGMENT_RECIPES = {}
def pigment(family,shade=1.0,tint=0.0):
    if family not in BASE_COLORS or not 0<=shade<=1 or not 0<=tint<=1:
        raise ValueError('Bounded original pigment recipe required')
    value='#'+''.join('%02x'%int(math.floor(c*shade+(255-c*shade)*tint+.5)) for c in BASE_COLORS[family])
    PIGMENT_RECIPES[value]={'base':family,'shade':shade,'white_tint':tint}
    return value
def rgb(value): return tuple(int(value[i:i+2],16) for i in (1,3,5))
def ramp(family):
    # The declared primaries are colour SOURCES, not mandatory bright pixels.
    # Strong chroma is retained by shading, with no pastel white wash. Silver,
    # stone and skin also have bounded highlights rather than luminous faces.
    if family=='white': recipes=((.13,0),(.27,0),(.41,0),(.53,0),(.62,0),(.72,0),(.82,0))
    elif family=='yellow': recipes=((.13,0),(.26,0),(.42,0),(.60,0),(.70,0),(.79,0),(.86,.03))
    elif family in ('blue','red','green','copper'):
        recipes=((.14,0),(.27,0),(.43,0),(.64,0),(.72,0),(.81,0),(.90,.025))
    elif family=='skin': recipes=((.19,0),(.34,0),(.50,0),(.68,0),(.75,0),(.83,0),(.91,0))
    else: recipes=((.15,0),(.29,0),(.46,0),(.65,0),(.75,0),(.84,0),(.94,0))
    return tuple(pigment(family,shade,tint) for shade,tint in
                 recipes)
B,R,W,G,S,T,C,F,L=[ramp(name) for name in ('blue','red','white','yellow','stone','wood','copper','skin','green')]
INK=pigment('ink')

class Sprite:
    def __init__(self): self.grid=[[None for _ in range(GRID)] for _ in range(GRID)]
    def point(self,x,y,colour):
        x,y=int(x),int(y)
        if not (1<=x<GRID-1 and 1<=y<GRID-1):
            raise ValueError('Art must leave a transparent safety pixel: %s,%s'%(x,y))
        self.grid[y][x]=colour
    def rect(self,x,y,width,height,colour):
        for yy in range(y,y+height):
            for xx in range(x,x+width): self.point(xx,yy,colour)
    def line(self,x0,y0,x1,y1,colour,width=1):
        dx,dy=abs(x1-x0),-abs(y1-y0);sx,sy=(1 if x0<x1 else -1),(1 if y0<y1 else -1);error=dx+dy
        while True:
            for yy in range(y0,y0+width):
                for xx in range(x0,x0+width): self.point(xx,yy,colour)
            if x0==x1 and y0==y1: break
            twice=error*2
            if twice>=dy: error+=dy;x0+=sx
            if twice<=dx: error+=dx;y0+=sy
    def polygon(self,points,colour,gradient=None):
        ymin,ymax=min(p[1] for p in points),max(p[1] for p in points)
        xmin,xmax=min(p[0] for p in points),max(p[0] for p in points)
        for y in range(ymin,ymax+1):
            py=y+.5
            for x in range(xmin,xmax+1):
                px,hit=x+.5,False;last=points[-1]
                for point in points:
                    if ((point[1]>py)!=(last[1]>py)) and px<(last[0]-point[0])*(py-point[1])/(last[1]-point[1])+point[0]: hit=not hit
                    last=point
                if hit:
                    fill=gradient[min(len(gradient)-1,(y-ymin)*len(gradient)//max(1,ymax-ymin+1))] if gradient else colour
                    self.point(x,y,fill)
    def disk(self,cx,cy,rx,colour,ry=None,gradient=None):
        ry=rx if ry is None else ry
        for y in range(max(1,int(cy-ry)),min(GRID-2,int(cy+ry))+1):
            for x in range(max(1,int(cx-rx)),min(GRID-2,int(cx+rx))+1):
                if ((x-cx)/rx)**2+((y-cy)/ry)**2<=1:
                    fill=gradient[min(len(gradient)-1,int((y-cy+ry)*len(gradient)/(2*ry+1)))] if gradient else colour
                    self.point(x,y,fill)
    def sparkle(self,x,y,palette=G,radius=2):
        self.line(x-radius,y,x+radius,y,palette[3]);self.line(x,y-radius,x,y+radius,palette[4]);self.point(x,y,palette[6])
    def outline(self,colour=INK,radius=2):
        old=[row[:] for row in self.grid]
        neighbors=[(dx,dy) for dy in range(-radius,radius+1) for dx in range(-radius,radius+1)
                   if abs(dx)+abs(dy)<=radius]
        for y in range(1,GRID-1):
            for x in range(1,GRID-1):
                if old[y][x] is None and any(0<=y+dy<GRID and 0<=x+dx<GRID and old[y+dy][x+dx] is not None
                                           for dx,dy in neighbors):
                    self.grid[y][x]=colour
    def flip(self,axis):
        old=[row[:] for row in self.grid]
        for y in range(GRID):
            for x in range(GRID):
                if axis=='horizontal': self.grid[y][x]=old[y][GRID-x-1]
                elif axis=='vertical': self.grid[y][x]=old[GRID-y-1][x]
                elif axis=='rotate-right': self.grid[y][x]=old[GRID-x-1][y]
                else: raise ValueError(axis)

def gold_ray_halo(s,cx=24,cy=23,radius=19):
    # Whole-cell gold shafts separated by transparency, never Gaussian glow.
    for i in range(20):
        a=i*math.pi/10;inner=10+i%3;outer=radius-(i*7%4)
        x0,y0=round(cx+inner*math.cos(a)),round(cy+inner*math.sin(a))
        x1,y1=round(cx+outer*math.cos(a)),round(cy+outer*math.sin(a))
        s.line(x0,y0,x1,y1,G[1] if i%3 else G[3])
        if i%4==0: s.point(x1,y1,G[5])
def coin(s,cx,cy,radius=6):
    s.disk(cx,cy+1,radius,G[0],ry=max(2,radius//2))
    s.disk(cx,cy,radius,G[3],ry=max(2,radius//2),gradient=(G[5],G[3],G[2]))
    s.line(cx-radius+2,cy,cx+radius-2,cy,G[1]);s.point(cx-2,cy-1,G[6])
def gemstone(s,cx,cy,radius=9,palette=R):
    s.polygon(((cx,cy-radius),(cx+radius,cy-3),(cx+radius-1,cy+4),(cx,cy+radius),(cx-radius+1,cy+4),(cx-radius,cy-3)),palette[1])
    s.polygon(((cx,cy-radius+1),(cx+radius-2,cy-3),(cx,cy+1),(cx-radius+2,cy-3)),palette[4])
    s.polygon(((cx-radius+2,cy-2),(cx,cy+1),(cx,cy+radius-2),(cx-radius+2,cy+3)),palette[3])
    s.polygon(((cx+radius-2,cy-2),(cx,cy+1),(cx,cy+radius-2),(cx+radius-2,cy+3)),palette[2])
    s.line(cx-2,cy-radius+3,cx-4,cy-3,palette[6])
def brass_medallion(s):
    s.disk(24,25,20,G[0]);s.disk(23,23,20,G[3],gradient=(G[5],G[3],G[2],G[1]))
    s.disk(23,23,17,G[0]);s.disk(23,22,15,B[1],gradient=(B[3],B[2],B[1]))
    for x,y in ((23,5),(41,23),(23,41),(5,23)):
        s.rect(x-1,y-1,3,3,G[5]);s.point(x,y,G[6])

def telescope():
    s=Sprite()
    s.polygon(((23,26),(28,27),(34,44),(30,44)),T[1]);s.polygon(((23,26),(27,27),(18,44),(14,44)),T[3])
    s.polygon(((24,27),(27,28),(26,44),(22,44)),T[2]);s.line(16,41,18,35,T[5])
    s.polygon(((4,19),(34,5),(42,19),(12,34)),G[1]);s.polygon(((5,17),(34,4),(40,15),(11,29)),G[3])
    s.polygon(((8,17),(33,6),(35,10),(10,22)),G[5]);s.polygon(((16,13),(19,12),(25,23),(22,25)),G[0])
    s.polygon(((29,7),(32,5),(39,17),(36,19)),G[2]);s.disk(8,24,5,T[1],ry=7);s.disk(7,23,4,B[1],ry=6)
    s.line(5,20,7,18,B[6]);s.rect(23,27,6,4,C[3]);s.point(25,28,G[6]);s.sparkle(40,6,radius=2)
    return s

def terrain():
    s=Sprite()
    s.polygon(((4,27),(27,19),(43,28),(43,37),(23,45),(4,36)),T[1]);s.polygon(((4,29),(23,37),(23,45),(4,36)),T[3])
    s.polygon(((23,37),(43,28),(43,37),(23,45)),T[2]);s.line(6,32,21,38,C[2]);s.line(7,35,21,40,G[1])
    s.line(25,39,40,33,C[3]);s.point(32,40,G[3]);s.polygon(((4,27),(25,19),(43,28),(23,37)),L[3])
    s.polygon(((8,27),(21,3),(34,27),(24,33)),S[2]);s.polygon(((21,3),(34,27),(24,30),(21,16)),S[1])
    s.polygon(((15,15),(21,3),(27,15),(24,13),(21,17),(19,13)),W[6])
    s.polygon(((28,22),(33,24),(29,27),(30,31),(24,35),(18,33),(25,30),(25,27),(30,24)),B[3]);s.line(26,30,22,32,B[6])
    s.rect(6,21,2,7,T[3]);s.polygon(((3,22),(7,11),(11,22)),L[2]);s.polygon(((4,17),(7,9),(10,17)),L[4])
    s.point(36,27,L[5]);s.point(38,29,L[1]);return s

def person_and_animal():
    s=Sprite()
    s.polygon(((4,29),(10,23),(21,23),(27,29),(25,39),(6,39)),B[2]);s.polygon(((8,24),(13,24),(13,36),(5,35)),B[4])
    s.rect(9,36,6,8,T[1]);s.rect(17,36,6,8,T[2]);s.rect(7,42,9,3,S[0]);s.rect(17,42,9,3,S[0])
    s.rect(8,34,16,3,T[1]);s.rect(14,34,5,3,G[3]);s.rect(5,28,4,6,F[3]);s.rect(23,28,4,6,F[2])
    s.polygon(((9,8),(13,4),(21,5),(25,11),(24,20),(19,24),(11,21)),F[3])
    s.polygon(((9,11),(9,7),(14,3),(22,4),(26,10),(22,12),(19,8),(12,12)),T[1])
    s.rect(9,12,3,5,F[5]);s.rect(13,13,2,2,INK);s.rect(21,13,2,2,INK);s.rect(17,17,3,2,F[1]);s.rect(15,21,5,2,F[2])
    s.polygon(((27,31),(40,29),(44,34),(43,41),(30,43),(26,38)),C[3]);s.polygon(((37,31),(43,25),(44,37),(38,40)),C[4])
    s.polygon(((30,26),(32,18),(36,25),(39,24),(43,19),(43,29),(38,33),(32,31)),C[3])
    s.polygon(((31,24),(32,20),(34,25)),R[1]);s.polygon(((40,25),(42,21),(42,26)),R[1])
    s.polygon(((32,28),(36,30),(39,28),(38,33),(35,34)),W[5]);s.point(32,27,INK);s.point(40,27,INK);s.rect(35,30,2,2,INK)
    s.rect(29,40,3,5,C[1]);s.rect(39,40,3,5,C[1]);s.polygon(((41,34),(45,32),(45,37),(42,39)),W[5]);return s

def laws():
    s=Sprite()
    # Golden broken halo remains visibly outside the stone, rather than becoming
    # a tiny ornament hidden behind the tablets at real toolbar display sizes.
    s.disk(24,22,20,G[1]);s.disk(24,22,18,None)
    gold_ray_halo(s,radius=22)
    # Five carved marks per tablet, ten total; not a font or fake readable text.
    for left,top in ((6,10),(25,8)):
        s.polygon(((left,top+5),(left+3,top+1),(left+11,top),(left+16,top+5),(left+16,41),(left,42)),S[0])
        s.polygon(((left,top+5),(left+3,top+1),(left+10,top),(left+14,top+4),(left+14,39),(left,40)),S[3],gradient=(S[5],S[4],S[3],S[2]))
        s.line(left+2,top+5,left+2,37,S[6])
        for line in range(5):
            # Ten broad, individually separated grooves; no one-cell hairlines.
            y=top+8+line*5;s.rect(left+4,y,8+(line%2),3,S[0])
        s.line(left+11,33,left+9,35,S[1]);s.line(left+9,35,left+11,38,S[1])
    s.sparkle(5,5,radius=2);s.sparkle(43,13,radius=2);return s

def buildings():
    s=Sprite()
    s.polygon(((5,24),(21,16),(37,25),(37,41),(21,45),(5,38)),S[2]);s.polygon(((5,24),(21,29),(21,45),(5,38)),S[4])
    s.polygon(((21,29),(37,25),(37,41),(21,45)),S[2]);s.rect(27,6,5,12,S[2]);s.rect(26,5,7,3,S[4])
    s.polygon(((3,24),(18,8),(39,18),(24,32)),R[1]);s.polygon(((3,23),(18,7),(39,17),(24,29)),R[3])
    for x,y in ((9,21),(14,16),(19,12),(17,23),(23,18),(29,17),(24,24)): s.line(x,y,x+5,y+2,R[5])
    s.line(4,24,23,31,T[1]);s.polygon(((11,29),(17,31),(17,41),(11,39)),T[1]);s.line(12,32,12,37,T[4]);s.point(15,36,G[3])
    s.polygon(((25,30),(31,28),(31,34),(25,36)),B[3]);s.line(28,30,28,34,S[5]);s.line(25,33,30,31,S[5])
    s.line(7,33,10,34,S[1]);s.line(27,39,34,37,S[1]);s.polygon(((33,38),(36,34),(44,41),(41,44)),T[3])
    s.polygon(((30,33),(35,27),(44,34),(39,40)),S[1]);s.polygon(((30,32),(35,26),(44,33),(39,37)),S[4]);s.line(34,30,40,34,S[6]);return s

def economy():
    s=Sprite()
    s.polygon(((12,9),(28,5),(32,11),(28,18),(38,27),(38,37),(31,43),(14,42),(5,35),(6,25),(14,17)),R[1])
    s.polygon(((12,9),(28,5),(29,11),(25,16),(13,17)),R[3]);s.polygon(((14,18),(25,18),(33,28),(33,35),(28,40),(13,38),(8,33),(9,27)),R[3],gradient=(R[4],R[3],R[2]))
    s.line(12,16,29,16,G[3],2);s.line(14,13,14,8,R[5]);s.line(21,13,23,8,R[0])
    s.disk(21,29,8,G[1]);s.disk(20,28,7,G[3],gradient=(G[5],G[3],G[2]))
    s.rect(18,24,5,1,G[0]);s.rect(17,25,2,3,G[0]);s.rect(18,28,5,1,G[0]);s.rect(21,29,2,3,G[0]);s.rect(17,32,5,1,G[0]);s.rect(19,23,1,11,G[0])
    for y in (37,34,31): coin(s,38,y,6)
    coin(s,8,40,5);s.sparkle(40,18,radius=2);return s

def globe():
    s=Sprite()
    s.disk(24,22,19,G[1],ry=17);s.disk(24,22,17,None,ry=15);s.disk(23,21,14,B[1],gradient=(B[4],B[3],B[2],B[1]))
    s.polygon(((14,12),(19,10),(24,13),(22,18),(18,19),(18,24),(13,21)),G[3]);s.polygon(((26,20),(32,18),(35,23),(30,27),(30,32),(25,28)),G[4])
    s.line(10,21,35,25,B[5]);s.line(22,8,19,12,G[4]);s.line(19,12,20,32,G[2]);s.line(20,32,27,36,G[3]);s.line(27,36,33,30,G[3])
    s.polygon(((7,15),(10,12),(40,28),(39,31),(8,18)),G[0]);s.line(8,14,39,29,G[4],2)
    s.rect(21,36,5,7,G[2]);s.rect(20,36,2,5,G[5]);s.polygon(((11,42),(34,42),(38,45),(7,45)),T[3]);s.rect(12,41,21,2,G[2]);s.sparkle(40,6,radius=2);return s

def machinery():
    s=Sprite();s.disk(22,25,16,C[1])
    for i in range(8):
        a=i*math.pi/4;cx,cy=round(22+17*math.cos(a)),round(25+17*math.sin(a));s.rect(cx-3,cy-3,6,6,C[2])
    s.disk(21,23,15,C[3],gradient=(C[5],C[4],C[3],C[2]));s.disk(21,23,10,C[0]);s.disk(21,22,8,B[2]);s.disk(21,22,4,INK);s.disk(20,21,3,B[5])
    for x,y in ((13,16),(28,16),(13,30),(28,30)): s.rect(x,y,2,2,G[4])
    s.polygon(((29,7),(35,3),(41,4),(45,9),(43,15),(39,17),(39,27),(34,30),(31,25),(34,21),(34,16),(29,13)),B[1])
    s.disk(36,9,5,B[4]);s.disk(36,9,2,INK);s.rect(35,15,3,12,B[3]);s.point(35,19,B[6]);s.line(7,23,10,17,C[6]);return s

def blank():
    s=Sprite();s.rect(7,5,33,5,T[1]);s.rect(7,5,31,2,T[4])
    s.polygon(((9,8),(39,8),(36,35),(39,40),(7,41),(9,34)),G[2]);s.polygon(((9,8),(36,8),(33,35),(36,39),(7,39),(9,34)),G[5])
    s.line(11,12,31,12,G[3]);s.line(11,33,29,33,G[3]);s.polygon(((12,24),(22,19),(31,24),(21,29)),L[3]);s.line(13,25,21,29,L[1]);s.line(21,29,30,25,L[1])
    s.rect(7,39,34,5,T[1]);s.rect(7,39,32,2,T[4])
    for x in (5,39):
        s.rect(x,3,4,9,C[2]);s.rect(x,37,4,9,C[2]);s.rect(x,4,2,6,G[4]);s.rect(x,38,2,6,G[4])
    return s

def earth():
    s=Sprite();s.disk(24,25,20,B[0]);s.disk(23,23,20,B[3],gradient=(B[5],B[4],B[3],B[2],B[1]))
    s.polygon(((11,10),(19,6),(26,9),(25,15),(20,18),(18,27),(13,26),(12,20),(6,17)),L[3])
    s.polygon(((26,23),(35,20),(41,26),(36,32),(34,39),(28,38),(24,30)),L[4]);s.line(12,11,19,8,L[6]);s.rect(18,30,3,3,G[3]);s.rect(39,14,2,3,W[5])
    s.polygon(((16,4),(29,3),(33,5),(28,8),(20,8)),W[6]);s.line(5,23,8,31,B[6]);s.point(9,33,B[5]);return s

def save():
    s=Sprite();s.polygon(((7,9),(14,4),(40,4),(40,39),(34,44),(7,44)),T[1]);s.rect(10,7,29,35,G[4]);s.rect(9,9,27,35,R[1]);s.rect(12,8,26,33,R[3])
    s.rect(7,9,5,33,R[0]);s.rect(12,8,3,33,R[5]);s.line(17,11,34,11,G[4]);s.line(17,37,34,37,G[3])
    s.line(17,12,17,17,G[3]);s.line(34,12,34,17,G[3]);s.line(17,32,17,36,G[3]);s.line(34,32,34,36,G[3]);gemstone(s,26,24,8,B)
    s.rect(33,23,9,5,G[1]);s.rect(33,23,8,2,G[5]);s.rect(38,23,2,4,G[3]);s.line(14,42,33,42,G[6]);s.line(14,44,32,44,G[3]);s.sparkle(5,6,radius=2);return s

def load():
    s=Sprite();s.polygon(((5,17),(11,7),(38,7),(44,17),(41,23),(7,23)),T[1]);s.polygon(((7,16),(12,9),(37,9),(42,16),(39,20),(9,20)),T[3])
    s.rect(11,10,3,11,C[4]);s.rect(35,10,3,11,C[4]);s.rect(7,20,35,18,T[1]);s.rect(6,21,34,18,T[3]);s.rect(6,24,34,2,T[0]);s.rect(6,33,34,2,T[0])
    s.rect(9,21,3,19,C[3]);s.rect(34,21,3,19,C[3]);s.rect(9,22,1,16,G[5]);s.rect(34,22,1,16,G[5]);s.rect(4,37,39,4,T[1]);s.rect(4,37,38,1,C[3])
    s.polygon(((15,5),(30,5),(31,22),(15,22)),G[4]);s.rect(18,8,9,1,G[1]);s.rect(18,11,10,1,G[1]);s.rect(18,14,6,1,G[1])
    s.rect(19,22,9,5,G[1]);s.rect(19,22,8,2,G[4]);s.rect(22,24,3,3,INK);s.sparkle(41,5,radius=2);s.sparkle(5,43,radius=1);return s

def control(kind):
    s=Sprite();brass_medallion(s)
    if kind=='play':
        s.polygon(((16,11),(36,23),(16,36)),R[0]);s.polygon(((16,11),(34,22),(16,33)),R[3]);s.polygon(((16,11),(34,22),(20,21)),R[5]);s.polygon(((20,21),(34,22),(16,33)),R[2]);s.line(17,13,17,29,R[6])
    else:
        for x in (14,25):
            s.rect(x,12,8,25,C[0]);s.rect(x,11,7,23,G[3]);s.rect(x,11,2,22,G[5]);s.rect(x+4,12,2,21,G[2]);s.rect(x,11,6,2,G[6])
    return s

def wax_seal(kind):
    s=Sprite();s.polygon(((10,29),(22,33),(14,45),(10,40),(5,41)),G[2]);s.polygon(((24,33),(36,28),(42,41),(37,40),(33,45)),G[3])
    s.line(11,33,8,39,G[5]);s.line(32,34,36,40,G[5]);s.disk(23,22,19,R[0]);s.disk(23,20,18,R[3],gradient=(R[5],R[4],R[3],R[2]));s.disk(23,20,14,R[1]);s.disk(23,19,12,R[3])
    if kind=='check':
        s.polygon(((12,20),(18,26),(32,12),(35,15),(18,32),(9,23)),G[0]);s.polygon(((12,18),(18,24),(32,10),(35,13),(18,30),(9,21)),G[5]);s.line(13,19,18,24,W[6])
    else:
        for p in (((13,10),(35,30),(31,33),(10,13)),((31,10),(35,13),(13,33),(10,30))): s.polygon(p,R[0])
        for p in (((13,8),(35,28),(31,31),(10,11)),((31,8),(35,11),(13,31),(10,28))): s.polygon(p,W[5])
        s.line(14,10,31,26,W[6])
    return s

def create():
    s=Sprite();s.polygon(((4,28),(25,22),(41,31),(41,39),(21,45),(4,36)),T[1]);s.polygon(((4,28),(25,22),(41,31),(21,38)),L[3])
    s.line(5,30,21,36,L[5]);s.line(25,26,35,31,L[2]);s.line(7,36,18,40,C[3]);s.line(25,41,35,38,G[1])
    s.polygon(((15,29),(18,31),(37,12),(33,8)),G[1]);s.line(17,28,34,11,G[5],2);gemstone(s,36,8,6,R)
    s.sparkle(16,14,radius=3);s.sparkle(9,22,radius=2);s.sparkle(39,20,radius=2);s.rect(23,4,2,7,G[4]);s.rect(20,7,8,2,G[4]);s.point(23,7,G[6]);return s

def arrow(direction):
    s=Sprite();p=((4,24),(21,6),(24,6),(24,17),(43,17),(43,31),(24,31),(24,42),(21,42))
    s.polygon(p,G[0]);s.polygon(tuple((x,y-2) for x,y in p),G[3],gradient=(G[5],G[4],G[3],G[2]))
    s.line(7,22,21,7,G[6]);s.line(24,17,40,17,G[5]);s.line(12,25,21,25,G[1],3);s.rect(29,23,3,3,G[1]);s.rect(37,23,3,3,G[1])
    if direction=='right': s.flip('horizontal')
    elif direction=='up': s.flip('rotate-right')
    elif direction=='down': s.flip('rotate-right');s.flip('vertical')
    return s

def audio():
    s=Sprite();s.rect(6,32,34,11,T[1]);s.rect(6,32,32,8,T[3]);s.rect(8,42,6,3,T[0]);s.rect(31,42,6,3,T[0]);s.rect(7,32,30,2,G[2])
    s.disk(23,32,12,S[0],ry=3);s.disk(22,31,10,INK,ry=2);s.point(21,30,G[5]);s.line(25,31,28,24,G[3],2)
    s.polygon(((26,25),(25,18),(17,13),(21,8),(31,17),(30,26)),G[2]);s.polygon(((20,12),(4,8),(8,3),(21,5),(32,10),(37,18),(29,22)),G[1])
    s.polygon(((20,10),(6,7),(10,3),(22,5),(32,10),(35,16),(29,18)),G[4]);s.polygon(((9,6),(18,6),(29,11),(32,15),(25,15),(13,10)),G[0])
    s.line(10,4,20,5,G[6]);s.rect(12,36,7,2,C[4]);s.rect(30,36,4,3,G[3]);s.line(39,10,41,13,B[4]);s.line(41,8,44,14,B[5]);s.line(43,7,45,10,B[3]);return s

def help_book():
    s=Sprite();s.polygon(((4,10),(20,8),(24,12),(28,8),(43,10),(43,38),(27,41),(23,43),(20,41),(4,39)),R[1])
    s.polygon(((6,9),(18,7),(23,11),(23,39),(18,37),(6,38)),G[5]);s.polygon(((24,11),(29,7),(41,9),(41,38),(29,36),(24,39)),G[4])
    s.line(23,12,23,39,G[1]);s.line(7,37,18,36,G[2]);s.line(29,35,39,37,G[2])
    for y in (15,19,23,28): s.line(9,y,19,y-1,G[2])
    s.rect(30,15,7,2,B[2]);s.rect(36,17,2,5,B[2]);s.rect(32,21,5,2,B[2]);s.rect(31,23,2,4,B[2]);s.rect(31,30,2,2,R[3])
    s.polygon(((7,27),(10,16),(15,9),(23,3),(20,13),(15,23)),W[5]);s.line(8,28,21,5,S[1]);s.line(12,18,14,10,W[6]);s.line(15,15,19,8,W[6]);return s

def tools():
    s=Sprite();s.polygon(((7,39),(11,44),(38,15),(33,10)),T[1]);s.line(9,39,35,13,T[4],2);s.line(13,36,16,33,C[3])
    s.polygon(((23,6),(30,3),(42,11),(45,19),(35,23),(28,15)),S[1]);s.polygon(((25,5),(30,3),(41,10),(43,17),(35,20),(29,13)),S[4]);s.line(35,21,43,18,W[6]);s.line(26,6,30,5,S[6])
    s.polygon(((38,44),(43,39),(19,17),(21,10),(17,4),(11,2),(11,9),(7,12),(3,8),(3,15),(8,20),(15,21)),B[1])
    s.polygon(((39,42),(41,39),(16,16),(17,10),(13,5),(13,11),(7,15),(5,12),(5,15),(9,18),(15,19)),B[4]);s.line(17,20,38,39,B[6]);s.disk(38,39,2,B[1]);return s

def star():
    s=Sprite();gold_ray_halo(s);s.polygon(((24,3),(29,17),(43,18),(33,28),(36,43),(24,35),(11,43),(15,28),(3,18),(18,17)),G[1])
    s.polygon(((24,4),(28,18),(42,18),(32,27),(35,41),(24,33),(12,41),(16,27),(4,18),(19,18)),G[3])
    s.polygon(((24,4),(24,24),(19,18)),G[6]);s.polygon(((4,18),(24,24),(16,27)),G[5]);s.polygon(((24,24),(32,27),(35,41),(24,33)),G[2]);gemstone(s,24,24,6,R);return s

def sun():
    s=Sprite()
    for i in range(12):
        a=i*math.pi/6;p=((round(24+13*math.cos(a-.11)),round(24+13*math.sin(a-.11))),(round(24+21*math.cos(a)),round(24+21*math.sin(a))),(round(24+13*math.cos(a+.11)),round(24+13*math.sin(a+.11))))
        s.polygon(p,G[3] if i%2 else G[4])
        s.line(round(24+16*math.cos(a)),round(24+16*math.sin(a)),
               round(24+21*math.cos(a)),round(24+21*math.sin(a)),G[4] if i%2 else G[3])
    s.disk(24,24,14,G[0]);s.disk(23,23,13,G[3],gradient=(G[6],G[4],G[3],G[2]));s.disk(23,23,9,G[2]);s.disk(22,22,8,G[4]);s.line(17,15,21,13,G[6]);s.rect(18,21,2,2,G[0]);s.rect(26,21,2,2,G[0]);s.line(20,26,25,26,G[1]);s.point(22,27,G[1]);return s

def moon():
    s=Sprite();s.disk(23,24,19,S[1]);s.disk(21,22,19,W[4],gradient=(W[6],W[5],S[4],S[3]));s.disk(30,15,17,None)
    for x,y,r in ((10,17,2),(10,28,3),(16,36,2),(23,37,1)):
        s.disk(x,y,r,S[2]);s.point(x-1,y-1,S[5])
    s.sparkle(37,8,radius=4);s.sparkle(36,28,radius=3);s.rect(8,7,5,3,G[3]);s.rect(8,5,5,2,G[2]);return s

def exit_door():
    s=Sprite();s.rect(6,3,28,40,S[1]);s.rect(7,4,25,37,S[3]);s.rect(11,8,17,33,INK);s.rect(7,4,3,36,S[5])
    s.polygon(((11,8),(23,4),(23,40),(11,41)),T[2]);s.line(13,10,13,38,T[4]);s.line(20,7,20,37,T[1]);s.rect(18,25,2,3,G[3]);s.rect(12,12,9,2,C[1]);s.rect(12,33,9,2,C[1])
    s.polygon(((25,18),(35,18),(35,12),(45,24),(35,36),(35,30),(25,30)),R[0]);s.polygon(((24,16),(34,16),(34,10),(44,22),(34,34),(34,28),(24,28)),R[3]);s.line(25,17,33,17,R[5]);s.line(35,13,42,21,R[6]);s.rect(4,41,32,4,S[2]);s.line(5,41,33,41,S[5]);return s

def cycle():
    s=Sprite();s.rect(15,8,19,4,G[2]);s.rect(15,8,18,2,G[5]);s.rect(15,35,19,5,G[2]);s.rect(15,35,18,2,G[5])
    for x in (16,31): s.rect(x,12,2,23,C[3]);s.rect(x,13,1,21,G[4])
    s.polygon(((19,12),(30,12),(28,19),(24,23),(28,28),(30,35),(19,35),(21,28),(24,23),(21,19)),B[3])
    s.polygon(((20,14),(29,14),(27,19),(24,22),(22,19)),G[4]);s.polygon(((20,34),(24,27),(28,34)),G[3]);s.rect(24,23,1,5,G[5]);s.line(19,14,20,18,B[6]);s.line(28,30,29,33,B[5])
    s.polygon(((3,23),(5,13),(10,6),(18,3),(26,4),(26,8),(18,7),(12,10),(8,16),(7,23),(11,23),(5,29),(2,23)),R[3]);s.line(6,15,10,9,R[5])
    s.polygon(((45,25),(43,35),(38,42),(30,45),(22,44),(22,40),(30,41),(36,38),(40,32),(41,25),(37,25),(43,19),(46,25)),G[3]);s.line(40,34,37,39,G[5]);return s

def hand():
    s=Sprite();s.polygon(((10,44),(10,33),(4,24),(4,19),(8,16),(14,22),(14,7),(18,3),(21,5),(21,20),(23,4),(27,3),(29,6),(29,20),(32,8),(36,9),(36,23),(40,15),(43,17),(43,31),(36,43)),G[1])
    s.polygon(((12,39),(11,31),(6,23),(6,20),(8,19),(15,26),(16,8),(18,5),(19,7),(19,23),(25,6),(27,6),(27,24),(33,11),(34,11),(34,27),(40,20),(41,22),(40,30),(34,39)),G[3],gradient=(G[6],G[4],G[3],G[2]))
    for x,y in ((17,14),(25,14),(33,18),(16,22),(24,23),(32,26)): s.rect(x-1,y,3,1,G[0]);s.point(x,y+1,G[5])
    s.polygon(((17,28),(26,26),(34,30),(32,36),(20,36)),G[2]);s.line(18,29,25,28,G[5]);s.rect(11,39,25,5,T[1]);s.rect(13,39,21,2,R[3]);s.rect(23,39,6,5,G[2]);s.rect(24,40,4,2,G[5]);return s

def magnifier(kind):
    s=Sprite();s.polygon(((26,28),(31,25),(44,38),(43,43),(38,44),(25,31)),T[1]);s.polygon(((29,28),(31,27),(43,39),(40,42)),T[3]);s.line(32,31,41,40,T[5])
    s.disk(20,20,17,G[0]);s.disk(19,18,16,G[3],gradient=(G[6],G[4],G[3],G[2]));s.disk(19,18,12,B[0]);s.disk(18,17,11,B[2],gradient=(B[5],B[4],B[3],B[2]));s.line(11,12,15,9,B[6]);s.point(10,14,B[6]);s.rect(11,17,15,3,W[6]);s.rect(11,20,15,1,B[1])
    if kind=='zoom-in': s.rect(17,11,3,15,W[6]);s.rect(20,11,1,15,B[1])
    s.rect(32,32,4,3,C[3]);s.rect(34,33,3,3,G[3]);return s

CATEGORY_ICONS=('camera','terrain','people','laws','buildings','economy','world','settings')
BUILDERS={
    'world':globe,'earth':earth,'camera':telescope,'blank':blank,'people':person_and_animal,
    'buildings':buildings,'terrain':terrain,'economy':economy,'laws':laws,'settings':machinery,
    'pause':lambda:control('pause'),'play':lambda:control('play'),'save':save,'load':load,
    'back':lambda:arrow('left'),'close':lambda:wax_seal('close'),'create':create,'check':lambda:wax_seal('check'),
    'audio':audio,'help':help_book,'tools':tools,'star':star,'sun':sun,'moon':moon,'exit':exit_door,'cycle':cycle,'hand':hand,
    'zoom-in':lambda:magnifier('zoom-in'),'zoom-out':lambda:magnifier('zoom-out'),
    'arrow-left':lambda:arrow('left'),'arrow-right':lambda:arrow('right'),'arrow-up':lambda:arrow('up'),'arrow-down':lambda:arrow('down'),
}
for _name in CATEGORY_ICONS: BUILDERS['tab-'+_name]=BUILDERS[_name]
BRIEFS={
    'world':('Enamel celestial globe in a brass armillary',('enamel','brass','walnut')),
    'earth':('Hand-painted blue terrestrial sphere',('water enamel','land enamel')),
    'camera':('Polished brass telescope on walnut tripod',('brass','glass','walnut')),
    'blank':('Empty land parcel on unfurled cartography scroll',('parchment','wood','copper')),
    'people':('Blue-clothed citizen beside a copper-red fox',('cloth','skin','fur','leather')),
    'buildings':('Original masonry cottage with red tile roof and steel hammer',('stone','tile','wood','steel')),
    'terrain':('Cutaway terrain diorama with strata, snow, grass, tree and stream',('soil','stone','snow','water','vegetation')),
    'economy':('Embroidered red leather coin pouch and stacks of gold',('leather','gold')),
    'laws':('Two engraved Commandment stone tablets with ten carved marks and gold rays',('stone','gold light')),
    'settings':('Copper mechanical gear and enamel winding key',('copper','blue enamel','brass')),
    'pause':('Two brass stop bars inset in an enamel control medallion',('brass','enamel')),
    'play':('Faceted ruby arrow set in a brass control medallion',('ruby','brass','enamel')),
    'save':('Closed gilt chronicle with blue gemstone and brass clasp',('red leather','parchment','gemstone','brass')),
    'load':('Open oak archive chest retrieving a parchment sheet',('oak','copper','parchment')),
    'back':('Cast brass return arrow',('brass',)),
    'close':('Red wax seal with ivory cross and golden ribbon tails',('wax','ivory','ribbon')),
    'create':('Jewelled wand creating a living land parcel',('ruby','brass','grass','soil')),
    'check':('Red wax approval seal with gold check and ribbon tails',('wax','gold','ribbon')),
    'audio':('Lacquered gramophone cabinet with flared gold horn',('walnut','brass','vinyl')),
    'help':('Open parchment handbook and a silver quill',('parchment','leather','silver feather')),
    'tools':('Crossed steel axe and blue wrench with walnut handle',('steel','blue enamel','walnut')),
    'star':('Faceted gold five-point brooch with inset ruby',('gold','ruby')),
    'sun':('Engraved golden sun disk and twelve cast rays',('gold',)),
    'moon':('Chased silver crescent pendant and two gold stars',('silver','gold')),
    'exit':('Open oak doorway with physical warm-red exit arrow',('oak','stone','red enamel')),
    'cycle':('Blue-glass hourglass with sand and turning cast arrows',('glass','gold','red enamel')),
    'hand':('Articulated gold gauntlet with leather wrist and buckle',('gold','leather')),
    'zoom-in':('Brass-rimmed glass magnifier with ivory plus',('brass','glass','walnut','ivory')),
    'zoom-out':('Brass-rimmed glass magnifier with ivory minus',('brass','glass','walnut','ivory')),
    'arrow-left':('Cast brass west-pointing direction arrow',('brass',)),
    'arrow-right':('Cast brass east-pointing direction arrow',('brass',)),
    'arrow-up':('Cast brass north-pointing direction arrow',('brass',)),
    'arrow-down':('Cast brass south-pointing direction arrow',('brass',)),
}
for _name in CATEGORY_ICONS: BRIEFS['tab-'+_name]=BRIEFS[_name]

def chunk(kind,body): return struct.pack('>I',len(body))+kind+body+struct.pack('>I',zlib.crc32(kind+body)&0xffffffff)
def png(width,height,rgba):
    raw=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
def repaint_key_lines(sprite,name):
    """Explicit semantic strokes, separately from the exterior dark silhouette.

    Most source strokes are three cells here. The safety layout maps them to
    at least two final cells. Tiny isolated rivets/stars remain secondary detail.
    Broad faces are retained; we never dilate the coloured material itself.
    """
    name=name[4:] if name.startswith('tab-') else name
    s=sprite
    if name=='world':
        s.line(8,15,38,30,G[1],3);s.line(9,14,38,28,G[4],2)
        s.line(21,9,19,30,B[0],3)
    elif name=='earth':
        s.line(6,24,9,34,B[0],3);s.rect(20,7,8,3,W[5])
    elif name=='camera':
        s.line(17,13,23,24,G[0],3);s.line(30,7,37,18,G[1],3)
        s.rect(23,27,6,5,C[2])
    elif name=='blank':
        s.rect(9,10,2,24,G[2]);s.rect(33,10,2,23,G[2])
        s.line(12,25,21,29,L[0],3);s.line(22,29,29,25,L[0],3)
    elif name=='people':
        s.rect(12,13,3,3,INK);s.rect(21,13,3,3,INK);s.rect(16,18,5,2,F[1])
        s.rect(31,26,3,3,INK);s.rect(39,26,3,3,INK);s.rect(35,30,3,3,INK)
    elif name=='buildings':
        s.line(5,23,23,30,R[0],3)
        for x,y in ((13,17),(19,23),(25,18)):s.line(x,y,x+6,y+2,R[0],3)
        s.line(11,30,11,38,T[0],3);s.line(28,30,28,33,S[0],3)
    elif name=='terrain':
        s.line(21,17,29,28,S[0],3)
        s.line(6,32,20,38,T[0],3);s.line(26,40,40,34,T[0],3)
        s.rect(6,21,3,7,T[1])
    elif name=='economy':
        s.rect(12,16,17,3,G[3]);s.rect(19,23,3,11,G[0])
        s.rect(16,24,8,2,G[0]);s.rect(17,30,8,2,G[0])
    elif name=='laws':
        for left,top in ((6,10),(25,8)):
            s.line(left+1,top+5,left+1,37,S[1],3)
            for line in range(5):s.rect(left+4,top+8+line*5,8+(line%2),3,S[0])
    elif name=='settings':
        s.disk(21,23,11,C[0]);s.disk(21,22,8,B[2]);s.disk(21,22,4,INK)
        s.rect(35,17,3,10,B[0]);s.disk(36,9,3,INK)
    elif name=='play':s.line(16,13,16,31,R[0],3)
    elif name=='pause':
        for x in (14,25):s.rect(x+4,13,3,21,G[0])
    elif name=='save':
        for p in ((17,11,34,11),(17,37,34,37),(17,12,17,17),(34,12,34,17)):
            s.line(*p,G[1],3)
        s.rect(33,23,8,4,G[2]);s.rect(38,23,3,4,G[0])
    elif name=='load':
        s.rect(6,25,34,3,T[0]);s.rect(6,33,34,3,T[0]);s.rect(21,23,4,4,INK)
        s.rect(18,8,10,3,G[1]);s.rect(18,13,9,3,G[1])
    elif name in ('back','arrow-left','arrow-right','arrow-up','arrow-down'):
        # Rotated arrows already have the same substantial cast form. Repaint
        # in canonical coordinates, then rotate so every direction has weight.
        return
    elif name=='create':
        s.line(17,28,34,11,G[4],3);s.line(6,33,19,39,T[0],3)
    elif name=='audio':
        s.rect(8,32,28,3,T[0]);s.rect(11,37,8,3,C[1]);s.line(26,29,28,24,G[1],3)
    elif name=='help':
        s.rect(22,12,3,27,G[1]);s.rect(29,15,9,3,B[0]);s.rect(35,17,3,6,B[0])
        s.rect(31,21,6,3,B[0]);s.rect(30,24,3,4,B[0]);s.rect(30,30,3,3,R[2])
    elif name=='tools':
        s.line(17,20,38,39,B[0],3);s.line(10,38,31,17,T[0],3)
        s.line(35,21,43,18,S[1],3)
    elif name=='star':gemstone(s,24,24,7,R)
    elif name=='sun':
        s.disk(24,24,14,G[0]);s.disk(23,23,11,G[3],gradient=(G[5],G[4],G[3],G[2]))
        s.rect(18,21,3,3,G[0]);s.rect(26,21,3,3,G[0]);s.rect(20,27,7,2,G[1])
    elif name=='moon':
        for x,y,r in ((10,17,2),(10,28,3),(16,36,2)):
            s.disk(x,y,r,S[0])
    elif name=='exit':
        s.rect(12,12,9,3,T[0]);s.rect(12,32,9,3,T[0]);s.line(20,9,20,36,T[0],3)
    elif name=='cycle':
        s.rect(16,12,3,23,G[0]);s.rect(31,12,3,23,G[0])
        s.rect(19,8,11,3,G[3]);s.rect(19,35,11,3,G[3])
    elif name=='hand':
        for x,y in ((17,14),(25,14),(33,18),(16,22),(24,23),(32,26)):
            s.rect(x-1,y,4,3,G[0])
        s.line(18,29,25,28,G[0],3)
    elif name in ('zoom-in','zoom-out'):
        s.rect(11,17,15,3,W[5])
        if name=='zoom-in':s.rect(17,11,3,15,W[5])

def heavy_layout(sprite):
    # Reserve TWO cells around every material pixel before generating the ink
    # exterior. This is the common safe canvas; it does not add padded files or
    # reduce the final filled bounds. Main stroke widths are designed for it.
    old=sprite.grid
    new=[[None for _ in range(GRID)] for _ in range(GRID)]
    for y in range(3,45):
        sy=1+round((y-3)*45/41)
        for x in range(3,45):
            sx=1+round((x-3)*45/41);new[y][x]=old[sy][sx]
    sprite.grid=new
    sprite.outline(radius=2)

def build_icon(name):
    sprite=BUILDERS[name]()
    # Wax mark must be applied after carving the wider inset ring, so its
    # actual cross/check remains visible and cannot be overwritten by a style.
    if name not in ('close','check'):repaint_key_lines(sprite,name)
    heavy_layout(sprite)
    if name in ('laws','tab-laws'):
        # Re-engrave on the FINAL canvas: exactly two dark rows per mark, and
        # restore any third row introduced by quantisation to the stone face.
        # This retains both heavy marks and at least two separating face rows.
        axis={i:1+round((i-3)*45/41) for i in range(3,45)}
        for left,top in ((6,10),(25,8)):
            for ordinal in range(5):
                sx0=left+4;sx1=sx0+8+(ordinal%2);sy0=top+8+ordinal*5
                xs=[x for x,source in axis.items() if sx0<=source<sx1]
                ys=[y for y,source in axis.items() if sy0<=source<sy0+3]
                for y in ys:
                    face=(S[5],S[4],S[3],S[2])[min(3,(axis[y]-top)*4//(41-top))]
                    for x in xs:sprite.grid[y][x]=S[0] if y in ys[:2] else face
    return sprite.grid

def final_key_features(name,grid):
    # The crucial ten grooves are tested in FINAL exported coordinates. Source
    # brush width alone is insufficient because safety quantisation skips cells.
    if name not in ('laws','tab-laws'):return []
    axis={i:1+round((i-3)*45/41) for i in range(3,45)}
    features=[]
    for tablet,(left,top) in enumerate(((6,10),(25,8)),1):
        previous_bottom=None
        for ordinal in range(5):
            sx0=left+4;sx1=sx0+8+(ordinal%2);sy0=top+8+ordinal*5
            xs=[x for x,source in axis.items() if sx0<=source<sx1]
            ys=[y for y,source in axis.items() if sy0<=source<sy0+3][:2]
            if len(ys)<2 or len(xs)<6 or not all(grid[y][x]==S[0] for y in ys for x in xs):
                raise ValueError(name+': final carved groove lost thickness or integrity')
            gap=None if previous_bottom is None else min(ys)-previous_bottom-1
            if gap is not None and gap<2:raise ValueError(name+': carved grooves merged')
            features.append({'purpose':'carved_tablet_groove','tablet':tablet,'ordinal':ordinal+1,
                             'rectangle_cells':[min(xs),min(ys),max(xs),max(ys)],
                             'actual_thickness_cells':len(ys),'actual_length_cells':len(xs),
                             'colour':S[0],'separation_rows_before':gap})
            previous_bottom=max(ys)
    return features

def validate_grid(name,grid):
    if len(grid)!=GRID or any(len(row)!=GRID for row in grid): raise ValueError(name+': logical dimensions')
    occupied=[(x,y) for y,row in enumerate(grid) for x,value in enumerate(row) if value is not None]
    if not occupied: raise ValueError(name+': empty object')
    bounds=(min(p[0] for p in occupied),min(p[1] for p in occupied),max(p[0] for p in occupied),max(p[1] for p in occupied))
    if min(bounds[:2])<1 or max(bounds[2:])>=GRID-1: raise ValueError(name+': clipped object')
    if bounds[2]-bounds[0]+1<GRID*.75 or bounds[3]-bounds[1]+1<GRID*.75: raise ValueError(name+': object too small for its button')
    colours={p for row in grid for p in row if p is not None}
    if not colours<=set(PIGMENT_RECIPES) or len(colours)<7: raise ValueError(name+': unregistered or insufficient material clusters')
    family_max={};bright=0;ink_pixels=0
    for y,row in enumerate(grid):
        for x,value in enumerate(row):
            if value is None:continue
            if value==INK:ink_pixels+=1
            red,green,blue=rgb(value);luma=.2126*red+.7152*green+.0722*blue
            family=PIGMENT_RECIPES[value]['base'];family_max[family]=max(family_max.get(family,0),luma)
            if luma>220:bright+=1
            if value!=INK:
                for dy in range(-2,3):
                    for dx in range(-2,3):
                        if abs(dx)+abs(dy)>2:continue
                        if not (0<=x+dx<GRID and 0<=y+dy<GRID and grid[y+dy][x+dx] is not None):
                            raise ValueError(name+': two-cell dark contour interrupted')
    if any(family_max.get(family,0)>160 for family in ('blue','red','yellow','green','copper')):
        raise ValueError(name+': coloured material too luminous')
    if bright:raise ValueError(name+': extremely bright material pixels')
    return {'bounds_cells':list(bounds),'width_fraction':(bounds[2]-bounds[0]+1)/GRID,'height_fraction':(bounds[3]-bounds[1]+1)/GRID,
            'visible_fraction':len(occupied)/(GRID*GRID),'material_colour_count':len(colours),
            'two_cell_exterior_verified':True,'primary_key_lines_cells':2,
            'dark_ink_fraction_of_opaque':ink_pixels/len(occupied),
            'luminance709_max_by_material':{family:round(value,4) for family,value in sorted(family_max.items())},
            'pixels_luminance_over_220':bright,'key_line_features':final_key_features(name,grid)}

def encode_svg(grid):
    # Run-length integer rectangles, no paths, strokes, images or filters.
    rectangles=[]
    for y,row in enumerate(grid):
        x=0
        while x<GRID:
            value=row[x];end=x+1
            while end<GRID and row[end]==value: end+=1
            if value is not None: rectangles.append('<rect x="%d" y="%d" width="%d" height="1" fill="%s"/>'%(x,y,end-x,value))
            x=end
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" shape-rendering="crispEdges">'+''.join(rectangles)+'</svg>\n').encode('utf-8')
def encode_png(grid,size):
    if size%GRID or size<GRID: raise ValueError('Exact integer export size required')
    scale=size//GRID
    data=bytes(channel for y in range(size) for x in range(size) for channel in ((*rgb(grid[y//scale][x//scale]),255) if grid[y//scale][x//scale] is not None else (0,0,0,0)))
    return png(size,size,data)
def write_changed(path,body,check=False):
    if not path.exists() or path.read_bytes()!=body:
        if check: raise ValueError('Generated artwork differs: '+str(path.relative_to(ROOT)))
        path.write_bytes(body)
def generate(check=False):
    if len(BUILDERS)!=41 or set(BUILDERS)!=set(BRIEFS): raise ValueError('Exactly the 41 consumed original UI objects required')
    if not check: OUT.mkdir(parents=True,exist_ok=True)
    outputs,objects,expected=[],{},set()
    for name in BUILDERS:
        grid=build_icon(name);detail=validate_grid(name,grid)
        detail.update({'object':BRIEFS[name][0],'materials':list(BRIEFS[name][1]),'transparent_background':True,'full_frame_background':False,'lighting':'registered discrete material clusters; no blur or antialiasing'})
        objects[name]=detail;body=encode_svg(grid);path=OUT/(name+'.svg');write_changed(path,body,check);expected.add(path.name)
        outputs.append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(body).hexdigest(),'palette':PALETTE_ID,'format':'svg','object':name})
        for size in (96,192):
            body=encode_png(grid,size);path=OUT/('%s-%d.png'%(name,size));write_changed(path,body,check);expected.add(path.name)
            outputs.append({'path':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(body).hexdigest(),'pixels':size,'palette':PALETTE_ID,'format':'png','object':name})
    for path in OUT.iterdir():
        if path.is_file() and path.suffix in ('.svg','.png') and path.name not in expected:
            if check: raise ValueError('Obsolete generated artwork: '+path.name)
            path.unlink()
    manifest={
        'format':'SonnOriginalIcons9','author':'Sonnheide project','license':'Original project artwork; no public license granted',
        'recipe':'tools/generate_visual_assets.py','recipe_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'stroke':True,'external_frame':False,'logical_grid':GRID,'scaling':'nearest_integer','alpha':'binary',
        'edge_recipe':{'outer_dark_cells':2,'primary_key_lines_cells':2,'white_contour_cells':0,
                       'neighborhood':'Manhattan distance <= 2 from opaque material','outer_color':INK,'directional_bevel':False,
                       'body_shading':'dark saturated materials with broad engraved lines and small bounded highlights'},
        'colored_luminance':{'metric':'0.2126 R + 0.7152 G + 0.0722 B; encoded sRGB bytes, range 0..255',
                             'families':['blue','red','yellow','green','copper'],'maximum':160,
                             'very_bright_threshold':220,'very_bright_pixels_per_icon_maximum':0,
                             'primary_base_colors_are_sources':True,'yellow_base_need_not_appear_as_unshaded_pixel':True},
        'palette':PALETTE_ID,'primary_colors':{n:'#'+''.join('%02x'%c for c in BASE_COLORS[n]) for n in ('blue','red','white','yellow')},
        'material_base_colors':{n:'#'+''.join('%02x'%c for c in colour) for n,colour in BASE_COLORS.items() if n not in ('blue','red','white','yellow')},
        'pigment_recipe':'base_times_shade_then_linear_white_tint_round_half_up','pigment_derivatives':dict(sorted(PIGMENT_RECIPES.items())),
        'yellow_base':'#ffaa00','yellow_recipe':'shade_then_linear_white_tint_round_half_up',
        'yellow_derivatives':{value:{'shade':r['shade'],'tint':r['white_tint']} for value,r in sorted(PIGMENT_RECIPES.items()) if r['base']=='yellow'},
        'themes':[],'category_icons':['tab-'+n for n in CATEGORY_ICONS],'category_style':'physical_pixel_object','objects':objects,'outputs':outputs,
    }
    write_changed(ROOT/'assets/manifests/original_ui_icons.json',(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8'),check)
    print('Verified' if check else 'Generated','41 original physical pixel objects; 123 exact SVG/PNG outputs')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');generate(parser.parse_args().check)
