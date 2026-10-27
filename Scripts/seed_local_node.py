"""
Fills a LOCAL test node with a demo creator and a few seeds (image, 3D, audio)
with generated thumbnails, so the feed has something to show.

    py -3 Scripts/seed_local_node.py http://127.0.0.1:8000

Refuses non-local nodes. Prints seed ids only; the demo password is random and never shown.
"""
import io
import math
import secrets
import struct
import sys
import zlib
from urllib.parse import urlparse

import httpx


def png(width, height, pixel):
    rows = []
    for y in range(height):
        rows.append(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(width)))
    raw = b"".join(rows)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def gradient(a, b):
    def px(x, y):
        t = (x + y) / 510.0
        return [int(a[i] + (b[i] - a[i]) * t) for i in range(3)]
    return px


def planet(x, y):
    dx, dy = x - 128, y - 128
    if dx * dx + dy * dy < 80 * 80:
        return [40 + (x // 4) % 60, 90 + y // 3, 200]
    return [5, 8, 25] if (x * 7 + y * 13) % 97 else [255, 255, 255]


def planet_glb():
    """A textured UV sphere with a coloured ring: ~3k triangles, embedded PNG."""
    import json
    pos, nrm, uv, idx = [], [], [], []
    seg, rings = 48, 24
    for r in range(rings + 1):
        v = r / rings
        phi = math.pi * v
        for s_ in range(seg + 1):
            u = s_ / seg
            th = 2 * math.pi * u
            x, y, z = math.sin(phi) * math.cos(th), math.cos(phi), math.sin(phi) * math.sin(th)
            pos += [x, y, z]; nrm += [x, y, z]; uv += [u, v]
    for r in range(rings):
        for s_ in range(seg):
            a, b = r * (seg + 1) + s_, (r + 1) * (seg + 1) + s_
            idx += [a, b, a + 1, a + 1, b, b + 1]
    sphere_vcount = len(pos) // 3
    ring_start = len(idx)
    for i in range(64 + 1):
        t = 2 * math.pi * i / 64
        for rad in (1.5, 2.1):
            pos += [rad * math.cos(t), 0.0, rad * math.sin(t)]; nrm += [0, 1, 0]; uv += [0, 0]
    for i in range(64):
        a = sphere_vcount + i * 2
        idx += [a, a + 1, a + 2, a + 2, a + 1, a + 3]
    tex = png(256, 128, lambda x, y: [40 + (x * 3 + y) % 90, 80 + (y * 2) % 120, 200 - (x // 3) % 80])

    def pack(fmt, vals):
        return struct.pack("<%d%s" % (len(vals), fmt), *vals)
    bpos, bnrm, buv = pack("f", pos), pack("f", nrm), pack("f", uv)
    bidx_s, bidx_r = pack("I", idx[:ring_start]), pack("I", idx[ring_start:])
    parts, views, off = [], [], 0
    for blob in (bpos, bnrm, buv, bidx_s, bidx_r, tex):
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(blob)})
        pad = (4 - len(blob) % 4) % 4
        parts.append(blob + b"\x00" * pad)
        off += len(blob) + pad
    vc = len(pos) // 3
    xs, ys, zs = pos[0::3], pos[1::3], pos[2::3]
    doc = {
        "asset": {"version": "2.0", "generator": "GFC seed_local_node.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "rotation": [0.1305, 0, 0, 0.9914]}],
        "meshes": [{"primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0},
            {"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 4, "material": 1}]}],
        "materials": [
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0, "roughnessFactor": 0.7}},
            {"pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.55, 0.1, 1.0], "metallicFactor": 0.3, "roughnessFactor": 0.4}}],
        "textures": [{"source": 0}], "images": [{"bufferView": 5, "mimeType": "image/png"}],
        "buffers": [{"byteLength": off}], "bufferViews": views,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": vc, "type": "VEC3", "min": [min(xs), min(ys), min(zs)], "max": [max(xs), max(ys), max(zs)]},
            {"bufferView": 1, "componentType": 5126, "count": vc, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": vc, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5125, "count": ring_start, "type": "SCALAR"},
            {"bufferView": 4, "componentType": 5125, "count": len(idx) - ring_start, "type": "SCALAR"}],
    }
    j = json.dumps(doc).encode()
    j += b" " * ((4 - len(j) % 4) % 4)
    b = b"".join(parts)
    body = struct.pack("<II", len(j), 0x4E4F534A) + j + struct.pack("<II", len(b), 0x004E4942) + b
    return struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body


def wav():
    rate, n = 8000, 8000
    data = b"".join(struct.pack("<h", int(8000 * math.sin(2 * math.pi * 440 * i / rate))) for i in range(n))
    fmt = struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
    return b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " + fmt + b"data" + struct.pack("<I", len(data)) + data


def main():
    node = (sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8000").rstrip("/")
    if urlparse(node).hostname not in ("127.0.0.1", "localhost", "::1"):
        sys.exit("Refusing to seed a non-local node.")
    c = httpx.Client(base_url=node, timeout=20)
    user, password = f"demo_creator_{secrets.token_hex(3)}", secrets.token_urlsafe(18)
    r = c.post("/auth/register", data={"username": user, "password": password})
    r.raise_for_status()
    h = {"Authorization": f"Bearer {r.json()['token']}"}
    c.post("/wallets/create", data={"name": "creator"}, headers=h).raise_for_status()

    sunrise = png(256, 256, gradient((255, 120, 20), (40, 0, 90)))
    seeds = [
        ("Andromeda Sunrise", "A sunrise over a gas giant.", "image,space", "andromeda.png", sunrise, sunrise),
        ("Ringed World", "A textured planet with a ring, made by the seed script.", "3d,planet", "ringed_world.glb", planet_glb(), png(256, 256, planet)),
        ("Pulsar Loop", "A 440 Hz pulsar ping.", "audio,loop", "pulsar.wav", wav(),
         png(256, 256, gradient((10, 200, 180), (5, 20, 60)))),
    ]
    for title, desc, tags, name, data, thumb in seeds:
        r = c.post("/seeds/upload", headers=h,
                   data={"title": title, "description": desc, "tags": tags},
                   files={"file": (name, io.BytesIO(data)), "thumbnail": ("thumb.jpg", io.BytesIO(thumb), "image/png")})
        r.raise_for_status()
        print(r.json()["id"])


if __name__ == "__main__":
    main()
