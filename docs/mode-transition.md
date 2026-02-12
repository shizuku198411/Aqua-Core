# Mode Transition (S-Mode <-> U-Mode)

対象:

- `src/kernel/kernel.c`
- `src/kernel/proc/process.c`
- `src/kernel/trap/trap_handler.c`

関連:

- [Trap Handler](./trap-handler.md)

## S-Mode -> U-Mode

`create_process()` は初期コンテキストの `ra` に `user_entry` を積みます。
初回 `switch_context()` 復帰先が `user_entry` になり、以下で U-Mode へ遷移します。

```c
csrw sepc, USER_BASE
csrw sstatus, SSTATUS_SPIE
sret
```

## U-Mode -> S-Mode

1. U-Mode `ecall`/割り込みで trap
2. `stvec` で `kernel_entry` に遷移
3. `kernel_entry` が `csrrw sp, sscratch, sp` でスタック切替
4. `handle_trap -> handle_syscall`
5. `sret` でユーザ復帰

## `sscratch` 運用

trap_entryが `csrrw sp, sscratch, sp` 前提のため、`sscratch` は常に有効なカーネル stack top を指す必要があります。

- コンテキストスイッチ時: `sscratch = &next->stack[sizeof(next->stack)]`
- runnable 不在で `wfi` 前: `csrw sscratch, sp`
