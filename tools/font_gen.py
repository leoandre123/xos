from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "UbuntuMono[wght].ttf"
FONT_SIZE = 16
GLYPH_WIDTH = 8
GLYPH_HEIGHT = 16
FIRST_CHAR = 32
LAST_CHAR = 126
THRESHOLD = 128

OUTPUT_C = "kernel/font.c"
OUTPUT_H = "kernel/font.h"

font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
ascent, descent = font.getmetrics()
font_height = ascent + descent

# shared vertical offset for all glyphs
y_offset = (GLYPH_HEIGHT - font_height) // 2

def render_glyph(ch: str):
    img = Image.new("L", (GLYPH_WIDTH, GLYPH_HEIGHT), 0)
    draw = ImageDraw.Draw(img)

    bbox = draw.textbbox((0, 0), ch, font=font)
    if bbox:
        x0, y0, x1, y1 = bbox
        glyph_w = x1 - x0

        x = (GLYPH_WIDTH - glyph_w) // 2 - x0
        y = y_offset

        draw.text((x, y), ch, fill=255, font=font)

    rows = []
    for row in range(GLYPH_HEIGHT):
        byte = 0
        for col in range(GLYPH_WIDTH):
            if img.getpixel((col, row)) >= THRESHOLD:
                byte |= (1 << (7 - col))
        rows.append(byte)

    return rows, img

glyphs = {}
for code in range(FIRST_CHAR, LAST_CHAR + 1):
    ch = chr(code)
    rows, _ = render_glyph(ch)
    glyphs[code] = rows

with open(OUTPUT_H, "w", encoding="utf-8") as f:
    f.write("#pragma once\n")
    f.write("#include \"types.h\"\n\n")
    f.write(f"#define FONT_FIRST_CHAR {FIRST_CHAR}\n")
    f.write(f"#define FONT_LAST_CHAR {LAST_CHAR}\n")
    f.write(f"#define FONT_GLYPH_WIDTH {GLYPH_WIDTH}\n")
    f.write(f"#define FONT_GLYPH_HEIGHT {GLYPH_HEIGHT}\n")
    f.write(f"#define FONT_GLYPH_COUNT {LAST_CHAR - FIRST_CHAR + 1}\n\n")
    f.write(
        f"extern const ubyte kernel_font[FONT_GLYPH_COUNT][FONT_GLYPH_HEIGHT];\n"
    )

with open(OUTPUT_C, "w", encoding="utf-8") as f:
    f.write('#include "font.h"\n\n')
    f.write(
        f"const ubyte kernel_font[FONT_GLYPH_COUNT][FONT_GLYPH_HEIGHT] = {{\n"
    )

    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        ch = chr(code)
        rows = glyphs[code]
        safe_comment = ch
        if ch == "\\":
            safe_comment = "\\\\"
        elif ch == "'":
            safe_comment = "\\'"
        f.write(f"    /* {code} '{safe_comment}' */ {{ ")
        f.write(", ".join(f"0x{row:02X}" for row in rows))
        f.write(" },\n")

    f.write("};\n")

print(f"Generated {OUTPUT_H} and {OUTPUT_C}")