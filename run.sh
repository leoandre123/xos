#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

BOOTLOADER_DIR="$ROOT_DIR/bootloader/uefi"
KERNEL_DIR="$ROOT_DIR/kernel"
USER_DIR="$ROOT_DIR/user"
BUILD_DIR="$ROOT_DIR/build"
ESP_DIR="$BUILD_DIR/esp"

# ── Parse flags ───────────────────────────────────────────────────────────────
INIT_MODE="terminal"
DEBUG_FLAGS=""
USB_MODE=false
PXE_MODE=false
# Tftpd64 root on Windows — override with PXE_ROOT=/your/path ./run.sh --pxe
TFTP_ROOT="${PXE_ROOT:-/mnt/c/tftpboot}"

for arg in "$@"; do
    case "$arg" in
        --desktop)  INIT_MODE="dafne" ;;
        --terminal) INIT_MODE="terminal" ;;
        --debug|-d) DEBUG_FLAGS="-s -S" ;;
        --usb)      USB_MODE=true ;;
        --pxe)      PXE_MODE=true ;;
    esac
done

# ── Build ─────────────────────────────────────────────────────────────────────
echo "==== Building kernel ===="
cd "$KERNEL_DIR"
make clean
make

echo "==== Building bootloader ===="
cd "$BOOTLOADER_DIR"
make clean
make

echo "==== Building user apps ===="
if ! command -v mcopy &>/dev/null; then
    echo "ERROR: mtools not found. Run: sudo apt install mtools"
    exit 1
fi

for app_dir in "$USER_DIR/apps"/*/; do
    [ -f "$app_dir/Makefile" ] || continue
    echo "  Building $(basename "$app_dir")..."
    cd "$app_dir"
    make clean
    make
done

# ── Prepare ESP ───────────────────────────────────────────────────────────────
echo "==== Preparing EFI filesystem ===="
rm -rf "$ESP_DIR"
mkdir -p "$ESP_DIR/EFI/BOOT"
cp "$BOOTLOADER_DIR/build/BOOTX64.EFI" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"
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

# ── PXE deploy ────────────────────────────────────────────────────────────────
if $PXE_MODE; then
    echo "==== Deploying to TFTP root: $TFTP_ROOT ===="
    mkdir -p "$TFTP_ROOT/EFI/BOOT"
    cp "$BOOTLOADER_DIR/build/BOOTX64.EFI" "$TFTP_ROOT/EFI/BOOT/BOOTX64.EFI"
    cp "$KERNEL_DIR/build/kernel.bin"      "$TFTP_ROOT/kernel.bin"
    echo "Copied:"
    echo "  BOOTX64.EFI → $TFTP_ROOT/EFI/BOOT/BOOTX64.EFI"
    echo "  kernel.bin  → $TFTP_ROOT/kernel.bin"
    echo ""
    echo "Tftpd64 DHCP boot-file name: EFI/BOOT/BOOTX64.EFI"
    echo "Reboot the target machine to PXE boot."
    exit 0
fi

# ── Disk setup ────────────────────────────────────────────────────────────────
echo "  Init mode: $INIT_MODE"

if $USB_MODE; then
    # ── USB image mode ────────────────────────────────────────────────────────
    # Create a 128 MB GPT image with two partitions:
    #   Partition 1 (EFI System, LBA 2048–34815): FAT32 with BOOTX64.EFI + kernel.bin
    #   Partition 2 (Data,       LBA 34816–end):  FAT32 with user apps + /init
    #
    # OVMF boots from USB, finds the EFI partition, loads BOOTX64.EFI.
    # The bootloader's DeviceHandle is then a USB device → boot_device=1.
    # The kernel reads the GPT at LBA 2 to find partition 2's start LBA.
    # ─────────────────────────────────────────────────────────────────────────
    echo "==== Creating USB image ===="
    USB_IMAGE="$BUILD_DIR/usb.img"
    EFI_PART_LBA=2048
    DATA_PART_LBA=34816

    dd if=/dev/zero of="$USB_IMAGE" bs=1M count=128 status=none

    # Write a real GPT so OVMF can find the EFI System Partition
    sfdisk "$USB_IMAGE" <<SFDISK_EOF
label: gpt
start=$EFI_PART_LBA,  size=$((DATA_PART_LBA - EFI_PART_LBA)), type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B
start=$DATA_PART_LBA, size=+,                                  type=0FC63DAF-8483-4772-8E79-3D69D8477DE4
SFDISK_EOF

    EFI_BYTE=$((EFI_PART_LBA * 512))
    DATA_BYTE=$((DATA_PART_LBA * 512))

    # Format and populate EFI partition
    mformat -i "$USB_IMAGE"@@"$EFI_BYTE" -F -v "EFI" ::
    mmd    -i "$USB_IMAGE"@@"$EFI_BYTE" ::/EFI
    mmd    -i "$USB_IMAGE"@@"$EFI_BYTE" ::/EFI/BOOT
    mcopy  -i "$USB_IMAGE"@@"$EFI_BYTE" "$BOOTLOADER_DIR/build/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
    mcopy  -i "$USB_IMAGE"@@"$EFI_BYTE" "$KERNEL_DIR/build/kernel.bin"      ::kernel.bin

    # Format and populate data partition
    mformat -i "$USB_IMAGE"@@"$DATA_BYTE" -F -v "XOS" ::
    echo "==== Copying user apps to USB image ===="
    for elf in "$USER_DIR/apps"/*/build/*.elf; do
        [ -f "$elf" ] || continue
        elf_name="$(basename "$elf")"
        echo "  Copying $elf_name..."
        mcopy -D o -i "$USB_IMAGE"@@"$DATA_BYTE" "$elf" "::/$elf_name"
    done
    echo -n "$INIT_MODE" | mcopy -D o -i "$USB_IMAGE"@@"$DATA_BYTE" - "::init"

else
    # ── Normal ATA disk mode ──────────────────────────────────────────────────
    echo "==== Copying user apps to disk ===="
    for elf in "$USER_DIR/apps"/*/build/*.elf; do
        [ -f "$elf" ] || continue
        elf_name="$(basename "$elf")"
        echo "  Copying $elf_name..."
        mcopy -D o -i "$ROOT_DIR/disk.bin" "$elf" "::/$elf_name"
    done
    echo -n "$INIT_MODE" | mcopy -D o -i "$ROOT_DIR/disk.bin" - "::init"
fi

# ── QEMU ──────────────────────────────────────────────────────────────────────
echo "==== Starting QEMU ===="

if [ -n "$DEBUG_FLAGS" ]; then
    echo "Debug mode: waiting for GDB on port 1234"
fi

QEMU_DRIVES=(
    -drive "if=pflash,format=raw,readonly=on,file=$CODE_FD"
    -drive "if=pflash,format=raw,file=$BUILD_DIR/OVMF_VARS.fd"
)

QEMU_USB_DEVS=(-device usb-kbd,bus=xhci.0)

if $USB_MODE; then
    # No IDE ESP — EFI partition lives on the USB image.
    # OVMF enumerates USB storage, finds the EFI partition, and boots from it.
    # The bootloader's DeviceHandle is then a USB device → boot_device=1 in BootInfo.
    QEMU_DRIVES+=(-drive "id=usbdrive,format=raw,file=$USB_IMAGE,if=none")
    QEMU_USB_DEVS+=(-device usb-storage,bus=xhci.0,drive=usbdrive)
else
    QEMU_DRIVES+=(-drive "format=raw,file=fat:rw:$ESP_DIR")
    QEMU_DRIVES+=(-drive "format=raw,file=$ROOT_DIR/disk.bin")
fi

qemu-system-x86_64 \
    -m 256M \
    "${QEMU_DRIVES[@]}" \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -device qemu-xhci,id=xhci \
    "${QEMU_USB_DEVS[@]}" \
    -object filter-dump,id=dump0,netdev=net0,file=/tmp/xos.pcap \
    -serial stdio \
    -d int,cpu_reset -D /tmp/qemu.log \
    $DEBUG_FLAGS
