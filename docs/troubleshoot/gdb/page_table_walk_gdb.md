# Page Table Walk GDB手順

## 概要
Sv32 のページテーブルを GDB で手動ウォークし、`VA -> PA` 変換結果と PTE フラグを確認する手順。  
`Illegal Instruction` / `Page Fault` / MMIOアクセス不良の切り分けに使います。

## 発生事象
以下のような症状のときに有効です。

- `PANIC: ... Illegal Instruction ... sepc=...`
- `PANIC: ... Load/Store page fault ... stval=...`
- ユーザアプリの `exec/fork` 後に不正動作
- MMIO (`virtio` / `RTC`) アクセス時の異常

## 調査
### 1. QEMU を GDB待ちで起動

```bash
qemu-system-riscv32 \
  -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
  -S -s \
  -kernel ./bin/kernel.elf
```

### 2. gdb-multiarch 接続

```gdb
gdb-multiarch ./bin/kernel.elf
(gdb) set pagination off
(gdb) set confirm off
(gdb) set architecture riscv:rv32
(gdb) target remote :1234
```

必要ならユーザアプリシンボルも追加:

```gdb
(gdb) add-symbol-file ./bin/shell.elf 0x01000000
```

## 原因特定に至った観測結果
ここでは「`stval` で示された仮想アドレスを、実際の PTE で追う」ことで原因位置を確定します。

### 1. まず trap レジスタを確認

```gdb
(gdb) info reg scause stval sepc satp
```

- `stval`: fault対象 VA（命令/データ）
- `sepc`: 例外発生 PC
- `satp`: その時点の root page table

### 2. `satp` から L1 root 物理アドレスを計算

```gdb
(gdb) p/x $satp
(gdb) set $pt1 = ($satp & 0x003fffff) << 12
(gdb) p/x $pt1
```

### 3. 対象 VA を VPN 分解

例として `stval` を対象にする:

```gdb
(gdb) set $va = $stval
(gdb) set $vpn1 = ($va >> 22) & 0x3ff
(gdb) set $vpn0 = ($va >> 12) & 0x3ff
(gdb) set $off  = $va & 0xfff
(gdb) p/x $vpn1
(gdb) p/x $vpn0
```

### 4. L1/L0 PTE を読む

```gdb
(gdb) x/wx $pt1 + 4*$vpn1
(gdb) set $pte1 = *(unsigned int *)($pt1 + 4*$vpn1)
(gdb) p/x $pte1

(gdb) set $pt0 = (($pte1 >> 10) << 12)
(gdb) x/wx $pt0 + 4*$vpn0
(gdb) set $pte0 = *(unsigned int *)($pt0 + 4*$vpn0)
(gdb) p/x $pte0
```

### 5. 最終 PA とフラグ確認

```gdb
(gdb) set $pa = (($pte0 >> 10) << 12) | $off
(gdb) p/x $pa

(gdb) p $pte0 & 1          # V
(gdb) p ($pte0 >> 1) & 1   # R
(gdb) p ($pte0 >> 2) & 1   # W
(gdb) p ($pte0 >> 3) & 1   # X
(gdb) p ($pte0 >> 4) & 1   # U
```

## 根本原因
よくある根本原因は次です。

1. `V=0` で該当ページが未マップ
2. `U=0` なのに U-mode からアクセス
3. 命令フェッチ先で `X=0`
4. `exec/fork` で user map が想定と不一致
5. `satp` 切替先 (`next->page_table`) が想定外

## 対策
実装上の確認ポイント:

- `map_page()`:
  - `vpn1/vpn0` 計算
  - `table1[vpn1]` が無効なら L0 を確保できているか
- `create_process()`:
  - `USER_BASE + off` に `PAGE_U|PAGE_R|PAGE_W|PAGE_X` で map
  - MMIO/RTC map があるか
- `yield()`:
  - `csrw satp, SATP_SV32 | (page_table/4096)` の更新先が正しいか
  - `sfence.vma` が実行されているか
- `process_exec()`:
  - 旧ユーザページ解放後に新イメージを再 map しているか
  - `sepc=USER_BASE` へ戻しているか

## 結果
上記手順で、fault時点の `VA -> PTE -> PA` を再現できます。  
特に `stval` 起点で追うと、未マップ・権限不一致・誤 `satp` を短時間で切り分けできます。

## GDBコマンド一覧（コピペ用）

```gdb
set pagination off
set confirm off
set architecture riscv:rv32
target remote :1234
info reg scause stval sepc satp
set $va = $stval
set $pt1 = ($satp & 0x003fffff) << 12
set $vpn1 = ($va >> 22) & 0x3ff
set $vpn0 = ($va >> 12) & 0x3ff
set $off  = $va & 0xfff
set $pte1 = *(unsigned int *)($pt1 + 4*$vpn1)
set $pt0  = (($pte1 >> 10) << 12)
set $pte0 = *(unsigned int *)($pt0 + 4*$vpn0)
set $pa   = (($pte0 >> 10) << 12) | $off
p/x $pte1
p/x $pte0
p/x $pa
```

## 使い方例
例: `PANIC: ... stval=01000000` のとき

```gdb
(gdb) info reg stval satp
stval 0x01000000
satp  0x800802c2

(gdb) set $va=0x01000000
(gdb) set $pt1=($satp & 0x003fffff) << 12
(gdb) set $vpn1=($va >> 22) & 0x3ff
(gdb) set $vpn0=($va >> 12) & 0x3ff
(gdb) set $pte1=*(unsigned int*)($pt1 + 4*$vpn1)
(gdb) set $pt0=(($pte1 >> 10) << 12)
(gdb) set $pte0=*(unsigned int*)($pt0 + 4*$vpn0)
(gdb) p/x $pte0
```

`$pte0` が `0x0` なら未マップ、`X=0` なら命令フェッチ不可、と判断できます。

