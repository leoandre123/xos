# XOS

A custom x86-64 operating system written in C and NASM, booting via UEFI with a window compositor, networking stack, and a small suite of user-space applications.

---

## Running

**Terminal mode (QEMU):**
```bash
./run.py        # preferred — rich progress UI
```

**Desktop mode (dafne compositor):**
```bash
./run.py --desktop
```

**GDB debug (waits on port 1234):**
```bash
./run.py --debug
```

**Real hardware — USB:**
```bash
./run.py --usb
./flash_usb.sh /dev/sdX
```

**Required host tools:** `gcc`, `nasm`, `ld`, `mtools`, `qemu-system-x86_64`, OVMF firmware, `gnu-efi`
```bash
sudo apt install ovmf mtools
```

---

## Architecture

### Boot sequence
1. **UEFI bootloader** (`bootloader/uefi/`) — produces `BOOTX64.EFI`. Loads `kernel.bin` from the EFI partition and passes a `BootInfo` struct (framebuffer, memory map) to the kernel.
2. **Bootstrap** (`kernel/bootstrap/`) — sets up initial page tables (identity map, higher-half kernel at `0xFFFFFFFF80000000`, HHDM at `0xFFFF800000000000`), then jumps to the higher-half kernel.
3. **`kernel_main`** (`kernel/kernel.c`) — initialises GDT, IDT, PIC, PMM, VMM, heap, drivers, FAT32, scheduler, and syscalls. Reads `init` from the data partition to pick the first user process (`terminal` or `dafne`).

### Virtual memory layout
| Region | Base |
|---|---|
| Kernel | `0xFFFFFFFF80000000` |
| HHDM | `0xFFFF800000000000` |
| Framebuffer | `0xFFFF900000000000` |
| Kernel heap | `0xFFFFA00000000000` |
| Kernel stack | `0xFFFFB00000000000` |
| User framebuffer | `0x0000600000000000` |

### Kernel subsystems
- **PMM** — physical page frame allocator
- **VMM** — per-process page tables; each task has its own `address_space`
- **Scheduler** — round-robin; context switch in `context_switch.asm`; user tasks run in ring 3
- **Syscalls** — SYSCALL/SYSRET; numbers in `shared/syscalls.h`
- **Filesystem** — FAT32 (ATA, USB, AHCI, NVMe); `file_open` / `file_read` / `file_readdir`
- **Compositor** (`kernel/compositor/`) — kernel-side window manager; each window gets a private framebuffer mapped into the client's address space
- **Networking** (`kernel/net/`) — e1000 driver, Ethernet, ARP, IP, ICMP, UDP, TCP, DNS, HTTP, routing; sockets via syscalls

### Disk layout (GPT)
- **Partition 1** (EFI, FAT32): `BOOTX64.EFI` + `kernel.bin`
- **Partition 2** (Data, FAT32): user ELF binaries, `rootfs/`, `init` file

---

## User space

### Libraries (`user/lib/`)
- Freestanding libc replacement (`stdio`, `string`, `math`)
- `gfx.h` — pixel/rect/blit/text helpers
- `image.c` — `.lbm` bitmap loader
- `socket.c` — networking
- `threads.c` — user-space threading
- `window/ui/retained_ui.h` — retained-mode UI tree (vstack, hstack, grid, button, label, img)
- `window/ui/immediate_ui.h` — immediate-mode per-frame draw calls

### Applications (`user/apps/`)
| App | Description |
|---|---|
| `shell` | Command-line shell |
| `terminal` / `terminal_2` | Terminal emulators |
| `dafne` | Desktop compositor |
| `files` | File browser |
| `navigator` | Web/network navigator |
| `list` | Process list |
| `performance_monitor` | CPU/memory stats |
| `settings` | System settings |
| `routecfg` | Network routing configuration |

### Adding a new app
1. Create `user/apps/<name>/Makefile`:
   ```makefile
   APP_NAME = <name>
   include ../../app.mk
   ```
2. Add `user/apps/<name>/main.c` with `int main(void)`.
3. `run.py` picks it up and copies the ELF to the data partition automatically.

---

## Debugging

Serial output → QEMU stdout. QEMU log → `/tmp/qemu.log`. Packets → `/tmp/xos.pcap`.

**Crash / page fault analysis:**
```bash
.venv/bin/python tools/pagefault.py <rip>           # kernel
.venv/bin/python tools/pagefault.py <rip> --elf shell  # user app
```
Resolves a fault address to a function name and source line via `nm` + `addr2line` + `objdump`.
