"""Build-only exact allowlist. Never walk data/, config/ or assets/ to discover personal files."""
from pathlib import Path
import hashlib,sys,zipfile,json
from audit import source,binary
root=Path(__file__).resolve().parents[1];out=root/'out';out.mkdir(exist_ok=True);source();binary(sys.argv[1]);release='PetDesk-2.0.0-Haru-Preview';app=release+'-Windows-x64';ext=release+'-Luogu-Optional';builtin=sorted(x for x in (root/'assets/models/builtin').rglob('*') if x.is_file())
assets=['chat.txt','haru-motion.lut',*[x+'.pcm' for x in ('birthday','ime','chime','cracker','satisfied','sigh','sob','steps')],*['fx-'+x+'.bgra' for x in ('star','heart','flowers','lantern')]]
def package(name,files,dirs=()):
    p=out/(name+'.zip')
    with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for d in dirs:z.writestr(name+'/'+d+'/',b'')
        for src,dst in files:z.write(src,name+'/'+dst)
    with zipfile.ZipFile(p) as z:assert z.testzip() is None
    digest=hashlib.sha256(p.read_bytes()).hexdigest();(out/(p.name+'.sha256')).write_text(digest+'  '+p.name+'\n',encoding='ascii');print(p.name,p.stat().st_size,digest)
package(app,[(sys.argv[1],'PetDesk.exe'),(root/'docs/LOCAL-TEST.md','START-HERE.zh-CN.md'),(root/'vendor/json/LICENSE.MIT','LICENSE-json.txt'),(root/'vendor/purism/LICENSE','LICENSE-Purism.txt'),(root/'vendor/purism/ORIGIN.txt','Runtime-origin.txt'),(root/'config/intent_mapping.json','config/intent_mapping.json'),(root/'config/policy.json','config/policy.json'),*((root/'assets/models/haru'/x,'assets/models/haru/'+x) for x in json.loads((root/'tools/haru-lock.json').read_text())),(root/'config/settings.json','config/settings.json'),(root/'config/builtin_models.json','config/builtin_models.json'),(root/'config/feed_limits.json','config/feed_limits.json'),(root/'assets/models/haru/model_adapter.json','assets/models/haru/model_adapter.json'),*((x,'assets/models/builtin/'+x.relative_to(root/'assets/models/builtin').as_posix()) for x in builtin),*((root/'assets'/x,'assets/'+x) for x in assets)],['data'])
package(ext,[(root/'extension'/x,x) for x in ('manifest.json','site.js','worker.js','pair.js','pair.html')]+[(root/'docs/LUOGU.md','README.zh-CN.md')])
