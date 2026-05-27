#!/usr/bin/env python3
import argparse
import glob
import os
import shutil
import stat
import subprocess
import sys

import rich.box
from rich.console import Console
from rich.panel import Panel
from rich.progress import BarColumn, MofNCompleteColumn, Progress, SpinnerColumn, TextColumn, TimeElapsedColumn
from rich.rule import Rule
from rich.table import Table

console = Console(highlight=False)

ROOT       = os.path.dirname(os.path.abspath(__file__))
BOOTLOADER = os.path.join(ROOT, "bootloader/uefi")
KERNEL     = os.path.join(ROOT, "kernel")
USER       = os.path.join(ROOT, "user")
BUILD      = os.path.join(ROOT, "build")

EFI_PART_LBA  = 2048
DATA_PART_LBA = 34816
EFI_BYTE      = EFI_PART_LBA  * 512
DATA_BYTE     = DATA_PART_LBA * 512


def run(*args, cwd=None, input=None, label=None, quiet=False):
    str_args = [str(a) for a in args]
    label = label or os.path.basename(str_args[0])

    def _run():
        result = subprocess.run(str_args, capture_output=True, cwd=cwd, input=input)
        if result.returncode != 0:
            output = (result.stdout + result.stderr).decode(errors="replace").strip()
            console.print(Panel(output, title=f"[red bold]✗  {label}[/red bold]", border_style="red"))
            sys.exit(result.returncode)

    if quiet:
        _run()
    else:
        with console.status(f"[cyan]{label}[/cyan]", spinner="dots"):
            _run()


def section(title):
    console.print()
    console.rule(f"[bold white]{title}[/bold white]", style="dim white")
    console.print()


def ok(msg):
    console.print(f"  [bold green]✓[/bold green]  {msg}")


def find_ovmf():
    code  = glob.glob("/usr/share/**/OVMF_CODE*.fd", recursive=True)
    vars_ = glob.glob("/usr/share/**/OVMF_VARS*.fd", recursive=True)
    if not code or not vars_:
        console.print(Panel("[red]Could not find OVMF firmware[/red]", border_style="red"))
        sys.exit(1)
    return code[0], vars_[0]


def build(perf=False):
    section("Build")

    make_args = ["PERF=1"] if perf else []
    run("make", *make_args, cwd=KERNEL, label="kernel build")
    ok("Kernel")

    run("make", cwd=BOOTLOADER, label="bootloader build")
    ok("Bootloader")

    if not shutil.which("mcopy"):
        console.print("[red]ERROR: mtools not found. Run: sudo apt install mtools[/red]")
        sys.exit(1)

    app_dirs = sorted(
        d for pattern in ("apps", "services")
        for d in glob.glob(os.path.join(USER, pattern, "*", ""))
        if os.path.isfile(os.path.join(d, "Makefile"))
    )

    with Progress(SpinnerColumn(), TextColumn("[cyan]{task.description}"),
                  BarColumn(), MofNCompleteColumn(), TimeElapsedColumn(),
                  console=console) as progress:
        task = progress.add_task("User apps", total=len(app_dirs))
        for app_dir in app_dirs:
            name = os.path.basename(os.path.dirname(app_dir))
            progress.update(task, description=f"[cyan]Building [bold]{name}[/bold]")
            run("make", cwd=app_dir, quiet=True)
            progress.advance(task)

    ok(f"{len(app_dirs)} user app(s) + service(s)")


def prepare_esp():
    section("Artifacts")

    esp = os.path.join(BUILD, "esp")
    shutil.rmtree(esp, ignore_errors=True)
    os.makedirs(os.path.join(esp, "EFI", "BOOT"))
    shutil.copy(os.path.join(BOOTLOADER, "build/BOOTX64.EFI"), os.path.join(esp, "EFI/BOOT/BOOTX64.EFI"))
    shutil.copy(os.path.join(KERNEL,     "build/kernel.bin"),  os.path.join(esp, "kernel.bin"))

    table = Table(box=rich.box.ROUNDED, border_style="dim", header_style="bold magenta", pad_edge=False)
    table.add_column("File",  style="cyan")
    table.add_column("Size",  justify="right", style="green")
    table.add_column("Path",  style="dim")

    for path in [os.path.join(BOOTLOADER, "build/BOOTX64.EFI"),
                 os.path.join(KERNEL,     "build/kernel.bin")]:
        table.add_row(os.path.basename(path),
                      f"{os.path.getsize(path) / 1024:.1f} KB",
                      os.path.relpath(path, ROOT))

    console.print(table)


