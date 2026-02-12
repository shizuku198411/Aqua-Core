# Linker Script

対象: `src/kernel/kernel.ld`

## 目的

`kernel.ld` はカーネル ELF の配置とランタイムシンボルを定義します。

## 主な定義

```ld
ENTRY(boot)

SECTIONS {
    . = 0x80200000;
    __kernel_base = .;

    .text : {
        KEEP(*(.text.boot));
        *(.text .text.*);
    }

    .rodata : ALIGN(4) { *(.rodata .rodata.*); }
    .data   : ALIGN(4) { *(.data .data.*); }

    .bss : ALIGN(4) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }

    . = ALIGN(4);
    . += 128 * 1024;
    __stack_top = .;

    . = ALIGN(4096);
    __free_ram = .;
    . += 64 * 1024 * 1024;
    __free_ram_end = .;
}
```

## 実装での使い道

- `__bss`, `__bss_end`
  - `src/kernel/kernel.c` で `.bss` をゼロクリア
- `__stack_top`
  - `boot()` の初期スタック
- `__free_ram`, `__free_ram_end`
  - `src/kernel/mm/memory.c` の bitmap allocator 管理範囲
- `__kernel_base`
  - `src/kernel/proc/process.c` のカーネル領域恒等マップ

関連:

- [Kernel Bootstrap](./kernel-bootstrap.md)
- [Memory / Process](./memory-process.md)
