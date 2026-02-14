# Fork / Exec (Minimal Implementation)

対象:

- `src/include/process.h`
- `src/kernel/proc/process.c`
- `src/kernel/trap/syscall_process.c`
- `src/kernel/trap/trap_handler.c`
- `src/user/runtime/user_syscall.c`
- `src/user/apps/shell/main.c`

関連:

- [Process Management](./process-management.md)
- [Syscall](./syscall.md)
- [Mode Transition](./mode-transition.md)

## 概要

- `fork()`
  - 親: 子PIDを返す
  - 子: `0` を返す
- `exec(app_id)`
  - 現在プロセスのユーザ空間を新イメージへ置換
  - PIDは維持
- `execv(app_id, argv)`
  - `exec` と同様にイメージ置換
  - 追加で引数をカーネル経由で次イメージへ受け渡す

## 1. fork の実装

### 1.1 syscall入口

`syscall_handle_fork()` で `process_fork(f)` を呼び、戻り値を `a0` へ返却する。

```c
int child_pid = process_fork(f);
f->a0 = (child_pid < 0) ? -1 : child_pid;
```

### 1.2 子プロセススロット確保

`alloc_proc_slot()` が `PROC_UNUSED` を探索し、`pid=index` で確保する。

### 1.3 ユーザ空間複製

`process_fork()` では次を実施する。

1. 子用 page table 作成
2. kernel/MMIO/RTC を map
3. 親の `user_pages` 分をページ単位でコピー
   - 親PTEを走査して物理ページ取得
   - 子ページを `alloc_pages(1)` で確保
   - `memcpy` で4KiBコピー
   - 同一flagsで `map_page`

失敗時は `recycle_process_slot(child)` で回収する。

### 1.4 子の trap 復帰文脈

`fork_child_trap_return()` を用意し、子カーネルスタック上に:

- `trap_frame`（親からコピー）
- `switch_context` 用保存領域（`sstatus`, `ra`, `s0..s11`）

を構築する。

ポイント:

- 子 `trap_frame.a0 = 0`
- `s11 = parent sepc + 4`（`ecall` の次命令）
- `ra = fork_child_trap_return`
- `s0 = child trap_frame pointer`

これにより、子は `sret` で親と同じ実行位置へ戻り、`fork()` 戻り値だけ `0` になる。

## 2. exec の実装

### 2.1 syscall入口

`syscall_handle_exec()` は `app_id -> image/name` を解決し、`process_exec(image, size, name)` を呼ぶ。

`syscall_handle_execv()` は `exec` に加えて `argv` を受け取り、`process_exec(image, size, name, argc, argv_copy)` を呼ぶ。

### 2.2 ユーザ空間置換

`process_exec()` では:

1. 既存ユーザページを解放（page tableは維持）
2. 新イメージを `USER_BASE` へ再配置
3. `user_pages`, `name`, `wait_reason`, `wait_pid`, `time_slice`, `run_ticks` を更新

### 2.3 ecall復帰PCの扱い

`exec` 成功時は旧コードへ戻らない必要がある。  
そのため `trap_handler` の `ecall` 処理で:

- 通常: `user_pc += 4`
- `SYSCALL_EXEC` 成功時: `user_pc = USER_BASE`

として復帰先を切り替える。

`execv` も同様に成功時は `user_pc = USER_BASE` へ切り替える。

### 2.4 execv の引数受け渡し

現在実装は「ユーザスタック直構築」ではなく、`process` 構造体へ引数を保持する方式。

流れ:

1. user `execv(app_id, argv)`  
  - syscall ABI で `a0=app_id`, `a1=argv_ptr`, `a3=SYSCALL_EXECV`
2. kernel `syscall_handle_execv`  
  - `SSTATUS_SUM` を有効化してユーザ `argv` を読み取り
  - `copy_user_argv()` で固定長バッファへコピー
3. `process_exec(..., argc, argv_copy)`  
  - `current_proc->exec_argc/exec_argv` を更新
4. 新イメージ起動後、ユーザランタイムが `getargs()` を呼び `main(argc, argv)` へ渡す

制約:

- 引数最大数: `PROC_EXEC_ARGV_MAX`（現状 8）
- 1引数最大長: `PROC_EXEC_ARG_LEN-1`（現状 31 文字）

補足:

- 詳細は [Execv Argument Passing](./execv-args.md) を参照。

## 3. テスト

### 3.1 fork_test（DEBUG）

shell の `fork_test` で親子戻り値と `waitpid` を確認。

- 親: `child pid > 0`
- 子: `fork() returned 0`
- 親: `waitpid(child) == child`

### 3.2 exec_test（DEBUG）

子で `exec(APP_ID_IPC_RX)` を実行し、`ipc_rx` へ置換されることを確認。

確認結果:

- `ps` で `APP=ipc_rx`
- `ipc_send` / `ipc_rx` 受信が成立

## 4. 現在の制約（最小実装）

- `fork` は COW ではなくページ全コピー
- `exec` は `app_id` 指定のみ（path指定なし）
- `execv` 引数は固定長配列経由（POSIX の argv/envp スタック配置ではない）
- FD継承/close-on-exec の詳細は未実装（Issue 4以降）
