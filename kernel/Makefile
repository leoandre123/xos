CC = gcc
LD = ld
AS = nasm
OBJCOPY = objcopy

CFLAGS = -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -mcmodel=kernel -O2 -Wall -Wextra
LDFLAGS = -T linker.ld

BUILD = build
ELF = $(BUILD)/kernel.elf
BIN = $(BUILD)/kernel.bin

SRCS := $(wildcard *.c)
ASMS := $(wildcard *.asm)

C_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,$(BUILD)/%.o,$(ASMS))

OBJS := $(ASM_OBJS) $(C_OBJS)

all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm | $(BUILD)
	$(AS) -f elf64 $< -o $@

$(ELF): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(ELF)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BIN)

clean:
	rm -rf $(BUILD)

.PHONY: all clean