def deploy_pxe(tftp_root):
    section("PXE Deploy")
    os.makedirs(os.path.join(tftp_root, "EFI", "BOOT"), exist_ok=True)
    shutil.copy(os.path.join(BOOTLOADER, "build/BOOTX64.EFI"), os.path.join(tftp_root, "EFI/BOOT/BOOTX64.EFI"))
    shutil.copy(os.path.join(KERNEL,     "build/kernel.bin"),  os.path.join(tftp_root, "kernel.bin"))
    console.print(f"  [cyan]BOOTX64.EFI[/cyan]  →  {tftp_root}/EFI/BOOT/")
    console.print(f"  [cyan]kernel.bin[/cyan]   →  {tftp_root}/")
    console.print(f"\n[dim]Tftpd64 boot file: EFI/BOOT/BOOTX64.EFI[/dim]")


def create_image(image):
    section("Disk Image")

    run("dd", "if=/dev/zero", f"of={image}", "bs=1M", "count=128", "status=none",
        label="Zeroing 128 MB image")

    gpt = (
        f"label: gpt\n"
        f"start={EFI_PART_LBA},  size={DATA_PART_LBA - EFI_PART_LBA},"
        f" type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B\n"
        f"start={DATA_PART_LBA}, size=+,"
        f" type=0FC63DAF-8483-4772-8E79-3D69D8477DE4\n"
    )
    run("sfdisk", image, input=gpt.encode(), label="Writing GPT")
    ok("Partition table")

    run("mformat", "-i", f"{image}@@{EFI_BYTE}", "-F", "-v", "EFI", "::", label="Formatting EFI partition")
    run("mmd",     "-i", f"{image}@@{EFI_BYTE}", "::/EFI")
    run("mmd",     "-i", f"{image}@@{EFI_BYTE}", "::/EFI/BOOT")
    run("mcopy",   "-i", f"{image}@@{EFI_BYTE}",
        os.path.join(BOOTLOADER, "build/BOOTX64.EFI"), "::/EFI/BOOT/BOOTX64.EFI",
        label="Copying BOOTX64.EFI")
    run("mcopy",   "-i", f"{image}@@{EFI_BYTE}",
        os.path.join(KERNEL, "build/kernel.bin"), "::kernel.bin",
        label="Copying kernel.bin")
    ok("EFI partition")


def populate_data(image, init_mode):
    run("mformat", "-i", f"{image}@@{DATA_BYTE}", "-F", "-v", "XOS", "::",
        label="Formatting data partition")

    elfs   = sorted(
        e for pattern in ("apps", "services")
        for e in glob.glob(os.path.join(USER, pattern, "*", "build", "*.elf"))
    )
    assets = sorted(glob.glob(os.path.join(ROOT, "rootfs", "*")))
    total  = len(elfs) + 1 + len(assets)

    with Progress(SpinnerColumn(), TextColumn("[cyan]{task.description}"),
                  BarColumn(), MofNCompleteColumn(), TimeElapsedColumn(),
                  console=console) as progress:
        task = progress.add_task("Data partition", total=total)

        for entry in assets:
            name = os.path.basename(entry)
            progress.update(task, description=f"[cyan]Copying [bold]{name}[/bold]")
            flags = ["-s"] if os.path.isdir(entry) else []
            subprocess.run(["mcopy", *flags, "-i", f"{image}@@{DATA_BYTE}", entry, f"::{name}"],
                           capture_output=True, check=True)
            progress.advance(task)

        progress.update(task, description="[cyan]Writing [bold]init[/bold]")
        subprocess.run(["mcopy", "-i", f"{image}@@{DATA_BYTE}", "-", "::boot/init"],
                       input=init_mode.encode(), capture_output=True, check=True)
        progress.advance(task)

        subprocess.run(["mmd", "-i", f"{image}@@{DATA_BYTE}", "-D", "s", "::/sys"],
                       capture_output=True)
        subprocess.run(["mmd", "-i", f"{image}@@{DATA_BYTE}", "-D", "s", "::/sys/programs"],
                       capture_output=True)
        for elf in elfs:
            name = os.path.basename(elf)
            progress.update(task, description=f"[cyan]Copying [bold]{name}[/bold]")
            subprocess.run(["mcopy", "-i", f"{image}@@{DATA_BYTE}", elf, f"::/sys/programs/{name}"],
                           capture_output=True, check=True)
            progress.advance(task)

    ok("Data partition")


