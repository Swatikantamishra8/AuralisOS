#!/bin/bash
# ==============================================================================
# AuralisOS Debian ISO Generation Engine: scripts/build-iso-debian.sh
# Target: Debian 13 (Trixie) + OpenRC + seatd + Wayfire (<130MB Idle RAM)
# Toolchain: Debian live-build (Hermetic Docker or Native Debian)
# ==============================================================================

set -euo pipefail

# ANSI color formatting
BOLD="\033[1m"
GREEN="\033[32m"
CYAN="\033[36m"
YELLOW="\033[33m"
RED="\033[31m"
RESET="\033[0m"

OUT_DIR="${1:-/workspace/dist}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEBIAN_BUILD_DIR="${WORKSPACE_ROOT}/build/debian"
BUILD_WORK_DIR="/tmp/auralis-debian-work"

echo -e "${CYAN}================================================================${RESET}"
echo -e "${BOLD} AuralisOS Debian ISO Generation Engine${RESET}"
echo -e " Target Base  : ${GREEN}Debian 13 (Trixie) + OpenRC + glibc${RESET}"
echo -e " Workspace    : ${WORKSPACE_ROOT}"
echo -e " Build Source : ${DEBIAN_BUILD_DIR}"
echo -e " Output Dir   : ${OUT_DIR}"
echo -e " Work Dir     : ${BUILD_WORK_DIR}"
echo -e "${CYAN}================================================================${RESET}"

mkdir -p "$OUT_DIR"

# ------------------------------------------------------------------------------
# 1. Dependency Verification
# ------------------------------------------------------------------------------
echo -e "\n[*] Checking required live-build tools..."
REQUIRED_TOOLS=("lb" "debootstrap" "xorriso" "mcopy")
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo -e "${RED}[!] Missing required tool: ${tool}${RESET}"
        echo -e "    Please ensure you are running inside the Debian builder container."
        exit 1
    fi
done
echo -e "${GREEN}[+] All prerequisite tools verified.${RESET}"

# ------------------------------------------------------------------------------
# 2. Compile Native Desktop Applications
# ------------------------------------------------------------------------------
echo -e "\n[*] Compiling native C & Go desktop applications for Debian glibc..."

APPS=("auralis-shell" "auralis-dashboard" "auralis-welcome" "auralis-taskmgr" "auralis-store" "auralis-installer")
for app in "${APPS[@]}"; do
    if [ -d "${WORKSPACE_ROOT}/src/${app}" ]; then
        echo -e "  -> Building ${app}..."
        make -C "${WORKSPACE_ROOT}/src/${app}"
        mkdir -p "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin"
        cp -f "${WORKSPACE_ROOT}/src/${app}/${app}" "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin/"
        chmod +x "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin/${app}"
        
        # Copy app style assets if present
        if [ -f "${WORKSPACE_ROOT}/src/${app}/style.css" ]; then
            mkdir -p "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/share/${app}"
            cp -f "${WORKSPACE_ROOT}/src/${app}/style.css" "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/share/${app}/"
        fi
        if [ -f "${WORKSPACE_ROOT}/src/${app}/backend.sh" ]; then
            cp -f "${WORKSPACE_ROOT}/src/${app}/backend.sh" "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin/auralis-installer-backend"
            chmod +x "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin/auralis-installer-backend"
        fi
    fi
done

# Compile Go router
if [ -d "${WORKSPACE_ROOT}/src/auralis-router" ]; then
    echo -e "  -> Building auralis-router (static)..."
    make -C "${WORKSPACE_ROOT}/src/auralis-router" static
    cp -f "${WORKSPACE_ROOT}/src/auralis-router/auralis-router" "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin/"
    chmod +x "${DEBIAN_BUILD_DIR}/config/includes.chroot/usr/local/bin/auralis-router"
fi

echo -e "${GREEN}[+] All native applications compiled and staged in includes.chroot.${RESET}"

# ------------------------------------------------------------------------------
# 3. Prepare Work Directory
# ------------------------------------------------------------------------------
echo -e "\n[*] Staging live-build environment in ${BUILD_WORK_DIR}..."
rm -rf "$BUILD_WORK_DIR"
mkdir -p "$BUILD_WORK_DIR"
cp -r "${DEBIAN_BUILD_DIR}/"* "$BUILD_WORK_DIR/"
cd "$BUILD_WORK_DIR"

# Ensure script permissions
chmod +x auto/* 2>/dev/null || true
chmod +x config/hooks/live/* 2>/dev/null || true

# ------------------------------------------------------------------------------
# 4. Execute live-build Configuration & Generation
# ------------------------------------------------------------------------------
echo -e "\n[*] Running lb config..."
lb config

echo -e "\n[*] Running lb build (generating squashfs and hybrid ISO)..."
lb build

# ------------------------------------------------------------------------------
# 5. Extract ISO and Generate Checksums
# ------------------------------------------------------------------------------
echo -e "\n[*] Finalizing AuralisOS Debian ISO image..."
GENERATED_ISO="$(find . -maxdepth 1 -name "*.iso" 2>/dev/null | head -n1 || true)"

if [ -z "$GENERATED_ISO" ] || [ ! -f "$GENERATED_ISO" ]; then
    echo -e "${RED}[!] Error: No ISO file was generated by live-build!${RESET}"
    exit 1
fi

TARGET_ISO="${OUT_DIR}/auralis-os-debian-x86_64.iso"
cp -f "$GENERATED_ISO" "$TARGET_ISO"

cd "$OUT_DIR"
sha256sum "$(basename "$TARGET_ISO")" > "$(basename "$TARGET_ISO").sha256"
chmod -R a+rw "$OUT_DIR" 2>/dev/null || true

ISO_SIZE_BYTES="$(stat -c%s "$TARGET_ISO" 2>/dev/null || stat -f%z "$TARGET_ISO" 2>/dev/null || wc -c < "$TARGET_ISO")"
ISO_SIZE_MB=$(( ISO_SIZE_BYTES / 1024 / 1024 ))
SHA256_HASH="$(cut -d' ' -f1 < "${TARGET_ISO}.sha256")"

# Cleanup work dir
rm -rf "$BUILD_WORK_DIR"

echo -e "\n${CYAN}================================================================${RESET}"
echo -e "${GREEN}${BOLD}[+] AURALISOS DEBIAN TRIXIE ISO BUILD COMPLETE!${RESET}"
echo -e "${CYAN}================================================================${RESET}"
echo -e " File       : ${BOLD}${TARGET_ISO}${RESET}"
echo -e " Size       : ${BOLD}${ISO_SIZE_MB} MB${RESET} (${ISO_SIZE_BYTES} bytes)"
echo -e " SHA256     : ${SHA256_HASH}"
echo -e " Platform   : Debian 13 (Trixie) Minimal + OpenRC + glibc"
echo -e " Desktop    : Wayfire + seatd + C/GTK4 Liquid Glass Suite"
echo -e " Target RAM : 110–145 MB Idle Budget (Floor: 95 MB)"
echo -e "${CYAN}================================================================${RESET}"
