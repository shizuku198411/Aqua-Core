# Kernel Memory Map (ASCII)

対象:

- `src/kernel/kernel.ld`
- `src/include/kernel.h`
- `src/include/rtc.h`
- `src/kernel/fs/blockdev_virtio.c`
- `src/kernel/proc/process.c`

関連:

- [SV32 Paging](./sv32.md)
- [VFS / RAMFS / VirtIO Block Storage](./vfs.md)
- [RTC / Time Syscall](./rtc.md)

## 概要

AquaCore は QEMU `virt` (RV32) を前提に、
カーネル空間をほぼ identity map しつつ、`USER_BASE` にユーザ空間をマップします。

- `KERNEL_BASE = 0x80200000`
- `USER_BASE   = 0x01000000`
- MMIO(virtio用): `0x10000000 .. 0x1000ffff`
- RTC MMIO: `0x00101000 .. 0x00101fff`

## 物理アドレス空間（Not to Scale）

```text
  0xffffffff +----------------------------------------------+
             |                                              |
             |                (unused / others)             |
             |                                              |
  0x88000000 +----------------------------------------------+
             |              DRAM (QEMU virt, end)           |
             |                                              |
  0x80400000 +----------------------------------------------+
             |  __free_ram .. __free_ram_end (64 MiB pool)  |
             |  - bitmap allocator managed pages            |
             |  - process page tables / user pages          |
  0x80200000 +----------------------------------------------+
             |  Kernel image base (KERNEL_BASE)             |
             |  .text/.rodata/.data/.bss + boot stack area  |
  0x80000000 +----------------------------------------------+
             |  DRAM start (QEMU virt)                      |
             +----------------------------------------------+

  0x10010000 +----------------------------------------------+
             |  MMIO window end (MMIO_END)                  |
  0x10008000 +----------------------------------------------+
             |  virtio-mmio device #7                       |
  0x10007000 +----------------------------------------------+
             |  virtio-mmio device #6                       |
  0x10006000 +----------------------------------------------+
             |  virtio-mmio device #5                       |
  0x10005000 +----------------------------------------------+
             |  virtio-mmio device #4                       |
  0x10004000 +----------------------------------------------+
             |  virtio-mmio device #3                       |
  0x10003000 +----------------------------------------------+
             |  virtio-mmio device #2                       |
  0x10002000 +----------------------------------------------+
             |  virtio-mmio device #1                       |
  0x10001000 +----------------------------------------------+
             |  virtio-mmio device #0 (blk uses here)       |
  0x10000000 +----------------------------------------------+
             |  UART8250 MMIO                               |
             +----------------------------------------------+

  0x00102000 +----------------------------------------------+
             |  RTC MMIO end (RTC_MMIO_END)                 |
  0x00101000 +----------------------------------------------+
             |  Google Goldfish RTC MMIO                    |
             |  - TIME_LOW  @ +0x00                         |
             |  - TIME_HIGH @ +0x04                         |
             +----------------------------------------------+
```

## カーネルリンク配置 (`kernel.ld`)

`src/kernel/kernel.ld` の要点:

- 開始: `. = 0x80200000`
- `__kernel_base` を先頭に設定
- `.text/.rodata/.data/.bss` を配置
- その後に 128KiB のブート/カーネルスタック領域
- 4KiB align して `__free_ram` を作成
- `__free_ram` から 64MiB を allocator 対象 (`__free_ram_end`)

## SV32 マッピング観点

`src/kernel/proc/process.c` の `create_process()` では以下を map:

1. カーネル領域: `__kernel_base .. __free_ram_end` を identity map
2. MMIO領域(virtio/UART想定): `MMIO_BASE .. MMIO_END`
3. RTC領域: `RTC_MMIO_BASE .. RTC_MMIO_END`
4. ユーザイメージ: `USER_BASE + off` に alloc page を map

これにより、syscall 文脈（プロセス page table 利用中）でも
virtio と RTC MMIO へアクセスできます。

## ページテーブル権限ビット（SV32 / AquaCore実装）

`src/include/memory.h` で定義しているビット:

```text
bit 0 : PAGE_V  (Valid)
bit 1 : PAGE_R  (Readable)
bit 2 : PAGE_W  (Writable)
bit 3 : PAGE_X  (Executable)
bit 4 : PAGE_U  (User-accessible)
```

PTE(下位ビット)のイメージ:

```text
 ... PPN ... | U | X | W | R | V
             |4  |3  |2  |1  |0
```

主な利用パターン（`create_process()`）:

- カーネル通常領域: `PAGE_R | PAGE_W | PAGE_X`
- MMIO領域(virtio/UART): `PAGE_R | PAGE_W`
- RTC領域: `PAGE_R | PAGE_W`
- ユーザ領域: `PAGE_U | PAGE_R | PAGE_W | PAGE_X`

補足:

- `map_page()` は渡された `flags` に加えて常に `PAGE_V` を立てます。
- `PAGE_U` が無いページは U-Mode からアクセス不可です。

## ユーザ空間アドレス

```text
USER_BASE = 0x01000000

0x01000000 +------------------------------+
           | user .text/.data/.bss image  |
           | (create_processでページ割当)  |
           +------------------------------+
```

## メモ

- 本図は AquaCore 実装定数と QEMU `virt` の標準レイアウトを基準にした概要図です。
- 厳密な runtime 配置は `kernel.map` と実際の DTB を併せて確認してください。
