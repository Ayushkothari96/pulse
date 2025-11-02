# Icon Generator for Pulse Monitor
# Creates a simple .ico file for the executable

from PIL import Image, ImageDraw
import os

def create_pulse_icon():
    """Create a simple icon with a pulse/heartbeat symbol"""
    
    # Create image with multiple sizes for Windows icon
    sizes = [256, 128, 64, 48, 32, 16]
    images = []
    
    for size in sizes:
        # Create a new image with a gradient background
        img = Image.new('RGB', (size, size), color='#2c3e50')
        draw = ImageDraw.Draw(img)
        
        # Draw a circular background
        margin = size // 8
        draw.ellipse([margin, margin, size - margin, size - margin], 
                     fill='#3498db', outline='#2980b9', width=max(1, size // 32))
        
        # Draw pulse wave symbol
        center_y = size // 2
        line_width = max(2, size // 16)
        
        # Pulse wave coordinates (simplified heartbeat shape)
        points = [
            (size * 0.1, center_y),
            (size * 0.25, center_y),
            (size * 0.35, center_y - size * 0.25),  # Up spike
            (size * 0.45, center_y + size * 0.15),  # Down spike
            (size * 0.55, center_y),
            (size * 0.9, center_y)
        ]
        
        # Draw the pulse line
        for i in range(len(points) - 1):
            draw.line([points[i], points[i + 1]], fill='white', width=line_width)
        
        images.append(img)
    
    # Save as .ico file with multiple sizes
    icon_path = os.path.join(os.path.dirname(__file__), 'pulse_icon.ico')
    images[0].save(icon_path, format='ICO', sizes=[(s, s) for s in sizes])
    
    print(f"Icon created: {icon_path}")
    return icon_path

if __name__ == "__main__":
    # Check if Pillow is installed
    try:
        from PIL import Image
        create_pulse_icon()
        print("\n✓ Icon created successfully!")
        print("  Location: pulse_icon.ico")
        print("\nNow rebuild the .exe with: python build_exe.py")
    except ImportError:
        print("ERROR: Pillow library is required to create icons")
        print("Install it with: pip install Pillow")
        print("\nAlternatively, provide your own .ico file and name it 'pulse_icon.ico'")
