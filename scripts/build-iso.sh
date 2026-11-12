#!/bin/bash
# ==============================================================================
# AuralisOS ISO Build Pipeline: scripts/build-iso.sh
# Phase 1 to 5: Custom Ultra-Low-Memory Linux Distribution (<95MB Idle RAM)
# Target: Hybrid BIOS/UEFI bootable ISO with "Liquid Glass" GUI & Store Hydrator
# ==============================================================================

set -euo pipefail

# ANSI color formatting
BOLD="\033[1m"
GREEN="\033[32m"
CYAN="\033[36m"
YELLOW="\033[33m"
RED="\033[31m"
RESET="\033[0m"

# ------------------------------------------------------------------------------
# 1. Parameter Resolution
# ------------------------------------------------------------------------------
FLAVOR="${1:-${AURALIS_FLAVOR:-metal}}"
OUT_DIR="${2:-/workspace/dist}"
WORKDIR="/tmp/auralis-build-work-${FLAVOR}"

# Resolve workspace root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo -e "${CYAN}================================================================${RESET}"
echo -e "${BOLD} AuralisOS ISO Generation Engine (Phases 1-5)${RESET}"
echo -e " Target Flavor : ${GREEN}${FLAVOR}${RESET} (vm=slim VirtualBox/QEMU, metal=full hardware)"
echo -e " Workspace     : ${WORKSPACE_ROOT}"
echo -e " Output Dir    : ${OUT_DIR}"
echo -e " Work Dir      : ${WORKDIR}"
echo -e "${CYAN}================================================================${RESET}"

# Ensure execution permissions on helper scripts
chmod +x "${WORKSPACE_ROOT}/genapkovl-auralis.sh" 2>/dev/null || true
if [ -f "${WORKSPACE_ROOT}/profiles/genapkovl-auralis.sh" ]; then
    chmod +x "${WORKSPACE_ROOT}/profiles/genapkovl-auralis.sh" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/overlay/usr/local/bin/auralis-session" ]; then
    chmod +x "${WORKSPACE_ROOT}/overlay/usr/local/bin/auralis-session" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/overlay/usr/bin/auralis-reaper" ]; then
    chmod +x "${WORKSPACE_ROOT}/overlay/usr/bin/auralis-reaper" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/overlay/etc/init.d/auralis-reaper" ]; then
    chmod +x "${WORKSPACE_ROOT}/overlay/etc/init.d/auralis-reaper" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/overlay/etc/init.d/auralis-hwdetect" ]; then
    chmod +x "${WORKSPACE_ROOT}/overlay/etc/init.d/auralis-hwdetect" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/overlay/usr/local/bin/auralis-hydrate" ]; then
    chmod +x "${WORKSPACE_ROOT}/overlay/usr/local/bin/auralis-hydrate" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/src/auralis-installer/backend.sh" ]; then
    chmod +x "${WORKSPACE_ROOT}/src/auralis-installer/backend.sh" 2>/dev/null || true
fi
if [ -f "${WORKSPACE_ROOT}/src/auralis-installer/auralis-installer" ]; then
    chmod +x "${WORKSPACE_ROOT}/src/auralis-installer/auralis-installer" 2>/dev/null || true
fi

# ------------------------------------------------------------------------------
# 2. Dependency Verification
# ------------------------------------------------------------------------------
echo -e "\n[*] Checking ISO build dependencies..."
REQUIRED_TOOLS=("apk" "fakeroot" "xorriso" "mtools" "syslinux" "grub-mkimage")
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo -e "${RED}[!] Missing required tool: ${tool}${RESET}"
        echo -e "    Please ensure you are running inside the Docker builder container or Alpine host."
        exit 1
    fi
done
echo -e "${GREEN}[+] All prerequisite tools verified.${RESET}"

# ------------------------------------------------------------------------------
# 3. Locate Alpine mkimage.sh Engine
# ------------------------------------------------------------------------------
MKIMAGE_BIN=""
for candidate in \
    "/opt/aports/scripts/mkimage.sh" \
    "${WORKSPACE_ROOT}/mkimage.sh" \
    "/usr/local/bin/mkimage.sh"; do
    if [ -x "$candidate" ]; then
        MKIMAGE_BIN="$candidate"
        break
    fi
