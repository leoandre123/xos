CC = gcc
LD = ld

CFLAGS = -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -O2 -nostdlib -nostdinc
LDFLAGS = -T linker.ld -nostdlib

BUILD = build
ELF = $(BUILD)/terminal.elf

SRCS = main.c
OBJS = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

all: $(ELF)

$(ELF): $(OBJS) linker.ld
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) $(OBJS) -o $(ELF)

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
