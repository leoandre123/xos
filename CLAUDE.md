# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

**Build and launch in QEMU (terminal mode):**
```bash
./run.py              # preferred — rich progress UI
./run.sh              # plain bash alternative
```

**Desktop mode (dafne compositor):**
```bash
./run.py --desktop
./run.sh --desktop
```

**GDB debug mode (waits on port 1234):**
```bash
./run.py --debug
```

**USB drive mode (instead of ATA disk image):**
```bash
./run.py --usb
```

**PXE deploy (copies to TFTP root for real-hardware network boot):**
```bash
./run.sh --pxe        # default TFTP root: /mnt/c/tftpboot
PXE_ROOT=/your/path ./run.sh --pxe
```

**Flash to a real USB drive:**
```bash
./flash_usb.sh /dev/sdX
```

**Build kernel only:**
```bash
cd kernel && make
```

**Build a single user app:**
```bash
cd user/apps/shell && make
```

**Clean:**
```bash
cd kernel && make clean
cd user/apps/<name> && make clean
```

**Required host tools:** `gcc`, `nasm`, `ld`, `mtools` (`mcopy`, `mformat`), `qemu-system-x86_64`, OVMF firmware (`sudo apt install ovmf mtools`), `gnu-efi` for the bootloader.

**Debug output:** serial output goes to the QEMU terminal (stdout). QEMU interrupt/reset log goes to `/tmp/qemu.log`. Network packets captured to `/tmp/xos.pcap`.

**Crash / page fault analysis:**
```bash
.venv/bin/python tools/pagefault.py <rip-address>              # kernel (default)
.venv/bin/python tools/pagefault.py <rip-address> --elf shell  # user app by name
```
Resolves an RIP/fault address to a function name, source location, and disassembly context using `nm` + `addr2line` + `objdump` on `kernel/build/kernel.elf` (or a user ELF). Use this whenever the kernel prints an interrupt frame with a fault address.

## Architecture Overview

### Boot sequence
1. **UEFI bootloader** (`bootloader/uefi/`) — built with gnu-efi, produces `BOOTX64.EFI`. Reads `kernel.bin` from the EFI partition, locates the framebuffer and memory map, and passes a `BootInfo` struct to the kernel bootstrap.
2. **Bootstrap** (`kernel/bootstrap/bootstrap.c`) — runs in physical address space. Sets up initial page tables with three mappings: identity map for first 4 GiB (2 MiB huge pages), higher-half kernel map at `0xFFFFFFFF80000000`, and HHDM at `0xFFFF800000000000`. Loads CR3 and calls `enter_higher_half`.
3. **Kernel entry** (`arch/x86_64/boot.asm`) — sets up the kernel stack, calls `kernel_pre_main` then `kernel_main`.
4. **`kernel_main`** (`kernel/kernel.c`) — initialises GDT, IDT, PIC, PMM, VMM, heap, drivers (ATA/xHCI/e1000), FAT32, scheduler, syscalls. Reads the `init` file from the data partition to decide which user process to launch first (`terminal` or `dafne`).

### Memory layout (virtual)
| Region | Base |
|---|---|
| Kernel | `0xFFFFFFFF80000000` |
| HHDM (physical ↔ virtual) | `0xFFFF800000000000` |
| Framebuffer mapping | `0xFFFF900000000000` |
| Kernel heap | `0xFFFFA00000000000` |
| Kernel stack | `0xFFFFB00000000000` |
| User framebuffer (`SYS_MAP_FB`) | `0x0000600000000000` |

Use `PHYS_TO_HHDM(p)` / `HHDM_TO_PHYS(v)` macros (`kernel/memory/vmm.h`) to convert between physical and virtual addresses.

### Kernel subsystems
- **PMM** (`kernel/memory/pmm.c`) — physical page frame allocator.
- **VMM** (`kernel/memory/vmm.c`) — per-process page table management; each task has its own `address_space`.
- **Scheduler** (`kernel/scheduler/`) — cooperative/round-robin. `schedule()` picks the next `TASK_READY` task. Context switch is in `context_switch.asm`. User tasks enter via `jump_to_userspace` into ring 3.
- **Syscalls** (`kernel/cpu/syscall.c`, `shared/syscalls.h`) — SYSCALL/SYSRET mechanism. `syscall_entry` (ASM) saves registers and calls `syscall_dispatch`. The kernel preserves RDI/RSI/RDX across sysret so compiler register assumptions hold.
- **Filesystem** (`kernel/filesystem/`) — FAT32 driver; `file_open`/`file_read`/`file_readdir` API. 8.3 filenames, case-insensitive. Driver abstracted via `BootDevice` (ATA, USB, AHCI, NVMe).
- **Compositor** (`kernel/compositor/`) — kernel-side window manager. Each window has its own framebuffer mapped into the client task's address space. `dafne` is the desktop compositor user process.
- **Networking** (`kernel/net/`) — e1000 Ethernet driver, ARP, DHCP, DNS, ICMP, UDP, TCP, HTTP. Sockets are exposed via syscalls 100–105.

### Disk layout
GPT image (`disk.bin` for ATA, `build/usb.img` for USB):
- **Partition 1** (EFI System, LBA 2048–34815): FAT32 with `BOOTX64.EFI` + `kernel.bin`.
- **Partition 2** (Data, LBA 34816–end): FAT32 with user ELF binaries, `rootfs/` contents, and the `init` file (contains the init process name as plain text).

### Shared types (`shared/`)
Headers shared between kernel and user space:
- `types.h` — `ubyte`, `ushort`, `uint`, `ulong`, `bool`
- `syscalls.h` — syscall numbers
- `boot_info.h` — `BootInfo` / `BootDevice` structs passed from bootloader to kernel
- `fb_info.h` — framebuffer descriptor with dirty-rect tracking
- `window_event.h`, `compositor_event.h` — IPC event types

### User-space (`user/`)
- **`user/lib/`** — freestanding libc replacement: `stdio.c`, `string.h`, `gfx.h` (inline pixel/rect/blit/text ops), `image.c` (`.lbm` bitmap loader), `socket.c`, `fs/file.c`, `keyboard.h`, `mouse.h`.
- **`user/lib/crt0.c`** — startup code; linked first so `_start` is the first `.text` symbol.
- **`user/linker.ld`** — user-space linker script.
- **`user/app.mk`** — shared Makefile for all user apps. Each app sets `APP_NAME` then `include ../../app.mk`. All `.c`/`.asm` files in the app directory are compiled automatically; `user/lib/` (excluding `crt0.c`) is compiled and linked into every app.

### Window & UI system
User apps create windows via `window_open()` → `SYS_WINDOW_CREATE` syscall. The compositor maps a private framebuffer into the app's address space.

Two UI modes available in `user/lib/window/ui/`:
- **Retained mode** (`retained_ui.h`) — tree of `ui_node` structs (vstack, hstack, grid, button, label, img). Call `ui_update(mouse_event)` each frame, then `ui_render(size)` + `window_end_paint()`.
- **Immediate mode** (`immediate_ui.h`) — stateless per-frame draw calls.

### Adding a new user app
1. Create `user/apps/<name>/Makefile`:
   ```makefile
   APP_NAME = <name>
   include ../../app.mk
   ```
2. Create `user/apps/<name>/main.c` with a `int main(void)` entry point.
3. `run.py` / `run.sh` will build and copy the ELF to the data partition automatically.
