# Memory / Process 管理

対象:

- `src/kernel/mm/memory.c`
- `src/kernel/proc/process.c`
- `src/include/memory.h`
- `src/include/process.h`

関連:

- [Trap Handler](./trap-handler.md)
- [Syscall](./syscall.md)
- [SV32 Paging](./sv32.md)
- [Process Management](./process-management.md)

## 1. bitmap allocator

`memory_init()` で `__free_ram..__free_ram_end` を初期化します。

- 1bit = 1page
- `0 = free`, `1 = used`
- bitmap 本体は free 領域先頭に配置
- 残りを `managed_base..` として管理

`alloc_pages(n)` は first-fit で連続確保、`free_pages(paddr,n)` は範囲/整列/二重解放チェック付きです。

SV32 の VPN 計算や PTE 形式は [SV32 Paging](./sv32.md) を参照してください。

## 2. プロセス生成/終了

`create_process(image, size, name)`:

1. `reap_exited_processes()`
2. `PROC_UNUSED` スロット確保
3. 初期カーネルスタック作成 (`ra=user_entry`)
4. page table 作成、カーネル恒等マップ
5. ユーザイメージを `USER_BASE` にマップ

`exit` は `PROC_EXITED` 化のみ行い、回収は以下で行います。

- 親なし: `reap_exited_processes()`
- 親あり: `waitpid()` が回収

`kill` の詳細挙動（即時回収/自己kill例外）は [Process Management](./process-management.md) を参照してください。

## 3. タイムスライス付き RR スケジューラ

- 各プロセスは `time_slice` を持つ (`SCHED_TIME_SLICE_TICKS`)
- timer tick ごとに `scheduler_on_timer_tick()` が slice を減算
- slice 0 で `need_resched=true`
- trap 側で `scheduler_should_yield()` を見て `yield()`

## 4. context switch 図 (text)

```text
yield()
  -> runnable scan (round-robin)
    -> if none: wfi
    -> if same proc: slice reload if needed, continue
    -> if next proc:
         satp <- next->page_table
         sscratch <- next kernel stack top
         switch_context(prev, next)
           save prev callee-saved regs + sstatus + sp
           load next callee-saved regs + sstatus + sp
           ret
```

## 5. 待機理由と wakeup

- `PROC_WAIT_CONSOLE_INPUT`: `getchar` 待機
- `PROC_WAIT_CHILD_EXIT`: `waitpid` 待機
- `PROC_WAIT_IPC_RECV`: `ipc_recv` 待機

入力到着、子終了、IPC着信でそれぞれ対象プロセスのみを `RUNNABLE` に戻します。

## 6. IPC mailbox

- 各プロセスに単一スロット (`ipc_has_message`, `ipc_from_pid`, `ipc_message`)
- `ipc_send`: 宛先 mailbox が空なら格納、満杯なら `-2`
- `ipc_recv`: 空なら `PROC_WAIT_IPC_RECV` でブロック、受信後にスロットをクリア
