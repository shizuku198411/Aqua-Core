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
- [Process Management](./process-management.md)

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
SYSCALL_KILL   = 10
```

## ユーザ側 ABI

`src/user/runtime/user_syscall.c` の `syscall()` が
`a0-a2=arg`, `a3=sysno` で `ecall` を発行します。

提供ラッパ:

- `putchar` / `getchar`
- `ps(index, struct ps_info *out)`
- `clone(app_id)` / `spawn(app_id)`
- `waitpid(pid)`
- `kill(pid)`
- `ipc_send(pid, message)`
- `ipc_recv(&from_pid)`
- `bitmap(index)`
- `exit`

## カーネル側の分割

- `syscall_handler.c`: ディスパッチのみ
- `syscall_console.c`: `putchar`, `getchar`, `poll_console_input`
- `syscall_process.c`: `exit`, `ps`, `clone`, `waitpid`, `kill`
- `syscall_ipc.c`: `ipc_send`, `ipc_recv`
- `syscall_debug.c`: `bitmap`

## `ps` の返却形式

`ps` は packed int ではなく `struct ps_info` をユーザバッファへ書き戻す。

```c
struct ps_info {
    int  pid;
    int  parent_pid;
    int  state;
    int  wait_reason;
    char name[PROC_NAME_MAX];
};
```

使い方:

- `int ret = ps(index, &info);`
- `ret == 0` で成功
- `ret < 0` で失敗（index範囲外）

## `kill` の意味

- `kill(pid)` は `process_kill(pid)` を呼ぶ
- 返り値:
  - `>0`: 成功（対象pid）
  - `-1`: 無効pid (`pid <= 0`)
  - `-2`: 対象なし / 既に終了
  - `-3`: initプロセスはkill禁止
