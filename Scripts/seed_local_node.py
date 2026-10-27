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


def minimal_glb():
    doc = b'{"asset":{"version":"2.0"},"scenes":[{"nodes":[]}],"scene":0}'
    doc += b" " * ((4 - len(doc) % 4) % 4)
    body = struct.pack("<II", len(doc), 0x4E4F534A) + doc
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
        ("Nebula Cruiser", "Low-poly cruiser for the fleet.", "3d,ship", "cruiser.glb", minimal_glb(), png(256, 256, planet)),
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
