#!/bin/sh -e
# ==============================================================================
# AuralisOS Overlay Generator: genapkovl-auralis.sh
# Phase 1 to 5: Custom Ultra-Low-Memory Linux Distribution (<95MB Idle RAM)
# Target Architecture: x86_64, OpenRC, musl, eudev, zram-init, Android Binder
# Desktop Environment: "Liquid Glass" Wayland (Wayfire + auralis-shell + auralis-taskmgr)
# Multi-World Subsystem: auralis-router & auralis-reaper (Windows, Android, Linux, macOS)
# Modular Hydration: auralis-store GUI & auralis-hydrate SquashFS Subsystem Extension
# Bare-Metal & Loopback: auralis-hwdetect, iwd/dhcpcd, ntfsloop Windows Co-Existence
# ==============================================================================

HOSTNAME="${1:-auralis}"

CLEANUP=
cleanup() {
    [ -n "$CLEANUP" ] && rm -rf "$CLEANUP"
}
trap cleanup EXIT INT TERM

# Resolve workspace directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_ROOT="$SCRIPT_DIR"
if [ ! -d "$WORKSPACE_ROOT/src" ] && [ -d "/workspace/src" ]; then
    WORKSPACE_ROOT="/workspace"
fi

# Create temporary staging root
TMP="$(mktemp -d /tmp/genapkovl-auralis.XXXXXX)"
CLEANUP="$TMP"

mkdir -p "$TMP"/etc
mkdir -p "$TMP"/etc/runlevels/sysinit
mkdir -p "$TMP"/etc/runlevels/boot
mkdir -p "$TMP"/etc/runlevels/default
mkdir -p "$TMP"/etc/runlevels/shutdown
mkdir -p "$TMP"/etc/conf.d
mkdir -p "$TMP"/etc/sysctl.d
mkdir -p "$TMP"/etc/udev/rules.d
mkdir -p "$TMP"/etc/binfmt.d
mkdir -p "$TMP"/etc/init.d
mkdir -p "$TMP"/etc/mkinitfs/features.d
mkdir -p "$TMP"/etc/network
mkdir -p "$TMP"/etc/local.d
mkdir -p "$TMP"/etc/ssh
mkdir -p "$TMP"/etc/wayfire
mkdir -p "$TMP"/etc/xdg
mkdir -p "$TMP"/etc/auralis
mkdir -p "$TMP"/etc/iwd
mkdir -p "$TMP"/usr/bin
mkdir -p "$TMP"/usr/local/bin
mkdir -p "$TMP"/usr/share/applications
mkdir -p "$TMP"/usr/share/mkinitfs/hooks
mkdir -p "$TMP"/usr/share/auralis
mkdir -p "$TMP"/usr/share/auralis-shell
mkdir -p "$TMP"/usr/share/auralis-taskmgr
mkdir -p "$TMP"/usr/share/auralis-store
mkdir -p "$TMP"/usr/share/backgrounds
mkdir -p "$TMP"/var/lib/auralis/subsystems
mkdir -p "$TMP"/var/lib/auralis/mnt
mkdir -p "$TMP"/media/windows_c
mkdir -p "$TMP"/root/.config
mkdir -p "$TMP"/var/log

# ------------------------------------------------------------------------------
# 1. System Identity, Hostname & OS-Release
# ------------------------------------------------------------------------------
echo "$HOSTNAME" > "$TMP"/etc/hostname

cat > "$TMP"/etc/hosts <<EOF
127.0.0.1   localhost $HOSTNAME
::1         localhost ip6-localhost ip6-loopback
fe00::0     ip6-localnet
ff00::0     ip6-mcastprefix
ff02::1     ip6-allnodes
ff02::2     ip6-allrouters
EOF

cat > "$TMP"/etc/os-release <<'EOF'
NAME="AuralisOS"
ID=auralis
PRETTY_NAME="AuralisOS Liquid Glass Edition"
VERSION="1.0"
HOME_URL="https://auralis.org"
BUG_REPORT_URL="https://github.com/BinduOS/AuralisOS/issues"
EOF

# ------------------------------------------------------------------------------
# 2. OpenRC Service Runlevel Wiring
#    Requirement: eudev and kmod in 'sysinit' runlevel
#    Requirement: seatd, binfmt, auralis-hwdetect, zram-init in 'boot' runlevel
#    Requirement: auralis-reaper in 'default' runlevel
# ------------------------------------------------------------------------------

