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

# kernel
KERNEL_ELF := $(BIN_DIR)/kernel.elf

# apps
# shell
SHELL_ELF := $(BIN_DIR)/shell.elf
SHELL_BIN := $(BIN_DIR)/shell.bin
SHELL_OBJ := $(OBJ_DIR)/shell.bin.o
# ipc_rx
IPC_RX_ELF := $(BIN_DIR)/ipc_rx.elf
IPC_RX_BIN := $(BIN_DIR)/ipc_rx.bin
IPC_RX_OBJ := $(OBJ_DIR)/ipc_rx.bin.o
# ps
PS_ELF := $(BIN_DIR)/ps.elf
PS_BIN := $(BIN_DIR)/ps.bin
PS_OBJ := $(OBJ_DIR)/ps.bin.o
# date
DATE_ELF := $(BIN_DIR)/date.elf
DATE_BIN := $(BIN_DIR)/date.bin
DATE_OBJ := $(OBJ_DIR)/date.bin.o
# ls
LS_ELF := $(BIN_DIR)/ls.elf
LS_BIN := $(BIN_DIR)/ls.bin
LS_OBJ := $(OBJ_DIR)/ls.bin.o
# mkdir
MKDIR_ELF := $(BIN_DIR)/mkdir.elf
MKDIR_BIN := $(BIN_DIR)/mkdir.bin
MKDIR_OBJ := $(OBJ_DIR)/mkdir.bin.o
# rmdir
RMDIR_ELF := $(BIN_DIR)/rmdir.elf
RMDIR_BIN := $(BIN_DIR)/rmdir.bin
RMDIR_OBJ := $(OBJ_DIR)/rmdir.bin.o
# touch
TOUCH_ELF := $(BIN_DIR)/touch.elf
TOUCH_BIN := $(BIN_DIR)/touch.bin
TOUCH_OBJ := $(OBJ_DIR)/touch.bin.o
# rm
RM_ELF := $(BIN_DIR)/rm.elf
RM_BIN := $(BIN_DIR)/rm.bin
RM_OBJ := $(OBJ_DIR)/rm.bin.o
# write
WRITE_ELF := $(BIN_DIR)/write.elf
WRITE_BIN := $(BIN_DIR)/write.bin
WRITE_OBJ := $(OBJ_DIR)/write.bin.o
# cat
CAT_ELF := $(BIN_DIR)/cat.elf
CAT_BIN := $(BIN_DIR)/cat.bin
CAT_OBJ := $(OBJ_DIR)/cat.bin.o
# kill
KILL_ELF := $(BIN_DIR)/kill.elf
KILL_BIN := $(BIN_DIR)/kill.bin
KILL_OBJ := $(OBJ_DIR)/kill.bin.o
# kernel_info
KERNEL_INFO_ELF := $(BIN_DIR)/kernel_info.elf
KERNEL_INFO_BIN := $(BIN_DIR)/kernel_info.bin
KERNEL_INFO_OBJ := $(OBJ_DIR)/kernel_info.bin.o
# bitmap
BITMAP_ELF := $(BIN_DIR)/bitmap.elf
BITMAP_BIN := $(BIN_DIR)/bitmap.bin
BITMAP_OBJ := $(OBJ_DIR)/bitmap.bin.o

.PHONY: all build run start debug release run-debug run-release start-debug start-release clean distclean dirs disk

all: build

build: $(KERNEL_ELF)

debug: clean
	$(MAKE) build CPPFLAGS=-DDEBUG

release: clean
	$(MAKE) build CPPFLAGS=

dirs:
	mkdir -p $(MAP_DIR) $(OBJ_DIR) $(BIN_DIR)

