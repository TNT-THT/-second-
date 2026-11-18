"""Sample authorized Haru motion curves offline. Preserve originals intact in assets/models/haru."""
import json,struct,math
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/'assets/models/haru/motions';files=['haru_g_idle','haru_g_m15','haru_g_m06','haru_g_m26'];out=bytearray(b'PDTR'+struct.pack('<I',len(files)))
def curve(segments,t):
    at=2;x,y=segments[:2]
    while at<len(segments):
        typ=int(segments[at]);at+=1
        if typ==1:
            x1,y1,x2,y2,nx,ny=segments[at:at+6];at+=6
        else:nx,ny=segments[at:at+2];at+=2
        if t<nx:
            f=max(0,min(1,(t-x)/max(1e-7,nx-x)))
            if typ==1:
                a,b=0,1
                for _ in range(16):
                    u=(a+b)*.5;v=1-u;cx=v**3*x+3*v*v*u*x1+3*v*u*u*x2+u**3*nx
                    if cx<t:a=u
                    else:b=u
                u=(a+b)/2;v=1-u;return v**3*y+3*v*v*u*y1+3*v*u*u*y2+u**3*ny
            return y if typ==2 else ny if typ==3 else y+(ny-y)*f
        x,y=nx,ny
    return y
for name in files:
    j=json.loads((base/(name+'.motion3.json')).read_text());tracks=[c for c in j['Curves'] if c['Target'] in ('Parameter','PartOpacity')];frames=math.ceil(j['Meta']['Duration']*30)+1;out+=struct.pack('<II',len(tracks),frames)
    for c in tracks:
        s=c['Id'].encode();out+=bytes([len(s),int(c['Target']=='PartOpacity')])+s;out+=struct.pack('<'+'f'*frames,*(curve(c['Segments'],f/30) for f in range(frames)))
(root/'assets/haru-motion.lut').write_bytes(out)
