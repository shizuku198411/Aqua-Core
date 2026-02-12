# Syscall

対象:

- `src/include/syscall.h`
- `src/kernel/trap/syscall_handler.c`
- `src/kernel/trap/syscall_console.c`
- `src/kernel/trap/syscall_process.c`
- `src/kernel/trap/syscall_ipc.c`
- `src/kernel/trap/syscall_debug.c`
- `src/user/runtime/user_syscall.c`

関連:

- [Trap Handler](./trap-handler.md)
- [Memory / Process](./memory-process.md)

## syscall 番号

```c
SYSCALL_PUTCHAR = 1
SYSCALL_GETCHAR = 2
SYSCALL_EXIT    = 3
SYSCALL_PS      = 4
SYSCALL_CLONE   = 5
SYSCALL_BITMAP  = 6
SYSCALL_WAITPID = 7
SYSCALL_IPC_SEND= 8
SYSCALL_IPC_RECV= 9
```

`SYSCALL_SPAWN` は `SYSCALL_CLONE` の別名です。

## ユーザ側 ABI

`src/user/runtime/user_syscall.c` の `syscall()` が
`a0-a2=arg`, `a3=sysno` で `ecall` を発行します。

提供ラッパ:

- `putchar` / `getchar`
- `ps(index)`
- `clone(app_id)` / `spawn(app_id)`
- `waitpid(pid)`
- `ipc_send(pid, message)`
- `ipc_recv(&from_pid)`
- `bitmap(index)`
- `exit`

## カーネル側の分割

- `syscall_handler.c`: ディスパッチのみ
- `syscall_console.c`: `putchar`, `getchar`, `poll_console_input`
- `syscall_process.c`: `exit`, `ps`, `clone`, `waitpid`
- `syscall_ipc.c`: `ipc_send`, `ipc_recv`
- `syscall_debug.c`: `bitmap`

## `waitpid` の意味

- 親プロセスは `waitpid` で `PROC_WAIT_CHILD_EXIT` に入りブロック
- 子が `exit` すると親が起床
- 回収は `waitpid` 側で実施（親なしプロセスは scheduler 側で回収）

## IPC (`ipc_send` / `ipc_recv`)

- 宛先プロセスごとに単一 mailbox (`ipc_has_message`) を保持
- `ipc_send` は mailbox が空なら配達、満杯なら `-2`
- `ipc_recv` はメッセージ到着まで `PROC_WAIT_IPC_RECV` で待機
- `ipc_recv` の `from_pid` 書き戻しは `sstatus.SUM` を一時有効化して実施
