###############################################################################
# Makefile for 2oo2 Cross-Comparison Architecture
#
# Peer-to-peer voting without central voter process
# Processes A and B communicate directly for cross-comparison
#
# Targets:
#   west_process_a  - Computation + comparison (CPU0)
#   west_process_b  - Verification + comparison (CPU1)
#   west_process_c  - Service channel (printf)
#
# Usage:
#   make              # Build all
#   make run          # Build and run
#   make clean        # Clean
###############################################################################

CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
LDFLAGS =

TARGETS = west_process_a west_process_b west_process_c

.PHONY: all clean run info

all: $(TARGETS)
	@echo "✓ Built all processes"

west_process_a: west/process_a.c
	$(CC) $(CFLAGS) $(LDFLAGS) west/process_a.c -o west_process_a
	@echo "  ✓ west_process_a"

west_process_b: west/process_b.c
	$(CC) $(CFLAGS) $(LDFLAGS) west/process_b.c -o west_process_b
	@echo "  ✓ west_process_b"

west_process_c: west/process_c.c
	$(CC) $(CFLAGS) $(LDFLAGS) west/process_c.c -o west_process_c
	@echo "  ✓ west_process_c"

run: all
	@echo "\nStarting 2oo2 cross-comparison processes..."
	@echo "Process A: ./west_process_a"
	@echo "Process B: ./west_process_b"
	@echo "Process C: ./west_process_c"
	@echo ""
	./west_process_c

clean:
	rm -f $(TARGETS) *.o
	@echo "✓ Cleaned"

info:
	@echo "2oo2 Cross-Comparison Architecture"
	@echo "===================================="
	@echo "Targets:"
	@echo "  west_process_a - Computation + comparison (CPU0)"
	@echo "  west_process_b - Verification + comparison (CPU1)"
	@echo "  west_process_c - Service channel (printf)"
	@echo ""
	@echo "Key feature: A & B communicate directly, no central voter"
	@echo "Both execute identical comparison logic"
	@echo "Decision: Joint output if they agree, safe state if not"
	@echo ""
	@echo "Build: make"
	@echo "Run: make run (or run each in separate terminal)"
	@echo "Clean: make clean"

.SUFFIXES:
.IMPLICIT:
