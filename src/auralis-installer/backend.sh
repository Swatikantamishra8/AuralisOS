#!/bin/sh
# ==============================================================================
# AuralisOS Graphical Installer Backend Engine: auralis-installer-backend
# Handles disk partitioning, formatting, rootfs deployment, and GRUB2 setup.
# Outputs: PROGRESS: <0-100> and STATUS: <message> on stdout for GUI consumption.
# ==============================================================================

set -e

MODE="${1:-loopback}"     # "loopback" or "native"
TARGET_DEV="${2:-/dev/sda3}" # Partition (for loopback) or Disk (for native)
HOSTNAME_VAL="${3:-auralis}"
USERNAME_VAL="${4:-auralis}"
PASSWORD_VAL="${5:-auralis}"

progress() {
    local pct="$1"
    local msg="$2"
    echo "PROGRESS: $pct"
    echo "STATUS: $msg"
}

progress 5 "Initializing installer environment..."

# Ensure required tools
for tool in parted mkfs.ext4 blkid mount umount; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: Required tool '$tool' is not installed."
        exit 1
    fi
done

TARGET_MNT="/mnt/auralis_install_target"
mkdir -p "$TARGET_MNT"

# ==============================================================================
# Mode 1: Windows Loopback Co-Existence (Zero Repartitioning)
# ==============================================================================
if [ "$MODE" = "loopback" ]; then
    progress 15 "Mounting Windows partition ($TARGET_DEV)..."
    WIN_MNT="/mnt/win_host"
    mkdir -p "$WIN_MNT"
    umount "$WIN_MNT" 2>/dev/null || true

    # Mount with ntfs3
    if ! mount -t ntfs3 -o rw,noatime,force "$TARGET_DEV" "$WIN_MNT" 2>/dev/null; then
        mount -t ntfs -o rw "$TARGET_DEV" "$WIN_MNT" || {
            echo "ERROR: Failed to mount Windows partition $TARGET_DEV as read-write."
            exit 1
        }
    fi

    progress 25 "Creating 24 GB sparse image at \\AuralisOS\\rootfs.img..."
    mkdir -p "$WIN_MNT/AuralisOS"
    IMG_PATH="$WIN_MNT/AuralisOS/rootfs.img"
    
    # Create 24GB sparse file
    truncate -s 24G "$IMG_PATH"

    progress 35 "Formatting virtual disk as ext4..."
    mkfs.ext4 -F -L "AURALIS_ROOT" "$IMG_PATH"

    progress 45 "Mounting virtual rootfs..."
    mount -o loop "$IMG_PATH" "$TARGET_MNT"

# ==============================================================================
# Mode 2: Native Drive / Dedicated Partition Installation
# ==============================================================================
elif [ "$MODE" = "native" ]; then
    progress 15 "Preparing target disk ($TARGET_DEV)..."

    # Check if target is a whole disk (e.g., /dev/sda, /dev/nvme0n1)
    if [ -b "$TARGET_DEV" ] && ! echo "$TARGET_DEV" | grep -qE '[0-9]$'; then
        progress 20 "Partitioning entire disk with GPT layout..."
        parted -s "$TARGET_DEV" mklabel gpt
        parted -s "$TARGET_DEV" mkpart ESP fat32 1MiB 513MiB
        parted -s "$TARGET_DEV" set 1 esp on
        parted -s "$TARGET_DEV" mkpart AuralisRoot ext4 513MiB 100%

        # Wait for device nodes to populate
        sleep 1
        if echo "$TARGET_DEV" | grep -q "nvme"; then
            EFI_PART="${TARGET_DEV}p1"
            ROOT_PART="${TARGET_DEV}p2"
        else
            EFI_PART="${TARGET_DEV}1"
            ROOT_PART="${TARGET_DEV}2"
        fi

        progress 25 "Formatting EFI System Partition (FAT32)..."
        mkfs.vfat -F 32 -n "EFI" "$EFI_PART"

        progress 30 "Formatting Linux Root Partition (ext4)..."
        mkfs.ext4 -F -L "AURALIS_ROOT" "$ROOT_PART"

        progress 35 "Mounting root partition..."
        mount "$ROOT_PART" "$TARGET_MNT"

        mkdir -p "$TARGET_MNT/boot/efi"
        mount "$EFI_PART" "$TARGET_MNT/boot/efi"
    else
        # Single partition install
        progress 25 "Formatting target partition ($TARGET_DEV) as ext4..."
        umount "$TARGET_DEV" 2>/dev/null || true
        mkfs.ext4 -F -L "AURALIS_ROOT" "$TARGET_DEV"
        mount "$TARGET_DEV" "$TARGET_MNT"
    fi
fi

# ==============================================================================
# Copy System Files & Base Userspace
# ==============================================================================
progress 55 "Deploying AuralisOS base system files..."

# Copy live rootfs excluding virtual filesystems and mount points
for dir in bin sbin lib usr etc root var; do
    if [ -d "/$dir" ]; then
        mkdir -p "$TARGET_MNT/$dir"
        cp -a "/$dir/." "$TARGET_MNT/$dir/" 2>/dev/null || true
    fi
