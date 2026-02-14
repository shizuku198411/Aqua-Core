# Execv Argument Passing (Current Implementation)

## 概要

AquaCore における `execv` は、  
POSIX のように「ユーザスタック上に `argc/argv/envp` を直接構築する方式」ではなく、  
カーネル内の `process` 構造体へ保持し、ユーザ起動時に `getargs` syscall で取得する方式を採用しています。

---

## 全体フロー

```text
shell(run_external)
  -> fork()
    -> child
      -> execv(app_id, argv)
        -> ecall (a0=app_id, a1=argv_ptr, a3=SYSCALL_EXECV)
          -> syscall_handle_execv()
            -> user argv を kernel バッファへコピー
            -> process_exec(..., argc, argv_copy)
               - old user pages free
               - new image map
               - current_proc->exec_argc/exec_argv を更新
               - sepc = USER_BASE
          -> trap_handler
             - execv 成功時は sepc=USER_BASE で復帰
      -> user start (new image)
        -> user_main_entry()
          -> getargs(&args)
          -> main(argc, argv)
```

---

## ABI とレジスタ

共通 syscall ABI (`src/user/runtime/user_syscall.c`)：
- `a0`: arg0
- `a1`: arg1
- `a2`: arg2
- `a3`: syscall 番号
- 返り値: `a0`

`execv` 呼び出し:
- ユーザ側: `execv(app_id, argv)`
- 実際のレジスタ:
  - `a0 = app_id`
  - `a1 = (int)argv`（`const char **` の先頭）
  - `a2 = 0`
  - `a3 = SYSCALL_EXECV`

`getargs` 呼び出し:
- ユーザ側: `getargs(struct exec_args *out)`
- レジスタ:
  - `a0 = (int)out`
  - `a3 = SYSCALL_GETARGS`

---

## データ構造

`src/include/process.h`:

- `PROC_EXEC_ARGV_MAX = 8`
- `PROC_EXEC_ARG_LEN = 32`
- `struct process` 内:
  - `int exec_argc`
  - `char exec_argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN]`
- ユーザ転送用:
  - `struct exec_args { int argc; char argv[8][32]; }`

このため現在の制約は以下です。
- 引数は最大8
- 各引数長は最大31文字（終端NUL込みで32）
- 超過分は切り詰め（エラーにはしていない）

---

## カーネル側処理

### 1) `syscall_handle_execv`
`src/kernel/trap/syscall_process.c`

1. `app_id` から実行イメージ（`_binary___bin_*_start/size`）を解決
2. `SSTATUS_SUM` を有効化してユーザ空間 `argv` を読める状態にする
3. `copy_user_argv()` で固定長バッファへコピー
4. `process_exec(image, size, name, argc, argv_copy)` を実行

### 2) `process_exec`
`src/kernel/proc/process.c`

1. 既存ユーザページを解放
2. 新イメージを `USER_BASE` からマップ
3. `current_proc->exec_argc/exec_argv` を更新
4. `sepc = USER_BASE` を設定

### 3) trap 復帰PCの扱い
`src/kernel/trap/trap_handler.c`

`ecall` 後の PC 処理で、`SYSCALL_EXEC` と `SYSCALL_EXECV` 成功時は `sepc += 4` せず、  
`USER_BASE` から再開するようにしている。

```c
if ((sysno == SYSCALL_EXEC || sysno == SYSCALL_EXECV) && f->a0 == 0) {
    user_pc = USER_BASE;
} else {
    user_pc += 4;
}
```

---

## ユーザ起動時の受け取り

`src/user/runtime/user.c`:

- `start`（naked）で `sp = __stack_top`
- `user_main_entry()` を呼ぶ
- `user_main_entry()` が `getargs(&args)` を呼ぶ
- ローカル配列 `char *argv[]` を `args.argv[i]` で構成
- `main(argc, argv)` を通常 C 呼び出し

### スタック上のイメージ（概念図）

```text
user_main_entry stack frame
  +-------------------------------+
  | struct exec_args args         |  <- getargs() の出力先
  |   argc                        |
  |   argv[0][32] ... argv[7][32] |
  +-------------------------------+
  | char *argv_local[9]           |  <- main() へ渡すポインタ配列
  +-------------------------------+
```

`argv_local[i]` は `args.argv[i]` を指すため、引数文字列自体は同一スタックフレーム内にあります。

---

## `fork` との関係

`src/kernel/proc/process.c` の `process_fork()` で、
- FD テーブルを子へコピー（`fs_fork_copy_fds`）
- `exec_argc/exec_argv` も親から子へコピー

したがって、`fork` 後に `execv` しない経路でも引数領域は一貫した値を持ちます。

---

## 現在方式と POSIX 方式の違い

現在方式:
- `process` 構造体に引数を保持
- ユーザは `getargs` で取得
- 実装が単純でデバッグしやすい

POSIX 方式（将来候補）:
- `exec` 時に新プロセスのユーザスタックへ `argc/argv/envp/auxv` を直接配置
- CRT がそれを読み `main(argc, argv, envp)` へ渡す
