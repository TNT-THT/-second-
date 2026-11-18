"""Bake the shipped 2D performance. Inputs are the straight-alpha masters in art-src/qwen/.
Optional artist tooling (not used at runtime): Pillow, numpy, scipy, opencv-python-headless.
Run from repository root. Coordinates are on the 848 x 1264 drawing, not screen pixels.
"""

import json, math
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
from scipy.interpolate import RBFInterpolator

SRC = Path("art-src/qwen")
OUT = Path("public/art/motion")
OUT.mkdir(exist_ok=True, parents=True)
W, H, ZOOM = 352, 480, 0.390
PI = math.pi
TAU = PI * 2
# Each track has its own timing; feet/hands are subsequently sampled by DISTANCE in the player.
CLIPS = {
    "idle": ("idle", 28, 3.5),
    "notice": ("idle", 16, 1.35),
    "walk": ("walk", 24, 1.0),
    "climb": ("climb", 24, 1.30),
    "glide": ("glide", 20, 1.65),
    "held": ("held", 20, 1.35),
    "eat": ("eat", 24, 1.60),
    "play": ("play", 24, 1.55),
    "pat": ("pat", 18, 1.2),
    "sleep": ("pat", 20, 3.0),
    "land": ("pat", 12, 0.38),
}


# Premultiply BEFORE resampling, so fine strands have neither green nor dark fringes.
def premul(rgba):
    a = np.asarray(rgba).astype("float32")
    a[:, :, :3] *= a[:, :, 3:] / 255
    return a.astype("uint8")


def straight(a):
    v = a.astype("float32")
    v[:, :, :3] = np.clip(v[:, :, :3] * 255 / np.maximum(1, v[:, :, 3:]), 0, 255)
    return Image.fromarray(v.astype("uint8"))


masters = {p.stem: np.array(Image.open(p).convert("RGBA")) for p in SRC.glob("*.webp")}


def rotate(pts, pivot, angle):
    a = angle * PI / 180
    mat = np.array([[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]])
    return (pts - np.array(pivot)) @ mat.T + np.array(pivot)


class Cage:
    def __init__(self):
        self.src = []
        self.dst = []

    def part(self, points, dx=0, dy=0, angle=0, pivot=(424, 500)):
        p = np.asarray(points, float).reshape(-1, 2)
        q = rotate(p, pivot, angle) + (dx, dy)
        self.src.extend(p)
        self.dst.extend(q)

    def warp(self, tex, origin):
        # Interpolate inverse skin displacement on a fine mesh. Rigid landmark islands keep face,
        # fan ribs, palms and shoes intact; in-between cloth bends instead of tearing at joints.
        self.part(
            [
                (-180, -150),
                (425, -160),
                (1020, -160),
                (-180, 650),
                (1020, 650),
                (-160, 1420),
                (420, 1420),
                (1020, 1420),
            ]
        )
        p = np.array(self.src)
        q = np.array(self.dst)
        rbf = RBFInterpolator(
            q / 800, (p - q), kernel="thin_plate_spline", smoothing=0.000001
        )
        x, y = SMALL
        pts = np.stack([(x - origin[0]) / ZOOM, (y - origin[1]) / ZOOM], axis=-1)
        offsets = rbf(pts.reshape(-1, 2) / 800).reshape(*x.shape, 2).astype("float32")
        # Coarse nodes include the last output pixel; remap the displacement field by its actual spacing.
        dense = cv2.remap(
            offsets,
            COARSE_X,
            COARSE_Y,
            cv2.INTER_LINEAR,
            borderMode=cv2.BORDER_REPLICATE,
        )
        mx = (PX - origin[0]) / ZOOM + dense[:, :, 0]
        my = (PY - origin[1]) / ZOOM + dense[:, :, 1]
        return cv2.remap(
            tex,
            mx.astype("float32"),
            my.astype("float32"),
            cv2.INTER_LINEAR,
            borderMode=cv2.BORDER_CONSTANT,
        )


