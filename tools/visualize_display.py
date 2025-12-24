#!/usr/bin/env python3
"""
Visualize a 16x16 RGB LED matrix from three arrays of 16-bit integers.
Each array has 16 elements (rows). A bit value of 1 means LED on in that color.

Input format (JSON file):
{
  "red":   [65535, 0, 0, ... 16 items ...],
  "green": [0, 65535, 0, ... 16 items ...],
  "blue":  [0, 0, 65535, ... 16 items ...]
}

By default, the least significant bit (LSB) is the leftmost column.
Use --msb-left if your data treats the most significant bit (MSB) as the leftmost.

Examples:
  python tools/visualize_display.py --input examples/sample.json --show
  python tools/visualize_display.py --input examples/sample.json --outfile out.png --scale 20
  python tools/visualize_display.py --input examples/sample.json --ascii
"""

import argparse
import json
import os
import sys
from typing import List, Tuple

# Optional Pillow import for image output
try:
    from PIL import Image
except Exception:
    Image = None

WIDTH = 16
HEIGHT = 16

def _ensure_16_rows(name: str, arr: List[int]) -> List[int]:
    if len(arr) != HEIGHT:
        raise ValueError(f"{name} must have {HEIGHT} elements (rows), got {len(arr)}")
    return arr

def _coerce_ints(name: str, arr: List) -> List[int]:
    out = []
    for i, v in enumerate(arr):
        if isinstance(v, int):
            out.append(v & 0xFFFF)
        elif isinstance(v, str):
            v = v.strip()
            if v.lower().startswith("0x"):
                out.append(int(v, 16) & 0xFFFF)
            elif v.lower().startswith("0b"):
                out.append(int(v, 2) & 0xFFFF)
            else:
                out.append(int(v) & 0xFFFF)
        else:
            raise ValueError(f"{name}[{i}] must be int or str, got {type(v)}")
    return out

def _bit_on(value: int, col: int, msb_left: bool) -> bool:
    if msb_left:
        bit_index = WIDTH - 1 - col  # col 0 uses bit 15
    else:
        bit_index = col              # col 0 uses bit 0
    return ((value >> bit_index) & 0x1) == 1

def build_rgb_matrix(red_rows: List[int], green_rows: List[int], blue_rows: List[int], msb_left: bool) -> List[List[Tuple[int,int,int]]]:
    matrix = []
    for r in range(HEIGHT):
        row_pixels = []
        rv = red_rows[r]
        gv = green_rows[r]
        bv = blue_rows[r]
        for c in range(WIDTH):
            r_on = _bit_on(rv, c, msb_left)
            g_on = _bit_on(gv, c, msb_left)
            b_on = _bit_on(bv, c, msb_left)
            # Custom color mixing per requirements:
            # - Red + Green  -> Orange
            # - Red + Blue   -> Purple
            # - Blue + Green -> Cyan
            # - All three    -> White
            # - Single color -> Pure channel
            if r_on and g_on and b_on:
                color = (255, 255, 255)  # white
            elif r_on and g_on:
                color = (255, 165, 0)    # orange
            elif r_on and b_on:
                color = (255, 0, 255)    # purple (brighter/magenta)
            elif g_on and b_on:
                color = (0, 255, 255)    # cyan
            elif r_on:
                color = (255, 0, 0)      # red
            elif g_on:
                color = (0, 255, 0)      # green
            elif b_on:
                color = (0, 0, 255)      # blue
            else:
                color = (0, 0, 0)        # off
            row_pixels.append(color)
        matrix.append(row_pixels)
    return matrix

def render_ascii(matrix: List[List[Tuple[int,int,int]]], use_color: bool = True) -> None:
    # ANSI color codes
    RESET = '\033[0m'
    BLACK = '\033[40m'      # Background black
    RED = '\033[41m'
    GREEN = '\033[42m'
    YELLOW = '\033[43m'
    BLUE = '\033[44m'
    MAGENTA = '\033[45m'
    CYAN = '\033[46m'
    WHITE = '\033[47m'
    
    # Map RGB triplets to ANSI color codes
    def pix_color(rgb: Tuple[int,int,int]) -> str:
        r, g, b = rgb
        if r == g == b == 0:
            return BLACK + '  '
        if r and g and b:
            return WHITE + '  '
        if r and g:
            return YELLOW + '  '
        if r and b:
            return MAGENTA + '  '
        if g and b:
            return CYAN + '  '
        if r:
            return RED + '  '
        if g:
            return GREEN + '  '
        if b:
            return BLUE + '  '
        return BLACK + '  '

    for row in matrix:
        if use_color:
            print(''.join(pix_color(px) for px in row) + RESET)
        else:
            # Fallback to ASCII characters if color not available
            def pix_char(rgb: Tuple[int,int,int]) -> str:
                r, g, b = rgb
                if r == g == b == 0:
                    return '.'
                if r and g and b:
                    return 'W'
                if r and g:
                    return 'Y'
                if r and b:
                    return 'M'
                if g and b:
                    return 'C'
                if r:
                    return 'R'
                if g:
                    return 'G'
                return 'B'
            print(''.join(pix_char(px) for px in row))

def render_image(matrix: List[List[Tuple[int,int,int]]], scale: int, outfile: str = None, show: bool = False) -> None:
    if Image is None:
        print("Pillow (PIL) not available. Install with: pip install pillow", file=sys.stderr)
        print("Falling back to ASCII output:")
        render_ascii(matrix)
        return

    img = Image.new('RGB', (WIDTH, HEIGHT))
    for y in range(HEIGHT):
        for x in range(WIDTH):
            img.putpixel((x, y), matrix[y][x])

    if scale and scale > 1:
        img = img.resize((WIDTH * scale, HEIGHT * scale), resample=Image.NEAREST)

    if outfile:
        img.save(outfile)
        print(f"Saved image to {outfile}")
    if show:
        img.show()

def load_input_json(path: str) -> Tuple[List[int], List[int], List[int]]:
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    try:
        red = _ensure_16_rows('red', _coerce_ints('red', data['red']))
        green = _ensure_16_rows('green', _coerce_ints('green', data['green']))
        blue = _ensure_16_rows('blue', _coerce_ints('blue', data['blue']))
    except KeyError as e:
        raise ValueError(f"Missing key in JSON: {e}")
    return red, green, blue

def main():
    parser = argparse.ArgumentParser(description='Visualize 16x16 RGB matrix from three 16-bit row arrays')
    parser.add_argument('input', nargs='?', default=None, help='Path to JSON file with keys red/green/blue (each 16-element list)')
    parser.add_argument('--msb-left', action='store_true', help='Treat MSB as the leftmost column (default LSB-left)')
    parser.add_argument('--ascii', action='store_true', help='Render as ASCII in console')
    parser.add_argument('--no-color', action='store_true', help='Disable ANSI colors in ASCII output')
    parser.add_argument('--scale', type=int, default=16, help='Pixel scaling factor for image output (default: 16)')
    parser.add_argument('--outfile', type=str, help='Output image path (PNG recommended)')
    parser.add_argument('--show', action='store_true', help='Open image viewer after rendering')

    args = parser.parse_args()

    if args.input is None:
        print('No input file provided. Using a simple demo pattern.', file=sys.stderr)
        # Demo: diagonal red, anti-diagonal green, border blue
        red = [(1 << i) for i in range(WIDTH)]
        green = [(1 << (WIDTH - 1 - i)) for i in range(WIDTH)]
        blue = []
        for r in range(HEIGHT):
            row = 0
            for c in range(WIDTH):
                if r in (0, HEIGHT-1) or c in (0, WIDTH-1):
                    row |= (1 << c)
            blue.append(row)
    else:
        red, green, blue = load_input_json(args.input)

    matrix = build_rgb_matrix(red, green, blue, msb_left=args.msb_left)

    if args.ascii or Image is None and not (args.outfile or args.show):
        use_color = not args.no_color
        render_ascii(matrix, use_color=use_color)
    else:
        render_image(matrix, scale=args.scale, outfile=args.outfile, show=args.show)

if __name__ == '__main__':
    main()
