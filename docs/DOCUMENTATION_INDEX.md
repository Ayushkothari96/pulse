# Pulse Device - Documentation Index

Welcome to the Pulse Predictive Maintenance Device documentation! This index will help you find the information you need.

---

## 📚 Quick Navigation

### For New Users
- **[QUICK_START.md](../QUICK_START.md)** - Get up and running in minutes
- **[README.md](../README.md)** - Complete project overview and setup

### For GUI Users
- **[scripts/ICON_GUIDE.md](../scripts/ICON_GUIDE.md)** - Customize the executable icon
- **[gui_update_summary.md](gui_update_summary.md)** - GUI features and changelog

### For Developers
- **[USB_SHELL_GUIDE.md](../USB_SHELL_GUIDE.md)** - USB console commands and API
- **[flash_storage.md](flash_storage.md)** - Persistent training data implementation
- **[prd/phm_device_prd.md](../prd/phm_device_prd.md)** - Product requirements document

### Hardware
- **[hardware/list_of_hardware_issues.txt](../hardware/list_of_hardware_issues.txt)** - Known hardware issues

---

## 📖 Documentation by Topic

### Getting Started

| Document | Description | Audience |
|----------|-------------|----------|
| [QUICK_START.md](../QUICK_START.md) | 5-minute setup guide | Everyone |
| [README.md](../README.md) | Complete project documentation | Everyone |

### GUI Monitor Application

| Document | Description | Audience |
|----------|-------------|----------|
| [gui_update_summary.md](gui_update_summary.md) | GUI features and updates | GUI users |
| [scripts/ICON_GUIDE.md](../scripts/ICON_GUIDE.md) | Icon customization guide | GUI users |
| [scripts/requirements.txt](../scripts/requirements.txt) | Python dependencies | Developers |

### Device Usage

| Document | Description | Audience |
|----------|-------------|----------|
| [USB_SHELL_GUIDE.md](../USB_SHELL_GUIDE.md) | Console commands reference | All users |
| Testing examples in QUICK_START | Real-world testing procedures | Testers |

### Technical Details

| Document | Description | Audience |
|----------|-------------|----------|
| [flash_storage.md](flash_storage.md) | Flash memory architecture | Developers |
| [prd/phm_device_prd.md](../prd/phm_device_prd.md) | Product requirements | Product managers |
| [hardware/list_of_hardware_issues.txt](../hardware/list_of_hardware_issues.txt) | Hardware troubleshooting | Developers |

---

## 🎯 Find What You Need

### I want to...

**...get started quickly**
→ Read [QUICK_START.md](../QUICK_START.md)

**...use the GUI monitor**
→ See [gui_update_summary.md](gui_update_summary.md)

**...understand the commands**
→ Check [USB_SHELL_GUIDE.md](../USB_SHELL_GUIDE.md)

**...customize the icon**
→ Follow [scripts/ICON_GUIDE.md](../scripts/ICON_GUIDE.md)

**...build a standalone .exe**
→ See "Creating Standalone Executable" in [USB_SHELL_GUIDE.md](../USB_SHELL_GUIDE.md)

**...test on real machinery**
→ See "Testing Workflow" in [QUICK_START.md](../QUICK_START.md)

**...understand flash storage**
→ Read [flash_storage.md](flash_storage.md)

**...troubleshoot issues**
→ Check "Troubleshooting" sections in [QUICK_START.md](../QUICK_START.md) and [README.md](../README.md)

**...understand the product vision**
→ Read [prd/phm_device_prd.md](../prd/phm_device_prd.md)

**...build and flash firmware**
→ See "Building and Flashing" in [README.md](../README.md)

**...contribute to the project**
→ See "Contributing" in [README.md](../README.md)

---

## 📋 Document Summaries

### QUICK_START.md
**Audience:** Everyone  
**Length:** ~10 minutes to read  
**Content:**
- 5-minute build and flash guide
- GUI monitor quick start
- USB console command reference
- Real-world testing examples
- Troubleshooting common issues

### README.md
**Audience:** Everyone  
**Length:** ~30 minutes to read  
**Content:**
- Complete project overview
- Hardware and software requirements
- Detailed architecture explanation
- Build and configuration guide
- Comprehensive troubleshooting
- Performance metrics
- Contributing guidelines

### USB_SHELL_GUIDE.md
**Audience:** All users  
**Length:** ~15 minutes to read  
**Content:**
- GUI monitor features
- USB console command reference
- Command examples and workflows
- Terminal setup instructions
- Creating portable executables

### gui_update_summary.md
**Audience:** GUI users, developers  
**Length:** ~5 minutes to read  
**Content:**
- New GUI features
- Log toggle implementation
- Visual design improvements
- Firmware and GUI integration
- Build instructions

### flash_storage.md
**Audience:** Developers  
**Length:** ~10 minutes to read  
**Content:**
- NVS filesystem usage
- Training data persistence
- Memory optimization
- CRC validation
- Flash partition layout

### prd/phm_device_prd.md
**Audience:** Product managers, stakeholders  
**Length:** ~20 minutes to read  
**Content:**
- Product vision and goals
- User stories and requirements
- Target market analysis
- Success metrics
- Roadmap and timeline

### scripts/ICON_GUIDE.md
**Audience:** GUI users  
**Length:** ~5 minutes to read  
**Content:**
- Auto-generate icon
- Use custom icon
- Icon format requirements
- Online tools and resources
- Troubleshooting

---

## 🔄 Recently Updated

**November 2025:**
- ✅ Added GUI monitor application
- ✅ Implemented log toggle feature
- ✅ Created standalone .exe build system
- ✅ Added custom icon support
- ✅ Updated all README files
- ✅ Enhanced QUICK_START guide
- ✅ Expanded USB_SHELL_GUIDE

---

## 📞 Getting Help

### Found a Bug?
Open an issue on GitHub: [https://github.com/Ayushkothari96/pulse/issues](https://github.com/Ayushkothari96/pulse/issues)

### Have a Question?
Check the troubleshooting sections first:
- [QUICK_START.md - Troubleshooting](../QUICK_START.md#troubleshooting)
- [README.md - Troubleshooting](../README.md#-troubleshooting)

### Want to Contribute?
See [README.md - Contributing](../README.md#-contributing)

---

## 🗂️ File Organization

```
pulse/
├── README.md                    # Main documentation
├── QUICK_START.md              # Getting started guide
├── USB_SHELL_GUIDE.md          # Console commands reference
├── docs/
│   ├── DOCUMENTATION_INDEX.md  # This file
│   ├── flash_storage.md        # Flash memory details
│   └── gui_update_summary.md   # GUI features
├── prd/
│   └── phm_device_prd.md      # Product requirements
├── scripts/
│   ├── ICON_GUIDE.md          # Icon customization
│   ├── pulse_monitor.py       # GUI application
│   ├── build_exe.py           # Executable builder
│   ├── create_icon.py         # Icon generator
│   ├── run_pulse_monitor.bat  # Quick launcher
│   └── requirements.txt       # Dependencies
└── hardware/
    └── list_of_hardware_issues.txt
```

---

**Last Updated:** November 2025  
**Version:** 1.0.0
