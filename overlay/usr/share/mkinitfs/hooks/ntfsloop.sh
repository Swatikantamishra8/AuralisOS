#!/bin/sh
# ==============================================================================
# AuralisOS Initramfs Hook: ntfsloop.sh
# Phase 5: NTFS Loopback Root Mount Engine
# Boot parameter: auralis_loop=/AuralisOS/rootfs.img (or auto-detected on Windows C:)
# ==============================================================================

set -e

# Target loopback image path on Windows partition
LOOP_REL_PATH="${KOPT_auralis_loop:-/AuralisOS/rootfs.img}"
NTFS_MNT="/media/windows_ntfs"
FOUND_IMAGE=""

# Ensure mount point exists
mkdir -p "$NTFS_MNT"
mkdir -p /sysroot

echo "[AURALIS-INITRAMFS] Probing partitions for Windows NTFS host ($LOOP_REL_PATH)..."

# Load necessary modules if not built-in
modprobe ntfs3 2>/dev/null || modprobe ntfs 2>/dev/null || true
modprobe loop 2>/dev/null || true

# Wait for block devices to settle
sleep 1

# Iterate all block devices looking for NTFS partition hosting the loop file
for blk in $(blkid -t TYPE="ntfs" -o device 2>/dev/null || ls /dev/sd* /dev/nvme*n*p* /dev/vd* 2>/dev/null); do
    [ -b "$blk" ] || continue

    # Try mounting as ntfs3 (Linux 5.15+ high performance read-write NTFS driver)
    if mount -t ntfs3 -o rw,noatime,force "$blk" "$NTFS_MNT" 2>/dev/null || \
       mount -t ntfs -o ro "$blk" "$NTFS_MNT" 2>/dev/null; then
        
        # Check case-insensitive variations: \AuralisOS\rootfs.img, /AuralisOS/rootfs.img, /auralis/rootfs.img
        for candidate in \
            "$NTFS_MNT$LOOP_REL_PATH" \
            "$NTFS_MNT/AuralisOS/rootfs.img" \
            "$NTFS_MNT/auralis/rootfs.img" \
            "$NTFS_MNT/Auralis/rootfs.img"; do
            if [ -f "$candidate" ]; then
                FOUND_IMAGE="$candidate"
                FOUND_DEV="$blk"
                break 2
            fi
        done
        umount "$NTFS_MNT" 2>/dev/null || true
    fi
done

if [ -n "$FOUND_IMAGE" ]; then
    echo "[AURALIS-INITRAMFS] Found Windows partition at $FOUND_DEV hosting $FOUND_IMAGE"
    echo "[AURALIS-INITRAMFS] Setting up loop device /dev/loop0..."

    # Keep NTFS partition mounted
    mkdir -p /media/host_windows
    mount -t ntfs3 -o rw,noatime,force "$FOUND_DEV" /media/host_windows 2>/dev/null || \
    mount -t ntfs -o rw "$FOUND_DEV" /media/host_windows

    # Setup loop device
    losetup -P /dev/loop0 "/media/host_windows${LOOP_REL_PATH}" 2>/dev/null || \
    losetup /dev/loop0 "/media/host_windows/AuralisOS/rootfs.img"

    # Mount loopback image as target root filesystem
    echo "[AURALIS-INITRAMFS] Mounting loopback rootfs image to /sysroot..."
    mount -o rw,noatime /dev/loop0 /sysroot 2>/dev/null || \
    mount -t ext4 -o rw /dev/loop0 /sysroot 2>/dev/null || \
    mount -t squashfs -o ro /dev/loop0 /sysroot

    # Preserve host windows mount inside new root for seamless file access
    mkdir -p /sysroot/media/windows_c
    mount --bind /media/host_windows /sysroot/media/windows_c 2>/dev/null || true

    echo "[AURALIS-INITRAMFS] Loopback root ready. Continuing boot sequence."
else
    echo "[AURALIS-INITRAMFS] Warning: No Windows NTFS loopback image found. Falling back to standard boot."
fi
