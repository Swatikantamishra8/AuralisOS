# ==============================================================================
# AuralisOS mkimage Profile Script: mkimg.auralis.sh
# Phase 1 to 5: Custom Ultra-Low-Memory Linux Distribution (<95MB Idle RAM)
# Target Architecture: x86_64, OpenRC, musl, Hybrid BIOS/UEFI Boot
# Desktop: "Liquid Glass" Native Wayland (Wayfire + auralis-shell + auralis-taskmgr)
# Multi-World: auralis-router & auralis-reaper (Windows, Android, Linux, macOS)
# Modular Hydration: auralis-store GUI & auralis-hydrate SquashFS Subsystem Extension
# Bare-Metal: auralis-hwdetect, iwd/dhcpcd, ntfsloop Windows Co-Existence Hook
# ==============================================================================

profile_auralis() {
    # Attempt to locate and source mkimg.base.sh if present
    for base_profile in \
        "${scriptdir:-/opt/aports/scripts}/mkimg.base.sh" \
        "/opt/aports/scripts/mkimg.base.sh" \
        "$(dirname "$0")/mkimg.base.sh" \
        "$(dirname "$0")/../mkimg.base.sh"; do
        if [ -f "$base_profile" ]; then
            . "$base_profile"
            profile_base
            break
        fi
    done

    # OS Identification & ISO Metadata
    title="AuralisOS"
    desc="AuralisOS Ultra-Low-Memory Linux Distribution (<95MB RAM Idle)"
    url="https://github.com/BinduOS/AuralisOS"
    image_name="auralis-os"
    image_ext="iso"
    output_format="iso"
    arch="x86_64"
    hostname="auralis"

    # Kernel & Bootloader Parameters
    # - linux-lts provides long-term stability and broad hardware driver support
    # - serial + VGA console for headless/QEMU/VirtualBox and physical display
    # - binder.devices: provisions Android IPC endpoints for Waydroid / container runtimes
    # - quiet: prevents verbose kernel console printing to accelerate boot time and conserve RAM
    kernel_flavors="lts"
    kernel_cmdline="modules=loop,squashfs,sd-mod,usb-storage,ntfs3 console=tty0 console=ttyS0,115200 binder.devices=binder,hwbinder,vndbinder quiet"
    syslinux_serial="0 115200"

    # Initramfs Storage & Bus Features for mkinitfs
    # Includes custom Phase 5 'ntfsloop' hook for booting from Windows C:\AuralisOS\rootfs.img
    initfs_features="ata base cdrom ext4 mmc nvme scsi usb virtio ntfsloop"

    # Overlay generator script that constructs root filesystem configurations
    apkovl="genapkovl-auralis.sh"

    # 1. Core system packages:
    # - alpine-base, alpine-conf: Musl libc, BusyBox userland, OpenRC init
    # - eudev, kmod: Hotplug device manager & kernel module loader for PCI/USB modalias matching
    # - zram-init: Automatic compressed RAM swap creation using zstd algorithm
    # - util-linux, shadow, doas: Minimal system administration utilities
    apks="alpine-base alpine-conf eudev kmod zram-init util-linux busybox busybox-extras shadow doas"

    # 2. Kernel packages
    apks="$apks linux-lts"

    # 3. Minimal network & remote diagnostic tooling
    apks="$apks iproute2 ethtool openssh openssh-server openssh-client"

    # 4. Phase 2: "Liquid Glass" Wayland Desktop Environment (<95MB Idle RAM)
    # - wayfire, wayfire-plugins-extra: Compositor with Dual-Kawase blur & rounded corners
    # - seatd, libseat: Minimal seat management daemon (replaces bloated logind/elogind)
    # - gtk4.0, gtk4-layer-shell: Runtime libraries for auralis-shell, auralis-taskmgr, auralis-store
    # - mesa-dri-gallium, mesa-va-gallium: Gallium hardware & llvmpipe software rasterizer
    # - font-noto, font-dejavu: Crisp vector typography with minimal disk footprint
    # - adwaita-icon-theme: Essential system icons for desktop entries & tray
    # - foot: Ultra-lightweight Wayland terminal (2-3MB RAM footprint)
    # - libinput, eudev: Input event handling
    apks="$apks wayfire wayfire-plugins-extra seatd libseat gtk4.0 gtk4-layer-shell mesa-dri-gallium mesa-va-gallium font-noto font-dejavu adwaita-icon-theme foot libinput"

    # 5. Phase 3 & 4: Universal 4-World Binary Router & Subsystem Tooling
    # - shared-mime-info, file: MIME type database and file format sniffing
    # - p7zip: Extraction utility for macOS .dmg and archive inspection
    # - podman, distrobox: Rootless container runtime for .deb / .rpm translation
    apks="$apks shared-mime-info file p7zip podman distrobox"

    # 6. Phase 5: Bare-Metal Auto-Tuner & Wi-Fi Stack
    # - pciutils, usbutils: Hardware identification (lspci -nn, lsusb)
    # - iwd, dhcpcd: Modern, fast, low-memory Wi-Fi daemon & DHCP client
    # - util-linux-misc: Provides rfkill for radio transmitter control
    apks="$apks pciutils usbutils iwd dhcpcd util-linux-misc"

    # Flavor Resolution:
    # VM Mode: ultra-minimal, linux-firmware-none, 0MB firmware bloat for VirtualBox/QEMU
    # Metal Mode: full linux-firmware + sof-firmware (Sound Open Firmware) for real PC hardware
    FLAVOR="${AURALIS_FLAVOR:-${AURALIS_TARGET:-vm}}"

    case "$FLAVOR" in
        metal|baremetal|hardware)
            image_name="auralis-os-metal"
            # Full firmware payload for bare metal:
            # - linux-firmware: Intel, AMD, Realtek, Broadcom, Qualcomm, Atheros firmware blobs
            # - sof-firmware: Sound Open Firmware audio DSP binaries for modern Intel/AMD laptops & desktops
            # - wireless-tools, wpa_supplicant, iw: Wi-Fi stack
            # - pciutils, usbutils, nvme-cli, efibootmgr: Hardware inspection and NVMe/UEFI management
            apks="$apks linux-firmware sof-firmware wireless-tools wpa_supplicant iw pciutils usbutils efibootmgr nvme-cli"
            ;;
        vm|virtualbox|qemu|*)
            image_name="auralis-os-vm"
            # Ultra-slim payload for virtualized environments:
            # - linux-firmware-none: satisfies firmware dependencies with 0 MB disk/RAM overhead
            # - pciutils, usbutils: essential bus introspection
            apks="$apks linux-firmware-none pciutils usbutils"
            ;;
    esac
}

# Dedicated profile entrypoint for slim VirtualBox / QEMU builds
profile_auralis_vm() {
    export AURALIS_FLAVOR="vm"
    profile_auralis
}

# Dedicated profile entrypoint for full Bare-Metal hardware builds
profile_auralis_metal() {
    export AURALIS_FLAVOR="metal"
    profile_auralis
}