PX, PY = np.meshgrid(np.arange(W, dtype="float32"), np.arange(H, dtype="float32"))
SMALL = np.meshgrid(np.linspace(0, W - 1, 64), np.linspace(0, H - 1, 85))
COARSE_X = PX * 63 / (W - 1)
COARSE_Y = PY * 84 / (H - 1)
# Clip the original white legs out of the profile drawing. Two independently articulated legs
# are drawn BACK-TO-FRONT under its opaque, repaired skirt. Unlike a single mesh, they can cross.
walk = masters["walk"].copy()
leg_mask = np.zeros(walk.shape[:2], np.uint8)
for pol in [
    [
        (358, 876),
        (398, 883),
        (421, 877),
        (425, 869),
        (447, 870),
        (439, 901),
        (415, 940),
        (345, 1225),
        (209, 1225),
        (209, 1100),
    ],
    [
        (448, 869),
        (480, 880),
        (511, 882),
        (521, 879),
        (522, 907),
        (653, 1237),
        (475, 1249),
        (434, 1030),
        (446, 910),
    ],
]:
    cv2.fillPoly(leg_mask, [np.array(pol, np.int32)], 255)
# The lower component is isolated completely even where the silhouette is concave.
leg_mask[902:, :] = 255
walk[leg_mask != 0] = 0
skirt = np.zeros_like(walk)
cv2.fillPoly(
    skirt,
    [
        np.array(
            [
                (334, 857),
                (541, 855),
                (539, 876),
                (527, 890),
                (514, 894),
                (453, 896),
                (442, 894),
                (421, 896),
                (403, 891),
                (376, 890),
                (347, 880),
            ],
            np.int32,
        )
    ],
    (106, 99, 122, 255),
    cv2.LINE_AA,
)
walk = np.where(walk[:, :, 3:] != 0, walk, skirt)
torso = premul(walk)
K = np.array([482.0, 887.0])
A = np.array([519.0, 1158.0])
hip = np.array([448.0, 595.0])
hipfar = np.array([422.0, 595.0])
source = masters["walk"]
front = np.zeros_like(source)
front[889:, 445:] = source[889:, 445:]
front[889:903, 522:] = 0
shin = front.copy()
shin[1166:] = 0
shoe = front.copy()
shoe[:1148] = 0


def over(bg, fg):
    return np.clip(fg.astype(float) + bg * (1 - fg[:, :, 3:] / 255), 0, 255).astype(
        "uint8"
    )


def leg_at(phase, far, body_y, origin):
    p = phase % 1.0
    swing = max(0, (p - 0.5) * 2)
    amp = 87.0
    base = hipfar if far else hip
    dx = amp * (1 - 4 * p) if p <= 0.5 else -amp * math.cos(PI * swing)
    lift = 82 * math.sin(PI * swing) ** 1.3
    ankle = np.array([base[0] + dx, 1157 - lift])
    h = base + np.array([0, body_y])
    v = ankle - h
    d = np.linalg.norm(v)
    e = v / d
    a, b = 299.0, np.linalg.norm(A - K)
    d = min(d, a + b - 0.01)
    t = (a * a - b * b + d * d) / (2 * d)
    knee = h + e * t + np.array([e[1], -e[0]]) * math.sqrt(max(0, a * a - t * t))

    def xy(p):
        return tuple(np.round(np.array(p) * ZOOM + origin).astype(int))

    layer = np.zeros((H, W, 4), np.uint8)
    outline = (110, 104, 129, 255)
    shade = (206, 199, 215, 255) if far else (224, 219, 234, 255)
    cv2.line(layer, xy(h), xy(knee), outline, round(70 * ZOOM), cv2.LINE_AA)
    cv2.line(layer, xy(h), xy(knee), shade, round(65 * ZOOM), cv2.LINE_AA)

    # Shin and shoe have distinct pivots; the planted shoe stays flat during stance.
    def limb(image, pivot, end, angle):
        co = math.cos(angle) * ZOOM
        si = math.sin(angle) * ZOOM
        q = np.array([[co, -si], [si, co]])
        m = np.column_stack([q, end * ZOOM + origin - q @ pivot])
        out = cv2.warpAffine(image, m, (W, H), flags=cv2.INTER_LINEAR)
        if far:
            out[:, :, :3] = (out[:, :, :3] * 0.87).astype("uint8")
        return out

    v0 = A - K
    v1 = ankle - knee
    ang = math.atan2(*v1[::-1]) - math.atan2(*v0[::-1])
    layer = over(layer, limb(SHIN, K, knee, ang))
    toe = (
        (22 * math.sin(PI * min(1, swing * 1.6)) - 7 * math.sin(PI * swing))
        if swing
        else 0
    )
    return over(layer, limb(SHOE, A, ankle, toe * PI / 180))


