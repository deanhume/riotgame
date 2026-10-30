"""Original pixel-art branding. No fonts, image packages, or downloaded artwork."""

from pathlib import Path
import struct
import sys
import zlib


INK = "#182939"
NAVY = "#111e2c"
GOLD = "#ffce61"
PAPER = "#fff0ce"
CORAL = "#f47c71"
ORANGE_SHADE = "#c96148"
BLUE = "#6aafd8"
MINT = "#79dfc0"

Rect = tuple[int, int, int, int, str]

# A worried traffic cone in a police cap. Coordinates are on a 32-pixel grid.
ICON: list[Rect] = [
    (5, 1, 22, 30, INK), (2, 4, 28, 24, INK),
    (5, 2, 22, 28, GOLD), (3, 4, 26, 24, GOLD),
    (6, 4, 20, 24, NAVY), (4, 6, 24, 20, NAVY),
    (7, 3, 18, 1, PAPER),
    (7, 27, 19, 2, "#0a1520"),
    (13, 12, 6, 5, CORAL),
    (12, 17, 8, 4, PAPER),
    (11, 21, 10, 3, CORAL),
    (9, 24, 14, 2, CORAL),
    (18, 13, 1, 4, ORANGE_SHADE),
    (19, 17, 1, 4, "#dacba9"),
    (20, 21, 1, 3, ORANGE_SHADE),
    (21, 24, 2, 2, ORANGE_SHADE),
    (7, 25, 18, 3, ORANGE_SHADE), (8, 25, 16, 1, CORAL),
    (14, 14, 1, 2, INK), (17, 14, 1, 2, INK),
    (9, 6, 14, 6, INK), (10, 5, 12, 1, INK),
    (10, 6, 12, 5, BLUE), (11, 6, 10, 1, "#a3d5ec"),
    (10, 9, 12, 2, "#3f6e9c"),
    (15, 7, 3, 3, GOLD), (15, 7, 2, 1, PAPER),
    (8, 11, 16, 2, INK), (10, 11, 12, 1, "#0a1520"),
    (25, 15, 1, 3, MINT), (24, 17, 3, 2, MINT),
]

# Hand-simplified rather than shrinking away the cap, stripe and eyes at 16px.
SMALL_ICON: list[Rect] = [
    (3, 0, 10, 16, INK), (1, 2, 14, 12, INK),
    (3, 1, 10, 14, GOLD), (2, 2, 12, 12, GOLD),
    (4, 2, 8, 12, NAVY), (3, 3, 10, 10, NAVY),
    (7, 6, 3, 3, CORAL), (6, 9, 5, 2, PAPER),
    (5, 11, 7, 2, CORAL), (4, 13, 9, 1, ORANGE_SHADE),
    (11, 11, 1, 2, ORANGE_SHADE),
    (7, 8, 1, 1, INK), (9, 8, 1, 1, INK),
    (5, 3, 6, 3, BLUE), (5, 3, 6, 1, "#a3d5ec"),
    (8, 4, 1, 1, GOLD), (4, 6, 8, 1, INK),
]

# Only the lettering needed by the wordmark; all glyphs are drawn, not font references.
LETTERS = {
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "I": ("111", "010", "010", "010", "010", "010", "111"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "C": ("01111", "10000", "10000", "10000", "10000", "10000", "01111"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
}
ICON_SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)


def word_width(word: str, scale: int) -> int:
    return (sum(len(LETTERS[letter][0]) + 1 for letter in word) - 1) * scale


def wordmark(word: str, x: int, y: int, scale: int, colour: str) -> list[Rect]:
    pixels = []
    for letter in word:
        for row, line in enumerate(LETTERS[letter]):
            column = 0
            while column < len(line):
                if line[column] == "0":
                    column += 1
                    continue
                start = column
                while column < len(line) and line[column] == "1":
                    column += 1
                pixels.append((x + start * scale, y + row * scale, (column - start) * scale, scale, colour))
        x += (len(LETTERS[letter][0]) + 1) * scale
    shadow = [(x + 1, y + 2, w + 2, h + 2, NAVY) for x, y, w, h, _ in pixels]
    outline = [(x - 1, y - 1, w + 2, h + 2, INK) for x, y, w, h, _ in pixels]
    return shadow + outline + pixels


def placed(shapes: list[Rect], x: int, y: int, scale: int) -> list[Rect]:
    return [(x + px * scale, y + py * scale, w * scale, h * scale, colour)
            for px, py, w, h, colour in shapes]


def logo() -> list[Rect]:
    shapes = placed(ICON, 6, 14, 2)
    shapes += wordmark("RIOT", 84, 10, 5, PAPER)
    shapes += wordmark("CONTROL", 84, 53, 3, GOLD)
    shapes += [(84, 82, 123, 5, INK), (84, 81, 123, 3, MINT)]
    for x in range(88, 204, 12):
        shapes.append((x, 81, 6, 3, NAVY))
    return shapes


def rgba(shapes: list[Rect], width: int, height: int, scale: float = 1.0) -> tuple[int, int, bytes]:
    out_width, out_height = round(width * scale), round(height * scale)
    pixels = bytearray(out_width * out_height * 4)
    for x, y, w, h, colour in shapes:
        left, right = round(x * scale), round((x + w) * scale)
        top, bottom = round(y * scale), round((y + h) * scale)
        if not (0 <= left <= right <= out_width and 0 <= top <= bottom <= out_height):
            raise ValueError(f"Artwork rectangle is outside its canvas: {(x, y, w, h)}")
        fill = (bytes.fromhex(colour[1:]) + b"\xff") * (right - left)
        for row in range(top, bottom):
            start = (row * out_width + left) * 4
            pixels[start:start + len(fill)] = fill
    return out_width, out_height, bytes(pixels)


def png(width: int, height: int, pixels: bytes) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    stride = width * 4
    scanlines = b"".join(b"\0" + pixels[row * stride:(row + 1) * stride] for row in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(scanlines, 9)) + chunk(b"IEND", b""))


