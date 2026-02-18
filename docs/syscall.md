# Syscall

対象:

- `src/include/user/syscall.h`
- `src/kernel/trap/syscall_handler.c`
- `src/kernel/trap/syscall_console.c`
- `src/kernel/trap/syscall_process.c`
- `src/kernel/trap/syscall_ipc.c`
- `src/kernel/trap/syscall_debug.c`
- `src/kernel/trap/syscall_fs.c`
- `src/kernel/trap/syscall_time.c`
- `src/user/runtime/user_syscall.c`

関連:

- [Trap Handler](./trap-handler.md)
- [Memory / Process](./memory-process.md)
- [Process Management](./process-management.md)

## syscall 番号

`src/include/user/syscall.h` の定義に準拠:

```c
SYSCALL_PUTCHAR    = 1
SYSCALL_GETCHAR    = 2
SYSCALL_EXIT       = 3
SYSCALL_PS         = 4
SYSCALL_BITMAP     = 6
SYSCALL_WAITPID    = 7
SYSCALL_IPC_SEND   = 8
SYSCALL_IPC_RECV   = 9
SYSCALL_KILL       = 10
SYSCALL_KERNEL_INFO= 11
SYSCALL_OPEN       = 12
SYSCALL_CLOSE      = 13
SYSCALL_READ       = 14
SYSCALL_WRITE      = 15
SYSCALL_MKDIR      = 16
SYSCALL_READDIR    = 17
SYSCALL_UNLINK     = 18
SYSCALL_RMDIR      = 19
SYSCALL_GETTIME    = 20
SYSCALL_FORK       = 21
SYSCALL_DUP2       = 23
SYSCALL_GETARGS    = 25
SYSCALL_GETCWD     = 27
SYSCALL_CHDIR      = 28
SYSCALL_PING_TX    = 29
SYSCALL_SLEEP      = 30
SYSCALL_EXEC_PATH  = 31
SYSCALL_EXECV_PATH = 32
```

## ユーザ側 ABI

`src/user/runtime/user_syscall.c` の `syscall()` が `a0-a2=arg`, `a3=sysno` で `ecall` を発行します。

提供ラッパ例:

- `putchar` / `getchar`
- `ps(index, struct ps_info *out)`
- `waitpid(pid, status, options)`
- `kill(pid)`
- `ipc_send(pid, message)` / `ipc_recv(&from_pid)`
- `fs_open/read/write/...`
- `fork()`
- `exec_path(path)` / `execv_path(path, argv)`
- `dup2(old_fd, new_fd)`
- `getargs(out)` / `getcwd(path)` / `chdir(path)`
- `kernel_info(out)` / `gettime(out)` / `sleep(ms)` / `ping_tx(...)`
- `exit(status)`

## カーネル側の分割

- `syscall_handler.c`: ディスパッチのみ
- `syscall_console.c`: console I/O
- `syscall_process.c`: `exit`, `ps`, `waitpid`, `kill`, `fork`, `exec_path`, `execv_path`, `getargs`
- `syscall_ipc.c`: IPC
- `syscall_fs.c`: FS syscall
- `syscall_time.c`: 時刻/スリープ
- `syscall_debug.c`: bitmap/kernel_info

## `ps` の返却形式

`ps` は `struct ps_info` をユーザバッファへ書き戻します。

使い方:

- `int ret = ps(index, &info);`
- `ret == 0` で成功
- `ret < 0` で失敗（index範囲外やcopy失敗）

## `kill` の意味

- `kill(pid)` は `process_kill(pid)` を呼ぶ
- 返り値:
  - `>0`: 成功（対象pid）
  - `-1`: 無効pid (`pid <= 0`)
  - `-2`: 対象なし / 既に終了
  - `-3`: initプロセスはkill禁止
