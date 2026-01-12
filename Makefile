OPENSBI ?= C:/Users/Connor/Desktop/opensbi

CROSS ?= riscv64-unknown-elf-
CC     := $(CROSS)gcc
OBJCOPY:= $(CROSS)objcopy

ARCH ?= rv32imac
ABI  ?= ilp32

LINKER_SCRIPT := linker.ld

CFLAGS  := -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra \
		   -march=$(ARCH) -mabi=$(ABI) -mcmodel=medany
ASFLAGS := -march=$(ARCH) -mabi=$(ABI) -mcmodel=medany
LDFLAGS := -nostdlib -nostartfiles -ffreestanding -Wl,-T,$(LINKER_SCRIPT) \
		   -march=$(ARCH) -mabi=$(ABI) -mcmodel=medany

ASSEMBLY_SOURCES := $(wildcard assembly/*.S) $(wildcard assembly/*.s)
C_SOURCES        := $(wildcard source/*.c)
OBJECTS          := $(ASSEMBLY_SOURCES:.S=.o)
OBJECTS          += $(C_SOURCES:.c=.o)

OUTPUT_ELF := target/os.elf
OUTPUT_BIN := target/os.bin

all: $(OUTPUT_BIN)

target:
	mkdir target

%.o: %.S
	$(CC) -c $< -o $@ $(ASFLAGS)

%.o: %.s
	$(CC) -c $< -o $@ $(ASFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(OUTPUT_ELF): target $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) -lgcc

$(OUTPUT_BIN): $(OUTPUT_ELF)
	$(OBJCOPY) -O binary $< $@

run: $(OUTPUT_ELF)
	qemu-system-riscv32 -machine virt -bios $(OPENSBI)/build/platform/generic/firmware/fw_dynamic.bin -kernel $(OUTPUT_ELF)

clean:
	rm -f $(OBJECTS)
	rm -f $(OUTPUT_ELF)
	rm -f $(OUTPUT_BIN)