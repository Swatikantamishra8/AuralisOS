# ==============================================================================
# AuralisOS Hermetic ISO Builder Container
# Phase 1-5: Custom Ultra-Low-Memory Linux Distribution Pipeline
# Base: Alpine Linux (x86_64, musl, OpenRC)
# Target: Hybrid BIOS/UEFI bootable ISO with "Liquid Glass" GUI & Binary Router (<95MB)
# ==============================================================================

FROM alpine:latest

LABEL maintainer="AuralisOS Core Architecture Team"
LABEL description="Hermetic Alpine-based build environment for AuralisOS hybrid BIOS/UEFI ISOs"
LABEL version="5.0"

# 1. Install core development toolchain and ISO creation prerequisites
#    alpine-sdk, build-base, apk-tools, alpine-conf, busybox, fakeroot, syslinux,
#    xorriso, squashfs-tools, mtools, grub-efi, grub-bios, dosfstools
RUN apk update && apk add --no-cache \
    alpine-sdk \
    build-base \
    apk-tools \
    alpine-conf \
    busybox \
    busybox-extras \
    fakeroot \
    syslinux \
    xorriso \
    squashfs-tools \
    mtools \
    grub-efi \
    grub-bios \
    dosfstools \
    bash \
    coreutils \
    tar \
    gzip \
    sed \
    gawk \
    grep \
    findutils \
    git \
    curl \
    shadow \
    sudo \
    ca-certificates

# 2. Desktop & Toolchain Prerequisites
#    Libraries for compiling auralis-shell (GTK4), auralis-taskmgr, auralis-store, and auralis-router (Go)
RUN apk add --no-cache \
    gtk4.0-dev \
    gtk4-layer-shell-dev \
    wayland-dev \
    wayland-protocols \
    libxkbcommon-dev \
    cmake \
    meson \
    ninja \
    pkgconf \
    glib-dev \
    pango-dev \
    cairo-dev \
    gdk-pixbuf-dev \
    seatd \
    libseat-dev \
    go \
    p7zip \
    file

# 3. Clone aports repository to provide official Alpine mkimage.sh tooling and base profiles
RUN git clone --depth 1 https://gitlab.alpinelinux.org/alpine/aports.git /opt/aports \
    && ln -sf /opt/aports/scripts/mkimage.sh /usr/local/bin/mkimage.sh \
    && chmod +x /usr/local/bin/mkimage.sh

# 4. Configure non-root builder user with abuild and sudo privileges
RUN adduser -D -u 1000 builder -G abuild \
    && echo "builder ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
    && mkdir -p /home/builder/.abuild \
    && chown -R builder:abuild /home/builder

# Set up working directory
WORKDIR /workspace

# Default command to start bash shell
CMD ["/bin/bash"]