# Helper function to enable OpenRC service
rc_add() {
    local service="$1"
    local runlevel="$2"
    mkdir -p "$TMP/etc/runlevels/$runlevel"
    ln -sf "/etc/init.d/$service" "$TMP/etc/runlevels/$runlevel/$service"
}

# Sysinit runlevel: earliest hardware bootstrap & hotplug discovery
rc_add devfs sysinit
rc_add dmesg sysinit
rc_add modloop sysinit
rc_add udev sysinit
rc_add udev-trigger sysinit
rc_add udev-settle sysinit

# Boot runlevel: hardware auto-tuner, seatd, binfmt, zram & memory subsystem
rc_add hwclock boot
rc_add modules boot
rc_add sysctl boot
rc_add hostname boot
rc_add bootmisc boot
rc_add syslog boot
rc_add zram-init boot
rc_add seatd boot
rc_add binfmt boot
rc_add auralis-hwdetect boot

# Default runlevel: network, userland services & Phase 3 idle memory reaper
rc_add local default
rc_add networking default
rc_add sshd default
rc_add auralis-reaper default

# Shutdown runlevel: clean unmount
rc_add killprocs shutdown
rc_add mount-ro shutdown
rc_add savecache shutdown

# ------------------------------------------------------------------------------
# 3. Dynamic Hardware & PCI/USB Modalias Driver Loading Configuration
#    Supports: virtio, vboxguest, i915, amdgpu, nvme, iwlwifi
# ------------------------------------------------------------------------------

# Silence udev logging to save memory and avoid TTY spam
cat > "$TMP"/etc/udev/udev.conf <<'EOF'
# /etc/udev/udev.conf
udev_log=err
children_max=16
exec_delay=0
event_timeout=30
resolve_names=never
EOF

# Preload foundational virtual bus & block modules
cat > "$TMP"/etc/modules <<'EOF'
# Core virtual block, network and storage modules
loop
squashfs
virtio_pci
virtio_blk
virtio_net
virtio_balloon
virtio_console
virtio_ring
nvme
nvme_core
ntfs3
EOF

# Custom udev rules to guarantee modalias triggers and DRM / seat permissions
cat > "$TMP"/etc/udev/rules.d/50-auralis-drivers.rules <<'EOF'
# AuralisOS Dynamic Driver Loading Rules
# Automatically load modules matching MODALIAS for PCI, USB, VirtIO, ACPI
SUBSYSTEM=="*", ENV{MODALIAS}=="?*", RUN{builtin}+="kmod load $env{MODALIAS}"

# GPU acceleration nodes permissions
KERNEL=="card[0-9]*", SUBSYSTEM=="drm", MODE="0666", GROUP="video"
KERNEL=="renderD[0-9]*", SUBSYSTEM=="drm", MODE="0666", GROUP="video"

# VirtualBox Guest Additions device node permissions
KERNEL=="vboxguest", MODE="0666"
KERNEL=="vboxuser", MODE="0666"

# KVM & vhost virtualization acceleration nodes
KERNEL=="kvm", MODE="0666", GROUP="kvm"
KERNEL=="vhost-net", MODE="0660", GROUP="kvm"

# Input nodes for seatd / Wayland compositor
SUBSYSTEM=="input", MODE="0660", GROUP="input"
EOF

# Ensure group memberships for seatd, display and input
cat > "$TMP"/etc/group <<'EOF'
root:x:0:root
bin:x:1:root,bin,daemon
daemon:x:2:root,bin,daemon
sys:x:3:root,bin,adm
adm:x:4:root,adm,daemon
tty:x:5:
disk:x:6:root,adm
lp:x:7:lp
mem:x:8:
kmem:x:9:
wheel:x:10:root
floppy:x:11:
mail:x:12:mail
news:x:13:news
uucp:x:14:uucp
man:x:15:man
cron:x:16:cron
console:x:17:
audio:x:18:root
cdrom:x:19:
dialout:x:20:root
tape:x:26:root
video:x:27:root
netdev:x:28:
kvm:x:34:root
input:x:99:root
seat:x:100:root
users:x:1000:
EOF

