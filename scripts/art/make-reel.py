"""Make the README's short performance sampler from production frames (not a desktop recording)."""

from pathlib import Path
import json, math
from PIL import Image, ImageDraw, ImageFont, ImageOps

meta = json.loads(Path("core/qwen-atlas.json").read_text())
out = []
W, H = 540, 306
rate = 14
back = ImageOps.fit(
    Image.open("public/art/terrace.webp").convert("RGB"), (W, H), centering=(0.5, 0.55)
).convert("RGBA")
try:
    font = ImageFont.truetype("DejaVuSansMono.ttf", 11)
except OSError:
    font = ImageFont.load_default()
reels = [
    ("idle", "AT REST", 1, 1.2),
    ("walk", "WALK RIGHT", 1, 1.4),
    ("walk", "WALK LEFT", -1, 1.4),
    ("climb", "EDGE CLIMB", 1, 1.8),
    ("glide", "FAN GLIDE", -1, 1.8),
    ("held", "PICK UP", 1, 1.3),
    ("eat", "TEA BREAK", 1, 1.8),
    ("play", "FAN DANCE", 1, 1.8),
    ("pat", "CURTSY", 1, 1.2),
]
for order, (key, title, face, seconds) in enumerate(reels, 1):
    s = meta[key]
    atlas = Image.open("public/art/motion/" + key + ".webp")
    sz = (157, 214)
    for i in range(round(seconds * rate)):
        t = i / rate
        phase = t / s["seconds"]
        n = int(phase % 1 * s["frames"])
        x = n % s["cols"] * s["width"]
        y = n // s["cols"] * s["height"]
        im = atlas.crop((x, y, x + s["width"], y + s["height"])).resize(
            sz, Image.Resampling.LANCZOS
        )
        if face < 0:
            im = ImageOps.mirror(im)
        px = (W - sz[0]) // 2
        py = H - sz[1] - 14
        if key == "walk":
            px += int((t if face > 0 else seconds - t) * 214 * 4 * 87 * 0.39 / 480 - 42)
        if key == "climb":
            px = W - sz[0]
            py = int(H - sz[1] - t * 41)
        if key == "glide":
            px = W - sz[0] - int(t * 76)
            py = 0
        if key == "held":
            px += int(10 * math.sin(t * 3))
            py -= 15
        canvas = back.copy()
        canvas.alpha_composite(im, (px, py))
        label = Image.new("RGBA", (W, H))
        d = ImageDraw.Draw(label)
        d.rounded_rectangle((10, 10, 111, 30), radius=5, fill=(50, 43, 65, 180))
        d.text((19, 14), "MOTION ATELIER", fill="#fffaff", font=font)
        d.rounded_rectangle((10, H - 29, 145, H - 9), radius=5, fill=(50, 43, 65, 180))
        d.text((18, H - 26), f"{order:02d} / {title}", fill="white", font=font)
        out.append(Image.alpha_composite(canvas, label).convert("RGB"))
out[0].save(
    "docs/motion-preview.webp",
    save_all=True,
    append_images=out[1:],
    duration=round(1000 / rate),
    loop=0,
    quality=85,
    method=4,
)
