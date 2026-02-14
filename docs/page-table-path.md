# Page Table Mapping Path (SV32)

対象コード:

- `src/kernel/mm/memory.c`
- `src/kernel/proc/process.c`
- `src/include/memory.h`
- `src/include/kernel.h`

関連:

- [SV32 Paging](./sv32.md)
- [Memory Map](./memory-map.md)
- [Memory / Process](./memory-process.md)
- [Troubleshoot: Page Table Walk GDB手順](./troubleshoot/page_table_walk_gdb.md)

## 1. 概要

AquaCore は Sv32 の 2 段ページテーブルを使います。

- ページサイズ: `4096 (0x1000)` bytes
- 1 テーブル: `1024` entries (`4096 / 4`)
- 各 `process` が `page_table` (L1 root) を 1 つ持つ

`struct process` 上の対応:

- `page_table`: L1 root テーブル物理アドレス
- `user_pages`: `USER_BASE` から何ページ張られているか

ページテーブル自体の物理ページは `alloc_pages()` で確保されるため、固定アドレスではなく allocator 管理領域内に配置されます。

## 2. 仮想アドレスの分解

Sv32 の VA 分解:

```text
31                      22 21                      12 11            0
+------------------------+--------------------------+----------------+
|        VPN[1]          |         VPN[0]           |    OFFSET      |
+------------------------+--------------------------+----------------+
          10bit                     10bit                12bit
```

コード上の計算 (`map_page` と同じ式):

```c
vpn1 = (vaddr >> 22) & 0x3ff;
vpn0 = (vaddr >> 12) & 0x3ff;
off  =  vaddr        & 0xfff;
```

## 3. PTE 形式

```text
31                             10 9           5 4 3 2 1 0
+--------------------------------+-------------+-+-+-+-+-+
|              PPN               |   reserved  |U|X|W|R|V|
+--------------------------------+-------------+-+-+-+-+-+
```

- `PPN = PTE[31:10]`
- `PA base = PPN << 12`
- フラグは `PAGE_V/R/W/X/U` (`src/include/memory.h`)

## 4. カーネル/ユーザ/MMIO のマップ

`create_process()` で各プロセスのページテーブルを作るとき、次を張ります。

1. カーネル領域 identity map (`VA == PA`)
2. MMIO identity map (`MMIO_BASE..MMIO_END`)
3. RTC MMIO identity map (`RTC_MMIO_BASE..RTC_MMIO_END`)
4. ユーザイメージを `USER_BASE + off` に map (`PAGE_U` 付き)

主要定数:

- `KERNEL_BASE = 0x80200000`
- `USER_BASE   = 0x01000000`
- `MMIO_BASE   = 0x10000000`
- `MMIO_END    = 0x10010000`
- `RTC_MMIO_BASE = 0x00101000`
- `RTC_MMIO_END  = 0x00102000`

イメージ:

```text
VA space (per process)

0xFFFF_FFFF
    ...
0x8020_0000+------------------------------+  kernel text/data/heap (identity map)
           | S-mode only (U=0)            |
           +------------------------------+
0x1000_0000+------------------------------+  virtio MMIO (identity map)
           | S-mode only (U=0)            |
           +------------------------------+
0x0100_0000+------------------------------+  user image (.text/.data/.bss)
           | U-mode accessible (U=1)      |
           +------------------------------+
0x0010_1000+------------------------------+  goldfish RTC MMIO (identity map)
           | S-mode only (U=0)            |
           +------------------------------+
0x0000_0000
```

## 5. テーブル生成の流れ (`map_page`)

`map_page(table1, vaddr, paddr, flags)` は次の順で動きます。

```text
L1 root table (table1, 1024 entries)
   index = vpn1
      |
      | if invalid:
      |   alloc_pages(1) で L0 table を新規確保
      v
L0 table (table0, 1024 entries)
   index = vpn0
      |
      v
leaf PTE = ((paddr >> 12) << 10) | flags | V
```

実コード相当:

```c
if ((table1[vpn1] & PAGE_V) == 0) {
    pt_paddr = alloc_pages(1);
    table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
}
table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
```

## 6. CPU がアクセス時に辿る経路

### 6.1 `satp` から root を得る

スケジューラで `yield()` 時に次プロセスへ切替:

```c
csrw satp, SATP_SV32 | ((uint32_t)next->page_table / PAGE_SIZE)
sfence.vma
```

`satp.PPN` が L1 root table の物理ページを指します。

### 6.2 ハードウェアページウォーク

```text
CPU issues VA
   |
   +--> split VA -> vpn1/vpn0/off
   |
   +--> read satp.PPN -> pt1 base (PA)
   |
   +--> pte1 = pt1[vpn1]
   |      (V=0なら fault)
   |
   +--> pt0 base = (pte1.PPN << 12)
   |
   +--> pte0 = pt0[vpn0]
   |      (V=0 or perm NGなら fault)
   |
   +--> PA = (pte0.PPN << 12) | off
   |
   +--> memory/MMIO bus access
```

式で書くと:

```text
pt1_base = (satp & 0x003fffff) << 12
pte1     = *(uint32_t *)(pt1_base + 4*vpn1)
pt0_base = (pte1 >> 10) << 12
pte0     = *(uint32_t *)(pt0_base + 4*vpn0)
pa       = ((pte0 >> 10) << 12) | off
```

## 7. 具体例

`VA = USER_BASE = 0x01000000` の場合:

- `vpn1 = (0x01000000 >> 22) & 0x3ff = 0x4`
- `vpn0 = (0x01000000 >> 12) & 0x3ff = 0x0`
- `off  = 0x000`

つまり、L1 の index 4 -> L0 の index 0 を辿って最初のユーザページに到達します。

## 8. fork/exec で何が変わるか

### `fork`

- 子の `page_table` を新規作成
- kernel/MMIO/RTC map を再作成
- 親の `USER_BASE + i*PAGE_SIZE` を走査して、ページ内容を子へコピーして map
- 親子で `satp` を切り替えれば独立空間として動作

### `exec`

- 現在プロセスのユーザページだけ解放 (`page_table` 自体は維持)
- 新イメージを `USER_BASE` から再 map
- `sepc = USER_BASE` にして新イメージ先頭へ復帰

## 9. どこを見れば追跡しやすいか

- マップ生成: `src/kernel/mm/memory.c` `map_page()`
- 初回プロセス構築: `src/kernel/proc/process.c` `create_process()`
- `fork` の複製: `src/kernel/proc/process.c` `process_fork()`
- `exec` の張り直し: `src/kernel/proc/process.c` `process_exec()`
- `satp` 切替: `src/kernel/proc/process.c` `yield()`
