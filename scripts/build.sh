#!/bin/bash
set -xue

MAP_DIR=./map
OBJ_DIR=./obj
BIN_DIR=./bin

LIB_SRC_DIR=./src/lib
KERNEL_SRC_DIR=./src/kernel
USER_SRC_DIR=./src/user
USER_RUNTIME_DIR=$USER_SRC_DIR/runtime
USER_APPS_DIR=$USER_SRC_DIR/apps

OBJCOPY=llvm-objcopy
CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib -I./src/include -I./src/user/include"

mkdir -p "$MAP_DIR" "$OBJ_DIR" "$BIN_DIR"

# shell
$CC $CFLAGS -Wl,-T$USER_SRC_DIR/user.ld -Wl,-Map=$MAP_DIR/shell.map -o $BIN_DIR/shell.elf \
    $USER_RUNTIME_DIR/*.c $USER_APPS_DIR/shell/*.c $LIB_SRC_DIR/commonlibs.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary $BIN_DIR/shell.elf $BIN_DIR/shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv $BIN_DIR/shell.bin $OBJ_DIR/shell.bin.o

# ipc_rx
$CC $CFLAGS -Wl,-T$USER_SRC_DIR/user.ld -Wl,-Map=$MAP_DIR/ipc_rx.map -o $BIN_DIR/ipc_rx.elf \
    $USER_RUNTIME_DIR/*.c $USER_APPS_DIR/ipc_rx/*.c $LIB_SRC_DIR/commonlibs.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary $BIN_DIR/ipc_rx.elf $BIN_DIR/ipc_rx.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv $BIN_DIR/ipc_rx.bin $OBJ_DIR/ipc_rx.bin.o

# kernel
$CC $CFLAGS -Wl,-T$KERNEL_SRC_DIR/kernel.ld -Wl,-Map=$MAP_DIR/kernel.map -o $BIN_DIR/kernel.elf \
    $LIB_SRC_DIR/commonlibs.c \
    $KERNEL_SRC_DIR/kernel.c \
    $KERNEL_SRC_DIR/mm/*.c \
    $KERNEL_SRC_DIR/proc/*.c \
    $KERNEL_SRC_DIR/fs/*.c \
    $KERNEL_SRC_DIR/trap/*.c \
    $KERNEL_SRC_DIR/time/*.c \
    $KERNEL_SRC_DIR/platform/*.c \
    $OBJ_DIR/shell.bin.o $OBJ_DIR/ipc_rx.bin.o
