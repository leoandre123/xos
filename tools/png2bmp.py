#!/usr/bin/env python3
"""Convert PNG files to the XOS raw bitmap format (uint32 width, height, then 0xAARRGGBB pixels)."""

import struct
import sys
from pathlib import Path
from PIL import Image


def convert(src: Path, dst: Path) -> None:
    img = Image.open(src).convert("RGBA")
    w, h = img.size
    pixels = img.load()

    with open(dst, "wb") as f:
        f.write(struct.pack("<II", w, h))
        for y in range(h):
            for x in range(w):
                r, g, b, a = pixels[x, y]
                f.write(struct.pack("<I", (a << 24) | (r << 16) | (g << 8) | b))

    print(f"{src} -> {dst}  ({w}x{h})")


def main() -> None:
    args = sys.argv[1:]
    if not args:
        print(f"usage: {sys.argv[0]} <file.png> [file2.png ...] [-o out.bmp]")
        sys.exit(1)

    # Simple -o flag: only valid when converting a single file
    if "-o" in args:
        idx = args.index("-o")
        if idx + 1 >= len(args):
            print("error: -o requires an output path")
            sys.exit(1)
        dst = Path(args[idx + 1])
        srcs = [Path(a) for a in args if a not in ("-o", args[idx + 1])]
        if len(srcs) != 1:
            print("error: -o can only be used with a single input file")
            sys.exit(1)
        convert(srcs[0], dst)
    else:
        for src in (Path(a) for a in args):
            convert(src, src.with_suffix(".bmp"))


if __name__ == "__main__":
    main()
