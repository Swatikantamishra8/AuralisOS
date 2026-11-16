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
| **UX-16** | Desktop Shell | First-Boot "Welcome to AuralisOS" Interactive Onboarding Tour | **P1** | 📋 Backlog |
| **UX-17** | `auralis-shell` | Native GDBus Scanned Wi-Fi SSID Dropdown in Control Center | **P1** | 📋 Backlog |
| **UX-18** | `auralis-store` | Real-Time Subsystem Hydration Download Progress Meter | **P1** | 📋 Backlog |
| **UX-19** | Audio Subsystem | PipeWire + WirePlumber Dynamic Audio Device Routing Profile | **P1** | 📋 Backlog |
| **UX-20** | CI/CD | Automated GitHub Actions Multi-Flavor ISO Release Pipeline | **P0** | ✅ Implemented |
| **UX-21** | Session / Overlay | Wayfire Boot Refusal on Bare Metal (`Unable to drop root`) | **P0** | ✅ Implemented |

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

### [UX-16] Onboarding Tour / Welcome Guide (Roadmap Item)
- **Component**: New Application `auralis-welcome`
- **Priority**: Medium (P1)
- **Description**: Provide a 3-slide introductory guide highlighting core hotkeys, gesture navigation, and demonstrating how to run Windows `.exe` and Android `.apk` files.

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
