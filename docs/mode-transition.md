# Mode Transition (S-Mode <-> U-Mode)

対象:

- `src/kernel/kernel.c`
- `src/kernel/proc/process.c`
- `src/kernel/trap/trap_handler.c`

## S-Mode -> U-Mode

`create_process()` は初期コンテキストの `ra` に `user_entry` を積みます。
初回 `switch_context()` 復帰先が `user_entry` になり、以下で U-Mode へ遷移します。

```c
csrw sepc, USER_BASE
csrw sstatus, SSTATUS_SPIE
sret
```

## U-Mode -> S-Mode

1. U-Mode `ecall` で trap
2. `stvec` で `kernel_entry` に遷移
3. `kernel_entry` が `csrrw sp, sscratch, sp` でスタック切替
4. `handle_trap -> handle_syscall` 実行
5. `sret` でユーザへ復帰

## `sscratch` の使い方

trap 入口が `csrrw sp, sscratch, sp` 前提のため、`sscratch` は常に有効なカーネル stack top を指す必要があります。

- コンテキストスイッチ時:
  - `sscratch = &next->stack[sizeof(next->stack)]`
- runnable 不在で S-Mode のまま `wfi` する前:
  - `csrw sscratch, sp`
