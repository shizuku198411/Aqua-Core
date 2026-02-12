## GDBによるカーネル操作
page/access faultやillegal instruction等Kernle Panicが発生した場合、
GDBを利用してカーネル操作によるメモリ/レジスタの調査を行う。

### QEMUの待ち受け起動
```
qemu-system-riscv32 -machine virt -bios default \
-nographic -serial mon:stdio --no-reboot \
-S -s -kernel ./bin/kernel.elf
```

### GDB接続
```
gdb-multiarch ./bin/kernel.elf

// gdb console
set pagination off
set confirm off
set architecture riscv:rv32
// ユーザアプリ側にBPを入れる場合はシンボル追加 (USER_BASE=0x01000000 ref:src/user/user.ld)
directory .
add-symbol-file ./bin/shell.elf 0x01000000
// 接続
target remote :1234
```

### GDB Command
```
# set Breakpoint
b path/to/target:linenum

# continue
c

# print
p <variable>

# print address
p/x <variable>

# backtrace
bt

# print registry
info reg <reg1> <reg2>...
```