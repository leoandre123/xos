ARCH            = x86_64
TARGET          = BOOTX64.EFI
BUILD           = build

EFIINC          = /usr/include/efi
EFIINC_ARCH     = /usr/include/efi/$(ARCH)
EFILIB          = /usr/lib
CRT0            = /usr/lib/crt0-efi-$(ARCH).o
LDSCRIPT        = /usr/lib/elf_$(ARCH)_efi.lds

CC              = gcc
LD              = ld
OBJCOPY         = objcopy

CFLAGS = -I$(EFIINC) -I$(EFIINC_ARCH) \
         -fpic -fshort-wchar -mno-red-zone \
         -Wall -Wextra

LDFLAGS = -nostdlib -znocombreloc -T $(LDSCRIPT) -shared -Bsymbolic
LIBS    = -L$(EFILIB) -lefi -lgnuefi

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

all: $(BUILD)/$(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/main.so: $(OBJS)
	$(LD) $(LDFLAGS) $(CRT0) $^ -o $@ $(LIBS)

$(BUILD)/$(TARGET): $(BUILD)/main.so
	$(OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .dynamic \
		-j .dynsym \
		-j .rel \
		-j .rela \
		-j .reloc \
		--target=efi-app-$(ARCH) \
		$< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean