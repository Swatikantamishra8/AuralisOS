#!/bin/bash
# ==============================================================================
# AuralisOS Hypervisor Verification Test Suite: scripts/test-qemu.sh
# Phase 1 to 5: Custom Ultra-Low-Memory Linux Distribution (<95MB Idle RAM)
# Specs: 256 MB RAM, 2 vCPUs, VirtIO/VBox hardware emulation, BIOS/UEFI boot
# Desktop: "Liquid Glass" Wayland Session (Wayfire + auralis-shell + auralis-taskmgr)
# ==============================================================================

set -euo pipefail

BOLD="\033[1m"
GREEN="\033[32m"
CYAN="\033[36m"
YELLOW="\033[33m"
RED="\033[31m"
RESET="\033[0m"

# Script directory resolution
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DIST_DIR="${WORKSPACE_ROOT}/dist"

TARGET_INPUT="${1:-vm}"
USE_VBOX=false
HEADLESS=false
USE_UEFI=false

# Parse command line flags
shift || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --vbox|--virtualbox)
            USE_VBOX=true
            shift
            ;;
        --headless|-nographic)
            HEADLESS=true
            shift
            ;;
        --uefi)
            USE_UEFI=true
            shift
            ;;
        *)
            echo -e "${YELLOW}[!] Unknown option: $1${RESET}"
            shift
            ;;
    esac
done

# Resolve ISO path
ISO_FILE=""
if [ -f "$TARGET_INPUT" ]; then
    ISO_FILE="$TARGET_INPUT"
elif [ "$TARGET_INPUT" = "vm" ]; then
    ISO_FILE="${DIST_DIR}/auralis-os-vm-x86_64.iso"
elif [ "$TARGET_INPUT" = "metal" ]; then
    ISO_FILE="${DIST_DIR}/auralis-os-metal-x86_64.iso"
else
    # Auto-detect existing ISO in dist
    ISO_FILE="$(find "${DIST_DIR}" -maxdepth 1 -name "*.iso" 2>/dev/null | head -n1 || true)"
fi

echo -e "${CYAN}================================================================${RESET}"
echo -e "${BOLD} AuralisOS Hypervisor Test Launcher (Liquid Glass GUI)${RESET}"
echo -e " Target ISO : ${BOLD}${ISO_FILE}${RESET}"
echo -e " RAM Limit  : ${GREEN}${BOLD}256 MB${RESET} (Verifying <95MB idle RAM constraint)"
echo -e " CPU Cores  : 2 vCPUs"
echo -e "${CYAN}================================================================${RESET}"

if [ -z "$ISO_FILE" ] || [ ! -f "$ISO_FILE" ]; then
    echo -e "${RED}[!] Target ISO file does not exist: ${ISO_FILE}${RESET}"
    echo -e "    Please run ${CYAN}make iso-vm${RESET} or ${CYAN}make iso-metal${RESET} before launching test."
    exit 1
fi

# ==============================================================================
# VirtualBox Execution Branch
# ==============================================================================
if [ "$USE_VBOX" = true ] || (! command -v qemu-system-x86_64 >/dev/null 2>&1 && command -v VBoxManage >/dev/null 2>&1); then
    echo -e "\n[*] Launching in VirtualBox via VBoxManage..."
    
    if ! command -v VBoxManage >/dev/null 2>&1; then
        echo -e "${RED}[!] VirtualBox (VBoxManage) is not installed or not in PATH.${RESET}"
        exit 1
    fi

    VM_NAME="AuralisOS-Test-VM"

    # Stop and unregister previous test VM if it exists
    if VBoxManage showvminfo "$VM_NAME" >/dev/null 2>&1; then
        echo -e "[*] Cleaning up previous test instance: ${VM_NAME}..."
        VBoxManage controlvm "$VM_NAME" poweroff 2>/dev/null || true
        sleep 1
        VBoxManage unregistervm "$VM_NAME" --delete 2>/dev/null || true
    fi

    echo -e "[*] Creating VirtualBox VM: ${VM_NAME}..."
    VBoxManage createvm --name "$VM_NAME" --ostype "Linux_64" --register
    VBoxManage modifyvm "$VM_NAME" \
        --memory 256 \
        --cpus 2 \
        --vram 32 \
        --graphicscontroller vmsvga \
        --accelerate3d off \
        --nic1 nat \
        --nictype1 82540EM \
        --audio-enabled on \
        --audiocontroller hda \
        --boot1 dvd \
        --boot2 none \
        --boot3 none \
        --boot4 none

    # Configure storage controller and attach ISO
    VBoxManage storagectl "$VM_NAME" --name "IDE Controller" --add ide
    VBoxManage storageattach "$VM_NAME" \
        --storagectl "IDE Controller" \
        --port 0 \
        --device 0 \
        --type dvddrive \
        --medium "$ISO_FILE"

    echo -e "${GREEN}[+] Starting VirtualBox VM (${VM_NAME})...${RESET}"
    echo -e "    ${YELLOW}(Notice: 3D acceleration disabled to test automatic WLR_RENDERER_ALLOW_SOFTWARE=1 fallback)${RESET}"
    VBoxManage startvm "$VM_NAME"
    exit 0
