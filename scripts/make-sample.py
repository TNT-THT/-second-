"""Rebuild the original animated, texture-free GLB sample. Python stdlib only."""
import struct, json, pathlib
v=[]; normals=[]; idx=[]; chunks=bytearray(); views=[]; acc=[]
# Six square faces; separate vertices preserve the blocky normals.
for corners,n in [([(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)],(0,0,1)), ([(1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)],(0,0,-1)),([(-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)],(-1,0,0)), ([(1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)],(1,0,0)),([(-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)],(0,1,0)),([(-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1)],(0,-1,0))]:
 i=len(v);v.extend(corners);normals.extend([n]*4);idx.extend([i,i+1,i+2,i,i+2,i+3])
def buf(values,kind,ctype,fmt,**meta):
 while len(chunks)%4:chunks.append(0)
 data=b''.join(struct.pack('<'+fmt,*(p if isinstance(p,tuple) else (p,))) for p in values);views.append(dict(buffer=0,byteOffset=len(chunks),byteLength=len(data)));chunks.extend(data);acc.append(dict(bufferView=len(views)-1,componentType=ctype,count=len(values),type=kind,**meta));return len(acc)-1
buf(v,'VEC3',5126,'fff',min=[-1]*3,max=[1]*3);buf(normals,'VEC3',5126,'fff');buf(idx,'SCALAR',5123,'H');t=buf([0.,.6,1.2],'SCALAR',5126,'f',min=[0],max=[1.2]);s=buf([(1.,1.,1.),(1.04,.94,1.03),(1.,1.,1.)],'VEC3',5126,'fff');nodes=[{'name':'Mint','children':[]}]
for name,pos,scale,mat in [('body',(0,.72,0),(.72,.6,.48),0),('cap',(0,1.38,0),(.48,.10,.38),0),('left eye',(-.3,.85,.49),(.07,.11,.025),1),('right eye',(.3,.85,.49),(.07,.11,.025),1),('smile',(0,.62,.49),(.1,.035,.025),1),('cheek',(-.56,.63,.49),(.09,.045,.02),2),('cheek',(.56,.63,.49),(.09,.045,.02),2),('leaf',(.12,1.58,0),(.10,.17,.07),3)]:
 nodes[0]['children'].append(len(nodes));nodes.append(dict(name=name,mesh=mat,translation=pos,scale=scale))
g={'asset':{'version':'2.0','generator':'PetDesk sample'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':nodes,'buffers':[{'byteLength':len(chunks)}],'bufferViews':views,'accessors':acc,'meshes':[{'primitives':[{'attributes':{'POSITION':0,'NORMAL':1},'indices':2,'material':i}]} for i in range(4)],'materials':[{'pbrMetallicRoughness':{'baseColorFactor':[*c,1],'metallicFactor':0,'roughnessFactor':1}} for c in [[.24,.72,.53],[.09,.08,.17],[.96,.5,.58],[.38,.64,.22]]],'animations':[{'name':'Idle','channels':[{'sampler':0,'target':{'node':0,'path':'scale'}}],'samplers':[{'input':t,'output':s,'interpolation':'LINEAR'}]}]}
j=json.dumps(g,separators=(',',':')).encode();j+=b' '*(-len(j)%4);chunks+=b'\0'*(-len(chunks)%4);out=struct.pack('<4sII',b'glTF',2,28+len(j)+len(chunks))+struct.pack('<I4s',len(j),b'JSON')+j+struct.pack('<I4s',len(chunks),b'BIN\0')+chunks;pathlib.Path('public/models/Mint.glb').write_bytes(out)
