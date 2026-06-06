#!/bin/bash

set -e

echo "==================================================="
echo "   Ark OS: Full VM & Kernel Build Process"
echo "==================================================="

KERNEL_VERSION="7.0.11"
CORES=$(nproc)
BUSYBOX_VERSION="1.36.1"

sudo apt-get update
sudo apt-get install -y build-essential
sudo apt-get install -y flex bison libelf-dev libssl-dev bc bzip2
sudo apt install -y qemu-system qemu-utils virt-manager libvirt-daemon-system libvirt-clients
sudo systemctl enable --now libvirtd
sudo adduser $USER libvirt


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



if [ ! -d "busybox-${BUSYBOX_VERSION}/_install" ]; then
    echo "[1/5] Downloading and compiling BusyBox..."
    wget -nc https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2
    tar -xf busybox-${BUSYBOX_VERSION}.tar.bz2
    cd busybox-${BUSYBOX_VERSION}
    
    make defconfig
    
    sed -i 's/^.*CONFIG_STATIC[^_].*$/CONFIG_STATIC=y/g' .config
    sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/g' .config
    
    make -j$(nproc)
    make install
    cd ..
else
    echo "[1/5] BusyBox is already compiled, skipping download..."
fi

echo "[2/5] Preparing clean root filesystem..."
rm -rf rootfs
mkdir -p rootfs/bin rootfs/sbin rootfs/usr rootfs/dev rootfs/proc rootfs/sys
cp -a busybox-${BUSYBOX_VERSION}/_install/* rootfs/

echo "[3/5] Compiling Noah (PID 1) and Custom Commands..."
gcc -static Noah.c -o rootfs/Noah
chmod +x rootfs/Noah

if [ -f "poweroff.c" ]; then
    gcc -static poweroff.c -o rootfs/bin/ark-down
    chmod +x rootfs/bin/ark-down
fi





if [ ! -d "rootfs" ]; then
    echo "Error: rootfs not found!"
    exit 1
fi

echo "[3/4] Packaging filesystem into initramfs..."
cd rootfs
# Use -print0 and --null for safer file path handling, and drop the gzip pipe entirely
find . -print0 | cpio --null -o -H newc > ../initramfs.cpio
cd ..

echo "[4/4] Booting Ark OS in QEMU..."
echo "To exit the emulator later, press Ctrl+A, release, then X."
sleep 3

qemu-system-x86_64 \
    -kernel linux-${KERNEL_VERSION}/arch/x86/boot/bzImage \
    -initrd initramfs.cpio \
    -m 512M \
    -append "console=ttyS0 quiet rdinit=/Noah" \
    -nographic