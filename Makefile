# ==============================================================================
# AuralisOS Root Makefile
# Phase 1 to 5: Build Pipeline, Binary Suite & Windows Loopback Installer
# Desktop: "Liquid Glass" Native Wayland (Wayfire + auralis-shell + auralis-taskmgr)
# Multi-World: auralis-router & auralis-reaper (Windows, Android, Linux, macOS)
# Hydration: auralis-store & auralis-hydrate (On-Demand SquashFS Extensions)
# Hardware: auralis-hwdetect (Bare-Metal Auto-Tuner & Driver Optimization)
# Co-Existence: Auralis-Installer.exe (24 GB NTFS Loopback Boot from Windows C:)
# Memory Constraint: < 95 MB Idle RAM Footprint
# ==============================================================================

SHELL := /bin/bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c

IMAGE_NAME ?= auralis-builder:latest
DOCKER     ?= docker

ROOT_DIR   := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
DIST_DIR   := $(ROOT_DIR)dist
ISO_VM     := $(DIST_DIR)/auralis-os-vm-x86_64.iso
ISO_METAL  := $(DIST_DIR)/auralis-os-metal-x86_64.iso

# Docker execution helper
# --privileged is required for fakeroot, mknod, and loop/squashfs manipulations
DOCKER_CMD := $(DOCKER) run --rm --privileged \
	-v "$(ROOT_DIR):/workspace" \
	-w /workspace

.PHONY: all help builder desktop-bins router-bin installer-bin iso-vm iso-metal test-vm test-metal clean shell

all: help

help:
	@echo "=================================================================="
	@echo " AuralisOS Build Pipeline (Principal OS Architecture)"
	@echo " Ultra-Low-Memory Linux (<95MB Idle RAM, OpenRC, musl, x86_64)"
	@echo " Desktop: 'Liquid Glass' Wayland (Wayfire + C/GTK4 Native Shell)"
	@echo " Multi-World: auralis-router & auralis-reaper (PE, APK, deb/rpm, Mach-O/DMG)"
	@echo " Hydration: auralis-store & auralis-hydrate (On-Demand SquashFS Extensions)"
	@echo " Bare-Metal: auralis-hwdetect (GPU & Wi-Fi) + ntfsloop initramfs hook"
	@echo " Windows Co-Existence: Auralis-Installer.exe (24GB C:\AuralisOS\rootfs.img)"
	@echo "=================================================================="
	@echo " Primary Targets:"
	@echo "   make builder       Build the hermetic Alpine ISO builder container"
	@echo "   make router-bin    Compile static Go auralis-router binary"
	@echo "   make desktop-bins  Compile native C auralis-shell, taskmgr, & store"
	@echo "   make installer-bin Compile Windows Loopback Installer (.exe)"
	@echo "   make iso-vm        Build slim ISO for VirtualBox/QEMU (<95MB RAM)"
	@echo "   make iso-metal     Build bare-metal ISO (Full Firmware + SOF Audio)"
	@echo ""
	@echo " Verification & Testing:"
	@echo "   make test-vm       Launch the slim VM ISO in QEMU with 256MB RAM"
	@echo "   make test-metal    Launch the Metal ISO in QEMU with 256MB RAM"
	@echo ""
	@echo " Maintenance:"
	@echo "   make clean         Remove built ISO files and build caches"
	@echo "   make shell         Open interactive shell inside builder container"
	@echo "=================================================================="

# ------------------------------------------------------------------------------
# 1. Builder Container Image Target
# ------------------------------------------------------------------------------
builder:
	@echo "[*] Building AuralisOS hermetic builder container image: $(IMAGE_NAME)..."
	$(DOCKER) build -t $(IMAGE_NAME) -f Dockerfile.builder .
	@echo "[+] Builder container image ready: $(IMAGE_NAME)"

# ------------------------------------------------------------------------------
# 2. Compile Phase 3 Binary Router (Go Static)
# ------------------------------------------------------------------------------
router-bin:
	@echo "[*] Building auralis-router zero-dependency static binary..."
	@if command -v go >/dev/null 2>&1; then \
		CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -trimpath -o src/auralis-router/auralis-router src/auralis-router/main.go; \
	else \
		$(DOCKER_CMD) $(IMAGE_NAME) /bin/bash -c "cd src/auralis-router && make static"; \
	fi
	@echo "[+] Static auralis-router binary ready: src/auralis-router/auralis-router"

