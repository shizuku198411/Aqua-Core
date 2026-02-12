# Syscall

対象:

- `src/include/syscall.h`
- `src/kernel/trap/syscall_handler.c`
- `src/kernel/trap/syscall_console.c`
- `src/kernel/trap/syscall_process.c`
- `src/kernel/trap/syscall_debug.c`
- `src/user/runtime/user_syscall.c`

## syscall 番号

```c
SYSCALL_PUTCHAR = 1
SYSCALL_GETCHAR = 2
SYSCALL_EXIT    = 3
SYSCALL_PS      = 4
SYSCALL_CLONE   = 5
SYSCALL_BITMAP  = 6
```

`SYSCALL_SPAWN` は `SYSCALL_CLONE` の別名です。

## ユーザ側 ABI

`src/user/runtime/user_syscall.c` の `syscall()` が
`a0-a2=arg`, `a3=sysno` で `ecall` を発行します。

提供ラッパ:

- `putchar`
- `getchar`
- `ps(index)`
- `clone(app_id)` / `spawn(app_id)`
- `bitmap(index)`
- `exit`

## カーネル側の分割

`syscall_handler.c` はディスパッチ専用です。

- console系: `syscall_console.c`
  - `putchar`, `getchar`, `poll_console_input`
- process系: `syscall_process.c`
  - `exit`, `ps`, `clone`
- debug系: `syscall_debug.c`
  - `bitmap`

## `getchar` の現在動作

`syscall_handle_getchar()` は入力リングバッファ (`input_buf[64]`) から読み出します。

- バッファ空なら `poll_console_input()` で SBI から吸い上げ
- それでも空なら
  - `current_proc->wait_reason = PROC_WAIT_CONSOLE_INPUT`
  - `current_proc->state = PROC_WAITTING`
  - `yield()`
- timer 割り込みで `poll_console_input()` が走り、入力が入ると `wakeup_input_waiters()` で入力待ちのみ `RUNNABLE` に戻る

これにより busy loop を避けて待機できます。

## `clone/spawn` の意味

現在の `clone` は Linux の `fork` 互換ではなく、
埋め込み済みユーザバイナリを `app_id` 指定で新規起動する API です。

- `APP_ID_SHELL`
- `APP_ID_PS`

## `ps` の表示順序

shell (`src/user/apps/shell/main.c`) は `spawn(APP_ID_PS)` 後に `wait_for_pid(pid)` で対象プロセス終了を待ってから次プロンプトを表示するため、`ps` 出力後に `>` が出ます。
