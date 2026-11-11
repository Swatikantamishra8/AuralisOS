# AuralisOS Design Language & Brand Guidelines

```
     _                     _ _      ___  ____  
    / \  _   _ _ __ __ _  | (_)___ / _ \/ ___| 
   / _ \| | | | '__/ _` | | | / __| | | \___ \ 
  / ___ \ |_| | | | (_| | | | \__ \ |_| |___) |
 /_/   \_\__,_|_|  \__,_| |_|_|___/\___/|____/ 
 
      L I Q U I D   G L A S S   E D I T I O N
```

## 1. Visual Identity & Aesthetic Philosophy

**AuralisOS** combines the cold minimalism of Alpine Linux and musl libc with the fluid elegance of modern translucent glass surfaces inspired by the Aurora Borealis. 

The **Liquid Glass** aesthetic achieves high visual impact without the memory penalties typically associated with modern desktop environments. Every UI component is rendered natively in C using GTK4, with GPU-accelerated Dual-Kawase background blur provided by the Wayfire compositor.

### Key Pillars:
1. **Frosted Translucency**: Glass-like panels that sample and diffuse the wallpaper and underlying windows without obscuring information.
2. **Aurora Luminance**: Vibrant cyan and deep sapphire gradients that guide focus and highlight interactive controls.
3. **Sub-100MB RAM Discipline**: Extreme graphical fidelity with an ultra-lightweight memory budget (<95 MB total idle RAM). Zero web runtimes, zero Electron, zero Python in the desktop shell.

---

## 2. Color Palette

### Base Surfaces & Glass Layers
| Token | HEX | RGBA | Purpose |
| :--- | :--- | :--- | :--- |
| `onyx-midnight` | `#0a0e17` | `rgba(10, 14, 23, 1.0)` | Primary desktop backdrop & deep background |
| `glass-panel` | `#0e121c` | `rgba(14, 18, 28, 0.80)` | Taskbar surface & system tray container |
| `glass-popover` | `#0e121c` | `rgba(14, 18, 28, 0.90)` | Start Menu launcher & Control Center popover |
| `glass-window` | `#101520` | `rgba(16, 21, 32, 0.85)` | Native application windows (Task Manager, Store) |
| `glass-border` | `#ffffff` | `rgba(255, 255, 255, 0.16)`| 1px top border simulating light reflection on glass |
| `glass-subtle` | `#ffffff` | `rgba(255, 255, 255, 0.05)`| Pinned task items & unselected list rows |

### Aurora Accents & System States
| Token | HEX | RGBA | Purpose |
| :--- | :--- | :--- | :--- |
| `aurora-cyan` | `#00d4ff` | `rgba(0, 212, 255, 1.0)` | Primary glow, focus rings, and start button gradient |
| `sapphire-blue`| `#0072ff` | `rgba(0, 114, 255, 1.0)` | Secondary gradient tone for buttons & brand elements |
| `emerald-ok` | `#2ed573` | `rgba(46, 213, 115, 1.0)` | Active network status, healthy RAM indicator (<60%) |
| `amber-warning`| `#ffca28` | `rgba(255, 202, 40, 1.0)` | Memory warning threshold (60% - 85%), swap in use |
| `crimson-kill` | `#ff4757` | `rgba(255, 71, 87, 1.0)` | Critical RAM (>85%), process termination, power off |

### Typography & Text Hierarchy
| Token | HEX | RGBA | Purpose |
| :--- | :--- | :--- | :--- |
| `text-primary` | `#ffffff` | `rgba(255, 255, 255, 1.0)` | Window headers, bold metrics, active clock |
| `text-secondary`| `#cdd6e0` | `rgba(205, 214, 224, 0.9)` | App descriptions, table columns, tray indicators |
| `text-muted` | `#8898aa` | `rgba(136, 152, 170, 0.7)` | PID labels, secondary metadata, inactive states |

---

## 3. Typography Specifications

- **Primary UI Font**: `Noto Sans`, `DejaVu Sans`, sans-serif
  - Body Text: `13px / 400 Regular`
  - Control Labels & Badges: `12px / 500 Medium`
  - Window Titles & Section Headers: `14px / 700 Bold`
  - Hero Metrics (RAM / CPU %): `22px / 700 Bold`
- **Monospace / Terminal**: `DejaVu Sans Mono`, `JetBrains Mono`, monospace
  - Foot Terminal: `11px - 12px`
  - System Diagnostics / Logs: `12px`

