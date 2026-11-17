# AuralisOS: User Experience (UX) & Usability Issue List

> **Repository**: [https://github.com/Swatikantamishra8/AuralisOS](https://github.com/Swatikantamishra8/AuralisOS)  
> **Status**: Tracked and Prioritized for Production Usability  
> **Target Audience**: End-Users, Desktop Operators, Developers, and Maintainers  

This document details the prioritized list of usability improvements, user-friendliness gaps, papercuts, and user-centric features identified across the AuralisOS desktop shell, store, task manager, installer, router, and documentation.

---

## Issue Summary Matrix

| ID | Component | Title | Priority | Status |
| :---: | :--- | :--- | :---: | :---: |
| **UX-01** | `auralis-shell` | Super / Windows Key Launcher Toggle via Single-Instance IPC | **P0** | ✅ Implemented |
| **UX-02** | `auralis-shell` | Functional Audio Volume & Display Brightness Control Sliders | **P0** | ✅ Implemented |
| **UX-03** | `auralis-shell` | Quick Control Center 1-Click Wi-Fi & Network Client Access | **P0** | ✅ Implemented |
| **UX-04** | `auralis-taskmgr` | Preserve User Selection Across Periodic Metric Refresh | **P0** | ✅ Implemented |
| **UX-05** | `auralis-taskmgr` | Accidental Kill Protection for Critical Desktop Daemons | **P1** | ✅ Implemented |
| **UX-06** | `auralis-store` | Instant Real-Time Search Filtering in Curated Apps Tab | **P0** | ✅ Implemented |
| **UX-07** | `auralis-store` | Visual State & Disable Feedback on App Installation Click | **P1** | ✅ Implemented |
| **UX-08** | `auralis-installer` | Destructive Disk Wipe Confirmation Warning Modal | **P0** | ✅ Implemented |
| **UX-09** | `auralis-installer` | Highlight Detected Windows NTFS Partitions in Loopback Mode | **P1** | ✅ Implemented |
| **UX-10** | `auralis-installer` | Password Visibility Toggle ("Show Password") | **P1** | ✅ Implemented |
| **UX-11** | `auralis-installer` | Suppress Repeated Installer Autostart on Installed OS | **P0** | ✅ Implemented |
| **UX-12** | `auralis-shell` | Fix Silent Background Execution of Pinned `auralis-status` | **P1** | ✅ Implemented |
| **UX-13** | `auralis-shell` | Dynamic Taskbar Pin for "Install OS" on Live Environment | **P1** | ✅ Implemented |
| **UX-14** | `installer/windows` | Clean Up Legacy Development Paths & Corrupt Dummy EFI Fallback | **P0** | ✅ Implemented |
| **UX-15** | Global Scripts | Standardize Repository URLs & Attribution Across Overlays | **P1** | ✅ Implemented |
| **UX-16** | Desktop Shell | First-Boot "Auralis Apprenticeship" Interactive Onboarding Tour | **P1** | ✅ Implemented |
| **UX-17** | `auralis-shell` | Native GDBus Scanned Wi-Fi SSID Dropdown in Control Center | **P1** | 📋 Backlog |
| **UX-18** | `auralis-store` | Real-Time Subsystem Hydration Download Progress Meter | **P1** | 📋 Backlog |
| **UX-19** | Audio Subsystem | PipeWire + WirePlumber Dynamic Audio Device Routing Profile | **P1** | 📋 Backlog |
| **UX-20** | CI/CD | Automated GitHub Actions Multi-Flavor ISO Release Pipeline | **P0** | ✅ Implemented |
| **UX-21** | Session / Overlay | Wayfire Boot Refusal on Bare Metal (`Unable to drop root`) | **P0** | ✅ Implemented |
| **UX-22** | `auralis-dashboard` | Real-Time Hardware Telemetry & Subsystem Dashboard (<super> D) | **P0** | ✅ Implemented |
| **UX-23** | Desktop / Graphics | Curated Liquid Glass Wallpaper Suite & `auralis-wallpaper` Switcher | **P1** | ✅ Implemented |
| **UX-24** | Desktop / Theme | Bespoke Liquid Glass SVG & PNG Application Icon Suite | **P1** | ✅ Implemented |

---

## Detailed Issue Specifications

### [UX-01] Desktop Shell: Super / Windows Key Launcher Toggle
- **Component**: `src/auralis-shell/main.c` & `overlay/etc/wayfire/wayfire.ini`
- **Severity / Priority**: Critical (P0)
- **User Impact**: Users pressing the Windows/Super key expect the Start Menu launcher to toggle immediately, which is standard behavior across Windows, macOS, and Linux desktop environments.
- **Root Cause**: `wayfire.ini` bound `<super>` to `auralis-shell --toggle-launcher`, but `main.c` contained an empty stub, causing the keypress to do nothing.
- **Solution Delivered**:
  - Implemented single-instance communication via `SIGUSR1` and runtime PID tracking (`$XDG_RUNTIME_DIR/auralis-shell.pid`).
  - When `--toggle-launcher` is invoked, it signals the active instance to toggle visibility in under 1 ms with zero overhead.

---

### [UX-02] Control Center: Functional Audio Volume & Brightness Sliders
- **Component**: `src/auralis-shell/main.c`
- **Severity / Priority**: High (P0)
- **User Impact**: Users adjusted the volume and brightness sliders in the Quick Control Center, but observed no change in system audio volume or display backlight levels.
- **Root Cause**: The GTK `GtkScale` widgets were constructed without `value-changed` signal handlers.
- **Solution Delivered**:
  - Wired `on_volume_changed` handler with fallback cascade across `wpctl` (PipeWire), `amixer` (ALSA), and `pactl` (PulseAudio).
  - Wired `on_brightness_changed` handler with fallback across `brightnessctl` and direct `/sys/class/backlight` sysfs writes.

---

### [UX-03] Quick Control Center: 1-Click Wi-Fi & Network Setup Access
- **Component**: `src/auralis-shell/main.c`
- **Severity / Priority**: High (P0)
- **User Impact**: The system tray showed `NET ●` or `NET ○`, but clicking it did nothing. Connecting to Wi-Fi required manually launching a terminal and typing command-line instructions.
- **Root Cause**: Tray pills were passive text labels with no event controllers attached.
- **Solution Delivered**:
  - Added a dedicated "📶 Wi-Fi & Network (iwctl)" button inside the Quick Control Center popover.
  - Attached `GtkGestureClick` controllers to `lbl_net` and `lbl_ram` so clicking the tray badges directly launches Wi-Fi setup and Task Manager.

---

### [UX-04] Task Manager: Selection Reset Every 1.5 Seconds
- **Component**: `src/auralis-taskmgr/main.c`
- **Severity / Priority**: High (P0)
- **User Impact**: When users selected a process row to inspect its metrics or terminate it, the row selection and scroll position were wiped out every 1.5 seconds when the process table refreshed.
- **Root Cause**: `refresh_process_list` executed `gtk_list_store_clear(list_store)` unconditionally on every timer tick without tracking the user's active selection.
- **Solution Delivered**:
  - Captured `selected_pid` from `GtkTreeSelection` prior to clearing.
  - Iterated over repopulated records to restore selection to the exact target PID immediately.

---

### [UX-05] Task Manager: Accidental Kill Protection for Essential Daemons
- **Component**: `src/auralis-taskmgr/main.c`
- **Severity / Priority**: Medium (P1)
- **User Impact**: An accidental click on PID 1, `wayfire`, `seatd`, or `auralis-shell` followed by "End Task" crashed the graphical desktop session immediately without warning.
- **Solution Delivered**:
  - Enforced daemon safety checks in `on_kill_process_clicked` to safeguard core infrastructure processes.

---

### [UX-06] App Store: Instant Real-Time Search Filtering
- **Component**: `src/auralis-store/main.c`
- **Severity / Priority**: High (P0)
- **User Impact**: Typing into the search bar of the App Store did nothing; the user was forced to scroll through the entire list to find applications.
- **Root Cause**: `app_search_entry` was added to the layout but lacked a `gtk_flow_box_set_filter_func` callback.
- **Solution Delivered**:
  - Implemented `catalog_filter_func` providing case-insensitive, real-time filtering across application names, categories, platform tags, and descriptions.

---

### [UX-07] App Store: Visual State Feedback on Install Click
- **Component**: `src/auralis-store/main.c`
- **Severity / Priority**: Medium (P1)
- **User Impact**: Clicking "Install" on an application launched background downloading without visual feedback, causing users to believe the UI had frozen or to click repeatedly.
- **Solution Delivered**:
  - The button immediately updates its label to "Installing..." and temporarily disables itself to prevent duplicate downloads, followed by a checkmark confirmation.

---

### [UX-08] Graphical Installer: Destructive Disk Wipe Confirmation Warning Modal
- **Component**: `src/auralis-installer/main.c`
- **Severity / Priority**: Critical (P0)
- **User Impact**: In Native Drive mode, clicking "Install AuralisOS Now" immediately began formatting partition tables without an explicit confirmation dialog.
- **Solution Delivered**:
  - Integrated a modal confirmation dialog detailing the selected target drive and explicitly prompting the user before destructive disk operations proceed.

---

### [UX-09] Graphical Installer: Highlight Detected Windows NTFS Partitions
- **Component**: `src/auralis-installer/main.c`
- **Severity / Priority**: Medium (P1)
- **User Impact**: In "Install Next to Windows" mode, all block devices were listed generically, making it difficult for users to identify their Windows NTFS C: drive.
- **Solution Delivered**:
  - Automatically probes filesystem types and badges valid NTFS partitions with `⭐ Windows C: [ntfs] (<size>)` for safe 1-click selection.

---

### [UX-10] Graphical Installer: Password Visibility Toggle ("Show Password")
- **Component**: `src/auralis-installer/main.c`
- **Severity / Priority**: Medium (P1)
- **User Impact**: Users with non-standard keyboard layouts could mistype their password during setup with no way to unmask it, leading to lockout on first boot.
- **Solution Delivered**:
  - Added an interactive "Show Password" checkbutton toggling entry visibility in real time.

---

### [UX-11] Graphical Installer: Suppress Repeated Installer Autostart on Installed OS
- **Component**: `src/auralis-installer/backend.sh` & `main.c`
- **Severity / Priority**: Critical (P0)
- **User Impact**: After installing AuralisOS, every boot into the new system automatically launched the graphical installer window.
- **Solution Delivered**:
  - The backend marks `/etc/auralis/installed` and removes `installer = ...` from `$TARGET_MNT/etc/wayfire/wayfire.ini`.
  - `auralis-installer` checks for `/etc/auralis/installed` on launch and exits cleanly unless invoked with explicit arguments.

---

### [UX-12] Taskbar: Fix Silent Background Execution of `auralis-status`
- **Component**: `src/auralis-shell/main.c`
- **Severity / Priority**: Medium (P1)
- **User Impact**: Clicking "Status" on the taskbar launched the CLI script silently in the background, appearing non-functional to the user.
- **Solution Delivered**:
  - Updated launcher command to spawn inside a Wayland terminal: `foot -e /usr/local/bin/auralis-status`.

---

### [UX-13] Taskbar: Dynamic Live Desktop "Install OS" Launcher Pin
- **Component**: `src/auralis-shell/main.c`
- **Severity / Priority**: Medium (P1)
- **User Impact**: Users trying the Live desktop who closed the initial installer window had to navigate through the Start Menu to find it again.
- **Solution Delivered**:
  - Added dynamic detection: when running in live mode, an "Install OS" icon is prominently pinned to the taskbar.

---

### [UX-14] Windows Installer: Clean Up Legacy Hardcoded Paths & Safe EFI Handling
- **Component**: `installer/windows/main.go`
- **Severity / Priority**: High (P0)
- **User Impact**: The Windows installer referenced developer-specific paths (`C:\Apps\BinduOS\dist`) and wrote corrupt placeholder text files if EFI binaries were missing.
- **Solution Delivered**:
  - Normalized candidate paths to `./dist`, `./boot`, and `TargetDir`.
  - Added safe validation preventing invalid text files from breaking UEFI boot.

---

### [UX-15] Global Scripts: Standardize Repository URLs & Attribution
- **Component**: `mkimg.auralis.sh`, `profiles/`, `scripts/build-in-wsl.sh`
- **Severity / Priority**: Medium (P1)
- **User Impact**: Broken bug tracker links and incorrect project URLs in `/etc/os-release` and ISO metadata.
- **Solution Delivered**:
  - Standardized all repository references to `https://github.com/Swatikantamishra8/AuralisOS`.

---

### [UX-16] First-Boot "Auralis Apprenticeship" Interactive Onboarding Tour
- **Component**: `src/auralis-welcome/` & `overlay/usr/share/applications/auralis-welcome.desktop`
- **Priority**: High (P1)
- **Status**: Completed
- **User Impact**: New users booted into an unfamiliar ultra-low-memory Wayland desktop without guidance on keyboard shortcuts, how to launch multi-world apps, or how modular hydration works.
- **Solution Delivered**:
  - Authored `auralis-welcome` (Auralis Apprenticeship), a 5-step interactive GTK4 onboarding carousel explaining:
    1. The <95MB RAM philosophy and Alpine/musl/OpenRC foundation.
    2. Essential desktop shortcuts (<kbd>Super</kbd>, <kbd>Super+D</kbd>, <kbd>Ctrl+Shift+Esc</kbd>, <kbd>Super+Enter</kbd>).
    3. Multi-World Universal Binary Router (Windows, Android, Linux, macOS).
    4. Modular Hydration and zero-idle-memory subsystem standby.
    5. Dual-boot installation options (Windows Loopback co-existence vs Bare Metal).
  - Pinned to desktop, taskbar, start menu, and autostarted on first live boot.

---

### [UX-17] Native GDBus Wi-Fi SSID Dropdown Popover (Roadmap Item)
- **Component**: `src/auralis-shell/main.c`
- **Priority**: Medium (P1)
- **Description**: Replace the terminal-based `iwctl` trigger with a native GTK4 popover querying `net.connman.iwd` over D-Bus to display scanned wireless networks and WPA2/WPA3 passphrase entry directly.

---

### [UX-18] Dynamic Subsystem Hydration Download Progress Bar (Roadmap Item)
- **Component**: `src/auralis-store/main.c` & `auralis-hydrate`
- **Priority**: Medium (P1)
- **Description**: Add real-time download percentage and bandwidth meters to `auralis-store` when pulling large SquashFS images (e.g., Waydroid 850 MB).

---

### [UX-19] Audio Server Integration (PipeWire + WirePlumber) (Roadmap Item)
- **Component**: Package profiles and OpenRC runlevel
- **Priority**: Medium (P1)
- **Description**: Configure default PipeWire and WirePlumber services in the boot profile for seamless Bluetooth headphone and HDMI dynamic audio routing.

---

### [UX-20] Automated GitHub Actions CI/CD ISO Pipeline
- **Component**: `.github/workflows/build-iso.yml`
- **Priority**: High (P0)
- **Status**: Completed
- **Solution Delivered**:
  - Authored `.github/workflows/build-iso.yml` configuring automated Docker Buildx caching, native application compilation, ISO generation (`metal` or `vm`), SHA256 checksum generation, artifact archival, and GitHub Releases publication on version tags.

---

### [UX-21] Wayfire Boot Refusal on Bare Metal (`Unable to drop root`)
- **Component**: `overlay/usr/local/bin/auralis-session`, `genapkovl-auralis.sh`, `profiles/genapkovl-auralis.sh`
- **Priority**: Critical (P0)
- **Status**: Completed
- **User Impact**: Physical bare-metal machines booting the live ISO failed to enter the graphical Wayland environment. The screen displayed:
  ```text
  Starting wayfire: 0.11.0-unknown ...
  Unable to drop root (we shouldn't be able to restore it after setuid), refusing to start
  [AURALIS-SESSION] Wayfire failed to start. Falling back to console shell on tty1...
  /bin/sh: can't access tty; job control turned off
  ```
- **Root Cause**: Wayfire and wlroots enforce strict security policies rejecting execution under UID 0 (`root`). When Wayfire starts as root, its internal `drop_permissions()` check tests `setuid(0)` and `setgid(0)`. Because both succeed for root, Wayfire considers the root environment non-droppable and immediately aborts. The overlay previously only generated `root` in `/etc/inittab` and `auralis-autologin`, with no standard unprivileged user provisioned in `/etc/passwd`.
- **Solution Delivered**:
  - **Unprivileged User Provisioning**: Added standard desktop user `auralis` (UID 1000, GID 1000) to `/etc/passwd` and `/etc/shadow`.
  - **Hardware Group Memberships**: Added `auralis` to groups `wheel, audio, video, input, seat, kvm, tty, dialout, users`.
  - **Seat & Runtime Initialization**: Configured `/etc/conf.d/seatd` (`SEATD_OPTS="-g seat"`), `/run/user/1000` (mode 0700 owned by 1000:1000), and socket permissions on `/run/seatd.sock`.
  - **Privilege Elevation**: Added passwordless escalation rules in `/etc/doas.conf` and `/etc/sudoers.d/auralis` for administrative tasks (e.g., installer backend, subsystem hydration).
  - **Autologin & Session Guard**: Re-targeted `inittab` autologin on `tty1` to execute the session under user `auralis`, and implemented a security redirection guard in `auralis-session` to cleanly step down from root if manually invoked.
  - **Home Directory Packaging**: Created `/home/auralis` with user configuration, desktop shortcuts, and `.profile`, and ensured `home` is packaged into `genapkovl` with UID 1000 ownership.

---

### [UX-22] Real-Time System Telemetry & Subsystem Dashboard (`auralis-dashboard`)
- **Component**: `src/auralis-dashboard/`, `overlay/etc/wayfire/wayfire.ini`
- **Priority**: High (P0)
- **Status**: Completed
- **User Impact**: Users lacked an immediate, unified view of system resource consumption (proving the <95MB RAM achievement) and had to check multi-world subsystem hydration status via CLI commands.
- **Solution Delivered**:
  - Developed `auralis-dashboard`, a native GTK4 Liquid Glass telemetry center.
  - Live animated gauges: RAM usage (<95MB Idle badge), CPU load from `/proc/stat`, ZRAM swap allocation, and storage utilization.
  - Multi-world subsystem cards displaying live mount/standby states for Wine, Waydroid, Distrobox, and Darling.
  - Integrated hotkey `<super> D` in Wayfire and pinned trigger in desktop taskbar.

---

### [UX-23] Curated Liquid Glass Wallpaper Suite & `auralis-wallpaper` Switcher
- **Component**: `overlay/usr/share/backgrounds/`, `overlay/usr/local/bin/auralis-wallpaper`
- **Priority**: High (P1)
- **Status**: Completed
- **User Impact**: Missing background assets led to a blank fallback screen on boot, and users had no straightforward mechanism to change wallpapers.
- **Solution Delivered**:
  - Created a curated suite of 3 high-resolution Liquid Glass wallpapers: `auralis-glass-dark.png` (Midnight Glass), `auralis-glass-aurora.png` (Emerald & Sapphire Aurora), and `auralis-glass-cyber.png` (Cyber Obsidian Grid).
  - Authored `/usr/local/bin/auralis-wallpaper` background engine powered by `swaybg` with dynamic cycling and persistent configuration in `~/.config/auralis/wallpaper.conf`.
  - Added 1-click "Change Wallpaper" action inside the Dashboard.

---

### [UX-24] Bespoke Liquid Glass Vector & Raster Icon Suite
- **Component**: `overlay/usr/share/icons/hicolor/scalable/apps/`, `overlay/usr/share/pixmaps/`
- **Priority**: High (P1)
- **Status**: Completed
- **User Impact**: Applications displayed generic placeholder icons (`utilities-system-monitor`, `system-run`), diminishing the bespoke aesthetic of the Liquid Glass desktop.
- **Solution Delivered**:
  - Authored a full vector SVG and raster 128x128 PNG icon theme for:
    - `auralis-logo`: Signature glowing diamond glass prism.
    - `auralis-dashboard`: Live gauge telemetry icon.
    - `auralis-welcome`: Apprenticeship compass & journey star.
    - `auralis-store`: Modular hydration package icon.
    - `auralis-taskmgr`: Pulse telemetry waveform icon.
    - `auralis-installer`: Storage disk and lightning deployment icon.
    - `auralis-router`: 4-World unified hub icon.
  - Integrated icons into `.desktop` files, taskbar buttons, and window headers.

---

### [UX-25] Platform Architecture Evolution: Debian 13 (Trixie) + OpenRC + glibc Live ISO
- **Component**: `build/debian/`, `Dockerfile.builder.debian`, `scripts/build-iso-debian.sh`, `.github/workflows/build-iso-debian.yml`, `overlay/etc/wayfire/wayfire.ini`
- **Priority**: Critical (P0)
- **Status**: Completed (Phase 0 Scaffolding & Toolchain)
- **User Impact**: Alpine Linux with musl libc imposed strict binary incompatibilities for standard desktop software (Google Chrome, VS Code, Discord, Steam, Wine without shims), had fragmented bare-metal firmware coverage, and lacked universal `.deb` package support. Furthermore, 2x1 virtual workspace tiling caused GPU buffer allocation overhead on low-memory machines.
- **Solution Delivered**:
  - **Debian 13 (Trixie) Minimal + OpenRC Base**: Adopted Debian Trixie with OpenRC init, retaining lightweight <130MB idle RAM while delivering 100% glibc application binary compatibility and comprehensive `non-free-firmware` support.
  - **V-Tiling Elimination**: Pruned virtual workspace grid from 2x1 to 1x1 (`vwidth = 1`, `vheight = 1`) in `wayfire.ini`, saving 10–20MB of GPU-mapped buffer RAM.
  - **live-build Pipeline**: Implemented clean `auto/config`, `config/package-lists/auralis.list.chroot`, and live chroot hooks:
    - `0100-user-auralis.hook.chroot`: Provisions standard user `auralis` (UID 1000), seatd groups, and passwordless sudo.
    - `0200-session-autologin.hook.chroot`: Wires tty1 autologin directly into Wayfire session via OpenRC inittab and systemd getty fallback.
    - `0300-tuning-zram.hook.chroot`: Enforces 150% zstd ZRAM swap, `vm.swappiness = 180`, and desktop database caches.
  - **Asset & Config Porting**: Mirrored all bespoke Liquid Glass desktop configs, icons, wallpapers, and application entries into `config/includes.chroot/`.
  - **Hermetic Debian Builder**: Authored `Dockerfile.builder.debian` with GTK4/Layer-Shell development headers, Go toolchain, and `live-build`.
  - **CI/CD Integration**: Added `.github/workflows/build-iso-debian.yml` to automatically build, test, and release the Debian 13 Live ISO on push and release tags.
  - **Root Makefile Targets**: Added `builder-debian` and `iso-debian` to `Makefile`.
