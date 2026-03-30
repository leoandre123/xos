#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

BOOTLOADER_DIR="$ROOT_DIR/bootloader/uefi"
KERNEL_DIR="$ROOT_DIR/kernel"
BUILD_DIR="$ROOT_DIR/build"
ESP_DIR="$BUILD_DIR/esp"

echo "==== Building kernel ===="
cd "$KERNEL_DIR"
make clean
make

echo "==== Building bootloader ===="
cd "$BOOTLOADER_DIR"
make clean
make

echo "==== Building user apps ===="
cd "$ROOT_DIR/user/apps/test_app"
make clean
make

cd "$ROOT_DIR/user/apps/shell"
make clean
make

echo "==== Copying user apps to disk ===="
if ! command -v mcopy &>/dev/null; then
    echo "ERROR: mtools not found. Run: sudo apt install mtools"
    exit 1
fi
mcopy -D o -i "$ROOT_DIR/disk.bin" "$ROOT_DIR/user/apps/test_app/build/test_app.elf" ::/test_app.elf
mcopy -D o -i "$ROOT_DIR/disk.bin" "$ROOT_DIR/user/apps/shell/build/shell.elf" ::/shell.elf

echo "==== Preparing EFI filesystem ===="
rm -rf "$ESP_DIR"
mkdir -p "$ESP_DIR/EFI/BOOT"

# Copy bootloader
cp "$BOOTLOADER_DIR/build/BOOTX64.EFI" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"

# Copy kernel
cp "$KERNEL_DIR/build/kernel.bin" "$ESP_DIR/kernel.bin"

echo "==== Finding OVMF firmware ===="
CODE_FD=$(find /usr/share -name 'OVMF_CODE*.fd' | head -n 1)
VARS_FD=$(find /usr/share -name 'OVMF_VARS*.fd' | head -n 1)

if [ -z "$CODE_FD" ] || [ -z "$VARS_FD" ]; then
    echo "ERROR: Could not find OVMF firmware"
    exit 1
fi

mkdir -p "$BUILD_DIR"
cp "$VARS_FD" "$BUILD_DIR/OVMF_VARS.fd"

echo "==== Debug info ===="
ls -l "$BOOTLOADER_DIR/build/BOOTX64.EFI"
ls -l "$KERNEL_DIR/build/kernel.bin"
echo "--- ESP contents ---"
find "$ESP_DIR" -maxdepth 4 -type f | sort

echo "==== Starting QEMU ===="

DEBUG_FLAGS=""
if [ "${1}" = "--debug" ] || [ "${1}" = "-d" ]; then
    DEBUG_FLAGS="-s -S"
    echo "Debug mode: waiting for GDB on port 1234"
fi

qemu-system-x86_64 \
    -m 256M \
    -drive if=pflash,format=raw,readonly=on,file="$CODE_FD" \
    -drive if=pflash,format=raw,file="$BUILD_DIR/OVMF_VARS.fd" \
    -drive format=raw,file="$ROOT_DIR/disk.bin" \
    -drive format=raw,file=fat:rw:"$ESP_DIR" \
    -serial stdio \
    -d int,cpu_reset -D /tmp/qemu.log \
    $DEBUG_FLAGS