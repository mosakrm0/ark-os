#!/bin/bash

set -e

echo "==================================================="
echo "   Ark OS: Full VM & Kernel Build Process"
echo "==================================================="

KERNEL_VERSION="7.0.11"
CORES=$(nproc)

if [ ! -f "linux-${KERNEL_VERSION}/arch/x86/boot/bzImage" ]; then
    echo "[1/4] Downloading and Compiling Linux Kernel ${KERNEL_VERSION}..."
    if [ ! -d "linux-${KERNEL_VERSION}" ]; then
        wget -nc https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-${KERNEL_VERSION}.tar.xz
        tar -xf linux-${KERNEL_VERSION}.tar.xz
    fi
    cd linux-${KERNEL_VERSION}
    make ARCH=x86 defconfig
    echo "Compiling kernel with ${CORES} cores. This may take a while..."
    make ARCH=x86 -j${CORES}
    cd ..
else
    echo "[1/4] Linux Kernel is already compiled. Skipping..."
fi

if [ ! -d "rootfs" ]; then
    echo "Error: rootfs not found! Please run ./build.sh first to generate the filesystem."
    exit 1
fi

echo "[3/4] Packaging filesystem into initramfs..."
cd rootfs
find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz
cd ..

echo "[4/4] Booting Ark OS in QEMU..."
echo "To exit the emulator later, press Ctrl+A, release, then X."
sleep 3

qemu-system-x86_64 \
    -kernel linux-${KERNEL_VERSION}/arch/x86/boot/bzImage \
    -initrd initramfs.cpio.gz \
    -append "console=ttyS0 quiet rdinit=/Noah" \
    -nographic