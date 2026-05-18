#include "elf_icon.h"
#include "fs/file.h"
#include "memory.h"
#include "syscall.h"
#include "types.h"

typedef struct {
  ubyte  magic[4];
  ubyte  class;
  ubyte  endianness;
  ubyte  version;
  ubyte  abi;
  ubyte  padding[8];
  ushort type;
  ushort machine;
  uint   elf_version;
  ulong  entry;
  ulong  phoff;
  ulong  shoff;
  uint   flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
} __attribute__((packed)) elf64_ehdr;

typedef struct {
  uint  sh_name;
  uint  sh_type;
  ulong sh_flags;
  ulong sh_addr;
  ulong sh_offset;
  ulong sh_size;
  uint  sh_link;
  uint  sh_info;
  ulong sh_addralign;
  ulong sh_entsize;
} __attribute__((packed)) elf64_shdr;

static int streq(const char *a, const char *b) {
  while (*a && *b && *a == *b) { a++; b++; }
  return *a == *b;
}

bitmap *elf_icon_load(const char *path) {
  file_handle h = file_open(path);
  if (!h) return 0;

  uint size = file_size(h);
  if (size < sizeof(elf64_ehdr)) { file_close(h); return 0; }

  ubyte *buf = sys_alloc(size);
  if (!buf) { file_close(h); return 0; }
  file_read(h, buf, size);
  file_close(h);

  elf64_ehdr *ehdr = (elf64_ehdr *)buf;
  if (ehdr->magic[0] != 0x7f || ehdr->magic[1] != 'E' ||
      ehdr->magic[2] != 'L' || ehdr->magic[3] != 'F' ||
      ehdr->shnum == 0 || ehdr->shstrndx == 0) {
    sys_free(buf, size);
    return 0;
  }

  if (ehdr->shoff + (ulong)ehdr->shnum * sizeof(elf64_shdr) > size) {
    sys_free(buf, size);
    return 0;
  }

  elf64_shdr *shdrs = (elf64_shdr *)(buf + ehdr->shoff);
  char *strtab = (char *)(buf + shdrs[ehdr->shstrndx].sh_offset);

  for (ushort i = 0; i < ehdr->shnum; i++) {
    if (streq(strtab + shdrs[i].sh_name, ".icon")) {
      ulong icon_size = shdrs[i].sh_size;
      if (icon_size < 8) break;
      ubyte *icon = sys_alloc(icon_size);
      if (!icon) break;
      memcpy(icon, buf + shdrs[i].sh_offset, icon_size);
      sys_free(buf, size);
      return (bitmap *)icon;
    }
  }

  sys_free(buf, size);
  return 0;
}