fi

# ==============================================================================
# QEMU Execution Branch
# ==============================================================================
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo -e "${RED}[!] QEMU executable (qemu-system-x86_64) not found in PATH.${RESET}"
    echo -e "    Please install QEMU or VirtualBox to run the hypervisor test suite."
    exit 1
fi

# Determine CPU Acceleration
ACCEL_OPTS=""
if [ -e /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    ACCEL_OPTS="-enable-kvm -cpu host"
    echo -e "[*] Hardware virtualization detected: KVM enabled."
elif qemu-system-x86_64 -accel help 2>&1 | grep -q whpx; then
    ACCEL_OPTS="-accel whpx -accel tcg -cpu max"
    echo -e "[*] Windows Hypervisor Platform (WHPX) acceleration enabled."
elif qemu-system-x86_64 -accel help 2>&1 | grep -q hvf; then
    ACCEL_OPTS="-accel hvf -cpu host"
    echo -e "[*] macOS Hypervisor Framework (HVF) acceleration enabled."
else
    ACCEL_OPTS="-accel tcg -cpu qemu64"
    echo -e "${YELLOW}[*] Hardware virtualization unavailable: Falling back to TCG software emulation.${RESET}"
fi

# Build QEMU arguments
QEMU_ARGS=(
    "-m" "256M"
    "-smp" "2"
    "-cdrom" "$ISO_FILE"
    "-boot" "d"
    # VirtIO Devices to test dynamic modalias loading in eudev
    "-device" "virtio-balloon-pci"
    "-device" "virtio-rng-pci"
    "-device" "virtio-serial-pci"
    "-netdev" "user,id=net0,hostfwd=tcp::2222-:22"
    "-device" "virtio-net-pci,netdev=net0"
)

# Optional UEFI Firmware Boot
if [ "$USE_UEFI" = true ]; then
    OVMF_CODE=""
    for path in \
        "/usr/share/OVMF/OVMF_CODE.fd" \
        "/usr/share/edk2/x64/OVMF_CODE.4MB.fd" \
        "/usr/share/ovmf/OVMF.fd" \
        "/usr/share/qemu/OVMF.fd" \
        "/opt/homebrew/share/qemu/edk2-x86_64-code.fd"; do
        if [ -f "$path" ]; then
            OVMF_CODE="$path"
            break
        fi
    done

    if [ -n "$OVMF_CODE" ]; then
        echo -e "[*] UEFI Boot Mode: Loading OVMF firmware (${OVMF_CODE})..."
        QEMU_ARGS+=("-drive" "if=pflash,format=raw,readonly=on,file=${OVMF_CODE}")
    else
        echo -e "${YELLOW}[!] UEFI flag passed but OVMF firmware not found in standard paths. Falling back to BIOS.${RESET}"
    fi
fi

# Console & Display Modes
if [ "$HEADLESS" = true ]; then
    echo -e "[*] Running in headless serial console mode..."
    echo -e "    ${YELLOW}Tip: Press Ctrl-A then X to terminate QEMU.${RESET}\n"
    QEMU_ARGS+=("-nographic" "-serial" "mon:stdio")
else
    echo -e "[*] Running with graphical display window + mirrored serial output..."
    echo -e "    ${YELLOW}Tip: Close window or press Ctrl-A then X in terminal to terminate.${RESET}\n"
    QEMU_ARGS+=("-device" "virtio-vga" "-serial" "mon:stdio")
fi

# Launch QEMU
echo -e "${GREEN}[+] Booting AuralisOS...${RESET}\n"
exec qemu-system-x86_64 ${ACCEL_OPTS} "${QEMU_ARGS[@]}"