# ------------------------------------------------------------------------------
# 4. ZRAM Memory Swap & Kernel Virtual Memory Optimization
#    Requirement: Allocate 150% of physical RAM using zstd compression
#    Requirement: vm.swappiness = 180
# ------------------------------------------------------------------------------

cat > "$TMP"/etc/conf.d/zram-init <<'EOF'
# /etc/conf.d/zram-init
# AuralisOS Ultra-Low-Memory Configuration

# Number of zram devices to create
num_devices=1

# Device 0: 150% of physical RAM allocated with ultra-fast zstd compression
type0=swap
size0=150%
algo0=zstd
max_streams0=0

# Enforce high swappiness inside zram-init
sysctl_vm_swappiness=180
EOF

# Sysctl kernel tuning for aggressive memory saving and rapid page reclamation
cat > "$TMP"/etc/sysctl.d/00-auralis-memory.conf <<'EOF'
# AuralisOS Ultra-Low-Memory Kernel Tuning (<95MB Idle Footprint)
# Aggressively swap anonymous memory to zram while preserving filesystem cache
vm.swappiness = 180

# Keep page cache reclaim active but preserve directory trees
vm.vfs_cache_pressure = 50

# Flusher thread tuning to prevent memory pressure stalls
vm.dirty_ratio = 10
vm.dirty_background_ratio = 5

# Memory compaction threshold
vm.extfrag_threshold = 500

# Enable panic on out of memory if critical
vm.panic_on_oom = 0

# Network stack memory minimization
net.core.rmem_default = 65536
net.core.wmem_default = 65536
net.core.rmem_max = 1048576
net.core.wmem_max = 1048576
EOF

# Also mirror swappiness in /etc/sysctl.conf for unconditional enforcement
cat > "$TMP"/etc/sysctl.conf <<'EOF'
vm.swappiness = 180
EOF

# ------------------------------------------------------------------------------
# 5. Android Binder Container Support
#    Boot param: binder.devices="binder,hwbinder,vndbinder"
# ------------------------------------------------------------------------------

cat > "$TMP"/etc/udev/rules.d/99-android-binder.rules <<'EOF'
# Android Binder IPC Endpoints permissions
KERNEL=="binder",   MODE="0666", OWNER="root", GROUP="root"
KERNEL=="hwbinder", MODE="0666", OWNER="root", GROUP="root"
KERNEL=="vndbinder", MODE="0666", OWNER="root", GROUP="root"
KERNEL=="binderfs", MODE="0755", OWNER="root", GROUP="root"
EOF

