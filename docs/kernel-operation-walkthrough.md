# Kernel Operation Walkthrough

## 1. 起動直後の流れ

1. `boot -> kernel_main -> kernel_bootstrap`
2. メモリ初期化、trap/timer設定、FS初期化（`/`, `/tmp`, `/proc`）
3. `idle` と `shell(init)` を作成
4. scheduler により shell が user mode で実行開始

イメージ:

```text
boot
  -> kernel_bootstrap
    -> memory_init
    -> trap/timer init
    -> fs_init: "/" + "/tmp" + "/proc"
    -> create idle(pid=0)
    -> create shell(pid=1)
  -> yield()
    -> run shell
```

## 2. コマンド実行時の基本経路

`ls /tmp` など外部コマンド実行時:

1. shell がコマンドを解析
2. `fork` で子を作成
3. 子が `execv(app, argv)` でイメージ置換
4. 親は `waitpid` で待機（`&` なしの場合）
5. 子終了で親が再開

イメージ:

```text
shell(pid=1)
  fork()
   ├─ parent: waitpid(child) -> WAIT_CHILD_EXIT
   └─ child : execv(ls, ["ls","/tmp"])
              -> run -> exit
parent wakes up -> prompt
```

## 3. `cd` と cwd の更新

`cd` は shell 組み込み（親プロセス）で処理される。

1. shell で `cd /etc` 実行
2. `chdir` syscall
3. kernel が `current_proc->cwd_mount_idx/cwd_node_idx/cwd_path` を更新
4. `procfs_sync_process` で `/proc/1/status` の `cwd` も更新

確認例:

```sh
cd /tmp
cat /proc/1/status
```

## 4. 相対パス解決

`cat a.txt` のように `/` で始まらない引数は、ユーザ側で `cwd` 基準に正規化してから syscall へ渡す。

```text
cwd = /tmp
input path = a.txt
resolved = /tmp/a.txt
```

これにより、`cat/rm/write/touch/mkdir/rmdir` の操作感が UNIX に近づく。

## 5. `/proc` で状態遷移を観測

`/proc/<pid>/status` は `state/wait_reason/cwd` を表示する。
`fork/wait/ipc/exit/kill/chdir` の遷移時に更新される。

観測例:

```sh
ps
cat /proc/1/status
ipc_rx receiver &
cat /proc/<pid>/status
kill <pid>
ls /proc
```

見るポイント:

- `state`: `RUN` / `WAIT` / `EXIT`
- `wait_reason`: `CHILD_EXIT` / `IPC_RECV` など
- `cwd`: `cd` 後のパス

## 6. リダイレクト時の動き

`cmd > file`, `cmd < file` は shell が `dup2` を使って `fd0/fd1` を差し替えてから実行する。

```text
before:  fd0 -> console, fd1 -> console
cmd > x: fd1 -> /path/x
cmd < x: fd0 -> /path/x
```

子プロセスは親の FD テーブルを fork 時に引き継ぐため、
リダイレクト付き実行が成立する。
