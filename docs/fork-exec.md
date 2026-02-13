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
- FD継承/close-on-exec の詳細は未実装（Issue 4以降）
