# Toolchain definition
CC = x86_64-elf-gcc
OBJDUMP = x86_64-elf-objdump
READELF = x86_64-elf-readelf

# Target binary name
TARGET = TYPE.ELF

# Linker script
LINKER_SCRIPT = linker.ld

# Source and object files
SRCS = type.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -O2 \
         -ffreestanding \
         -fno-builtin \
         -fno-stack-protector \
         -mno-red-zone \
         -fno-asynchronous-unwind-tables \
         -fPIE \
         -Wall \
         -Wextra

LDFLAGS = -T $(LINKER_SCRIPT) \
          -nostdlib \
          -pie \
          -Wl,--no-dynamic-linker \
          -Wl,--build-id=none

.PHONY: all clean dump info relocations

all: $(TARGET)

$(TARGET): $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

dump: $(TARGET)
	$(OBJDUMP) -d $(TARGET)

info: $(TARGET)
	$(READELF) -h -l $(TARGET)

relocations: $(TARGET)
	$(READELF) -r $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)