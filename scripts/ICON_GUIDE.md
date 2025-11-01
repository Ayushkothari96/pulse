# Customizing the Pulse Monitor Icon

## Quick Start - Use the Auto-Generated Icon

The simplest way to add an icon to your executable:

```powershell
cd c:\development\zephyr\zephyrproject\pulse\scripts

# Step 1: Install Pillow (image processing library)
pip install Pillow

# Step 2: Generate the default Pulse icon
python create_icon.py

# Step 3: Build the .exe with the icon
python build_exe.py
```

The auto-generated icon features a blue circle with a white pulse wave symbol.

---

## Option 2: Use Your Own Custom Icon

If you want to use a different icon:

### Method A: Use an Existing .ico File

1. Get your desired `.ico` file (Windows icon format)
2. Rename it to `pulse_icon.ico`
3. Place it in the `scripts` folder
4. Run: `python build_exe.py`

### Method B: Convert an Image to .ico

If you have a PNG or JPG image:

```powershell
# Install Pillow if you haven't already
pip install Pillow

# Use this Python command to convert
python -c "from PIL import Image; img = Image.open('your_image.png'); img.save('pulse_icon.ico', format='ICO', sizes=[(256,256), (128,128), (64,64), (48,48), (32,32), (16,16)])"
```

Replace `your_image.png` with your image file name.

---

## Icon Requirements

For best results, your icon should be:
- **Format**: .ico (Windows Icon)
- **Sizes**: Multiple sizes embedded (256x256, 128x128, 64x64, 48x48, 32x32, 16x16)
- **Style**: Simple, recognizable design that works at small sizes
- **Colors**: Limited palette for clarity at small sizes

---

## Online Icon Generators

If you want to create a professional icon online:

1. **Favicon.io** (https://favicon.io/)
   - Simple text-to-icon generator
   - Can create .ico files directly

2. **Icons8** (https://icons8.com/icons)
   - Free icons library
   - Download and convert to .ico

3. **ConvertICO** (https://convertico.com/)
   - Upload PNG/JPG
   - Converts to .ico format

4. **ICO Convert** (https://icoconvert.com/)
   - Online image to icon converter
   - Supports multiple sizes

---

## Verifying Your Icon

After building with `python build_exe.py`:

1. The executable will be at: `dist\PulseMonitor.exe`
2. Check the icon by:
   - Looking at the file in File Explorer
   - Right-clicking > Properties > checking the icon
   - Running the .exe and seeing it in the taskbar

---

## Troubleshooting

### Icon doesn't appear after building
- Make sure `pulse_icon.ico` exists in the scripts folder
- Rebuild with the `--clean` flag: the build script already does this
- Try deleting the `build` and `dist` folders, then rebuild

### Icon looks blurry or pixelated
- Your .ico file should contain multiple sizes
- Use the create_icon.py script which generates all required sizes
- Make sure your source image is at least 256x256 pixels

### "Icon file not found" error
- Check that `pulse_icon.ico` is in the same folder as `build_exe.py`
- Use forward slashes or double backslashes in paths: `C:\\path\\to\\icon.ico`

---

## Current Icon Design

The default icon created by `create_icon.py`:
- **Background**: Blue circle (#3498db)
- **Symbol**: White pulse/heartbeat wave
- **Style**: Modern, minimalist
- **Sizes**: 256, 128, 64, 48, 32, 16 pixels

Feel free to modify `create_icon.py` to customize the design!
