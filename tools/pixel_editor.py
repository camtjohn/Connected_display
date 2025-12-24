#!/usr/bin/env python3
"""
Interactive 16x16 RGB LED Matrix Pixel Editor

Click on cells to paint the selected color (choose from toolbar):
Off → Red → Green → Blue → Cyan → Magenta → Yellow → White

Features:
- Load/save JSON files
- Right-click to open menu with color presets
- Display hex values for red/green/blue arrays
- Real-time visualization
"""

import tkinter as tk
from tkinter import filedialog, messagebox
import json
import os
import re

WIDTH = 16
HEIGHT = 16
CELL_SIZE = 30

# Color definitions and cycling order
COLOR_CYCLE = [
    ('Off', (0, 0, 0), (200, 200, 200)),           # Off -> gray on black
    ('Red', (255, 0, 0), (255, 200, 200)),
    ('Green', (0, 255, 0), (200, 255, 200)),
    ('Blue', (0, 0, 255), (200, 200, 255)),
    ('Cyan', (0, 255, 255), (200, 255, 255)),
    ('Magenta', (255, 0, 255), (255, 200, 255)),
    ('Yellow', (255, 165, 0), (255, 220, 150)),
    ('White', (255, 255, 255), (255, 255, 200)),
]

COLOR_INDEX = {name: idx for idx, (name, _, _) in enumerate(COLOR_CYCLE)}

class PixelEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("16x16 RGB Pixel Editor")
        self.root.resizable(False, False)
        
        # Initialize grid state (each cell has RGB tuple)
        self.grid = [[(0, 0, 0) for _ in range(WIDTH)] for _ in range(HEIGHT)]

        # Currently selected brush color index (defaults to Red)
        self.selected_color_idx = COLOR_INDEX.get('Red', 1)
        self.color_buttons = []
        
        # Filename for load/save
        self.filename = None
        
        # Create UI
        self.create_menu()
        self.create_toolbar()
        self.create_grid_display()
        self.create_hex_display()
        
    def create_menu(self):
        """Create menu bar"""
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self.new_file)
        file_menu.add_command(label="Open...", command=self.open_file)
        file_menu.add_command(label="Save", command=self.save_file)
        file_menu.add_command(label="Save As...", command=self.save_as_file)
        file_menu.add_separator()
        file_menu.add_command(label="Export as PNG", command=self.export_png)
        file_menu.add_command(label="Export as C Code", command=self.export_c_code)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)
        
        edit_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Edit", menu=edit_menu)
        edit_menu.add_command(label="Clear", command=self.clear_grid)
        edit_menu.add_command(label="Fill Red", command=lambda: self.fill_color('Red'))
        edit_menu.add_command(label="Fill Green", command=lambda: self.fill_color('Green'))
        edit_menu.add_command(label="Fill Blue", command=lambda: self.fill_color('Blue'))
        edit_menu.add_command(label="Fill White", command=lambda: self.fill_color('White'))

    def create_toolbar(self):
        """Create color selection toolbar"""
        frame = tk.Frame(self.root)
        frame.pack(padx=10, pady=(6, 0), fill=tk.X)

        tk.Label(frame, text="Brush:").pack(side=tk.LEFT, padx=(0, 6))

        self.color_buttons = []
        for idx, (name, rgb, _) in enumerate(COLOR_CYCLE):
            hex_color = f'#{rgb[0]:02x}{rgb[1]:02x}{rgb[2]:02x}'
            btn = tk.Button(
                frame,
                text=name,
                width=8,
                bg=hex_color if name != 'Off' else '#222222',
                fg='black' if name not in ('Off', 'Blue', 'Magenta') else 'white',
                command=lambda i=idx: self.set_selected_color(i)
            )
            btn.pack(side=tk.LEFT, padx=2)
            self.color_buttons.append(btn)

        self.set_selected_color(self.selected_color_idx, update_buttons=True)
        
    def create_grid_display(self):
        """Create the 16x16 grid of clickable cells"""
        frame = tk.Frame(self.root)
        frame.pack(padx=10, pady=10)
        
        self.canvas = tk.Canvas(
            frame,
            width=WIDTH * CELL_SIZE,
            height=HEIGHT * CELL_SIZE,
            bg='black',
            highlightthickness=1
        )
        self.canvas.pack()
        self.canvas.bind('<Button-1>', self.on_cell_click)
        self.canvas.bind('<Button-3>', self.on_right_click)
        
        self.cells = {}
        self.redraw_grid()
        
    def redraw_grid(self):
        """Redraw all cells"""
        self.canvas.delete('all')
        for y in range(HEIGHT):
            for x in range(WIDTH):
                self.draw_cell(x, y)

    def set_selected_color(self, idx, update_buttons=False):
        """Set the active brush color"""
        self.selected_color_idx = idx % len(COLOR_CYCLE)

        if update_buttons:
            for i, btn in enumerate(self.color_buttons):
                relief = tk.SUNKEN if i == self.selected_color_idx else tk.RAISED
                btn.configure(relief=relief)
                
    def draw_cell(self, x, y):
        """Draw a single cell"""
        r, g, b = self.grid[y][x]
        
        # Pick display color
        if (r, g, b) == (0, 0, 0):
            display_color = '#333333'  # Dark gray for off
        else:
            display_color = f'#{r:02x}{g:02x}{b:02x}'
        
        x0 = x * CELL_SIZE
        y0 = y * CELL_SIZE
        x1 = x0 + CELL_SIZE
        y1 = y0 + CELL_SIZE
        
        self.canvas.create_rectangle(
            x0, y0, x1, y1,
            fill=display_color,
            outline='#555555',
            width=1
        )
        
    def on_cell_click(self, event):
        """Handle left click to paint with selected brush color"""
        x = event.x // CELL_SIZE
        y = event.y // CELL_SIZE
        
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            _, brush_rgb, _ = COLOR_CYCLE[self.selected_color_idx]
            self.grid[y][x] = brush_rgb
            self.draw_cell(x, y)
            self.update_hex_display()
            
    def on_right_click(self, event):
        """Handle right click for color preset menu"""
        x = event.x // CELL_SIZE
        y = event.y // CELL_SIZE
        
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            menu = tk.Menu(self.root, tearoff=True)
            for name, rgb, _ in COLOR_CYCLE:
                menu.add_command(
                    label=name,
                    command=lambda r=rgb, xx=x, yy=y: self.set_color(xx, yy, r)
                )
            menu.post(event.x_root, event.y_root)
            
    def set_color(self, x, y, rgb):
        """Set a cell to a specific color"""
        self.grid[y][x] = rgb
        self.draw_cell(x, y)
        self.update_hex_display()
        
    def rgb_to_name(self, rgb):
        """Convert RGB to color name"""
        for name, color_rgb, _ in COLOR_CYCLE:
            if color_rgb == rgb:
                return name
        return 'Off'
        
    def create_hex_display(self):
        """Create text area to show hex values"""
        frame = tk.Frame(self.root)
        frame.pack(padx=10, pady=(0, 10), fill=tk.BOTH, expand=True)
        
        label = tk.Label(frame, text="Hex Values (for JSON export):")
        label.pack(anchor='w')
        
        self.hex_text = tk.Text(frame, height=20, width=60, font=('Courier', 9))
        self.hex_text.pack(fill=tk.BOTH, expand=True)
        
        scrollbar = tk.Scrollbar(self.hex_text)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.hex_text.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.hex_text.yview)
        
        self.update_hex_display()
        
    def update_hex_display(self):
        """Update hex display with current grid state"""
        red_rows = []
        green_rows = []
        blue_rows = []
        
        for y in range(HEIGHT):
            red_val = 0
            green_val = 0
            blue_val = 0
            
            for x in range(WIDTH):
                r, g, b = self.grid[y][x]
                # MSB-left: bit 15 = column 0, bit 0 = column 15
                bit_pos = WIDTH - 1 - x
                if r > 128:
                    red_val |= (1 << bit_pos)
                if g > 128:
                    green_val |= (1 << bit_pos)
                if b > 128:
                    blue_val |= (1 << bit_pos)
                    
            red_rows.append(red_val)
            green_rows.append(green_val)
            blue_rows.append(blue_val)
        
        self.hex_text.config(state=tk.NORMAL)
        self.hex_text.delete('1.0', tk.END)
        
        output = "{\n"
        output += '  "red": [\n'
        for i, val in enumerate(red_rows):
            output += f'    {val}'
            if i < len(red_rows) - 1:
                output += ','
            output += '\n'
        output += '  ],\n'
        
        output += '  "green": [\n'
        for i, val in enumerate(green_rows):
            output += f'    {val}'
            if i < len(green_rows) - 1:
                output += ','
            output += '\n'
        output += '  ],\n'
        
        output += '  "blue": [\n'
        for i, val in enumerate(blue_rows):
            output += f'    {val}'
            if i < len(blue_rows) - 1:
                output += ','
            output += '\n'
        output += '  ]\n'
        output += '}\n'
        
        self.hex_text.insert('1.0', output)
        self.hex_text.config(state=tk.DISABLED)
        
    def grid_to_json(self):
        """Convert grid to JSON-compatible dict"""
        red_rows = []
        green_rows = []
        blue_rows = []
        
        for y in range(HEIGHT):
            red_val = 0
            green_val = 0
            blue_val = 0
            
            for x in range(WIDTH):
                r, g, b = self.grid[y][x]
                # MSB-left: bit 15 = column 0, bit 0 = column 15
                bit_pos = WIDTH - 1 - x
                if r > 128:
                    red_val |= (1 << bit_pos)
                if g > 128:
                    green_val |= (1 << bit_pos)
                if b > 128:
                    blue_val |= (1 << bit_pos)
                    
            red_rows.append(red_val)
            green_rows.append(green_val)
            blue_rows.append(blue_val)
        
        return {
            'red': red_rows,
            'green': green_rows,
            'blue': blue_rows
        }
        
    def json_to_grid(self, data):
        """Load grid from JSON data"""
        try:
            red = data.get('red', [0] * HEIGHT)
            green = data.get('green', [0] * HEIGHT)
            blue = data.get('blue', [0] * HEIGHT)
            
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    # MSB-left: bit 15 = column 0, bit 0 = column 15
                    bit_pos = WIDTH - 1 - x
                    r = 255 if (red[y] >> bit_pos) & 1 else 0
                    g = 255 if (green[y] >> bit_pos) & 1 else 0
                    b = 255 if (blue[y] >> bit_pos) & 1 else 0
                    self.grid[y][x] = (r, g, b)
        except (IndexError, KeyError, TypeError) as e:
            messagebox.showerror("Error", f"Invalid JSON format: {e}")
            return False
        return True
        
    def c_to_grid(self, content):
        """Load grid from C code with three uint16_t arrays"""
        try:
            # Extract all hex values from the content
            # Pattern: look for uint16_t arrays with hex values
            
            # Find red array
            red_match = re.search(r'image_\w+_red\[16\]\s*=\s*\{([^}]+)\}', content)
            green_match = re.search(r'image_\w+_green\[16\]\s*=\s*\{([^}]+)\}', content)
            blue_match = re.search(r'image_\w+_blue\[16\]\s*=\s*\{([^}]+)\}', content)
            
            if not (red_match and green_match and blue_match):
                messagebox.showerror("Error", "Could not find three uint16_t arrays (red, green, blue)")
                return False
            
            # Extract hex values from each array
            def extract_hex_values(match_text):
                # Find all hex/decimal values; allow 1-5 hex digits to capture 0x02A00, etc.
                hex_values = re.findall(r'0x[0-9A-Fa-f]+|\d+', match_text)
                values = []
                for h in hex_values:
                    try:
                        if h.lower().startswith('0x'):
                            values.append(int(h, 16) & 0xFFFF)
                        else:
                            values.append(int(h) & 0xFFFF)
                    except ValueError:
                        pass
                return values[:16]  # Take only first 16
            
            red_vals = extract_hex_values(red_match.group(1))
            green_vals = extract_hex_values(green_match.group(1))
            blue_vals = extract_hex_values(blue_match.group(1))
            
            if len(red_vals) != 16 or len(green_vals) != 16 or len(blue_vals) != 16:
                messagebox.showerror("Error", f"Arrays must have 16 elements. Got red:{len(red_vals)}, green:{len(green_vals)}, blue:{len(blue_vals)}")
                return False
            
            # Convert to grid
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    # MSB-left: bit 15 = column 0, bit 0 = column 15
                    bit_pos = WIDTH - 1 - x
                    r = 255 if (red_vals[y] >> bit_pos) & 1 else 0
                    g = 255 if (green_vals[y] >> bit_pos) & 1 else 0
                    b = 255 if (blue_vals[y] >> bit_pos) & 1 else 0
                    self.grid[y][x] = (r, g, b)
            
            return True
        except Exception as e:
            messagebox.showerror("Error", f"Failed to parse C file: {e}")
            return False
        
    def new_file(self):
        """Create new blank grid"""
        self.grid = [[(0, 0, 0) for _ in range(WIDTH)] for _ in range(HEIGHT)]
        self.filename = None
        self.redraw_grid()
        self.update_hex_display()
        self.root.title("16x16 RGB Pixel Editor - [Untitled]")
        
    def open_file(self):
        """Open JSON or C file"""
        path = filedialog.askopenfilename(
            filetypes=[('JSON files', '*.json'), ('C files', '*.c'), ('Header files', '*.h'), ('All files', '*.*')],
            initialdir=os.path.dirname(os.path.abspath(__file__))
        )
        if path:
            try:
                if path.endswith('.c') or path.endswith('.h'):
                    # Parse C file
                    with open(path, 'r') as f:
                        content = f.read()
                    if self.c_to_grid(content):
                        self.filename = path
                        self.redraw_grid()
                        self.update_hex_display()
                        self.root.title(f"16x16 RGB Pixel Editor - {os.path.basename(path)}")
                else:
                    # Parse JSON file
                    with open(path, 'r') as f:
                        data = json.load(f)
                    if self.json_to_grid(data):
                        self.filename = path
                        self.redraw_grid()
                        self.update_hex_display()
                        self.root.title(f"16x16 RGB Pixel Editor - {os.path.basename(path)}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to load file: {e}")
                
    def save_file(self):
        """Save to current file or prompt for filename"""
        if self.filename is None:
            self.save_as_file()
        else:
            try:
                with open(self.filename, 'w') as f:
                    json.dump(self.grid_to_json(), f, indent=2)
                messagebox.showinfo("Success", f"Saved to {self.filename}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save: {e}")
                
    def save_as_file(self):
        """Save with new filename"""
        path = filedialog.asksaveasfilename(
            filetypes=[('JSON files', '*.json'), ('All files', '*.*')],
            defaultextension='.json',
            initialdir=os.path.dirname(os.path.abspath(__file__))
        )
        if path:
            try:
                with open(path, 'w') as f:
                    json.dump(self.grid_to_json(), f, indent=2)
                self.filename = path
                self.root.title(f"16x16 RGB Pixel Editor - {os.path.basename(path)}")
                messagebox.showinfo("Success", f"Saved to {path}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save: {e}")
                
    def export_png(self):
        """Export grid as PNG image"""
        try:
            from PIL import Image
        except ImportError:
            messagebox.showerror("Error", "Pillow not installed. Run: pip install pillow")
            return
        
        path = filedialog.asksaveasfilename(
            filetypes=[('PNG files', '*.png')],
            defaultextension='.png',
            initialdir=os.path.dirname(os.path.abspath(__file__))
        )
        if path:
            try:
                img = Image.new('RGB', (WIDTH, HEIGHT))
                for y in range(HEIGHT):
                    for x in range(WIDTH):
                        img.putpixel((x, y), self.grid[y][x])
                
                # Scale up for visibility
                img = img.resize((WIDTH * 16, HEIGHT * 16), Image.NEAREST)
                img.save(path)
                messagebox.showinfo("Success", f"Saved PNG to {path}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to export: {e}")
                
    def export_c_code(self):
        """Export grid as C code (three uint16_t arrays)"""
        path = filedialog.asksaveasfilename(
            filetypes=[('C source files', '*.c'), ('Header files', '*.h'), ('Text files', '*.txt')],
            defaultextension='.c',
            initialdir=os.path.dirname(os.path.abspath(__file__))
        )
        if path:
            try:
                # Get the base name for variable names (without extension)
                base_name = os.path.splitext(os.path.basename(path))[0]
                
                # Generate hex values for each color channel
                red_rows = []
                green_rows = []
                blue_rows = []
                
                for y in range(HEIGHT):
                    red_val = 0
                    green_val = 0
                    blue_val = 0
                    
                    for x in range(WIDTH):
                        r, g, b = self.grid[y][x]
                        # MSB-left: bit 15 = column 0, bit 0 = column 15
                        bit_pos = WIDTH - 1 - x
                        if r > 128:
                            red_val |= (1 << bit_pos)
                        if g > 128:
                            green_val |= (1 << bit_pos)
                        if b > 128:
                            blue_val |= (1 << bit_pos)
                    
                    red_rows.append(red_val)
                    green_rows.append(green_val)
                    blue_rows.append(blue_val)
                
                # Generate C code
                c_code = f"// Generated from pixel editor\n"
                c_code += f"// {base_name}\n\n"
                
                c_code += f"const uint16_t image_{base_name}_red[16] = {{"
                for i, val in enumerate(red_rows):
                    if i % 4 == 0:
                        c_code += "\n    "
                    c_code += f"0x{val:04X}"
                    if i < len(red_rows) - 1:
                        c_code += ", "
                c_code += "\n};\n\n"
                
                c_code += f"const uint16_t image_{base_name}_green[16] = {{"
                for i, val in enumerate(green_rows):
                    if i % 4 == 0:
                        c_code += "\n    "
                    c_code += f"0x{val:04X}"
                    if i < len(green_rows) - 1:
                        c_code += ", "
                c_code += "\n};\n\n"
                
                c_code += f"const uint16_t image_{base_name}_blue[16] = {{"
                for i, val in enumerate(blue_rows):
                    if i % 4 == 0:
                        c_code += "\n    "
                    c_code += f"0x{val:04X}"
                    if i < len(blue_rows) - 1:
                        c_code += ", "
                c_code += "\n};\n"
                
                with open(path, 'w') as f:
                    f.write(c_code)
                
                messagebox.showinfo("Success", f"Exported C code to {path}\n\nYou can now copy and paste directly into your C files!")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to export: {e}")
                
    def clear_grid(self):
        """Clear all pixels"""
        if messagebox.askyesno("Confirm", "Clear all pixels?"):
            self.grid = [[(0, 0, 0) for _ in range(WIDTH)] for _ in range(HEIGHT)]
            self.redraw_grid()
            self.update_hex_display()
            
    def fill_color(self, color_name):
        """Fill entire grid with a color"""
        if color_name not in COLOR_INDEX:
            return
        idx = COLOR_INDEX[color_name]
        _, rgb, _ = COLOR_CYCLE[idx]
        if messagebox.askyesno("Confirm", f"Fill entire grid with {color_name}?"):
            self.grid = [[rgb for _ in range(WIDTH)] for _ in range(HEIGHT)]
            self.redraw_grid()
            self.update_hex_display()

if __name__ == '__main__':
    root = tk.Tk()
    app = PixelEditor(root)
    root.mainloop()
