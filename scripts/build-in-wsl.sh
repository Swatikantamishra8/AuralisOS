#!/bin/sh
# ==============================================================================
# AuralisOS WSL Host Execution Wrapper: scripts/build-in-wsl.sh
# Mounts the persistent Alpine Linux builder image (.build.img) and executes
# scripts/build-iso.sh inside the builder chroot.
# ==============================================================================

set -e

FLAVOR="${1:-metal}"
HOST_WORKSPACE="/mnt/host/c/Apps/BinduOS"
BUILDER_ROOT="/mnt/alpine_builder"
BUILDER_IMG="${HOST_WORKSPACE}/.build.img"

echo "[*] Initializing Alpine ISO builder environment for flavor: ${FLAVOR}..."

# 1. Mount persistent ext4 builder image
mkdir -p "$BUILDER_ROOT"
if ! mountpoint -q "$BUILDER_ROOT"; then
    echo "[*] Mounting ${BUILDER_IMG} to ${BUILDER_ROOT}..."
    mount -o loop "$BUILDER_IMG" "$BUILDER_ROOT"
fi

# 2. Prepare virtual filesystems and bind mounts
mkdir -p "$BUILDER_ROOT/proc" "$BUILDER_ROOT/sys" "$BUILDER_ROOT/dev" "$BUILDER_ROOT/dev/pts" "$BUILDER_ROOT/dev/shm" "$BUILDER_ROOT/workspace"
mountpoint -q "$BUILDER_ROOT/proc" || mount --bind /proc "$BUILDER_ROOT/proc"
mountpoint -q "$BUILDER_ROOT/sys"  || mount --bind /sys "$BUILDER_ROOT/sys"
mountpoint -q "$BUILDER_ROOT/dev"  || mount --bind /dev "$BUILDER_ROOT/dev"
mountpoint -q "$BUILDER_ROOT/dev/pts" || mount --bind /dev/pts "$BUILDER_ROOT/dev/pts" 2>/dev/null || true
mountpoint -q "$BUILDER_ROOT/dev/shm" || mount --bind /dev/shm "$BUILDER_ROOT/dev/shm" 2>/dev/null || true
mountpoint -q "$BUILDER_ROOT/workspace" || mount --bind "$HOST_WORKSPACE" "$BUILDER_ROOT/workspace"

# 3. Network DNS resolution
cp -f /etc/resolv.conf "$BUILDER_ROOT/etc/resolv.conf"

# 4. Synchronize mkinitfs features and hooks into builder
mkdir -p "$BUILDER_ROOT/etc/mkinitfs/features.d" "$BUILDER_ROOT/usr/share/mkinitfs/hooks"
cp -f "$HOST_WORKSPACE"/overlay/etc/mkinitfs/features.d/* "$BUILDER_ROOT/etc/mkinitfs/features.d/"
cp -f "$HOST_WORKSPACE"/overlay/usr/share/mkinitfs/hooks/* "$BUILDER_ROOT/usr/share/mkinitfs/hooks/"

# 5. Synchronize mkimg profiles and genapkovl generator
mkdir -p "$BUILDER_ROOT/opt/aports/scripts" "$BUILDER_ROOT/root/.mkimage"
cp -f "$HOST_WORKSPACE"/mkimg.auralis.sh "$BUILDER_ROOT/opt/aports/scripts/mkimg.auralis.sh"
cp -f "$HOST_WORKSPACE"/genapkovl-auralis.sh "$BUILDER_ROOT/opt/aports/scripts/genapkovl-auralis.sh"
chmod +x "$BUILDER_ROOT/opt/aports/scripts/mkimg.auralis.sh" "$BUILDER_ROOT/opt/aports/scripts/genapkovl-auralis.sh"
cp -f "$HOST_WORKSPACE"/mkimg.auralis.sh "$BUILDER_ROOT/root/.mkimage/mkimg.auralis.sh"
cp -f "$HOST_WORKSPACE"/genapkovl-auralis.sh "$BUILDER_ROOT/root/.mkimage/genapkovl-auralis.sh"

# 6. Ensure abuild keys exist and are trusted
if [ ! -d "$BUILDER_ROOT/root/.abuild" ] || ! ls "$BUILDER_ROOT/root/.abuild"/*.rsa >/dev/null 2>&1; then
    echo "[*] Generating abuild package signing keys..."
    chroot "$BUILDER_ROOT" abuild-keygen -a -n
fi
cp -f "$BUILDER_ROOT"/root/.abuild/*.rsa.pub "$BUILDER_ROOT/etc/apk/keys/" 2>/dev/null || true

# 7. Execute the ISO build script inside the chroot
echo "[*] Executing build-iso.sh inside Alpine builder..."
chroot "$BUILDER_ROOT" /bin/bash -c "
    export PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
    export HOME=/root
    bash /workspace/scripts/build-iso.sh \"$FLAVOR\" /workspace/dist
"

echo "[+] Build script execution completed."