def launch_qemu(image, drive_mode, code_fd, ovmf_vars, debug, ps2, use_sudo=False):
    section("QEMU")

    drives   = ["-drive", f"if=pflash,format=raw,readonly=on,file={code_fd}",
                "-drive", f"if=pflash,format=raw,file={ovmf_vars}"]
    usb_devs = [] if ps2 else ["-device", "usb-kbd,bus=xhci.0"]

    if drive_mode == "usb":
        drives   += ["-drive", f"id=usbdrive,format=raw,file={image},if=none"]
        usb_devs += ["-device", "usb-storage,bus=xhci.0,drive=usbdrive"]
    else:
        drives += ["-drive", f"format=raw,file={image}"]

    info = Table.grid(padding=(0, 2))
    info.add_column(style="dim")
    info.add_column(style="bold cyan")
    info.add_row("Drive", drive_mode)
    info.add_row("Image", os.path.basename(image))
    info.add_row("RAM",   "256 MB")
    info.add_row("Keyboard", "PS/2" if ps2 else "USB HID")
    if debug:
        info.add_row("Debug", "GDB on :1234")

    console.print(Panel(info, title="[bold green]Launching QEMU[/bold green]", border_style="green"))
    console.print()

    net = ["-netdev", "user,id=net0", "-device", "e1000,netdev=net0"]
    if not use_sudo:
        net += ["-object", "filter-dump,id=dump0,netdev=net0,file=/tmp/xos.pcap"]

    cmd = ["qemu-system-x86_64", "-m", "256M", "-cpu", "max", "-smp", "4", *drives,
           *net, "-device", "qemu-xhci,id=xhci", *usb_devs,
           "-serial", "stdio"]
    if not use_sudo:
        cmd += ["-d", "int,cpu_reset", "-D", "/tmp/qemu.log"]
    if debug:
        cmd += ["-s", "-S"]
    if use_sudo:
        cmd = ["sudo"] + cmd

    subprocess.run(cmd, check=True)


def clean():
    section("Clean")
    targets = [
        (os.path.join(KERNEL,     "build"), "Kernel build"),
        (os.path.join(BOOTLOADER, "build"), "Bootloader build"),
        (BUILD,                             "Top-level build"),
        (os.path.join(ROOT,       "disk.bin"), "disk.bin"),
    ]
    app_dirs = sorted(
        d for pattern in ("apps", "services")
        for d in glob.glob(os.path.join(USER, pattern, "*", ""))
        if os.path.isfile(os.path.join(d, "Makefile"))
    )
    for app_dir in app_dirs:
        name = os.path.basename(os.path.dirname(app_dir))
        targets.append((os.path.join(app_dir, "build"), f"{name} build"))

    for path, label in targets:
        if os.path.isdir(path):
            shutil.rmtree(path)
            ok(f"Removed  {os.path.relpath(path, ROOT)}")
        elif os.path.isfile(path):
            os.remove(path)
            ok(f"Removed  {os.path.relpath(path, ROOT)}")
        else:
            console.print(f"  [dim]–  {label} already clean[/dim]")