---

## 4. Compositor & Animation Standards

Configured directly inside `/etc/wayfire/wayfire.ini`:
- **Dual-Kawase Blur**:
  - `kawase_offset = 3`
  - `kawase_degrade = 1`
  - `kawase_iterations = 3`
  - `saturation = 1.2`
  - `contrast = 1.05`
  - `brightness = 0.95`
- **Corner Radii**:
  - Top-level windows (`rounded-corners`): `12px`
  - Start Menu launcher popup: `14px`
  - Controls, search inputs & buttons: `8px`
  - System Tray status pills: `14px` (fully rounded capsule)
- **Transitions**:
  - Window Open / Close: `zoom` (`180ms`)
  - Workspace Switch: `vswitch` (`200ms` with `20px` gap)
  - Snapping Grid: `150ms`

---

## 5. CLI Command Reference

All core AuralisOS commands follow the uniform `auralis-*` namespace:

### `auralis-router`
Universal 4-World binary router and dynamic launcher.
```bash
# Execute any supported format seamlessly
auralis-router /path/to/app.exe        # Spawns in Wine-Wayland native driver
auralis-router /path/to/app.apk        # Installs & launches in Waydroid container
auralis-router /path/to/package.deb    # Deploys in persistent Debian container
auralis-router /path/to/app.dmg        # Extracts and translates via Darling Mach-O

# Inspection & flags
auralis-router --detect <file>         # Prints identified world and magic bytes
auralis-router --hydrate <subsystem>   # Pre-activates wine, waydroid, distrobox, darling
auralis-router --version               # Displays binary router version
```

### `auralis-reaper`
Idle compatibility memory supervisor daemon.
```bash
# Runs as an OpenRC background daemon (45s cycle)
rc-service auralis-reaper status
rc-service auralis-reaper restart

# Actions performed when zero client windows remain:
# 1. wineserver -k
# 2. waydroid session stop
# 3. sync && echo 3 > /proc/sys/vm/drop_caches
```

### `auralis-hydrate`
Privileged on-demand SquashFS subsystem hydrator.
```bash
auralis-hydrate status [subsystem]    # Queries whether subsystem is mounted/cached
auralis-hydrate list                  # Formats summary table of all 4 engines
auralis-hydrate enable <subsystem>    # Fetches/generates SquashFS and loop-mounts it
auralis-hydrate disable <subsystem>   # Unmounts runtime without deleting cache
auralis-hydrate remove <subsystem>    # Unmounts and removes cached SquashFS
```

### `auralis-hwdetect`
Bare-metal hardware auto-tuner and GPU profiler.
```bash
# Executed during boot runlevel before Wayfire initializes
rc-service auralis-hwdetect start

# Inspect generated GPU environment:
cat /etc/profile.d/auralis-gpu.sh
```

### `auralis-status`
Comprehensive architectural verification and diagnostics.
```bash
auralis-status
# Reports:
# - Physical RAM and idle memory usage
# - ZRAM swap size and compression algorithm (zstd)
# - Active Wayland processes and memory consumption
# - Hardware GPU driver overrides
# - Mounted on-demand subsystems (/var/lib/auralis/mnt)
# - Windows NTFS loopback status (/media/windows_c)
```

### `auralis-shell`
Single-binary GTK4 layer-shell translucent taskbar and launcher.
```bash
auralis-shell                         # Primary desktop shell daemon
auralis-shell --toggle-launcher       # Hotkey trigger for Start Menu popup
```

### `auralis-taskmgr`
Low-overhead native GTK4 direct `/proc` resource monitor.
```bash
auralis-taskmgr                       # Opens full monitor (bound to Ctrl+Shift+Esc)
auralis-taskmgr --control-center      # Opens quick popover
```

### `auralis-store`
GTK4 modular subsystem hydrator GUI & curated app store.
```bash
auralis-store                         # Opens two-tab store interface
auralis-store --prompt <subsys> <path># Modal prompt offering on-the-spot activation
```

### `Auralis-Installer.exe`
Windows NTFS zero-repartitioning co-existence loopback installer.
```cmd
Auralis-Installer.exe --install       # Creates 24GB C:\AuralisOS\rootfs.img & EFI entry
Auralis-Installer.exe --uninstall     # Clean 1-click removal of EFI entry & rootfs
Auralis-Installer.exe --help          # Shows CLI flags
```