def icon_bitmap(size: int, pixels: bytes) -> bytes:
    # ICO DIBs store bottom-up BGRA pixels, then a DWORD-aligned 1-bit transparency mask.
    colour = bytearray()
    mask = bytearray()
    mask_stride = ((size + 31) // 32) * 4
    for y in reversed(range(size)):
        mask_row = bytearray(mask_stride)
        for x in range(size):
            r, g, b, a = pixels[(y * size + x) * 4:(y * size + x + 1) * 4]
            colour.extend((b, g, r, a))
            if a == 0:
                mask_row[x // 8] |= 0x80 >> (x % 8)
        mask.extend(mask_row)
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, len(colour), 0, 0, 0, 0)
    return header + colour + mask


def write_icon(path: Path) -> None:
    images = []
    for size in ICON_SIZES:
        source, grid = (SMALL_ICON, 16) if size < 32 else (ICON, 32)
        _, _, pixels = rgba(source, grid, grid, size / grid)
        images.append(png(size, size, pixels) if size == 256 else icon_bitmap(size, pixels))
    offset = 6 + 16 * len(images)
    entries = bytearray()
    for size, image in zip(ICON_SIZES, images):
        entries.extend(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(image), offset))
        offset += len(image)
    path.write_bytes(struct.pack("<HHH", 0, 1, len(images)) + entries + b"".join(images))


def write_svg(path: Path, shapes: list[Rect], width: int, height: int, scale: int, title: str) -> None:
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width * scale}" height="{height * scale}" '
        f'viewBox="0 0 {width} {height}" shape-rendering="crispEdges">',
        f"  <title>{title}</title>",
    ]
    lines += [f'  <rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{colour}"/>'
              for x, y, w, h, colour in shapes]
    lines.append("</svg>")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_sidebar(path: Path) -> None:
    width, height = 164, 314
    shapes = [(0, 0, width, height, NAVY)] + placed(ICON, 18, 36, 4)
    shapes += wordmark("RIOT", (width - word_width("RIOT", 4)) // 2, 184, 4, PAPER)
    shapes += wordmark("CONTROL", (width - word_width("CONTROL", 3)) // 2, 226, 3, GOLD)
    shapes += [(20, 273, 124, 3, MINT)]
    for x in range(24, 140, 12):
        shapes.append((x, 273, 6, 3, NAVY))
    _, _, pixels = rgba(shapes, width, height)
    stride = ((width * 3 + 3) // 4) * 4
    rows = bytearray()
    for y in reversed(range(height)):
        row = bytearray()
        for x in range(width):
            r, g, b = pixels[(y * width + x) * 4:(y * width + x) * 4 + 3]
            row.extend((b, g, r))
        rows.extend(row + bytes(stride - len(row)))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54)
    header += struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(rows), 0, 0, 0, 0)
    path.write_bytes(header + rows)


def main() -> None:
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    write_icon(directory / "RiotControl.ico")
    (directory / "icon.png").write_bytes(png(*rgba(ICON, 32, 32, 8)))
    write_svg(directory / "icon.svg", ICON, 32, 32, 8, "Riot Control: a traffic cone in a police cap")
    mark = logo()
    (directory / "logo.png").write_bytes(png(*rgba(mark, 220, 96, 4)))
    write_svg(directory / "logo.svg", mark, 220, 96, 4, "Riot Control")
    write_sidebar(directory / "installer-sidebar.bmp")


if __name__ == "__main__":
    main()
