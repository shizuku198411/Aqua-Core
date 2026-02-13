# RTC (Google Goldfish) / `gettime` Syscall

対象:

- `src/include/rtc.h`
- `src/kernel/rtc/rtc.c`
- `src/kernel/trap/syscall_time.c`
- `src/include/syscall.h`
- `src/user/runtime/user_syscall.c`
- `src/user/apps/shell/commands_sys.c`
- `src/lib/commonlibs.c`
- `src/kernel/kernel.c`
- `src/kernel/proc/process.c`

関連:

- [Kernel Bootstrap](./kernel-bootstrap.md)
- [Syscall](./syscall.md)
- [VFS / RAMFS / VirtIO Block Storage](./vfs.md)

## 概要

AquaCore では QEMU `virt` マシン上の Google Goldfish RTC を利用して、
現在時刻を取得できるようにしています。

実装済み機能:

- カーネル内での RTC 初期化 (`rtc_init`)
- 現在時刻の取得（ns / sec）
- `gettime` syscall (`SYSCALL_GETTIME = 20`)
- shell の `date` コマンド表示
- 2038年問題対策として、`time_spec` は 64-bit 秒相当を返却

## MMIOアドレスとマッピング

`src/include/rtc.h`:

- `GOLDFISH_RTC_BASE = 0x00101000`
- `RTC_MMIO_BASE = 0x00101000`
- `RTC_MMIO_END  = 0x00102000`

`src/kernel/proc/process.c` の `create_process()` で
`RTC_MMIO_BASE..RTC_MMIO_END` を各プロセス page table に map しています。

これにより、ユーザ由来の syscall 処理中でも RTC MMIO へ安全にアクセスできます。

## ドライバ実装

実装ファイル: `src/kernel/rtc/rtc.c`

公開 API:

- `int rtc_init(void)`
- `uint64_t rtc_now_ns(void)`
- `uint32_t rtc_now_sec(void)`

レジスタ:

- `RTC_TIME_LOW  = 0x00`
- `RTC_TIME_HIGH = 0x04`

64-bit 時刻読取は `hi -> lo -> hi` 再読込で整合性を確保しています。

## `gettime` syscall

- syscall 番号: `SYSCALL_GETTIME` (`src/include/syscall.h`)
- ディスパッチ: `src/kernel/trap/syscall_handler.c`
- ハンドラ: `src/kernel/trap/syscall_time.c`

`syscall_time.c` は以下を実施します。

1. `rtc_now_ns()` を取得
2. 1e9 で割って `sec` と `nsec` を計算
3. `struct time_spec` としてユーザへコピー（`sstatus.SUM` 一時有効化）

## 時刻構造体（64-bit秒対応）

`src/include/rtc.h`:

```c
struct time_spec {
    uint32_t sec_lo;
    uint32_t sec_hi;
    uint32_t nsec;
};
```

- `sec = ((uint64_t)sec_hi << 32) | sec_lo`
- 32-bit `time_t` 相当の上限を超えても扱えるようにしています。

## shell `date` コマンド

`src/user/apps/shell/commands_sys.c`:

- `gettime(&info)` で時刻取得
- `sec_hi/sec_lo` を 64-bit 秒へ再構築
- `unix_time_to_utc_str()` で UTC 文字列化して表示

表示例:

```text
aqua-core:$ date
Fri Feb 13 06:49:58 UTC 2026
```

## 文字列変換 (`commonlibs`)

`src/lib/commonlibs.c` の `unix_time_to_utc_str()` は 64-bit 秒対応です。

- シグネチャ: `int unix_time_to_utc_str(uint64_t unix_sec, char *out, size_t out_size)`
- freestanding 環境向けに 64-bit 除算/剰余を自前実装
  - `__udivdi3` / `__umoddi3` への依存を避ける設計

## ブート表示との連携

`src/kernel/kernel.c`:

- `kernel_bootstrap()` で `rtc_init()` を実行
- ブート時刻を UTC 文字列に変換して `kernel information` に表示
- `kernel_get_info()` でも `time` フィールドを埋め、
  shell `kernel_info` コマンドで表示できるようにしています。

## 今後の拡張候補

- タイムゾーン対応（現在は UTC 固定）
- `settime` syscall（時刻設定）
- `clock_gettime` 互換 API への整理（`CLOCK_REALTIME` / `CLOCK_MONOTONIC` 分離）
