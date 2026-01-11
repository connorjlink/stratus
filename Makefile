# Compiler and assembler
AS = i686-elf-as
CC = i686-elf-gcc

# Compiler and linker flags
CFLAGS = -std=c99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -ffreestanding -O2 -nostdlib

# Sources and output
ASM_SOURCES := $(wildcard *.s)
C_SOURCES := $(wildcard *.c)
OBJECTS := $(ASM_SOURCES:.s=.o) $(C_SOURCES:.c=.o)
OUTPUT_BIN = myos.bin
LINKER_SCRIPT = linker.ld

# Default target
all: $(OUTPUT_BIN)

# Pattern rule to assemble .s files to .o files
%.o: %.s
	$(AS) $< -o $@

# Pattern rule to compile .c files to .o files
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

# Link all object files into the final binary
$(OUTPUT_BIN): $(OBJECTS)
	$(CC) -T $(LINKER_SCRIPT) -o $@ $(LDFLAGS) $(OBJECTS) -lgcc

run: all
	qemu-system-riscv64 -machine virt -s -S -bios ~/opensbi/build/platform/generic/firmware/fw_dynamic.bin

clean:
	rm -f $(OBJECTS) $(OUTPUT_BIN)




