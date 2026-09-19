"""Measure local horizontal disparity in the captured 5120x1440 gameplay frame.
Read-only image analysis; coordinates below belong to captures/depth-check.bmp.
Positive/negative shifts are source-image correspondence, not physical IPD.
"""
from PIL import Image, ImageChops, ImageStat
from pathlib import Path
import json

path = Path('captures/depth-check.bmp')
im = Image.open(path).convert('L')
assert im.size == (5120, 1440), 'Patch coordinates require the original capture'
w = im.width // 2
left = im.crop((0, 0, w, im.height))
right = im.crop((w, 0, 2*w, im.height))
patches = {
    'player': (1060, 800, 1190, 1000),
    'ground_near': (800, 1160, 1000, 1300),
    'left_pillar': (340, 250, 500, 650),
    'right_pillar': (1780, 400, 1980, 650),
    'far_arch': (1150, 450, 1300, 650),
    'sky': (1100, 130, 1280, 300),
}
results = {}
for name, box in patches.items():
    patch = left.crop(box)
    scores = []
    for dx in range(-100, 101):
        candidate = right.crop((box[0]+dx, box[1], box[2]+dx, box[3]))
        error = ImageStat.Stat(ImageChops.difference(patch, candidate)).mean[0]
        scores.append((error, dx))
    error, shift = min(scores)
    results[name] = {'source_shift_pixels': shift, 'mean_absolute_error': error,
                     'predicted_shift_after_swap_and_infinity_alignment': -shift-53}
print(json.dumps({'capture': str(path), 'patches': results}, indent=2))
