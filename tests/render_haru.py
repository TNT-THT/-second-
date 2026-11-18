"""Only the test adapter decodes PNG via Pillow. Windows uses WIC. This is not a substitute sprite."""
import subprocess,sys,tempfile
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1];scratch=Path(sys.argv[2]) if len(sys.argv)>2 else Path(tempfile.mkdtemp());scratch.mkdir(exist_ok=True,parents=True)
for f in (root/'assets/models/haru/Haru.2048').glob('*.png'):(scratch/(f.stem+'.bgra')).write_bytes(Image.open(f).convert('RGBA').resize((1024,1024),Image.Resampling.LANCZOS).tobytes('raw','BGRA'))
subprocess.run([str(Path(sys.argv[1]).resolve()),str(root),str(scratch)],check=True)
for i in range(3):
    im=Image.frombytes('RGBa',(320,320),(scratch/f'pose{i}.bgra').read_bytes(),'raw','BGRa').convert('RGBA');im.save(scratch/f'haru{i}.png')
