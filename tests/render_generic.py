"""Test-only adapters: decode atlases with Pillow and save off-screen meshes for each official model.

The shipped Windows app decodes the same atlases through WIC; this script exists so Linux and CI can
compare real meshes without a desktop. It never supplies art outside the pinned model directories.
"""
import hashlib,subprocess,sys,tempfile
from pathlib import Path
from PIL import Image

root=Path(__file__).resolve().parents[1]
scratch=Path(sys.argv[2]) if len(sys.argv)>2 else Path(tempfile.mkdtemp())
models=[("haru",root/"assets/models/haru","Haru.moc3"),("hiyori",root/"tests/models/hiyori","Hiyori.moc3")]
for tag,folder,moc in models:
    out=scratch/tag;out.mkdir(parents=True,exist_ok=True)
    for atlas in folder.glob("*.2048"):
        for f in atlas.glob("*.png"):
            (out/(f.stem+'.bgra')).write_bytes(Image.open(f).convert('RGBA').resize((1024,1024),Image.Resampling.LANCZOS).tobytes('raw','BGRA'))
    run=subprocess.run([str(Path(sys.argv[1]).resolve()),str(root),str(scratch),str(folder),moc,tag],
                       check=True,capture_output=True,text=True)
    print(run.stdout.strip())
    # Same kernel, different real rigs: only actually bound physics may be modulated.
    expect=("bind sidelock: yes","bind skirt: no") if tag=="haru" else ("bind skirt: yes","bind sidelock: no")
    for line in expect:assert line in run.stdout,f"{tag}: {line} missing"
    for i in range(3):
        im=Image.frombytes('RGBa',(320,320),(out/f'pose{i}.bgra').read_bytes(),'raw','BGRa').convert('RGBA')
        im.save(out/f'{tag}{i}.png')
    print(tag,moc,hashlib.sha256((folder/moc).read_bytes()).hexdigest()[:16],'rendered 3 frames')
