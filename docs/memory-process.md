# Memory / Process 管理

対象:

- `src/kernel/mm/memory.c`
- `src/kernel/proc/process.c`
- `src/include/memory.h`
- `src/include/process.h`

## 1. bitmap allocator

`memory_init()` で `__free_ram..__free_ram_end` を初期化します。

- 1bit = 1page
- `0 = free`, `1 = used`
- bitmap 本体は free 領域先頭に配置
- 残りページを `managed_base..` として管理

### alloc/free

- `alloc_pages(n)`
  - first-fit で連続 `n` ページを確保
  - 確保ページを `memset(..., 0, n * PAGE_SIZE)`
- `free_pages(paddr, n)`
  - 範囲/整列/二重解放をチェックして解放

### debug API

- `bitmap_page_state(index)` -> `0/1` (`-1` は範囲外)
- `bitmap_page_count()`

## 2. プロセス生成

`create_process(image, size)`:

1. `reap_exited_processes()` で終了プロセスを先に回収
2. `PROC_UNUSED` スロットを確保
3. 初期カーネルスタック作成 (`ra=user_entry`)
4. top-level page table 生成
5. カーネル領域を恒等マップ
6. ユーザイメージを `USER_BASE` にページマップ
7. `PROC_RUNNABLE` 化

## 3. プロセス終了と回収

- `syscall_exit` は `current_proc->state = PROC_EXITED` のみ設定し、`yield()`
- 実際の解放は `reap_exited_processes()` が担当

`free_process_memory(proc)` で以下を `free_pages()`:

- user pages
- 2nd-level page tables
- top-level page table

その後スロットを `PROC_UNUSED` に戻します。

## 4. スケジューリング

`yield()`:

- 先頭で `reap_exited_processes()`
- ラウンドロビンで runnable を探索
- runnable 不在時は `sscratch` を同期して `wfi`
- 実行先決定後に `satp`/`sscratch` 更新して `switch_context`

## 5. 入力待機

`getchar` が入力待ち時に `PROC_WAITTING + PROC_WAIT_CONSOLE_INPUT` へ遷移します。

- timer 割り込みごとに `poll_console_input()` が入力をバッファ化
- 入力があれば `wakeup_input_waiters()` が入力待ちプロセスのみ起床

`WAITTING` 全体を起こさないため、他種の待機理由と干渉しにくい構成です。
