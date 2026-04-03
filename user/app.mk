# Shared build rules for all user apps.
# Each app's Makefile just sets APP_NAME, then includes this file.
# All .c and .asm files in the app directory (recursively) are compiled automatically.
# crt0 is compiled from user/lib/crt0.c and linked first automatically.
#
# Usage in an app's Makefile:
#   APP_NAME = my_app
#   include ../../app.mk

CC      = gcc
LD      = ld
AS      = nasm

GCC_BUILTIN_INCLUDES := $(shell $(CC) -print-file-name=include)

CFLAGS  = -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -O2 \
          -nostdlib -nostdinc \
          -I$(USER_ROOT)/lib \
          -I$(USER_ROOT)/../shared \
          -I$(GCC_BUILTIN_INCLUDES)
ASFLAGS = -f elf64
LDFLAGS = -T $(USER_ROOT)/linker.ld -nostdlib

# USER_ROOT is the user/ directory (where this file lives)
USER_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

BUILD = build
ELF   = $(BUILD)/$(APP_NAME).elf

CRT0 = $(BUILD)/crt0.o

C_SRCS   := $(shell find . -name '*.c')
ASM_SRCS := $(shell find . -name '*.asm')

# lib sources excluding crt0 (compiled separately)
LIB_SRCS := $(filter-out $(USER_ROOT)/lib/crt0.c, $(wildcard $(USER_ROOT)/lib/*.c))
LIB_OBJS := $(patsubst $(USER_ROOT)/lib/%.c, $(BUILD)/lib/%.o, $(LIB_SRCS))

C_OBJS   := $(patsubst ./%.c,  $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst ./%.asm,$(BUILD)/%.o, $(ASM_SRCS))
OBJS     := $(C_OBJS) $(ASM_OBJS) $(LIB_OBJS)

all: $(ELF)

# crt0 is linked first so _start is the first symbol in .text
$(ELF): $(CRT0) $(OBJS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) $(CRT0) $(OBJS) -o $(ELF)

$(CRT0): $(USER_ROOT)/lib/crt0.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/lib/%.o: $(USER_ROOT)/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
