# Trap Handler

対象: `src/kernel/trap/trap_handler.c`

## エントリ

`handle_trap(struct trap_frame *f)` が `scause/stval/sepc` を取得して分岐します。

## U-Mode ecall

```c
case SCAUSE_ENVIRONMENT_CALL_FROM_U_MODE:
    handle_syscall(f);
    user_pc += 4;
    break;
```

- `handle_syscall` に処理委譲
- `ecall` 再実行回避のため `sepc += 4`

## timer interrupt

```c
case SCAUSE_SUPERVISOR_TIMER:
    timer_set_next();
    poll_console_input();
    if (current_proc && current_proc->state == PROC_RUNNABLE) {
        yield();
    }
    return;
```

- 次回 timer を先に再設定
- console 入力を先にバッファへ吸い上げ
- 現在プロセスが runnable のときだけ `yield()`
- timer 分岐は `return` するため、通常分岐の `WRITE_CSR(sepc, user_pc)` は通らない

## 例外処理

未対応 fault は `PANIC`。

- illegal instruction
- access fault
- page fault
- S-Mode ecall など
