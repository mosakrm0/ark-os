#!/bin/bash

set -e

echo "==================================================="
echo "   Ark OS: Full Automated Build Process"
echo "==================================================="

BUSYBOX_VERSION="1.38.0"


sudo apt-get update
sudo apt-get install -y build-essential bzip2 flex bison libelf-dev libssl-dev bc

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

rm busybox-${BUSYBOX_VERSION}.tar.bz2

echo "[4/5] Building Docker Image..."
sudo docker build -t ark-os:latest .

echo "==================================================="
echo "   Ark OS Build Successfully Completed!"
echo "   Run with: sudo docker run -it ark-os"
echo "==================================================="
