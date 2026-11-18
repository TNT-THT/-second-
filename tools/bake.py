"""Authoring tool (not shipped). Bake small premultiplied sprites and 44.1 kHz mono PCM.
Only deterministic synthetic sound design; no microphone or third-party recordings.
"""
from pathlib import Path
from math import pi,sin,cos,exp
import random,struct
from PIL import Image, ImageDraw
out=Path(__file__).resolve().parents[1]/'assets'
seed=random.Random(32017)
def wav(name,duration,fn):
    vals=[fn(i/44100) for i in range(round(44100*duration))];peak=max(abs(x) for x in vals) or 1
    (out/(name+'.pcm')).write_bytes(struct.pack('<'+'h'*len(vals),*(round(32760*x/peak) for x in vals)))
def env(t,n):return min(1,t/.015,max(0,(n-t)/.045))
def tone(f,t):return sin(2*pi*f*t)+.12*sin(4*pi*f*t)
wav('ime',.3,lambda t:env(t,.3)*exp(-8*t)*tone(660,t))
wav('chime',.35,lambda t:env(t,.35)*exp(-8*t)*(tone(784,t)+.5*tone(1174.7,t)))
wav('satisfied',.8,lambda t:env(t,.8)*sin(pi*t/.8)**2*(tone(293.66+16*sin(t*7),t)+.4*tone(440,t)))
noise=0
def breath(t,n,cry):
    global noise
    noise=.94*noise+.06*seed.uniform(-1,1)
    return env(t,n)*sin(pi*t/n)**2*(noise*(1.3 if not cry else .6)+(.08+.04*sin(9*t))*tone(270+17*sin(17*t) if cry else 175-42*t,t))
wav('sigh',.6,lambda t:breath(t,.6,False));wav('sob',.8,lambda t:breath(t,.8,True))
wav('steps',.11,lambda t:env(t,.11)*exp(-55*t)*(sin(2*pi*140*t)+.25*seed.uniform(-1,1)))
wav('cracker',.38,lambda t:env(t,.38)*seed.uniform(-1,1)*exp(-30*(t% .12))*(1-t/.38))
notes=[(67,.75),(67,.25),(69,1),(67,1),(72,1),(71,2),(67,.75),(67,.25),(69,1),(67,1),(74,1),(72,2),(67,.75),(67,.25),(79,1),(76,1),(72,1),(71,1),(69,2),(77,.75),(77,.25),(76,1),(72,1),(74,1),(72,2)]
unit=8/sum(d for _,d in notes)
def music(t):
    for pitch,beats in notes:
        dur=beats*unit
        if t<dur:return env(t,dur)*exp(-3*t)*tone(440*2**((pitch-69)/12),t)
        t-=dur
    return 0
wav('birthday',8,music)
S=4
for kind,frames in [('star',8),('flowers',12),('heart',8),('lantern',6)]:
    atlas=bytearray()
    for frame in range(frames):
        t=frame/(frames-1); e=1-(1-t)**3
        img=Image.new('RGBA',(64*S,64*S));d=ImageDraw.Draw(img)
        def dot(x,y,r,fill):d.ellipse(tuple(int(a*S) for a in (x-r,y-r,x+r,y+r)),fill)
        def poly(points,col):d.polygon([(round(x*S),round(y*S)) for x,y in points],col)
        if kind=='star':
            x=30+4*e;y=12+31*e;alpha=round(255*(1-t));poly([(x+(11 if i%2==0 else 4.8)*cos(i*pi/5-pi/2),y+(11 if i%2==0 else 4.8)*sin(i*pi/5-pi/2)) for i in range(10)],(255,215,0,alpha))
        elif kind=='flowers':
            for i in range(12):
                a=i*2*pi/12+.2;r=(8+19*e)*(1 if i%2 else .65);x=32+r*cos(a);y=31+r*sin(a)+4*t;alpha=round(220*(1-t));c=(249,181,201,alpha) if i%2 else (210,196,252,alpha)
                for j in range(5):dot(x+2*cos(j*2*pi/5),y+2*sin(j*2*pi/5),1.6,c)
                dot(x,y,1,(253,227,170,alpha))
        elif kind=='heart':
            x=32;y=38-18*e;a=round(240*(1-t*t));poly([(x+13*sin(k/50*2*pi)**3,y-(13*cos(k/50*2*pi)-5*cos(2*k/50*2*pi)-2*cos(3*k/50*2*pi)-cos(4*k/50*2*pi))*.85) for k in range(51)],(255,183,197,a))
        else:
            x=32+3*sin(2*pi*frame/frames);dot(x,30,13,(244,157,125,248));d.line((x*S,14*S,x*S,8*S),fill=(232,188,120,230),width=S);d.line((x*S,43*S,x*S,50*S),fill=(255,196,139,245),width=2*S)
        for r,g,b,a in img.resize((64,64),Image.Resampling.LANCZOS).getdata():atlas.extend((round(b*a/255),round(g*a/255),round(r*a/255),a))
    (out/('fx-'+kind+'.bgra')).write_bytes(atlas)
