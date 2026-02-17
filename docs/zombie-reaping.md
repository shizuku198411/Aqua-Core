# Zombie Process Reaping

## 対象ファイル
- `src/include/proc/process.h`
- `src/include/proc/signal.h`
- `src/kernel/proc/process.c`
- `src/kernel/trap/syscall_process.c`
- `src/user/include/user_syscall.h`
- `src/user/runtime/user_syscall.c`

## 概要
現在の実装では、子プロセス終了時 (`PROC_EXITED`) の回収は以下の 2 経路で行います。

1. 親が `waitpid` で明示回収
2. 親に配送された `SIGCHLD` をカーネル側で処理し、非ブロッキング回収

## 状態遷移
- 子が `exit` すると `state=PROC_EXITED` になる
- `notify_child_exit(child)` が親へ `SIGCHLD` を設定する
- 親プロセスの `pending_signal` を scheduler/yield 経由で `process_apply_pending_signal()` が処理する
- `SIGCHLD` なら `wait_for_child_exit(parent_pid, -1, WAITPID_NOHANG)` を反復し、回収可能な子を再利用可能スロットへ戻す

## 実装詳細

### 1) waitpid options (`WNOHANG`)
- `WAITPID_NOHANG` を `process.h` に定義
- `wait_for_child_exit(parent_pid, target_pid, options)` が以下を実装
  - 回収対象がいれば PID を返す
  - 子はいるが未終了なら `WNOHANG` 時に `0` を返す
  - 子がいなければ `-1`

### 2) SIGCHLD による shell 非依存回収
- `notify_child_exit()` が親へ `SIGCHLD` を配送
- `process_apply_pending_signal()` の `SIGCHLD` 分岐で non-blocking reap を実行
- 親が `waitpid` ブロック中 (`WAIT_CHILD_EXIT`) の場合は従来通り wakeup される

### 3) 実行中プロセス回収の防止ガード
`wait_for_child_exit()` で `proc == current_proc` の場合は回収しません。

理由:
- 子が `exit()->yield()` の途中では、まだその子の kernel stack / page table を使用中
- ここで `recycle_process_slot()` すると解放済みメモリ参照となり page fault の原因になる

追加で、`SIGCHLD` 処理後に未回収 exited 子が残る場合は `SIGCHLD` を再度 pending に戻し、次回安全なタイミングで再試行します。

## 返り値ルール (`waitpid`)
- `>0`: 回収した子 pid
- `0`: `WNOHANG` かつ回収対象なし
- `<0`: 対象子なし/引数エラー

## 期待動作
- `date &` などバックグラウンド実行を繰り返しても、親 shell が明示 `waitpid` しなくてもゾンビが蓄積しにくい
- 同時に、終了直後の実行中子を誤回収しないため、`Load page fault` が発生しない
