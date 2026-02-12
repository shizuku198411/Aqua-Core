# Process Management

対象:

- `src/include/process.h`
- `src/kernel/proc/process.c`
- `src/kernel/trap/syscall_process.c`
- `src/kernel/trap/trap_handler.c`
- `src/kernel/time/timer.c`

関連:

- [Memory / Process](./memory-process.md)
- [Syscall](./syscall.md)
- [Mode Transition](./mode-transition.md)

## 1. プロセス構造体

`struct process` (`src/include/process.h`) は以下の責務を持つ。

- 識別情報:
  - `pid`
  - `name[PROC_NAME_MAX]`
  - `parent_pid`
- スケジューリング情報:
  - `state` (`PROC_UNUSED`, `PROC_RUNNABLE`, `PROC_WAITTING`, `PROC_EXITED`)
  - `wait_reason` (`NONE`, `CONSOLE_INPUT`, `CHILD_EXIT`, `IPC_RECV`)
  - `wait_pid`
  - `time_slice`, `run_ticks`, `schedule_count`
- メモリ/実行文脈:
  - `page_table`
  - `user_pages`
  - `sp` (context switch 用保存SP)
  - `stack[8192]` (カーネルスタック)
- IPC mailbox:
  - `ipc_has_message`, `ipc_from_pid`, `ipc_message`

## 2. PIDとスロット

実体は固定配列 `procs[PROCS_MAX]`。  
現在実装では `create_process()` で確保したスロット index をそのまま `pid` に採用している。

- `pid == 0`: idle 用に利用
- `pid > 0`: ユーザプロセス

プロセス探索は index 直参照ではなく `find_process_by_pid()` を利用する。

## 3. 作成フロー (`create_process`)

`create_process(image, image_size, name)` の流れ:

1. `reap_exited_processes()` で回収可能プロセスを先に清掃
2. `PROC_UNUSED` スロット探索
3. カーネルスタック初期化
   - 復帰先 `ra = user_entry`
   - 初期 `sstatus = 0`（カーネル文脈中の割り込みを抑制）
4. 1段目ページテーブル確保
5. カーネル領域を恒等マップ
6. ユーザイメージを `USER_BASE` へページ単位で配置
7. `state=PROC_RUNNABLE` と各種メタ情報（`name`, IPC, slice など）初期化

`kernel_main()` では:

- `idle_proc = create_process(NULL, 0, "idle")`
- `init_proc = create_process(shell_image, shell_size, "shell")`

## 4. 状態遷移の基本

主な遷移:

- `PROC_RUNNABLE -> PROC_WAITTING`
  - `getchar` 待機
  - `waitpid` 待機
  - `ipc_recv` 待機
- `PROC_WAITTING -> PROC_RUNNABLE`
  - 入力到着 (`wakeup_input_waiters`)
  - 子終了通知 (`notify_child_exit`)
  - IPC着信
- `PROC_RUNNABLE/WAITTING -> PROC_EXITED`
  - `exit` syscall
  - `kill` syscall
- `PROC_EXITED -> PROC_UNUSED`
  - 回収 (`reap_exited_processes` / `waitpid` / `kill` 即時回収)

## 5. タイムスライス付きラウンドロビン

### 5.1 tick更新

timer割り込み (`SCAUSE_SUPERVISOR_TIMER`) で:

1. `timer_set_next()`
2. `poll_console_input()`
3. `scheduler_on_timer_tick()`
4. `scheduler_should_yield()` が true なら `yield()`

`scheduler_on_timer_tick()` は実行中プロセスが `RUNNABLE` のときのみ:

- `run_ticks++`
- `time_slice--`
- `time_slice == 0` で `need_resched = true`

### 5.2 `yield()` の選択ロジック

`yield()` は `current_proc->pid` 起点の round-robin 走査を行う。

- runnable が見つからない:
  - `wfi` で待機
- `next == current_proc`:
  - slice が 0 なら再装填
  - そのまま継続
- 別プロセスへ切替:
  - `satp` 切替
  - `sscratch` に next のカーネルスタック先頭を設定
  - `switch_context(&prev->sp, &next->sp)`

`switch_context` は callee-saved レジスタ + `sstatus` を退避/復元する。

## 6. kill 実装の挙動

kill 実体は `process_kill(int target_pid)` (`src/kernel/proc/process.c`)。
`syscall_handle_kill` はこの戻り値をそのまま返す。

### 6.1 戻り値

- `>0`: kill成功（対象 pid）
- `-1`: 無効 pid (`pid <= 0`)
- `-2`: 対象なし / 既に終了
- `-3`: init プロセスは kill 禁止

### 6.2 処理内容

1. `find_process_by_pid()` で対象探索
2. `init_proc` 保護
3. `orphan_children(target->pid)`
4. `target->state = PROC_EXITED`
5. `notify_child_exit(target)` で `waitpid` 中の親を起床
6. 非self kill:
   - `target->parent_pid = 0`
   - `recycle_process_slot(target)` で即時回収
7. self kill:
   - 即時解放は危険（実行中スタック使用中）
   - `parent_pid=0` にして `yield()` で離脱

## 7. ps 連携

`ps` は `struct ps_info` でプロセス情報を返却。

`struct ps_info`:

- `pid`
- `parent_pid`
- `state`
- `wait_reason`
- `name[PROC_NAME_MAX]`

`syscall_handle_ps` は `sstatus.SUM` を一時有効化し、ユーザポインタへ書き戻す。
