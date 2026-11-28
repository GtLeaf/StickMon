"""Version 5 sprite pose bank and logical-frame script serialization.

Pose reuse compares visible RGB565 pixels exactly. Each script step retains the
original virtual canvas, registration, and kind; only eligible poses are cropped.
"""
import struct

from PIL import Image


STEP = struct.Struct('<HHBBBBhhBBBB')
POSE = struct.Struct('<BBBBIII')
EXTENSION = struct.Struct('<HH')


def quantized_key(image):
    pixels = image.get_flattened_data() if hasattr(image, 'get_flattened_data') else image.getdata()
    quantized = bytes(component for r, g, b, a in pixels
                      for component in ((r & 248, g & 252, b & 248, 255)
                                        if a > 16 else (0, 0, 0, 0)))
    return image.size, quantized


def build_pose_payload(writer, kind_map, active_kinds):
    """Return (raw payload, RLE word count, palette word count, pose count).

    A fresh writer preserves the existing RLE/palette alias sharing while
    avoiding retaining superseded full-canvas encodings in the pose bank.
    """
    encoded = type(writer)()
    poses = []
    steps = bytearray()
    pose_cache = {}
    for frame in writer.frames:
        image = frame['_image']
        eligible = frame['kind'] in active_kinds
        dx = dy = flags = 0
        if eligible:
            bounds = image.getchannel('A').point(lambda a: 255 if a > 16 else 0).getbbox()
            if bounds:
                dx, dy = bounds[:2]
                image = image.crop(bounds)
            else:
                image = Image.new('RGBA', (1, 1))
        key = quantized_key(image)
        cached = pose_cache.get(key) if eligible else None
        if cached is not None:
            pose_index, flags = cached
        else:
            pose_index = len(poses)
            fmt, palette_size, offset, length, palette_offset = encoded.encode(image)
            poses.append(POSE.pack(image.width, image.height, fmt, palette_size,
                                   offset, length, palette_offset))
            if eligible:
                pose_cache[key] = (pose_index, 0)
                mirrored = quantized_key(image.transpose(Image.Transpose.FLIP_LEFT_RIGHT))
                pose_cache.setdefault(mirrored, (pose_index, 1))
        if (not 0 < frame['width'] <= 255 or not 0 < frame['height'] <= 255
                or not 0 <= frame['ground_padding'] <= 254
                or dx + image.width > frame['width'] or dy + image.height > frame['height']):
            raise ValueError('invalid sprite pose geometry')
        steps.extend(STEP.pack(kind_map[frame['kind']], pose_index,
                               frame['width'], frame['height'], frame['ground_padding'], flags,
                               dx, dy, *frame['shadow']))
    if not 0 < len(poses) <= 256 or len(encoded.palettes) > 0xFFFF:
        raise ValueError('sprite pose bank exceeds pack limits')
    words = encoded.data + encoded.palettes
    payload = bytes(steps) + b''.join(poses) + struct.pack(f'<{len(words)}H', *words)
    return payload, len(encoded.data), len(encoded.palettes), len(poses)
