#!/usr/bin/env python3
"""
pagefault.py — resolve a page fault address to a kernel/user function.

Usage:
    python scripts/pagefault.py <address> [--elf path/to/binary.elf]

The address can be in hex (0x...) or decimal.
Defaults to kernel/build/kernel.elf if --elf is not given.
"""

import argparse
import glob
import os
import re
import subprocess
import sys

from rich.columns import Columns
from rich.console import Console
from rich.panel import Panel
from rich.rule import Rule
from rich.syntax import Syntax
from rich.table import Table
from rich import box

console = Console(highlight=False)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ── helpers ──────────────────────────────────────────────────────────────────

def die(msg: str) -> None:
    console.print(f"[bold red]error:[/bold red] {msg}")
    sys.exit(1)


def run(cmd: list[str]) -> str:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True)
        return r.stdout
    except FileNotFoundError:
        die(f"tool not found: {cmd[0]}")


def parse_addr(s: str) -> int:
    try:
        return int(s, 16) if s.startswith("0x") or s.startswith("0X") else int(s, 0)
    except ValueError:
        die(f"cannot parse address: {s!r}")


# ── symbol lookup via nm ──────────────────────────────────────────────────────

def load_symbols(elf: str) -> list[tuple[int, str]]:
    """Return sorted list of (addr, name) for all text symbols."""
    out = run(["nm", "--defined-only", "-n", elf])
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1].lower() in ("t", "w"):
            try:
                syms.append((int(parts[0], 16), parts[2]))
            except ValueError:
                pass
    return syms


