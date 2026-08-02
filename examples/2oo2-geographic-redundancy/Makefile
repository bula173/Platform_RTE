###############################################################################
# Makefile for Multi-Site Geographic Redundancy Example
#
# Railway system with West/East sites, active/standby failover
#
#   make                          # Native build
#   make run                       # Build and run
#   make clean                     # Clean
###############################################################################

CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
LDFLAGS =

SOURCES = redundancy_multi_site.c main.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = redundancy_multi_site.h
TARGET = 2oo2-multi-site

.PHONY: all clean run info

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "✓ Built $(TARGET)"

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Cleaned"

run: $(TARGET)
	./$(TARGET)

info:
	@echo "Target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "Build: make"
	@echo "Run:   make run"
	@echo "Clean: make clean"

.SUFFIXES:
.IMPLICIT:
