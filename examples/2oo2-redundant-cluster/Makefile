###############################################################################
# Makefile for 2oo2 Redundant Cluster Example
#
# Supports multiple hardware configurations:
#   make                          # Native build (x86-64 little-endian)
#   make ARCH=ppc32               # PowerPC 32-bit (big-endian)
#   make ARCH=ppc64               # PowerPC 64-bit (big-endian)
#   make ARCH=arm                 # ARM Cortex-A (little-endian)
#   make ARCH=mips                # MIPS (little-endian)
#   make ARCH=mips-be             # MIPS (big-endian)
###############################################################################

ARCH ?= native

# Compiler configuration per architecture
ifeq ($(ARCH),native)
    CC = gcc
    CFLAGS = -std=c99 -Wall -Wextra -O2
    DEFINES =
    TARGET = 2oo2-demo
else ifeq ($(ARCH),ppc32)
    CC = powerpc-linux-gnu-gcc
    CFLAGS = -std=c99 -Wall -Wextra -O2 -m32
    DEFINES = -DHARDWARE_CONFIG_BIG_ENDIAN
    TARGET = 2oo2-demo-ppc32
else ifeq ($(ARCH),ppc64)
    CC = powerpc-linux-gnu-gcc
    CFLAGS = -std=c99 -Wall -Wextra -O2 -m64
    DEFINES = -DHARDWARE_CONFIG_BIG_ENDIAN
    TARGET = 2oo2-demo-ppc64
else ifeq ($(ARCH),arm)
    CC = arm-linux-gnueabihf-gcc
    CFLAGS = -std=c99 -Wall -Wextra -O2 -march=armv7-a -mfpu=neon
    DEFINES =
    TARGET = 2oo2-demo-arm
else ifeq ($(ARCH),mips)
    CC = mips-linux-gnu-gcc
    CFLAGS = -std=c99 -Wall -Wextra -O2 -EL
    DEFINES =
    TARGET = 2oo2-demo-mips-le
else ifeq ($(ARCH),mips-be)
    CC = mips-linux-gnu-gcc
    CFLAGS = -std=c99 -Wall -Wextra -O2 -EB
    DEFINES = -DHARDWARE_CONFIG_BIG_ENDIAN
    TARGET = 2oo2-demo-mips-be
else
    $(error Unknown ARCH=$(ARCH). Valid: native, ppc32, ppc64, arm, mips, mips-be)
endif

SOURCES = redundancy.c main.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = redundancy.h

.PHONY: all clean info

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "✓ Built $(TARGET) for $(ARCH)"

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) 2oo2-demo*
	@echo "✓ Cleaned"

info:
	@echo "Architecture: $(ARCH)"
	@echo "Compiler: $(CC)"
	@echo "CFLAGS: $(CFLAGS) $(DEFINES)"
	@echo "Target: $(TARGET)"

run: $(TARGET)
	./$(TARGET)

# Disable implicit rules
.SUFFIXES:
.IMPLICIT:
