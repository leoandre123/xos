#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
ESP_DIR="$ROOT_DIR/build/esp"
DISK_IMG="$ROOT_DIR/disk.bin"

# ── argument ──────────────────────────────────────────────────────────────────
DEVICE="${1:-}"
if [ -z "$DEVICE" ]; then
    echo "Usage: $0 <device>   e.g.  $0 /dev/sdb"
    echo ""
    echo "Available block devices:"
    lsblk -d -o NAME,SIZE,MODEL | grep -v "^loop"
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo "ERROR: $DEVICE is not a block device"
    exit 1
fi

# ── sanity checks ─────────────────────────────────────────────────────────────
if [ ! -f "$ESP_DIR/EFI/BOOT/BOOTX64.EFI" ] || [ ! -f "$ESP_DIR/kernel.bin" ]; then
    echo "ERROR: Build outputs missing. Run ./run.sh first."
    exit 1
fi

if [ ! -f "$DISK_IMG" ]; then
    echo "ERROR: disk.bin not found at $DISK_IMG"
    exit 1
fi

# ── confirm ───────────────────────────────────────────────────────────────────
echo "WARNING: This will ERASE everything on $DEVICE"
lsblk "$DEVICE"
read -r -p "Type YES to continue: " confirm
if [ "$confirm" != "YES" ]; then
    echo "Aborted."
    exit 1
fi

# ── unmount any existing partitions ───────────────────────────────────────────
echo "==== Unmounting $DEVICE partitions ===="
for part in "$DEVICE"?*; do
    if mountpoint -q "$part" 2>/dev/null; then
        sudo umount "$part"
    fi
done

# ── partition ─────────────────────────────────────────────────────────────────
echo "==== Partitioning $DEVICE ===="
sudo parted "$DEVICE" --script mklabel gpt
sudo parted "$DEVICE" --script mkpart ESP  fat32 1MiB 256MiB
sudo parted "$DEVICE" --script set 1 esp on
sudo parted "$DEVICE" --script mkpart DATA fat32 256MiB 100%

# give the kernel time to re-read the partition table
sudo partprobe "$DEVICE" 2>/dev/null || true
sleep 1

# resolve partition names (sdb1 / sdb1 or nvme0n1p1 style)
if [[ "$DEVICE" == *nvme* || "$DEVICE" == *mmcblk* ]]; then
    PART1="${DEVICE}p1"
    PART2="${DEVICE}p2"
else
    PART1="${DEVICE}1"
    PART2="${DEVICE}2"
fi

# ── ESP: bootloader + kernel ──────────────────────────────────────────────────
echo "==== Formatting ESP ($PART1) ===="
sudo mkfs.fat -F32 -n EFI "$PART1"

MNT=$(mktemp -d)
sudo mount "$PART1" "$MNT"

sudo mkdir -p "$MNT/EFI/BOOT"
sudo cp "$ESP_DIR/EFI/BOOT/BOOTX64.EFI" "$MNT/EFI/BOOT/"
sudo cp "$ESP_DIR/kernel.bin" "$MNT/"

echo "  ESP contents:"
find "$MNT" -type f | sort | sed 's/^/    /'

sudo umount "$MNT"
rmdir "$MNT"

# ── DATA: raw disk image ──────────────────────────────────────────────────────
echo "==== Writing disk.bin to $PART2 ===="
DISK_IMG_SIZE=$(stat -c%s "$DISK_IMG")
PART2_SIZE=$(sudo blockdev --getsize64 "$PART2")

if [ "$DISK_IMG_SIZE" -gt "$PART2_SIZE" ]; then
    echo "ERROR: disk.bin ($DISK_IMG_SIZE bytes) is larger than $PART2 ($PART2_SIZE bytes)"
    sudo umount "$MNT" 2>/dev/null || true
    exit 1
fi

sudo dd if="$DISK_IMG" of="$PART2" bs=4M status=progress conv=fsync
echo ""

echo "==== Done ===="
echo "Eject the drive and boot from it."
echo "In your UEFI firmware, select: $DEVICE → EFI/BOOT/BOOTX64.EFI"
