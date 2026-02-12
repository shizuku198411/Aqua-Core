# Kernel Bootstrap

対象:

- `src/kernel/kernel.c`
- `src/kernel/time/timer.c`

## 起動シーケンス

1. `boot()` (`.text.boot`) で `sp = __stack_top`
2. `kernel_main()` へジャンプ
3. `kernel_main()` で以下を実行
   - `.bss` クリア
   - `memory_init()`
   - `stvec = kernel_entry`
   - `enable_timer_interrupt()`
   - `timer_set_next()`
   - banner 表示
   - idle プロセス作成 (`pid=0`)
   - shell プロセス作成
   - `yield()` 開始

主要コード:

```c
memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);
memory_init();
WRITE_CSR(stvec, (uint32_t) kernel_entry);
enable_timer_interrupt();
timer_set_next();

idle_proc = create_process(NULL, 0);
idle_proc->pid = 0;
current_proc = idle_proc;

create_process(_binary___bin_shell_bin_start,
               (size_t)_binary___bin_shell_bin_size);
yield();
```

## `kernel_entry` の役割

`kernel_entry` は trap 共通入口です。

- `csrrw sp, sscratch, sp` で S/U のスタックを交換
- 汎用レジスタを `trap_frame` 形式で保存
- `handle_trap()` 呼び出し
- 復元して `sret`

trap 入口が `sscratch` 前提なので、スケジューラ側で `sscratch` を次プロセスのカーネルスタック top に更新しています。

## timer 初期化

`enable_timer_interrupt()`:

- `sie.STIE` をセット
- `sstatus.SIE` をセット

`timer_set_next()`:

- `rdtime` 取得
- `stimecmp = now + TIMER_INTERVAL`
