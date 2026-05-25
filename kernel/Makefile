CC = gcc-14
LD = ld
AS = nasm
OBJCOPY = objcopy

CFLAGS = -std=gnu11 -ffreestanding -fno-stack-protector -mno-red-zone -fno-pic -m64 -mcmodel=kernel -O2 -Wall -Wextra \
         -mno-sse -mno-mmx -mno-sse2 -mno-avx \
         -fno-omit-frame-pointer \
         -MMD -MP \
         -I. -I../shared
LDFLAGS = -T linker.ld

BUILD = build
ELF = $(BUILD)/kernel.elf
BIN = $(BUILD)/kernel.bin

SRCS := $(shell find . -name '*.c')
ASMS := $(shell find . -name '*.asm')

C_OBJS := $(patsubst ./%.c,$(BUILD)/%.o,$(SRCS))
ASM_OBJS := $(patsubst ./%.asm,$(BUILD)/%.o,$(ASMS))
DEPS := $(C_OBJS:.o=.d)



ENTRY_OBJ := $(BUILD)/entry.o
OTHER_ASM_OBJS := $(filter-out $(ENTRY_OBJ),$(ASM_OBJS))
OBJS := $(ENTRY_OBJ) $(OTHER_ASM_OBJS) $(C_OBJS)

#OBJS := $(BUILD)/entry.o $(BUILD)/bootstrap/bootstrap.o $(BUILD)/serial.o $(BUILD)/font.o $(BUILD)/console.o $(BUILD)/memory/pmm.o $(BUILD)/memory/vmm.o $(BUILD)/kernel.o

all: $(BIN)

$(ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $(ELF)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BIN)

$(BUILD)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm Makefile
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPS)

.PHONY: all clean