# shell
$(SHELL_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/shell.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/shell/*.c $(LIB_SRC_DIR)/commonlibs.c

$(SHELL_BIN): $(SHELL_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(SHELL_OBJ): $(SHELL_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(SHELL_BIN) $@

# ipc rx
$(IPC_RX_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/ipc_rx.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/ipc_rx/*.c $(LIB_SRC_DIR)/commonlibs.c

$(IPC_RX_BIN): $(IPC_RX_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(IPC_RX_OBJ): $(IPC_RX_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(IPC_RX_BIN) $@

# ps
$(PS_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/ps.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/ps/*.c $(LIB_SRC_DIR)/commonlibs.c

$(PS_BIN): $(PS_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(PS_OBJ): $(PS_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(PS_BIN) $@

# date
$(DATE_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/date.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/date/*.c $(LIB_SRC_DIR)/commonlibs.c

$(DATE_BIN): $(DATE_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(DATE_OBJ): $(DATE_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(DATE_BIN) $@

# ls
$(LS_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/ls.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/ls/*.c $(LIB_SRC_DIR)/commonlibs.c

$(LS_BIN): $(LS_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(LS_OBJ): $(LS_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(LS_BIN) $@

# mkdir
$(MKDIR_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/mkdir.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/mkdir/*.c $(LIB_SRC_DIR)/commonlibs.c

$(MKDIR_BIN): $(MKDIR_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(MKDIR_OBJ): $(MKDIR_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(MKDIR_BIN) $@

# rmdir
$(RMDIR_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/rmdir.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/rmdir/*.c $(LIB_SRC_DIR)/commonlibs.c

$(RMDIR_BIN): $(RMDIR_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(RMDIR_OBJ): $(RMDIR_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(RMDIR_BIN) $@

# touch
$(TOUCH_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/touch.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/touch/*.c $(LIB_SRC_DIR)/commonlibs.c

$(TOUCH_BIN): $(TOUCH_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(TOUCH_OBJ): $(TOUCH_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(TOUCH_BIN) $@

# rm
$(RM_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/rm.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/rm/*.c $(LIB_SRC_DIR)/commonlibs.c

$(RM_BIN): $(RM_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(RM_OBJ): $(RM_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(RM_BIN) $@

# write
$(WRITE_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/write.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/write/*.c $(LIB_SRC_DIR)/commonlibs.c

$(WRITE_BIN): $(WRITE_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(WRITE_OBJ): $(WRITE_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(WRITE_BIN) $@

# cat
$(CAT_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/cat.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/cat/*.c $(LIB_SRC_DIR)/commonlibs.c

$(CAT_BIN): $(CAT_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(CAT_OBJ): $(CAT_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(CAT_BIN) $@

# kill
$(KILL_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/kill.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/kill/*.c $(LIB_SRC_DIR)/commonlibs.c

$(KILL_BIN): $(KILL_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(KILL_OBJ): $(KILL_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(KILL_BIN) $@


# kernel_info
$(KERNEL_INFO_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/kernel_info.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/kernel_info/*.c $(LIB_SRC_DIR)/commonlibs.c

$(KERNEL_INFO_BIN): $(KERNEL_INFO_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(KERNEL_INFO_OBJ): $(KERNEL_INFO_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(KERNEL_INFO_BIN) $@

# bitmap
$(BITMAP_ELF): dirs
	$(CC) $(CFLAGS) -Wl,-T$(USER_SRC_DIR)/user.ld -Wl,-Map=$(MAP_DIR)/bitmap.map -o $@ \
		$(USER_RUNTIME_DIR)/*.c $(USER_APPS_DIR)/bitmap/*.c $(LIB_SRC_DIR)/commonlibs.c

$(BITMAP_BIN): $(BITMAP_ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,contents -O binary $< $@

$(BITMAP_OBJ): $(BITMAP_BIN)
	$(OBJCOPY) -Ibinary -Oelf32-littleriscv ./$(BITMAP_BIN) $@


$(KERNEL_ELF): $(SHELL_OBJ) $(IPC_RX_OBJ) $(PS_OBJ) $(DATE_OBJ) $(LS_OBJ) \
	$(MKDIR_OBJ) $(RMDIR_OBJ) $(TOUCH_OBJ) $(RM_OBJ) $(WRITE_OBJ) $(CAT_OBJ) \
	$(KILL_OBJ) $(KERNEL_INFO_OBJ) $(BITMAP_OBJ)
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
			$(SHELL_OBJ) $(IPC_RX_OBJ) $(PS_OBJ) $(DATE_OBJ) $(LS_OBJ) \
			$(MKDIR_OBJ) $(RMDIR_OBJ) $(TOUCH_OBJ) $(RM_OBJ) $(WRITE_OBJ) $(CAT_OBJ) \
			$(KILL_OBJ) $(KERNEL_INFO_OBJ) $(BITMAP_OBJ)

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
	rm -f $(KERNEL_ELF) \
		$(SHELL_ELF) $(SHELL_BIN) $(SHELL_OBJ) \
		$(IPC_RX_ELF) $(IPC_RX_BIN) $(IPC_RX_OBJ) \
		$(PS_ELF) $(PS_BIN) $(PS_OBJ) \
		$(DATE_ELF) $(DATE_BIN) $(DATE_OBJ) \
		$(LS_ELF) $(LS_BIN) $(LS_OBJ) \
		$(MKDIR_ELF) $(MKDIR_BIN) $(MKDIR_OBJ) \
		$(RMDIR_ELF) $(RMDIR_BIN) $(RMDIR_OBJ) \
		$(TOUCH_ELF) $(TOUCH_BIN) $(TOUCH_OBJ) \
		$(RM_ELF) $(RM_BIN) $(RM_OBJ) \
		$(WRITE_ELF) $(WRITE_BIN) $(WRITE_OBJ) \
		$(CAT_ELF) $(CAT_BIN) $(CAT_OBJ) \
		$(KILL_ELF) $(KILL_BIN) $(KILL_OBJ) \
		$(KERNEL_INFO_ELF) $(KERNEL_INFO_BIN) $(KERNEL_INFO_OBJ) \
		$(BITMAP_ELF) $(BITMAP_BIN) $(BITMAP_OBJ)
	rm -f $(MAP_DIR)/*.map

distclean: clean
	rm -f $(DISK_IMG)