SHIN = premul(shin)
SHOE = premul(shoe)
# Fragment of the matching closed-eye expression for the idle blink.
eyemask = np.zeros((1264, 848), np.uint8)
cv2.ellipse(eyemask, (419, 195), (53, 23), -12, 0, 360, 255, -1, cv2.LINE_AA)
eyemask = cv2.GaussianBlur(eyemask, (13, 13), 4)[:, :, None] / 255


def render(kind, phase):
    src = CLIPS[kind][0]
    tex = premul(masters[src])
    c = Cage()
    a = phase * TAU
    s = math.sin(a)
    co = math.cos(a)
    s2 = math.sin(2 * a)
    lag = math.sin(a - 0.7)
    # Uniform scale and consistent head size across ALL poses. Only attachment origins change.
    origin = np.array([W / 2 - 425 * ZOOM, H - 3 - 1235 * ZOOM])
    head = [
        (330, 133),
        (342, 90),
        (402, 49),
        (466, 79),
        (518, 141),
        (478, 238),
        (401, 257),
        (367, 199),
    ]
    feet = [(365, 1170), (367, 1230), (427, 1219), (485, 1227), (478, 1160)]
    belt = [(365, 490), (431, 505), (518, 494)]
    if kind == "walk":
        by = 5 * math.cos(2 * a)
        c.part(
            [(353, 134), (414, 62), (493, 76), (511, 199), (491, 252), (440, 251)],
            0,
            by,
            1.4 * s,
            (445, 275),
        )
        c.part([(377, 309), (466, 287), (532, 411), (485, 509), (435, 511)], 0, by)
        # Arm swing counterbalances alternating hip rotation. Folded fan follows the wrist.
        c.part(
            [
                (301, 620),
                (461, 633),
                (398, 647),
                (405, 706),
                (445, 713),
                (550, 749),
                (492, 804),
            ],
            0,
            by,
            6.8 * s,
            (407, 312),
        )
        c.part([(160, 543), (171, 630), (255, 661)], 13 * lag, by + 4 * co)
        c.part(
            [(155, 800), (263, 858), (414, 872), (524, 879), (672, 833)],
            12 * co,
            by - 5 * math.cos(2 * a),
        )
        bg = leg_at(phase + 0.5, True, by, origin)
        bg = over(bg, leg_at(phase, False, by, origin))
        return straight(over(bg, c.warp(torso, origin)))
    if kind in ["idle", "notice", "pat", "sleep", "land", "eat"]:
        breathe = -3.4 * co
        nod = 1.7 * s
        dip = 0
        lean = 0
        if kind == "idle":
            bl = max(0, 1 - abs(phase - 0.82) / 0.051)
            tex = premul(
                (
                    masters["idle"] * (1 - eyemask * bl) + masters["pat"] * eyemask * bl
                ).astype("uint8")
            )
        if kind == "notice":
            lean = 6 + 5 * s
            nod = -4 + 2 * s
        if kind == "pat":
            dip = 13 * (1 - co)
            nod = -3 - 4 * (1 - co)
            lean = -7 * (1 - co)
        if kind == "sleep":
            dip = 14 + 3 * co
            nod = 7 + 2.3 * s
            lean = 8
        if kind == "land":
            dip = 31 * (math.sin(phase * PI) ** 2)
            nod = -3 * math.sin(phase * PI)
        if kind == "eat":
            nod = 2.4 * math.sin(2 * a)
            breathe = -4 * math.sin(2 * a)
        c.part(head, lean, breathe + dip, nod, (428, 277))
        c.part([(350, 292), (510, 301), (435, 369)], lean * 0.4, breathe + dip * 0.7)
        c.part(belt, 0, dip * 0.55 + breathe * 0.25)
        c.part(feet)
        if kind == "eat":
            # Cookie and fingers form a rigid unit; the forearm brings it to the mouth, while the saucer countersways.
            c.part(
                [
                    (395, 239),
                    (412, 250),
                    (421, 264),
                    (369, 272),
                    (361, 315),
                    (381, 328),
                ],
                -3 + 4 * co,
                7 - 13 * co,
                4.2 * s,
                (340, 405),
            )
            c.part([(335, 417), (356, 444)], 0, breathe)
            c.part(
                [(363, 422), (421, 422), (459, 430), (476, 437), (511, 444)],
                1.5 * s,
                3 * lag,
            )
        else:
            fan = 3 * s if kind != "notice" else 7 * s - 5
            c.part(
                [
                    (238, 239),
                    (296, 222),
                    (365, 246),
                    (411, 294),
                    (421, 380),
                    (305, 351),
                    (279, 325),
                    (300, 405),
                ],
                0,
                dip * 0.8 + breathe,
                fan,
                (303, 441),
            )
            c.part([(283, 463), (322, 471)], 0, dip * 0.65 + breathe * 0.4)
            c.part(
                [(550, 610), (659, 597), (623, 654), (656, 702)],
                4 * lag,
                dip * 0.5,
                1.8 * s,
                (548, 334),
            )
        c.part(
            [(165, 521), (170, 612), (236, 656)],
            9 * lag + lean * 0.4,
            4 * s + dip * 0.45,
        )
        c.part([(685, 468), (709, 556), (653, 619)], -7 * lag, 3 * co + dip * 0.45)
        c.part(
            [(162, 801), (281, 866), (401, 880), (531, 881), (684, 814)],
            4 * lag,
            dip * 0.72,
        )
        c.part([(380, 954), (473, 959)], -dip * 0.26, dip * 0.33)
    elif kind == "climb":
        # Alternating grips: planted palm travels down *relative to the rising torso*;
        # during recovery it pulls away from the wall and reaches up to the next hold.
        origin = np.array([W - 2 - 732 * ZOOM, H / 2 - 628 * ZOOM])
        c.part(
            [(333, 91), (428, 53), (488, 162), (468, 245)],
            4 * s,
            -5 * co,
            1.6 * s,
            (456, 266),
        )
        c.part([(377, 277), (478, 285), (534, 332)], 5 * s, 3 * co)
        c.part([(382, 492), (485, 497)], 9 * s, 5 * lag)
        for off, pts, elbow in [
            (
                0,
                [(684, 110), (711, 36), (728, 49), (702, 115), (676, 144)],
                [(592, 216), (627, 247)],
            ),
            (
                0.5,
                [(691, 310), (717, 244), (732, 257), (711, 334), (671, 360)],
                [(558, 375), (584, 411)],
            ),
        ]:
            v = (phase + off) % 1
            lift = (v * 4 - 1) if v < 0.5 else math.cos(PI * (v - 0.5) * 2)
            release = math.sin(PI * max(0, (v - 0.5) * 2))
            c.part(pts, -20 * release, (32 + 32 * lift) if off == 0 else 49 * lift)
            c.part(elbow, -15 * release, (16 + 16 * lift) if off == 0 else 25 * lift)
        c.part([(625, 587), (687, 594), (722, 682)], -11 * co, -25 * co)
        c.part([(613, 843), (596, 886), (704, 909), (725, 887)], -16 * (1 + co), 59 * s)
        c.part([(367, 1090), (338, 1144), (409, 1222)], 16 * s, -21 - 22 * s)
        c.part([(219, 870), (381, 896), (496, 858)], 10 * lag, -13 * co)
        c.part([(163, 515), (188, 600), (246, 659)], 16 * lag, -9 * s)
    elif kind == "glide":
        # A seated fan-rider: independently swinging ankles, fan pitch and hair lag in the airstream.
        origin = np.array([W / 2 - 420 * ZOOM, 7 - 133 * ZOOM])
        c.part(
            [(363, 212), (401, 153), (482, 137), (519, 208), (493, 300)],
            3 * s,
            3 * co,
            2.3 * s,
            (487, 316),
        )
        c.part(
            [(529, 145), (573, 193), (600, 264), (658, 287)],
            3 * s,
            4 * co,
            2 * s,
            (487, 316),
        )
        c.part([(333, 371), (493, 404), (395, 519), (499, 530)], 1 * s, 4 * co)
        c.part(
            [(41, 500), (69, 488), (115, 565), (207, 578), (243, 482)],
            14 * lag,
            -24 * lag,
        )
        c.part(
            [(42, 873), (193, 1010), (436, 1055), (660, 956), (800, 727), (756, 595)],
            0,
            6 * co,
            2.6 * s,
            (438, 713),
        )
        c.part([(589, 704), (663, 703)], 1 * s, 6 * co)
        c.part([(581, 922), (613, 945), (639, 1007), (652, 1005)], 12 * lag, 6 * co)
        c.part(
            [(705, 955), (753, 982), (798, 1028), (735, 1034)],
            9 * math.sin(a - 1.3),
            7 * s,
        )
    elif kind == "held":
        c.part(head, 0, 1.8 * s, 2 * s, (432, 266))
        c.part([(342, 300), (486, 297), (438, 424)], 0, 2 * co)
        c.part(belt, 6 * s, 4 * co)
        c.part([(523, 83), (543, 93), (679, 181), (666, 262)], 0, 0, 2 * s, (493, 281))
        c.part(
            [(145, 583), (203, 546), (62, 641), (100, 712)],
            16 * lag,
            6 * co,
            5 * s,
            (319, 410),
        )
        c.part([(148, 798), (353, 859), (641, 845)], 20 * lag, 6 * co)
        c.part(
            [(507, 1084), (538, 1084), (569, 1126), (571, 1190), (547, 1214)],
            18 * lag,
            -7 * co,
        )
        c.part(
            [(600, 968), (632, 959), (665, 1001), (669, 1070), (645, 1097)],
            9 * math.sin(a - 0.3),
            9 * s,
        )
        c.part([(93, 466), (157, 412), (165, 612), (687, 625)], 17 * lag, 10 * co)
    elif kind == "play":
        c.part(head, 9 * s, -7 * co, -4 * s, (421, 270))
        c.part([(360, 307), (483, 296), (436, 393)], 4 * s, -5 * co)
        c.part(belt, 6 * s, -7 * co)
        # The rigid fan sweeps about a real wrist; elbows follow the shoulder counterturn.
        c.part(
            [
                (36, 145),
                (122, 108),
                (196, 116),
                (278, 205),
                (280, 286),
                (146, 255),
                (111, 246),
                (179, 297),
            ],
            11 * s,
            -9 * co,
            14 * s,
            (180, 302),
        )
        c.part([(167, 342), (174, 404), (238, 380)], 4 * s, -8 * co)
        c.part(
            [(643, 537), (711, 457), (798, 494), (808, 527)],
            -8 * s,
            14 * co,
            -5 * s,
            (514, 323),
        )
        c.part([(166, 819), (423, 887), (730, 761), (754, 700)], 17 * lag, -14 * co)
        c.part([(697, 977), (781, 1079)], 13 * s, -22 * co, 7 * s, (587, 821))
        c.part([(396, 1208), (445, 1227)])
        c.part([(165, 529), (213, 639), (673, 624)], 19 * lag, 4 * s)
    return straight(c.warp(tex, origin))


manifest = {}
for name, (src, n, duration) in CLIPS.items():
    cols = 4
    atlas = Image.new("RGBA", (W * cols, H * math.ceil(n / cols)))
    for i in range(n):
        atlas.paste(render(name, i / n), (i % cols * W, i // cols * H))
    atlas.save(OUT / (name + ".webp"), quality=89, method=4, exact=False)
    manifest[name] = {
        "frames": n,
        "cols": cols,
        "width": W,
        "height": H,
        "seconds": duration,
    }
    print(name, n, "frames", flush=True)
Path("core/qwen-atlas.json").write_text(json.dumps(manifest, indent=2) + "\n")