def find_function(syms: list[tuple[int, str]], addr: int) -> tuple[int, str] | None:
    """Binary-search for the last symbol whose address <= addr."""
    lo, hi = 0, len(syms) - 1
    result = None
    while lo <= hi:
        mid = (lo + hi) // 2
        if syms[mid][0] <= addr:
            result = syms[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    return result


# ── disassembly ───────────────────────────────────────────────────────────────

_FUNC_RE = re.compile(r"^([0-9a-f]+) <(.+)>:$")
_INSN_RE = re.compile(r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s+(.+)$")

CONTEXT_LINES = 12   # instructions before/after fault address


def disassemble_around(elf: str, func_addr: int, fault_addr: int) -> list[tuple[int, str, bool]]:
    """
    Return a list of (addr, disasm_text, is_fault) for the function,
    clipped to CONTEXT_LINES before and after the fault instruction.
    """
    out = run(["objdump", "-d", "--no-show-raw-insn",
               f"--start-address=0x{func_addr:x}", elf])

    insns: list[tuple[int, str]] = []
    in_func = False

    for line in out.splitlines():
        m = _FUNC_RE.match(line)
        if m:
            if in_func:
                break          # hit the next function
            if int(m.group(1), 16) == func_addr:
                in_func = True
            continue

        if not in_func:
            continue

        # match instruction lines: "   addr:   mnemonic operands"
        parts = line.split(":", 1)
        if len(parts) == 2:
            try:
                addr = int(parts[0].strip(), 16)
                text = parts[1].strip()
                if text:
                    insns.append((addr, text))
            except ValueError:
                pass

    if not insns:
        return []

    # find fault index
    fault_idx = None
    for i, (a, _) in enumerate(insns):
        if a == fault_addr:
            fault_idx = i
            break
    if fault_idx is None:
        # pick closest
        fault_idx = min(range(len(insns)), key=lambda i: abs(insns[i][0] - fault_addr))

    lo = max(0, fault_idx - CONTEXT_LINES)
    hi = min(len(insns) - 1, fault_idx + CONTEXT_LINES)

    return [(a, t, a == fault_addr) for a, t in insns[lo:hi + 1]]


# ── addr2line ─────────────────────────────────────────────────────────────────

def addr2line(elf: str, addr: int) -> tuple[str, str]:
    """Return (function_name, file:line). Falls back to ('?', '?:?')."""
    out = run(["addr2line", "-e", elf, "-f", f"0x{addr:x}"])
    lines = out.strip().splitlines()
    fn   = lines[0].strip() if len(lines) > 0 else "?"
    loc  = lines[1].strip() if len(lines) > 1 else "?:?"
    return fn, loc


# ── rendering ─────────────────────────────────────────────────────────────────

def render(elf: str, fault_addr: int) -> None:
    elf_name = os.path.basename(elf)

    # ── header ──────────────────────────────────────────────────────────────
    console.print()
    console.rule(f"[bold cyan]Page Fault Resolver[/bold cyan]", style="cyan")
    console.print()

    # ── symbol table lookup ──────────────────────────────────────────────────
    with console.status("[cyan]Loading symbols…[/cyan]", spinner="dots"):
        syms = load_symbols(elf)
    if not syms:
        die("no text symbols found — was the ELF stripped?")

    hit = find_function(syms, fault_addr)

    # ── addr2line ────────────────────────────────────────────────────────────
    fn_name, src_loc = addr2line(elf, fault_addr)
    offset = fault_addr - hit[0] if hit else 0

    # ── summary table ────────────────────────────────────────────────────────
    t = Table(box=box.ROUNDED, show_header=False, border_style="cyan",
              pad_edge=False, padding=(0, 1))
    t.add_column("key",   style="bold dim white", no_wrap=True)
    t.add_column("value", style="bold white",     no_wrap=True)

    t.add_row("Fault address", f"[red]0x{fault_addr:016x}[/red]")
    t.add_row("Binary",        f"[yellow]{elf_name}[/yellow]")

    if hit:
        t.add_row("Function",  f"[green]{hit[1]}[/green]  "
                               f"[dim](+0x{offset:x} from 0x{hit[0]:x})[/dim]")
    else:
        t.add_row("Function",  "[red]not found[/red]")

    if fn_name not in ("?", "??"):
        t.add_row("Demangled",  f"[green]{fn_name}[/green]")
    if src_loc not in ("?", "??:?", "??:0"):
        t.add_row("Source",    f"[blue]{src_loc}[/blue]")

    console.print(t)
    console.print()

    if not hit:
        console.print("[yellow]Warning:[/yellow] address is outside all known symbols.")
        return

    # ── disassembly panel ────────────────────────────────────────────────────
    with console.status("[cyan]Disassembling…[/cyan]", spinner="dots"):
        insns = disassemble_around(elf, hit[0], fault_addr)

    if not insns:
        console.print("[yellow]Warning:[/yellow] objdump returned no instructions.")
        return

    lines: list[str] = []
    for addr, text, is_fault in insns:
        hex_addr = f"0x{addr:016x}"
        if is_fault:
            lines.append(f"  ► {hex_addr}:  {text}")
        else:
            lines.append(f"    {hex_addr}:  {text}")

    asm_text = "\n".join(lines)
    syn = Syntax(asm_text, "asm", theme="monokai", line_numbers=False,
                 word_wrap=False)

    console.print(Panel(syn,
                        title=f"[bold green]{hit[1]}[/bold green]  "
                              f"[dim]+0x{offset:x}[/dim]",
                        border_style="green",
                        subtitle=f"[dim]{elf_name}[/dim]"))

    # ── nearby symbols ───────────────────────────────────────────────────────
    idx = syms.index(hit)
    near = Table(box=box.SIMPLE, show_header=True, border_style="dim",
                 header_style="bold dim")
    near.add_column("Address",  style="dim cyan",   no_wrap=True)
    near.add_column("Symbol",   style="white",      no_wrap=True)
    near.add_column("",         style="dim yellow",  no_wrap=True)

    start = max(0, idx - 3)
    end   = min(len(syms) - 1, idx + 3)
    for i in range(start, end + 1):
        a, name = syms[i]
        marker = "◄ fault" if i == idx else ""
        style  = "bold green" if i == idx else ""
        near.add_row(f"0x{a:016x}", f"[{style}]{name}[/{style}]" if style else name, marker)

    console.print(Rule("[dim]Nearby symbols[/dim]", style="dim"))
    console.print(near)
    console.print()


# ── ELF resolution ───────────────────────────────────────────────────────────

def resolve_elf(name: str) -> str:
    """
    Turn a short name like 'terminal' or 'kernel' into an ELF path.
    Also accepts a path that already exists on disk.
    """
    if os.path.isfile(name):
        return name

    candidates = [
        os.path.join(ROOT, "kernel/build/kernel.elf"),
        *sorted(
            glob.glob(os.path.join(ROOT, "user/apps/*/build/*.elf"))
        ),
    ]

    needle = name.lower()
    matches = [p for p in candidates if os.path.basename(p).replace(".elf", "").lower() == needle]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        die(f"ambiguous name {name!r} — matches:\n" +
            "\n".join(f"  {m}" for m in matches))

    # fuzzy fallback: substring match
    fuzzy = [p for p in candidates if needle in os.path.basename(p).lower()]
    if len(fuzzy) == 1:
        return fuzzy[0]
    if len(fuzzy) > 1:
        die(f"ambiguous name {name!r} — matches:\n" +
            "\n".join(f"  {m}" for m in fuzzy))

    available = [os.path.basename(p).replace(".elf", "") for p in candidates]
    die(f"no ELF found for {name!r}.\nAvailable: {', '.join(available)}")


# ── main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    p = argparse.ArgumentParser(
        description="Resolve a page fault address to a function in an ELF binary.")
    p.add_argument("address",
                   help="Fault address (hex 0x… or decimal)")
    p.add_argument("--elf", "-e", default="kernel",
                   help="ELF binary or app name, e.g. 'terminal', 'dafne', 'kernel' (default: kernel)")
    args = p.parse_args()

    elf = resolve_elf(args.elf)

    fault_addr = parse_addr(args.address)
    render(elf, fault_addr)


if __name__ == "__main__":
    main()