# ------------------------------------------------------------------------------
# 3. Compile Windows Co-Existence Loopback Installer (Auralis-Installer.exe)
# ------------------------------------------------------------------------------
installer-bin:
	@echo "[*] Building Windows Loopback Installer (Auralis-Installer.exe)..."
	@if command -v go >/dev/null 2>&1; then \
		CGO_ENABLED=0 GOOS=windows GOARCH=amd64 go build -ldflags="-s -w" -trimpath -o installer/windows/Auralis-Installer.exe installer/windows/main.go; \
	fi
	@echo "[+] Windows installer ready: installer/windows/Auralis-Installer.exe"

# ------------------------------------------------------------------------------
# 4. Compile All Native Desktop Applications (GTK4 C & Go)
# ------------------------------------------------------------------------------
desktop-bins: router-bin installer-bin
	@echo "[*] Compiling Phase 2 & 4 native C applications (shell, taskmgr, store)..."
	@if [ -f "/.dockerenv" ] || [ -f "/etc/alpine-release" ]; then \
		make -C src/auralis-shell; \
		make -C src/auralis-taskmgr; \
		make -C src/auralis-store; \
	else \
		$(DOCKER_CMD) $(IMAGE_NAME) /bin/bash -c "make -C src/auralis-shell && make -C src/auralis-taskmgr && make -C src/auralis-store"; \
	fi
	@echo "[+] All native applications built successfully."

# ------------------------------------------------------------------------------
# 5. Slim ISO Target for VirtualBox / QEMU
# ------------------------------------------------------------------------------
iso-vm: desktop-bins
	@echo "[*] Triggering AuralisOS Slim VM ISO build (Phases 1-5)..."
	@mkdir -p "$(DIST_DIR)"
	@if [ -f "/.dockerenv" ] || [ -f "/etc/alpine-release" ]; then \
		echo "[*] Executing directly inside Alpine environment..."; \
		AURALIS_FLAVOR=vm /bin/bash scripts/build-iso.sh vm "$(DIST_DIR)"; \
	else \
		echo "[*] Executing via Docker builder container..."; \
		$(DOCKER_CMD) -e AURALIS_FLAVOR=vm $(IMAGE_NAME) /bin/bash scripts/build-iso.sh vm /workspace/dist; \
	fi
	@echo "[+] Slim VM ISO built successfully at: $(ISO_VM)"

# ------------------------------------------------------------------------------
# 6. Bare-Metal ISO Target (Full Linux Firmware + Sound Open Firmware)
# ------------------------------------------------------------------------------
iso-metal: desktop-bins
	@echo "[*] Triggering AuralisOS Bare-Metal ISO build (Phases 1-5)..."
	@mkdir -p "$(DIST_DIR)"
	@if [ -f "/.dockerenv" ] || [ -f "/etc/alpine-release" ]; then \
		echo "[*] Executing directly inside Alpine environment..."; \
		AURALIS_FLAVOR=metal /bin/bash scripts/build-iso.sh metal "$(DIST_DIR)"; \
	else \
		echo "[*] Executing via Docker builder container..."; \
		$(DOCKER_CMD) -e AURALIS_FLAVOR=metal $(IMAGE_NAME) /bin/bash scripts/build-iso.sh metal /workspace/dist; \
	fi
	@echo "[+] Bare-Metal ISO built successfully at: $(ISO_METAL)"

# ------------------------------------------------------------------------------
# 7. QEMU / VirtualBox Verification Targets (256 MB RAM)
# ------------------------------------------------------------------------------
test-vm:
	@/bin/bash scripts/test-qemu.sh vm

test-metal:
	@/bin/bash scripts/test-qemu.sh metal

# ------------------------------------------------------------------------------
# 8. Maintenance Targets
# ------------------------------------------------------------------------------
clean:
	@echo "[*] Removing build distribution directory and temporary artifacts..."
	rm -rf "$(DIST_DIR)"
	rm -rf /tmp/auralis-build-work-* 2>/dev/null || true
	rm -f *.apkovl.tar.gz 2>/dev/null || true
	make -C src/auralis-shell clean 2>/dev/null || true
	make -C src/auralis-taskmgr clean 2>/dev/null || true
	make -C src/auralis-store clean 2>/dev/null || true
	make -C src/auralis-router clean 2>/dev/null || true
	make -C installer/windows clean 2>/dev/null || true
	@echo "[+] Clean completed."

shell:
	@echo "[*] Launching interactive shell in $(IMAGE_NAME)..."
	$(DOCKER_CMD) -it $(IMAGE_NAME) /bin/bash
