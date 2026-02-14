# Procfs (`/proc` on RAMFS)

対象:

- `src/kernel/fs/fs.c`
- `src/kernel/proc/process.c`
- `src/kernel/trap/syscall_process.c`
- `src/kernel/trap/syscall_fs.c`
- `src/include/process.h`

関連:

- [VFS / RAMFS / VirtIO Block Storage](./vfs.md)
- [Process Management](./process-management.md)
- [Fork / Exec](./fork-exec.md)

## 概要

`/proc` を RAMFS バックエンドとしてマウントし、プロセスごとに
`/proc/<pid>/status` を生成・更新する実装を追加している。

目的:

- カーネル内部状態（`state`, `wait_reason`, `cwd` など）をユーザ空間から観測可能にする
- GDB なしでもプロセス遷移を確認しやすくする

## マウント構成

`fs_init()` で以下を実施:

1. ルートに `/proc` マウントポイントを作成（未作成時のみ）
2. `procfs` 用 `nodefs` インスタンスを `/proc` にマウント

結果として:

- `/`      : 永続 PFS
- `/tmp`   : 揮発 RAMFS
- `/proc`  : 揮発 RAMFS（プロセス情報）

## `status` ファイル形式

`/proc/<pid>/status` は行ベースの `key:\tvalue` 形式:

```text
pid:    2
ppid:   1
name:   shell
state_id:       2
state:  WAIT
wait_reason_id: 3
wait_reason:    IPC_RECV
cwd:    /tmp
```

主な項目:

- `pid`, `ppid`
- `name`
- `state_id`, `state`
- `wait_reason_id`, `wait_reason`
- `cwd`

## 更新トリガ

`procfs_sync_process()` は以下の契機で呼ばれる:

- `create_process`（初回作成）
- `process_fork`（子作成）
- `process_exec`（イメージ置換後）
- `chdir`（`cwd` 変更後）
- `wakeup_input_waiters`（`WAIT_CONSOLE_INPUT -> RUNNABLE`）
- `notify_child_exit`（親 `WAIT_CHILD_EXIT -> RUNNABLE`）
- `wait_for_child_exit`（親 `RUNNABLE -> WAITTING`）
- `process_ipc_send`（受信側 `WAITTING -> RUNNABLE`）
- `process_ipc_recv`（受信待ち `RUNNABLE -> WAITTING`）
- `process_kill`（対象 `-> EXITED`）
- `syscall_handle_exit`（最終 `EXITED` 遷移）

方針:

- 同期失敗は panic せずログ出力のみ（best-effort）

## クリーンアップ

`procfs_cleanup()` で以下を削除:

1. `/proc/<pid>/status`
2. `/proc/<pid>`

呼び出し位置:

- `recycle_process_slot()` の先頭

この位置に置く理由:

- reclaim 経路（`reap_exited_processes`, `wait_for_child_exit`, `process_kill`）を単一点でカバーできるため

## 検証シナリオ

1. 基本生成
 - shell 起動後に `ls /proc` を実行し `1`（shell pid）などが見える
 - `cat /proc/1/status` で `name/state/cwd` が読める

2. `fork/exec` 追従
 - `exec_test` などで子生成
 - `cat /proc/<child>/status` で `ppid/name/state` の変化を確認

3. `chdir` 追従
 - `cd /tmp`
 - `cat /proc/1/status` の `cwd` が `/tmp` になることを確認

4. `wait`/`ipc` 遷移
 - 親を `waitpid` 待機に入れて `WAIT_CHILD_EXIT` を確認
 - `ipc_rx` 待機中に `WAIT` + `IPC_RECV` を確認

5. 終了と削除
 - `kill <pid>` 実行後に `/proc/<pid>` が消えることを確認

## 既知制約

- 実体書き込み型:
 - 状態を RAMFS ファイルへ都度反映する方式のため、更新漏れがあると観測値が古くなる
- `readdir` 順序:
 - `/proc` エントリ列挙順は固定順ではない
- 性能:
 - 遷移ごとに open/write/close を行うため、遷移密度が高いとコストが増える

## 次段階（virtual procfs）

将来的には「実体ファイルを書かない procfs」へ移行する。

方向性:

1. `/proc` を専用 FS ops に分離
2. `read("/proc/<pid>/status")` 時にその場で文字列生成
3. `readdir("/proc")` 時に live process から pid 一覧を動的生成

利点:

- 二重管理（メモリ上の真実 vs ファイル内容）の解消
- cleanup/同期漏れの管理コスト削減
