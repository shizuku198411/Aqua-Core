#!/bin/bash
set -xue

# directory
BIN_DIR=./bin
DISK_IMG=./bin/disk.img

# create disk image when missing (16MiB)
if [ ! -f "$DISK_IMG" ]; then
  truncate -s 16M "$DISK_IMG"
fi

# qemu
QEMU=qemu-system-riscv32
# options
QEMU_OPT="-machine virt -bios default -nographic -serial mon:stdio --no-reboot -global virtio-mmio.force-legacy=true"

# start
$QEMU $QEMU_OPT \
  -drive file=$DISK_IMG,if=none,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0 \
  -kernel $BIN_DIR/kernel.elf