def flash_device(image, device):
    section("Flash to USB")

    if not os.path.exists(device):
        console.print(f"[red]ERROR: {device} does not exist[/red]")
        sys.exit(1)
    if not stat.S_ISBLK(os.stat(device).st_mode):
        console.print(f"[red]ERROR: {device} is not a block device[/red]")
        sys.exit(1)

    img_size = os.path.getsize(image)
    result = subprocess.run(["lsblk", device], capture_output=True, text=True)
    console.print(result.stdout.rstrip())
    console.print()
    console.print(f"[bold yellow]WARNING:[/bold yellow] This will ERASE everything on [bold]{device}[/bold]")
    console.print(f"  Image: {os.path.relpath(image, ROOT)}  ({img_size / 1024 / 1024:.1f} MB)")
    confirm = input("\nType YES to continue: ")
    if confirm.strip() != "YES":
        console.print("Aborted.")
        sys.exit(0)

    # unmount any mounted partitions
    result = subprocess.run(["lsblk", "-rno", "NAME,MOUNTPOINT", device],
                            capture_output=True, text=True)
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1]:
            subprocess.run(["sudo", "umount", f"/dev/{parts[0]}"], capture_output=True)

    console.print(f"\n  [cyan]Writing[/cyan] {os.path.relpath(image, ROOT)} → {device}\n")
    result = subprocess.run(
        ["sudo", "dd", f"if={image}", f"of={device}", "bs=4M", "status=progress", "conv=fsync"]
    )
    if result.returncode != 0:
        console.print("[red]ERROR: dd failed[/red]")
        sys.exit(result.returncode)

    console.print()
    ok(f"Flashed to {device}")
    console.print("\n  Eject the drive and boot from it.")
    console.print("  In your UEFI firmware select: EFI/BOOT/BOOTX64.EFI")


def patch_esp(device):
    """Update kernel.bin on the ESP partition of a physical device using mtools (no mount needed)."""
    run("sudo", "mcopy", "-i", f"{device}@@{EFI_BYTE}", "-o",
        os.path.join(KERNEL, "build/kernel.bin"), "::kernel.bin",
        label="Patching kernel.bin on ESP")
    ok(f"kernel.bin → {device} ESP")


def main():
    parser = argparse.ArgumentParser(description="XOS build & run")
    parser.add_argument("--clean",        action="store_true", help="Remove all build artifacts and exit")
    parser.add_argument("--desktop",     dest="init_mode",  action="store_const", const="dafne")
    parser.add_argument("--terminal",    dest="init_mode",  action="store_const", const="terminal")
    parser.add_argument("--usb",         dest="drive_mode", action="store_const", const="usb")
    parser.add_argument("--ata",         dest="drive_mode", action="store_const", const="ata")
    parser.add_argument("--debug", "-d", action="store_true")
    parser.add_argument("--pxe",         action="store_true")
    parser.add_argument("--flash",       metavar="DEVICE",  help="Flash to a physical USB drive (e.g. /dev/sdb)")
    parser.add_argument("--passthrough", metavar="DEVICE",  help="Boot QEMU from a physical USB drive, patching kernel.bin first (e.g. /dev/sdb)")
    parser.add_argument("--fast-boot",   action="store_true")
    parser.add_argument("--ps2",         action="store_true", help="Use PS/2 keyboard instead of USB HID")
    parser.add_argument("--perf",        action="store_true", help="Enable kernel performance counters (KERNEL_PERF)")
    parser.set_defaults(init_mode="terminal", drive_mode="ata")
    args = parser.parse_args()

    if args.clean:
        clean()
        return

    tftp_root = os.environ.get("PXE_ROOT", "/mnt/c/tftpboot")
    os.makedirs(BUILD, exist_ok=True)

    console.print(Panel(
        "[bold cyan]XOS Build System[/bold cyan]",
        subtitle=f"[dim]drive=[bold]{args.drive_mode}[/bold]  init=[bold]{args.init_mode}[/bold][/dim]",
        border_style="cyan",
        padding=(1, 4),
    ))

    build(perf=args.perf)
    prepare_esp()

    if args.pxe:
        deploy_pxe(tftp_root)
        return

    if args.flash:
        image = os.path.join(BUILD, "usb.img")
        create_image(image)
        populate_data(image, args.init_mode)
        flash_device(image, args.flash)
        return

    code_fd, vars_fd = find_ovmf()
    shutil.copy(vars_fd, os.path.join(BUILD, "OVMF_VARS.fd"))
    ovmf_vars = os.path.join(BUILD, "OVMF_VARS.fd")

    if args.passthrough:
        patch_esp(args.passthrough)
        launch_qemu(args.passthrough, "usb", code_fd, ovmf_vars, args.debug, args.ps2, use_sudo=True)
        return

    image = (os.path.join(BUILD, "usb.img") if args.drive_mode == "usb"
             else os.path.join(ROOT, "disk.bin"))

    create_image(image)
    populate_data(image, args.init_mode)
    launch_qemu(image, args.drive_mode, code_fd, ovmf_vars, args.debug, args.ps2)


if __name__ == "__main__":
    main()