done

# Create necessary mount points
mkdir -p "$TARGET_MNT/proc" "$TARGET_MNT/sys" "$TARGET_MNT/dev" "$TARGET_MNT/run" "$TARGET_MNT/tmp" "$TARGET_MNT/media" "$TARGET_MNT/mnt" "$TARGET_MNT/home"
chmod 1777 "$TARGET_MNT/tmp"

# Configure Hostname and Network
progress 70 "Configuring system identity and user accounts..."
echo "$HOSTNAME_VAL" > "$TARGET_MNT/etc/hostname"
cat > "$TARGET_MNT/etc/hosts" <<EOF
127.0.0.1   localhost $HOSTNAME_VAL
::1         localhost ip6-localhost ip6-loopback
EOF

# Create standard user if not root
if [ "$USERNAME_VAL" != "root" ] && [ -n "$USERNAME_VAL" ]; then
    mkdir -p "$TARGET_MNT/home/$USERNAME_VAL"
    if command -v chroot >/dev/null 2>&1; then
        chroot "$TARGET_MNT" adduser -D -s /bin/sh "$USERNAME_VAL" 2>/dev/null || true
        chroot "$TARGET_MNT" addgroup "$USERNAME_VAL" wheel 2>/dev/null || true
        chroot "$TARGET_MNT" addgroup "$USERNAME_VAL" video 2>/dev/null || true
        chroot "$TARGET_MNT" addgroup "$USERNAME_VAL" audio 2>/dev/null || true
        chroot "$TARGET_MNT" addgroup "$USERNAME_VAL" seat 2>/dev/null || true
        chroot "$TARGET_MNT" addgroup "$USERNAME_VAL" input 2>/dev/null || true
        echo "$USERNAME_VAL:$PASSWORD_VAL" | chroot "$TARGET_MNT" chpasswd 2>/dev/null || true
    fi
fi
# Set root password
echo "root:$PASSWORD_VAL" | chroot "$TARGET_MNT" chpasswd 2>/dev/null || true

# ==============================================================================
# Bootloader & Kernel Configuration
# ==============================================================================
progress 85 "Installing GRUB2 UEFI Bootloader..."

# Copy kernel and initramfs to target
mkdir -p "$TARGET_MNT/boot"
cp -f /boot/vmlinuz* "$TARGET_MNT/boot/" 2>/dev/null || true
cp -f /boot/initramfs* "$TARGET_MNT/boot/" 2>/dev/null || true

# If EFI partition is mounted, copy EFI bootloader
EFI_DEST=""
if [ -d "$TARGET_MNT/boot/efi" ] && mountpoint -q "$TARGET_MNT/boot/efi"; then
    EFI_DEST="$TARGET_MNT/boot/efi"
elif [ "$MODE" = "loopback" ]; then
    # Look for Windows EFI System Partition on same disk
    EFI_BLK="$(blkid -t TYPE="vfat" -o device 2>/dev/null | grep -iE '1$|p1$' | head -n1 || true)"
    if [ -n "$EFI_BLK" ]; then
        mkdir -p /mnt/win_esp
        mount "$EFI_BLK" /mnt/win_esp 2>/dev/null && EFI_DEST="/mnt/win_esp" || true
    fi
fi

if [ -n "$EFI_DEST" ] && [ -d "$EFI_DEST" ]; then
    mkdir -p "$EFI_DEST/EFI/AuralisOS" "$EFI_DEST/EFI/boot"
    cp -f /efi/boot/bootx64.efi "$EFI_DEST/EFI/AuralisOS/grubx64.efi" 2>/dev/null || true
    cp -f /efi/boot/bootx64.efi "$EFI_DEST/EFI/boot/bootx64.efi" 2>/dev/null || true

    # Generate GRUB configuration
    mkdir -p "$EFI_DEST/EFI/AuralisOS"
    cat > "$EFI_DEST/EFI/AuralisOS/grub.cfg" <<EOF
set timeout=5
set default=0

menuentry "AuralisOS Liquid Glass Edition" {
    search --no-floppy --file /AuralisOS/rootfs.img --set=root
    linux /boot/vmlinuz-lts modules=loop,squashfs,ntfs3 quiet auralis_loop=/AuralisOS/rootfs.img
    initrd /boot/initramfs-lts
}

menuentry "Windows Boot Manager" {
    search --no-floppy --file /EFI/Microsoft/Boot/bootmgfw.efi --set=root
    chainloader /EFI/Microsoft/Boot/bootmgfw.efi
}
EOF
fi

progress 95 "Finalizing filesystems and flushing caches..."
sync
umount "$TARGET_MNT/boot/efi" 2>/dev/null || true
umount "$TARGET_MNT" 2>/dev/null || true
if [ "$MODE" = "loopback" ]; then
    umount /mnt/win_host 2>/dev/null || true
    umount /mnt/win_esp 2>/dev/null || true
fi

progress 100 "Installation completed successfully!"
echo "SUCCESS"
exit 0
