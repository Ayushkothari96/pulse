import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import serial.tools.list_ports
import threading
import queue
import time
import re

class PulseMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Pulse Device Monitor - v1.0")
        self.root.geometry("700x600")
        self.root.configure(bg="#f0f0f0")

        self.serial_port = None
        self.is_connected = False
        self.log_queue = queue.Queue()
        self.status_polling_active = False
        self.last_similarity = None
        self.ml_state = "UNKNOWN"
        self.logs_enabled = True
        
        # Configure styles
        self.setup_styles()

        # --- UI Elements ---
        # Header
        header_frame = tk.Frame(root, bg="#2c3e50", height=60)
        header_frame.pack(fill="x", padx=0, pady=0)
        header_frame.pack_propagate(False)
        
        title_label = tk.Label(header_frame, text="🔧 PULSE DEVICE MONITOR", 
                              bg="#2c3e50", fg="white", 
                              font=("Segoe UI", 16, "bold"))
        title_label.pack(pady=15)
        
        # Connection Frame
        connection_frame = ttk.LabelFrame(root, text="🔌 Connection", style="Card.TLabelframe")
        connection_frame.pack(padx=15, pady=10, fill="x")

        self.port_label = ttk.Label(connection_frame, text="COM Port:", font=("Segoe UI", 10))
        self.port_label.pack(side=tk.LEFT, padx=8, pady=8)

        self.port_variable = tk.StringVar()
        self.port_dropdown = ttk.OptionMenu(connection_frame, self.port_variable, "None")
        self.port_dropdown.pack(side=tk.LEFT, padx=5, pady=8)

        self.refresh_button = ttk.Button(connection_frame, text="🔄 Refresh", command=self.refresh_ports, style="Action.TButton")
        self.refresh_button.pack(side=tk.LEFT, padx=5, pady=8)

        self.connect_button = ttk.Button(connection_frame, text="▶ Connect", command=self.toggle_connection, style="Primary.TButton")
        self.connect_button.pack(side=tk.LEFT, padx=5, pady=8)

        # Status Frame
        status_frame = ttk.LabelFrame(root, text="📊 Device Status", style="Card.TLabelframe")
        status_frame.pack(padx=15, pady=10, fill="x")

        self.status_indicator = tk.Label(status_frame, text="DISCONNECTED", 
                                        bg="#95a5a6", fg="white", 
                                        width=25, height=2,
                                        font=("Segoe UI", 16, "bold"),
                                        relief=tk.RAISED, bd=3)
        self.status_indicator.pack(pady=15, padx=15)
        
        self.similarity_label = tk.Label(status_frame, text="Similarity: N/A", 
                                        font=("Segoe UI", 11, "bold"),
                                        bg="#f0f0f0")
        self.similarity_label.pack(pady=5)

        # Controls Frame
        controls_frame = ttk.LabelFrame(root, text="🎛️ Controls", style="Card.TLabelframe")
        controls_frame.pack(padx=15, pady=10, fill="x")

        self.reset_button = ttk.Button(controls_frame, text="🔄 Reset & Learn", 
                                      command=self.reset_device, 
                                      state=tk.DISABLED,
                                      style="Action.TButton")
        self.reset_button.pack(side=tk.LEFT, padx=8, pady=10)

        self.status_button = ttk.Button(controls_frame, text="📋 Get Status", 
                                       command=self.request_status, 
                                       state=tk.DISABLED,
                                       style="Action.TButton")
        self.status_button.pack(side=tk.LEFT, padx=8, pady=10)
        
        self.logs_button = ttk.Button(controls_frame, text="📝 Toggle Logs", 
                                     command=self.toggle_logs, 
                                     state=tk.DISABLED,
                                     style="Action.TButton")
        self.logs_button.pack(side=tk.LEFT, padx=8, pady=10)
        
        self.logs_status = tk.Label(controls_frame, text="Logs: ON", 
                                   font=("Segoe UI", 9, "bold"),
                                   fg="#27ae60", bg="#f0f0f0")
        self.logs_status.pack(side=tk.LEFT, padx=8, pady=10)

        # Log Frame
        log_frame = ttk.LabelFrame(root, text="📄 Device Logs", style="Card.TLabelframe")
        log_frame.pack(padx=15, pady=10, fill="both", expand=True)

        self.log_text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, 
                                                  state=tk.DISABLED,
                                                  bg="#1e1e1e", fg="#d4d4d4",
                                                  font=("Consolas", 9),
                                                  insertbackground="white")
        self.log_text.pack(fill="both", expand=True, padx=5, pady=5)

        self.refresh_ports()
        self.root.after(100, self.process_logs)
    
    def setup_styles(self):
        """Configure custom ttk styles for a modern look"""
        style = ttk.Style()
        style.theme_use('clam')
        
        # Card style for LabelFrames
        style.configure("Card.TLabelframe", background="#f0f0f0", borderwidth=2, relief="solid")
        style.configure("Card.TLabelframe.Label", font=("Segoe UI", 11, "bold"), 
                       foreground="#2c3e50", background="#f0f0f0")
        
        # Primary button style
        style.configure("Primary.TButton", font=("Segoe UI", 10, "bold"), 
                       padding=8, background="#3498db", foreground="white")
        style.map("Primary.TButton", background=[("active", "#2980b9")])
        
        # Action button style
        style.configure("Action.TButton", font=("Segoe UI", 10), 
                       padding=8, background="#ecf0f1")
        style.map("Action.TButton", background=[("active", "#bdc3c7")])

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        menu = self.port_dropdown["menu"]
        menu.delete(0, "end")
        if ports:
            for port in ports:
                menu.add_command(label=port, command=lambda value=port: self.port_variable.set(value))
            self.port_variable.set(ports[0])
        else:
            self.port_variable.set("None")

    def toggle_connection(self):
        if not self.is_connected:
            self.connect()
        else:
            self.disconnect()

    def connect(self):
        port = self.port_variable.get()
        if port == "None":
            self.update_log("Error: No COM port selected.")
            return

        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.is_connected = True
            self.connect_button.config(text="Disconnect")
            self.update_status("CONNECTED", "lightblue")
            self.update_log(f"Connected to {port}")

            self.reset_button.config(state=tk.NORMAL)
            self.status_button.config(state=tk.NORMAL)
            self.logs_button.config(state=tk.NORMAL)

            self.reader_thread = threading.Thread(target=self.read_from_port, daemon=True)
            self.reader_thread.start()
            
            # Start automatic status polling
            self.status_polling_active = True
            self.poll_status()
            
            # Update button text
            self.connect_button.config(text="⏸ Disconnect")

        except serial.SerialException as e:
            self.update_log(f"Error: {e}")
            self.update_status("ERROR", "red")

    def disconnect(self):
        self.status_polling_active = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.is_connected = False
        self.connect_button.config(text="▶ Connect")
        self.update_status("DISCONNECTED", "#95a5a6")
        self.similarity_label.config(text="Similarity: N/A")
        self.update_log("Disconnected.")
        self.reset_button.config(state=tk.DISABLED)
        self.status_button.config(state=tk.DISABLED)
        self.logs_button.config(state=tk.DISABLED)

    def read_from_port(self):
        while self.is_connected and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    self.log_queue.put(line)
            except (serial.SerialException, TypeError):
                self.log_queue.put("--- Serial port disconnected ---")
                self.disconnect()
                break

    def process_logs(self):
        while not self.log_queue.empty():
            line = self.log_queue.get_nowait()
            self.update_log(line)
            self.parse_log_for_status(line)
        self.root.after(100, self.process_logs)

    def update_log(self, message):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def parse_log_for_status(self, line):
        # Parse ML State
        if "ML State:" in line:
            if "TRAINING" in line:
                self.ml_state = "TRAINING"
            elif "INFERENCING" in line:
                self.ml_state = "INFERENCING"
            elif "IDLE" in line:
                self.ml_state = "IDLE"
        
        # Parse Similarity score
        similarity_match = re.search(r'Similarity:\s+(\d+)%', line)
        if similarity_match:
            self.last_similarity = int(similarity_match.group(1))
            self.update_similarity_display()
        
        # Also parse from inline logs during inferencing
        if "main: Similarity:" in line:
            inline_match = re.search(r'Similarity:\s+(\d+)%', line)
            if inline_match:
                self.last_similarity = int(inline_match.group(1))
                self.update_similarity_display()
    
    def update_similarity_display(self):
        """Update the status indicator based on similarity score"""
        if self.last_similarity is not None:
            self.similarity_label.config(text=f"Similarity: {self.last_similarity}%")
            
            if self.ml_state == "INFERENCING":
                if self.last_similarity >= 90:
                    self.update_status("✓ NORMAL", "#27ae60")
                else:
                    self.update_status("⚠ ANOMALY DETECTED!", "#e74c3c")
            elif self.ml_state == "TRAINING":
                self.update_status("🔄 TRAINING", "#f39c12")
            elif self.ml_state == "IDLE":
                self.update_status("⏸ IDLE", "#3498db")
        else:
            self.similarity_label.config(text="Similarity: N/A")

    def update_status(self, text, color):
        self.status_indicator.config(text=text, bg=color)

    def send_command(self, command):
        if self.is_connected:
            try:
                self.serial_port.write((command + "\n").encode('utf-8'))
                self.update_log(f"CMD: {command}")
            except serial.SerialException as e:
                self.update_log(f"Error sending command: {e}")
                self.disconnect()
        else:
            self.update_log("Cannot send command: Not connected.")

    def reset_device(self):
        self.send_command("RESET")
    
    def request_status(self):
        self.send_command("STATUS")
    
    def toggle_logs(self):
        """Toggle verbose logging on the device"""
        self.send_command("LOGS")
        self.logs_enabled = not self.logs_enabled
        
        if self.logs_enabled:
            self.logs_status.config(text="Logs: ON", fg="#27ae60")
            self.update_log("--- Verbose logs ENABLED ---")
        else:
            self.logs_status.config(text="Logs: OFF", fg="#e74c3c")
            self.update_log("--- Verbose logs DISABLED ---")
    
    def poll_status(self):
        """Automatically poll device status every 250ms"""
        if self.status_polling_active and self.is_connected:
            self.send_command("STATUS")
            # Schedule next poll
            self.root.after(250, self.poll_status)

    def on_closing(self):
        self.disconnect()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = PulseMonitorApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()