cat > "$TMP"/etc/local.d/00-binderfs.start <<'EOF'
#!/bin/sh
# AuralisOS Android BinderFS Initializer
if grep -q binder /proc/filesystems 2>/dev/null; then
    mkdir -p /dev/binderfs
    mount -t binder binder /dev/binderfs 2>/dev/null || true
    [ -e /dev/binderfs/binder ] && ln -sf /dev/binderfs/binder /dev/binder 2>/dev/null || true
    [ -e /dev/binderfs/hwbinder ] && ln -sf /dev/binderfs/hwbinder /dev/hwbinder 2>/dev/null || true
    [ -e /dev/binderfs/vndbinder ] && ln -sf /dev/binderfs/vndbinder /dev/vndbinder 2>/dev/null || true
    chmod 666 /dev/binderfs/* /dev/binder* /dev/hwbinder /dev/vndbinder 2>/dev/null || true
fi
EOF
chmod +x "$TMP"/etc/local.d/00-binderfs.start

# ------------------------------------------------------------------------------
# 6. Phase 2: "Liquid Glass" Wayland Session & Auto-start Wiring
# ------------------------------------------------------------------------------

# Install Wayfire configuration from overlay
if [ -f "$WORKSPACE_ROOT/overlay/etc/wayfire/wayfire.ini" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/wayfire/wayfire.ini" "$TMP/etc/wayfire/wayfire.ini"
    cp "$WORKSPACE_ROOT/overlay/etc/wayfire/wayfire.ini" "$TMP/root/.config/wayfire.ini"
fi

# Install auralis-session launcher script
if [ -f "$WORKSPACE_ROOT/overlay/usr/local/bin/auralis-session" ]; then
    cp "$WORKSPACE_ROOT/overlay/usr/local/bin/auralis-session" "$TMP/usr/local/bin/auralis-session"
    chmod +x "$TMP/usr/local/bin/auralis-session"
fi

# Autologin handler on tty1
cat > "$TMP"/usr/local/bin/auralis-autologin <<'EOF'
#!/bin/sh
# Automatic root session launcher on tty1
export USER=root
export HOME=/root
export SHELL=/bin/sh
cd /root
exec /usr/local/bin/auralis-session
EOF
chmod +x "$TMP"/usr/local/bin/auralis-autologin

# Configure inittab:
# tty1: Auto-starts Wayfire + auralis-shell via seatd & auralis-autologin
# ttyS0: Serial console for headless hypervisors & recovery
cat > "$TMP"/etc/inittab <<'EOF'
# /etc/inittab - AuralisOS Inittab
::sysinit:/sbin/openrc sysinit
::wait:/sbin/openrc boot
::wait:/sbin/openrc default

# Auto-start Liquid Glass Wayland GUI on tty1
tty1::respawn:/sbin/getty -n -l /usr/local/bin/auralis-autologin 38400 tty1 linux

# Serial console emergency shell
ttyS0::respawn:/sbin/getty -n -l /bin/sh 115200 ttyS0 vt100

::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/openrc shutdown
EOF

# Passwordless root access
cat > "$TMP"/etc/shadow <<'EOF'
root::19000:0:99999:7:::
bin:!::0:::::
daemon:!::0:::::
adm:!::0:::::
lp:!::0:::::
sync:!::0:::::
shutdown:!::0:::::
halt:!::0:::::
mail:!::0:::::
news:!::0:::::
uucp:!::0:::::
operator:!::0:::::
man:!::0:::::
postmaster:!::0:::::
cron:!::0:::::
ftp:!::0:::::
sshd:!::0:::::
nobody:!::0:::::
EOF

# ------------------------------------------------------------------------------
# 7. Phase 3, 4 & 5: Subsystems, Hydrator, Auto-Tuner & Loopback Boot
# ------------------------------------------------------------------------------

echo "[*] Installing Phase 3, 4 & 5 System Extensions..."

# 1. Install binfmt.d registration
if [ -f "$WORKSPACE_ROOT/overlay/etc/binfmt.d/auralis.conf" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/binfmt.d/auralis.conf" "$TMP/etc/binfmt.d/auralis.conf"
fi

# 2. Install desktop entries and MIME associations
if [ -f "$WORKSPACE_ROOT/overlay/usr/share/applications/auralis-router.desktop" ]; then
    cp "$WORKSPACE_ROOT/overlay/usr/share/applications/auralis-router.desktop" "$TMP/usr/share/applications/auralis-router.desktop"
fi
if [ -f "$WORKSPACE_ROOT/overlay/usr/share/applications/auralis-store.desktop" ]; then
    cp "$WORKSPACE_ROOT/overlay/usr/share/applications/auralis-store.desktop" "$TMP/usr/share/applications/auralis-store.desktop"
fi

if [ -f "$WORKSPACE_ROOT/overlay/etc/xdg/mimeapps.list" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/xdg/mimeapps.list" "$TMP/etc/xdg/mimeapps.list"
    cp "$WORKSPACE_ROOT/overlay/etc/xdg/mimeapps.list" "$TMP/usr/share/applications/mimeapps.list"
fi

# 3. Install auralis-reaper script and OpenRC service
if [ -f "$WORKSPACE_ROOT/overlay/usr/bin/auralis-reaper" ]; then
    cp "$WORKSPACE_ROOT/overlay/usr/bin/auralis-reaper" "$TMP/usr/bin/auralis-reaper"
    cp "$WORKSPACE_ROOT/overlay/usr/bin/auralis-reaper" "$TMP/usr/local/bin/auralis-reaper"
    chmod +x "$TMP/usr/bin/auralis-reaper" "$TMP/usr/local/bin/auralis-reaper"
fi

if [ -f "$WORKSPACE_ROOT/overlay/etc/init.d/auralis-reaper" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/init.d/auralis-reaper" "$TMP/etc/init.d/auralis-reaper"
    chmod +x "$TMP/etc/init.d/auralis-reaper"
fi

# 4. Phase 5: Hardware Auto-Tuner service
if [ -f "$WORKSPACE_ROOT/overlay/etc/init.d/auralis-hwdetect" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/init.d/auralis-hwdetect" "$TMP/etc/init.d/auralis-hwdetect"
    chmod +x "$TMP/etc/init.d/auralis-hwdetect"
fi

# 5. Phase 5: Custom mkinitfs ntfsloop Hook & Features
if [ -f "$WORKSPACE_ROOT/overlay/etc/mkinitfs/features.d/ntfsloop.modules" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/mkinitfs/features.d/ntfsloop.modules" "$TMP/etc/mkinitfs/features.d/ntfsloop.modules"
fi
if [ -f "$WORKSPACE_ROOT/overlay/etc/mkinitfs/features.d/ntfsloop.files" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/mkinitfs/features.d/ntfsloop.files" "$TMP/etc/mkinitfs/features.d/ntfsloop.files"
fi
if [ -f "$WORKSPACE_ROOT/overlay/usr/share/mkinitfs/hooks/ntfsloop.sh" ]; then
    cp "$WORKSPACE_ROOT/overlay/usr/share/mkinitfs/hooks/ntfsloop.sh" "$TMP/usr/share/mkinitfs/hooks/ntfsloop.sh"
    chmod +x "$TMP/usr/share/mkinitfs/hooks/ntfsloop.sh"
fi

# 6. Install auralis-hydrate privileged helper script
if [ -f "$WORKSPACE_ROOT/overlay/usr/local/bin/auralis-hydrate" ]; then
    cp "$WORKSPACE_ROOT/overlay/usr/local/bin/auralis-hydrate" "$TMP/usr/local/bin/auralis-hydrate"
    cp "$WORKSPACE_ROOT/overlay/usr/local/bin/auralis-hydrate" "$TMP/usr/bin/auralis-hydrate"
    chmod +x "$TMP/usr/local/bin/auralis-hydrate" "$TMP/usr/bin/auralis-hydrate"
fi

# 7. Install curated catalog JSON
if [ -f "$WORKSPACE_ROOT/overlay/etc/auralis/catalog.json" ]; then
    cp "$WORKSPACE_ROOT/overlay/etc/auralis/catalog.json" "$TMP/etc/auralis/catalog.json"
    cp "$WORKSPACE_ROOT/overlay/etc/auralis/catalog.json" "$TMP/usr/share/auralis/catalog.json"
fi

# 8. Install auralis-router binary
if [ -f "$WORKSPACE_ROOT/src/auralis-router/auralis-router" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-router/auralis-router" "$TMP/usr/bin/auralis-router"
    cp "$WORKSPACE_ROOT/src/auralis-router/auralis-router" "$TMP/usr/local/bin/auralis-router"
    chmod +x "$TMP/usr/bin/auralis-router" "$TMP/usr/local/bin/auralis-router"
elif command -v go >/dev/null 2>&1 && [ -f "$WORKSPACE_ROOT/src/auralis-router/main.go" ]; then
    echo "[*] Compiling static auralis-router binary..."
    (cd "$WORKSPACE_ROOT/src/auralis-router" && CGO_ENABLED=0 go build -ldflags="-s -w" -trimpath -o "$TMP/usr/bin/auralis-router" main.go)
    ln -sf /usr/bin/auralis-router "$TMP/usr/local/bin/auralis-router"
    chmod +x "$TMP/usr/bin/auralis-router" "$TMP/usr/local/bin/auralis-router"
fi

# ------------------------------------------------------------------------------
# 8. Compile & Install Phase 2 & 4 Native Desktop Binaries (auralis-shell, auralis-taskmgr, auralis-store)
# ------------------------------------------------------------------------------

echo "[*] Wiring Native Desktop Applications (auralis-shell, auralis-taskmgr, auralis-store)..."

if command -v gcc >/dev/null 2>&1 && pkg-config --exists gtk4 2>/dev/null; then
    echo "[*] Building auralis-shell from source..."
    make -C "$WORKSPACE_ROOT/src/auralis-shell" DESTDIR="$TMP" install || true
    echo "[*] Building auralis-taskmgr from source..."
    make -C "$WORKSPACE_ROOT/src/auralis-taskmgr" DESTDIR="$TMP" install || true
    echo "[*] Building auralis-store from source..."
    make -C "$WORKSPACE_ROOT/src/auralis-store" DESTDIR="$TMP" install || true
fi

# Fallback copy if binaries exist in workspace
if [ -f "$WORKSPACE_ROOT/src/auralis-shell/auralis-shell" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-shell/auralis-shell" "$TMP/usr/local/bin/auralis-shell"
    chmod +x "$TMP/usr/local/bin/auralis-shell"
fi
if [ -f "$WORKSPACE_ROOT/src/auralis-taskmgr/auralis-taskmgr" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-taskmgr/auralis-taskmgr" "$TMP/usr/local/bin/auralis-taskmgr"
    chmod +x "$TMP/usr/local/bin/auralis-taskmgr"
fi
if [ -f "$WORKSPACE_ROOT/src/auralis-store/auralis-store" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-store/auralis-store" "$TMP/usr/local/bin/auralis-store"
    chmod +x "$TMP/usr/local/bin/auralis-store"
fi

# Copy CSS assets
if [ -f "$WORKSPACE_ROOT/src/auralis-shell/style.css" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-shell/style.css" "$TMP/usr/share/auralis-shell/style.css"
fi
if [ -f "$WORKSPACE_ROOT/src/auralis-taskmgr/style.css" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-taskmgr/style.css" "$TMP/usr/share/auralis-taskmgr/style.css"
fi
if [ -f "$WORKSPACE_ROOT/src/auralis-store/style.css" ]; then
    cp "$WORKSPACE_ROOT/src/auralis-store/style.css" "$TMP/usr/share/auralis-store/style.css"
fi

# ------------------------------------------------------------------------------
# 9. Standard Desktop Entries (.desktop)
# ------------------------------------------------------------------------------

cat > "$TMP"/usr/share/applications/auralis-taskmgr.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Auralis Task Manager
Comment=Real-time RAM and CPU Resource Monitor
Exec=/usr/local/bin/auralis-taskmgr
Icon=utilities-system-monitor
Categories=System;Monitor;
EOF

cat > "$TMP"/usr/share/applications/foot.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Foot Terminal
Comment=Fast, ultra-low-memory Wayland terminal emulator
Exec=foot
Icon=utilities-terminal
Categories=System;TerminalEmulator;
EOF

cat > "$TMP"/usr/share/applications/auralis-status.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Auralis Diagnostics
Comment=Architectural Verification and Diagnostics
Exec=foot -e /usr/local/bin/auralis-status
Icon=help-about
Categories=System;
EOF

# ------------------------------------------------------------------------------
# 10. Network & SSH Configuration
# ------------------------------------------------------------------------------

cat > "$TMP"/etc/network/interfaces <<'EOF'
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet dhcp

auto ens3
iface ens3 inet dhcp

auto enp0s3
iface enp0s3 inet dhcp
EOF

cat > "$TMP"/etc/resolv.conf <<'EOF'
nameserver 1.1.1.1
nameserver 8.8.8.8
EOF

cat > "$TMP"/etc/ssh/sshd_config <<'EOF'
Port 22
PermitRootLogin yes
PermitEmptyPasswords yes
ChallengeResponseAuthentication no
PasswordAuthentication yes
UsePAM no
X11Forwarding no
PrintMotd yes
Subsystem sftp internal-sftp
EOF

cat > "$TMP"/etc/local.d/sshd-keygen.start <<'EOF'
#!/bin/sh
if [ ! -f /etc/ssh/ssh_host_ed25519_key ]; then
    ssh-keygen -A 2>/dev/null || true
fi
EOF
chmod +x "$TMP"/etc/local.d/sshd-keygen.start

# ------------------------------------------------------------------------------
# 11. Diagnostics Utility & Root Shell Profile
# ------------------------------------------------------------------------------

cat > "$TMP"/usr/local/bin/auralis-status <<'EOF'
#!/bin/sh
# AuralisOS Diagnostics & Architecture Verification Utility
BOLD="\033[1m"
GREEN="\033[32m"
CYAN="\033[36m"
YELLOW="\033[33m"
RESET="\033[0m"

echo "${CYAN}================================================================${RESET}"
echo "${BOLD} AuralisOS Architectural Verification Status (Phase 5)${RESET}"
echo "${CYAN}================================================================${RESET}"

echo "\n${GREEN}[*] Idle Memory Consumption:${RESET}"
free -m

echo "\n${GREEN}[*] ZRAM Swap Device Status:${RESET}"
if command -v zramctl >/dev/null 2>&1; then
    zramctl
elif [ -d /sys/block/zram0 ]; then
    DISKSIZE=$(cat /sys/block/zram0/disksize 2>/dev/null || echo "0")
    DISKSIZE_MB=$(( DISKSIZE / 1024 / 1024 ))
    ALGO=$(cat /sys/block/zram0/comp_algorithm 2>/dev/null || echo "unknown")
    echo "  Device: /dev/zram0"
    echo "  Allocated Size: ${DISKSIZE_MB} MB"
    echo "  Compression Algorithm: ${ALGO}"
fi

echo "\n${GREEN}[*] Kernel Swappiness:${RESET}"
echo "  vm.swappiness = $(cat /proc/sys/vm/swappiness 2>/dev/null || echo 'unknown')"

echo "\n${GREEN}[*] Wayland & Desktop Processes:${RESET}"
ps aux | grep -E "wayfire|auralis-shell|auralis-taskmgr|auralis-store|seatd|foot|auralis-reaper" | grep -v grep || echo "  No GUI processes active."

echo "\n${GREEN}[*] GPU Profile (/etc/profile.d/auralis-gpu.sh):${RESET}"
if [ -f /etc/profile.d/auralis-gpu.sh ]; then
    cat /etc/profile.d/auralis-gpu.sh | grep -v '^#'
else
    echo "  GPU profile not yet generated."
fi

echo "\n${GREEN}[*] Active Subsystems Mounts (/var/lib/auralis/mnt):${RESET}"
mount | grep "/var/lib/auralis/mnt" || echo "  No on-demand subsystems currently mounted (ultra-low memory idle)."

echo "\n${GREEN}[*] Windows NTFS Host Mount (/media/windows_c):${RESET}"
if mountpoint -q /media/windows_c 2>/dev/null; then
    echo "  Windows C: partition mounted at /media/windows_c via ntfs3 loopback."
else
    echo "  Running in native ISO / hypervisor mode (not loopback boot)."
fi

echo "\n${GREEN}[*] Binary Formats (binfmt_misc):${RESET}"
if [ -d /proc/sys/fs/binfmt_misc ]; then
    ls /proc/sys/fs/binfmt_misc/ 2>/dev/null || echo "  binfmt_misc mounted but empty"
else
    echo "  binfmt_misc not mounted"
fi

echo "\n${CYAN}================================================================${RESET}"
EOF
chmod +x "$TMP"/usr/local/bin/auralis-status

cat > "$TMP"/etc/motd <<'EOF'
     _                     _ _      ___  ____  
    / \  _   _ _ __ __ _  | (_)___ / _ \/ ___| 
   / _ \| | | | '__/ _` | | | / __| | | \___ \ 
  / ___ \ |_| | | | (_| | | | \__ \ |_| |___) |
 /_/   \_\__,_|_|  \__,_| |_|_|___/\___/|____/ 
 
 Welcome to AuralisOS: Liquid Glass Edition
 Ultra-Low Memory Linux (<95MB Idle RAM, Wayland Native)
 Dual-Boot Loopback: Boots from Windows C:\AuralisOS\rootfs.img
 Dynamic Hardware Profiling: Intel/AMD/NVIDIA GPU + iwd/dhcpcd Wi-Fi
 Run 'auralis-status' for diagnostics or press Ctrl+Shift+Esc for Task Manager.

EOF

cat > "$TMP"/root/.profile <<'EOF'
alias ll='ls -la'
# If logged into tty1 and no Wayland display, launch GUI session
if [ "$(tty 2>/dev/null)" = "/dev/tty1" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    exec /usr/local/bin/auralis-session
elif [ -x /usr/local/bin/auralis-status ]; then
    /usr/local/bin/auralis-status
fi
EOF

# ------------------------------------------------------------------------------
# 12. Archive Generation (.apkovl.tar.gz)
# ------------------------------------------------------------------------------

ARCHIVE_NAME="$HOSTNAME.apkovl.tar.gz"

# Pack overlay tarball preserving file ownership and permissions
tar -c -C "$TMP" etc root usr var media | gzip -9n > "$ARCHIVE_NAME"

# Copy also to /tmp if run by mkimage.sh
cp "$ARCHIVE_NAME" "/tmp/$ARCHIVE_NAME" 2>/dev/null || true

echo "$ARCHIVE_NAME"
exit 0