done

if [ -z "$MKIMAGE_BIN" ]; then
    echo -e "${YELLOW}[*] mkimage.sh not found locally. Cloning Alpine aports repository...${RESET}"
    git clone --depth 1 https://gitlab.alpinelinux.org/alpine/aports.git /opt/aports
    MKIMAGE_BIN="/opt/aports/scripts/mkimage.sh"
    chmod +x "$MKIMAGE_BIN"
fi
echo -e "${GREEN}[+] Using mkimage engine: ${MKIMAGE_BIN}${RESET}"

# ------------------------------------------------------------------------------
# 4. Resolve Alpine Repository Version
# ------------------------------------------------------------------------------
if [ -f /etc/alpine-release ]; then
    ALPINE_VER="$(cat /etc/alpine-release | cut -d. -f1,2)"
    ALPINE_BRANCH="v${ALPINE_VER}"
else
    ALPINE_BRANCH="v3.20"
fi
echo -e "[*] Target Alpine Repository Branch: ${ALPINE_BRANCH}"

# ------------------------------------------------------------------------------
# 5. Compile Native Binaries (auralis-shell, auralis-taskmgr, auralis-store, auralis-router)
# ------------------------------------------------------------------------------
echo -e "\n[*] Compiling Native Applications..."
if command -v gcc >/dev/null 2>&1 && pkg-config --exists gtk4 2>/dev/null; then
    if [ -d "${WORKSPACE_ROOT}/src/auralis-shell" ]; then
        echo -e "    [*] Building auralis-shell..."
        make -C "${WORKSPACE_ROOT}/src/auralis-shell"
    fi
    if [ -d "${WORKSPACE_ROOT}/src/auralis-taskmgr" ]; then
        echo -e "    [*] Building auralis-taskmgr..."
        make -C "${WORKSPACE_ROOT}/src/auralis-taskmgr"
    fi
    if [ -d "${WORKSPACE_ROOT}/src/auralis-store" ]; then
        echo -e "    [*] Building auralis-store..."
        make -C "${WORKSPACE_ROOT}/src/auralis-store"
    fi
    if [ -d "${WORKSPACE_ROOT}/src/auralis-installer" ]; then
        echo -e "    [*] Building auralis-installer..."
        make -C "${WORKSPACE_ROOT}/src/auralis-installer"
    fi
    echo -e "${GREEN}[+] Native desktop applications built successfully.${RESET}"
else
    echo -e "${YELLOW}[!] Host/Container missing native GTK4 compiler. Overlay will compile on demand.${RESET}"
fi

if command -v go >/dev/null 2>&1; then
    if [ -d "${WORKSPACE_ROOT}/src/auralis-router" ] && [ ! -f "${WORKSPACE_ROOT}/src/auralis-router/auralis-router" ]; then
        echo -e "    [*] Building auralis-router static binary..."
        (cd "${WORKSPACE_ROOT}/src/auralis-router" && CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -trimpath -o auralis-router main.go)
        echo -e "${GREEN}[+] auralis-router binary compiled successfully.${RESET}"
    fi
fi

# Prepare output & working directories
mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR"/*.iso "$OUT_DIR"/*.sha256
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

# ------------------------------------------------------------------------------
# 6. Invoke mkimage with Custom AuralisOS Profile
# ------------------------------------------------------------------------------
echo -e "\n[*] Executing mkimage for flavor: ${BOLD}${FLAVOR}${RESET}..."

export AURALIS_FLAVOR="$FLAVOR"
export PATH="${WORKSPACE_ROOT}:${PATH}"

# Ensure signing keys are exported if available
if [ -z "${PACKAGER_PRIVKEY:-}" ]; then
    PRIVKEY="$(ls /root/.abuild/*.rsa 2>/dev/null | head -n1 || true)"
    if [ -n "$PRIVKEY" ]; then
        export PACKAGER_PRIVKEY="$PRIVKEY"
    fi
fi

