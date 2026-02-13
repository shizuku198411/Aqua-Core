SHELL := /bin/bash

MAP_DIR := map
OBJ_DIR := obj
BIN_DIR := bin

LIB_SRC_DIR := src/lib
KERNEL_SRC_DIR := src/kernel
USER_SRC_DIR := src/user
USER_RUNTIME_DIR := $(USER_SRC_DIR)/runtime
USER_APPS_DIR := $(USER_SRC_DIR)/apps

CC := clang
OBJCOPY := llvm-objcopy
QEMU := qemu-system-riscv32

CPPFLAGS ?=
CFLAGS := ${CPPFLAGS} -std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib -Isrc/include -Isrc/user/include
QEMU_OPT := -machine virt -bios default -nographic -serial mon:stdio --no-reboot -global virtio-mmio.force-legacy=false

DISK_IMG := $(BIN_DIR)/disk.img
DISK_SIZE := 16M

KERNEL_ELF := $(BIN_DIR)/kernel.elf
SHELL_ELF := $(BIN_DIR)/shell.elf
SHELL_BIN := $(BIN_DIR)/shell.bin
SHELL_OBJ := $(OBJ_DIR)/shell.bin.o
IPC_RX_ELF := $(BIN_DIR)/ipc_rx.elf
IPC_RX_BIN := $(BIN_DIR)/ipc_rx.bin
IPC_RX_OBJ := $(OBJ_DIR)/ipc_rx.bin.o

.PHONY: all build run start debug release run-debug run-release start-debug start-release clean distclean dirs disk

all: build

build: $(KERNEL_ELF)

debug: clean
	$(MAKE) build CPPFLAGS=-DDEBUG

release: clean
	$(MAKE) build CPPFLAGS=

dirs:
	mkdir -p $(MAP_DIR) $(OBJ_DIR) $(BIN_DIR)

$(SHELL_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/shell.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/shell/*.c $(LIB_SRC_DIR)/commonlibs.c

$(SHELL_BIN): $(SHELL_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(SHELL_OBJ): $(SHELL_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(SHELL_BIN) $@

$(IPC_RX_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/ipc_rx.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/ipc_rx/*.c $(LIB_SRC_DIR)/commonlibs.c

$(IPC_RX_BIN): $(IPC_RX_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(IPC_RX_OBJ): $(IPC_RX_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(IPC_RX_BIN) $@

$(KERNEL_ELF): $(SHELL_OBJ) $(IPC_RX_OBJ)
	$(CC) $(CFLAGS) -Wl,-T$(KERNEL_SRC_DIR)/kernel.ld -Wl,-Map=$(MAP_DIR)/kernel.map -o $@ \
		$(LIB_SRC_DIR)/commonlibs.c \
		$(KERNEL_SRC_DIR)/kernel.c \
		$(KERNEL_SRC_DIR)/mm/*.c \
		$(KERNEL_SRC_DIR)/proc/*.c \
		$(KERNEL_SRC_DIR)/fs/*.c \
		$(KERNEL_SRC_DIR)/rtc/*.c \
		$(KERNEL_SRC_DIR)/trap/*.c \
		$(KERNEL_SRC_DIR)/time/*.c \
		$(KERNEL_SRC_DIR)/platform/*.c \
		$(SHELL_OBJ) $(IPC_RX_OBJ)

disk: dirs
	@if [ ! -f "$(DISK_IMG)" ]; then \
		truncate -s $(DISK_SIZE) "$(DISK_IMG)"; \
	fi

run: $(KERNEL_ELF) disk
	$(QEMU) $(QEMU_OPT) \
		-drive file=$(DISK_IMG),if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0 \
		-kernel $(KERNEL_ELF)

run-debug:
	$(MAKE) run CPPFLAGS=-DDEBUG

run-release:
	$(MAKE) run CPPFLAGS=

start: disk
	$(QEMU) $(QEMU_OPT) \
		-drive file=$(DISK_IMG),if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0 \
		-kernel $(KERNEL_ELF)

start-debug:
	$(MAKE) start CPPFLAGS=-DDEBUG

start-release:
	$(MAKE) start CPPFLAGS=

clean:
	rm -f $(KERNEL_ELF) $(SHELL_ELF) $(SHELL_BIN) $(SHELL_OBJ) $(IPC_RX_ELF) $(IPC_RX_BIN) $(IPC_RX_OBJ)
	rm -f $(MAP_DIR)/*.map

distclean: clean
	rm -f $(DISK_IMG)
