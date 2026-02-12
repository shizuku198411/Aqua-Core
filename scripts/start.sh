#!/bin/bash
set -xue

# directory
BIN_DIR=./bin

# qemu
QEMU=qemu-system-riscv32
# options
QEMU_OPT="-machine virt -bios default -nographic -serial mon:stdio --no-reboot"

# start
$QEMU $QEMU_OPT -kernel $BIN_DIR/kernel.elf