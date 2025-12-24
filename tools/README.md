# Visualize Display Tool

A Python utility to visualize a 16×16 RGB LED matrix from three arrays of 16-bit integers.

## Installation

Requires Python 3.6+. For image output, install Pillow:

```bash
pip install pillow
```

## Usage (visualize_display)

### Basic Usage

Visualize with color in the terminal:

```bash
python visualize_display.py examples/sample.json
```

### Output Formats

**ASCII in Terminal (with ANSI colors - default)**
```bash
python visualize_display.py examples/sample.json --ascii
```

**ASCII without colors**
```bash
python visualize_display.py examples/sample.json --ascii --no-color
```

**Save as PNG image (256× scaling by default)**
```bash
python visualize_display.py examples/sample.json --outfile output.png
```

**Custom scaling for image output**
```bash
python visualize_display.py examples/sample.json --outfile output.png --scale 20
```

**Display image in viewer**
```bash
python visualize_display.py examples/sample.json --show
```

## Input Format

Provide a JSON file with three arrays named `red`, `green`, and `blue`. Each array must contain exactly 16 elements (rows), with each element being a 16-bit integer representing the pixel states in that row.

```json
{
  "red":   [65535, 0, 0, 255, ...],
  "green": [0, 65535, 0, 128, ...],
  "blue":  [0, 0, 65535, 64, ...]
}
```

Values can be:
- Decimal integers: `65535`
- Hexadecimal strings: `"0xFFFF"`
- Binary strings: `"0b1111111111111111"`

## Bit Ordering

By default, the **least significant bit (LSB)** is the leftmost column (bit 0 = column 0).

Use `--msb-left` if your data treats the most significant bit (MSB) as the leftmost:

```bash
python visualize_display.py examples/sample.json --msb-left
```

## Color Mixing

The visualizer supports RGB color mixing:

| Red | Green | Blue | Result  |
|-----|-------|------|---------|
| ✓   | ✓     | ✓    | White   |
| ✓   | ✓     |      | Orange  |
| ✓   |       | ✓    | Purple  |
|     | ✓     | ✓    | Cyan    |
| ✓   |       |      | Red     |
|     | ✓     |      | Green   |
|     |       | ✓    | Blue    |

## Demo Pattern

Run without a filename to see a demo pattern:

```bash
python visualize_display.py
```

This displays a diagonal red line, anti-diagonal green line, and blue border.

## Pixel Editor (interactive painter)

Launch the GUI painter (requires tkinter; on Windows ensure Python is installed with tcl/tk):

```bash
python pixel_editor.py
```

Workflow:
- Pick a brush color from the toolbar (Off, Red, Green, Blue, Cyan, Magenta, Yellow, White).
- Click the 16×16 grid to paint with the selected color (MSB-left: bit 15 = leftmost column).
- File → Open: load a JSON or C file with three `uint16_t` arrays.
- File → Export as C Code: saves three ready-to-paste `const uint16_t` arrays.
- File → Export as PNG: optional image snapshot (needs Pillow).

## Requirements

- Python 3.6+
- Pillow (optional, for PNG output and pixel_editor PNG export)
