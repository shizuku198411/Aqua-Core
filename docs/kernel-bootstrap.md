# Kernel Bootstrap

対象:

- `src/kernel/kernel.c`
- `src/kernel/time/timer.c`

関連:

- [Trap Handler](./trap-handler.md)
- [Mode Transition](./mode-transition.md)

## 起動シーケンス

1. `boot()` (`.text.boot`) で `sp = __stack_top`
2. `sscratch = sp` を初期化
3. `kernel_main()` へジャンプ
4. `kernel_bootstrap()` で以下を実行
   - `.bss` クリア
   - `memory_init()`
   - `stvec = kernel_entry`
   - `sscratch` を現在 `sp` で再設定
   - `enable_timer_interrupt()`
   - `timer_set_next()`
   - idle プロセス作成 (`pid=0`)
   - kernel 基本情報を表示
5. `banner()` 表示
6. shell(init) プロセス作成
7. `yield()` 開始

主要コード:

```c
memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);
memory_init();
WRITE_CSR(stvec, (uint32_t) kernel_entry);

uint32_t kernel_sp;
__asm__ __volatile__("mv %0, sp" : "=r"(kernel_sp));
WRITE_CSR(sscratch, kernel_sp);

enable_timer_interrupt();
timer_set_next();

idle_proc = create_process(NULL, 0, "idle");
idle_proc->pid = 0;
current_proc = idle_proc;

init_proc = create_process(_binary___bin_shell_bin_start,
                           (size_t)_binary___bin_shell_bin_size,
                           "shell");
yield();
```

## `kernel_entry` の役割

`kernel_entry` は trap 共通入口です。

- U-mode 由来トラップ時のみ `csrrw sp, sscratch, sp` でスタック交換
- S-mode 由来トラップ時は現在のカーネル `sp` を維持
- 汎用レジスタを `trap_frame` 形式で保存
- `handle_trap()` 呼び出し
- 復元して `sret`

`sscratch` の運用詳細は [Mode Transition](./mode-transition.md) を参照してください。

## 起動時に表示される情報

`kernel_bootstrap()` は以下を表示する。

- `version`
- `total pages`
- `page size`
- `kernel base`
- `user base`
- `proc max`
- `kernel stack bytes/proc`
- `time slice ticks`
- `timer interval (ms)`