case "$FLAVOR" in
    metal|baremetal|hardware)
        PROFILE="auralis_metal"
        ;;
    vm|virtualbox|qemu)
        PROFILE="auralis_vm"
        ;;
    *)
        PROFILE="auralis"
        ;;
esac

cd "$WORKSPACE_ROOT"

"$MKIMAGE_BIN" \
    --tag "5.0.0" \
    --outdir "$OUT_DIR" \
    --workdir "$WORKDIR" \
    --repository "https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}/main" \
    --repository "https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}/community" \
    --repository "https://dl-cdn.alpinelinux.org/alpine/edge/main" \
    --repository "https://dl-cdn.alpinelinux.org/alpine/edge/community" \
    --repository "https://dl-cdn.alpinelinux.org/alpine/edge/testing" \
    --profile "$PROFILE" \
    --arch "x86_64"

# ------------------------------------------------------------------------------
# 7. Post-Build Normalization & Integrity Checksums
# ------------------------------------------------------------------------------
echo -e "\n[*] Normalizing output ISO images..."

TARGET_ISO="${OUT_DIR}/auralis-os-${FLAVOR}-x86_64.iso"

# Look for generated ISO
GENERATED_ISO="$(find "$OUT_DIR" -maxdepth 1 -name "*.iso" -newer "$WORKDIR" 2>/dev/null | head -n1 || true)"
if [ -z "$GENERATED_ISO" ]; then
    GENERATED_ISO="$(find "$OUT_DIR" -maxdepth 1 -name "*auralis*.iso" 2>/dev/null | head -n1 || true)"
fi

if [ -n "$GENERATED_ISO" ] && [ "$GENERATED_ISO" != "$TARGET_ISO" ]; then
    cp -f "$GENERATED_ISO" "$TARGET_ISO"
fi

if [ ! -f "$TARGET_ISO" ]; then
    echo -e "${RED}[!] ISO build failed: target image not found at ${TARGET_ISO}${RESET}"
    exit 1
fi

# Generate SHA256 Checksum
(cd "$OUT_DIR" && sha256sum "$(basename "$TARGET_ISO")" > "$(basename "$TARGET_ISO").sha256")

ISO_SIZE_BYTES="$(stat -c%s "$TARGET_ISO" 2>/dev/null || stat -f%z "$TARGET_ISO" 2>/dev/null || wc -c < "$TARGET_ISO")"
ISO_SIZE_MB=$(( ISO_SIZE_BYTES / 1024 / 1024 ))
SHA256_HASH="$(cut -d' ' -f1 < "${TARGET_ISO}.sha256")"

# Clean temporary work directory
rm -rf "$WORKDIR"

echo -e "\n${CYAN}================================================================${RESET}"
echo -e "${GREEN}${BOLD}[+] AURALISOS PHASE 5 ISO BUILD SUCCESSFUL!${RESET}"
echo -e "${CYAN}================================================================${RESET}"
echo -e " File       : ${BOLD}${TARGET_ISO}${RESET}"
echo -e " Size       : ${BOLD}${ISO_SIZE_MB} MB${RESET} (${ISO_SIZE_BYTES} bytes)"
echo -e " SHA256     : ${SHA256_HASH}"
echo -e " GUI        : Wayfire 0.8+ Dual-Kawase Blur + auralis-shell + auralis-taskmgr"
echo -e " Hydration  : auralis-store + auralis-hydrate (On-demand modular SquashFS runtimes)"
echo -e " Router     : auralis-router v2.0 (On-the-spot subsystem activation prompts)"
echo -e " Reaper     : auralis-reaper (45s idle memory reclaim daemon)"
echo -e " Hardware   : auralis-hwdetect (GPU & Wi-Fi auto-tuner)"
echo -e " Loopback   : ntfsloop boot support (C:\\AuralisOS\\rootfs.img)"
echo -e " Memory     : Target < 95 MB RAM Idle Budget"
echo -e " Boot Mode  : Hybrid BIOS (Syslinux/ISOLINUX) + UEFI (GRUB2 EFI)"
echo -e " Test with  : ${CYAN}bash scripts/test-qemu.sh ${FLAVOR}${RESET}"
echo -e "${CYAN}================================================================${RESET}"
