# AquaCore Documentation

本ディレクトリは、AquaCore (RV32) の実装を機能単位で整理したドキュメントです。

- [Linker Script](./linker-script.md)
  - カーネル配置とリンカシンボル
- [Kernel Bootstrap](./kernel-bootstrap.md)
  - `boot` から `kernel_main`、`stvec`/timer 初期化
- [Trap Handler](./trap-handler.md)
  - trap 分岐、timer 割り込み、再スケジュール判定
- [Mode Transition](./mode-transition.md)
  - S-Mode <-> U-Mode 遷移 (`sret`, `ecall`, `sscratch`)
- [Syscall](./syscall.md)
  - syscall ABI、ディスパッチ、`waitpid`/`ipc_send`/`ipc_recv`
- [Memory / Process](./memory-process.md)
  - bitmap allocator、プロセス生成/解放、タイムスライス付き RR、IPC mailbox
- [Process Management](./process-management.md)
  - `struct process`、作成フロー、タイムスライス付きRR、`kill`/`waitpid`/`ps_info`
- [Fork / Exec](./fork-exec.md)
  - `fork` の親子分岐と `exec` のユーザ空間置換（最小実装）
- [Execv Argument Passing](./execv-args.md)
  - `execv` の引数受け渡し（レジスタ、trap、process保持、`main(argc, argv)` 受け取り）
- [Shell Redirection](./shell-redirection.md)
  - `>` / `<` のパース、`fd0/fd1` ベースの標準入出力、`dup2` 連携
- [VFS / RAMFS / VirtIO Block Storage](./vfs.md)
  - / と /tmp の共存、永続化PFS、virtio-blk、障害原因と対策
- [Procfs (`/proc` on RAMFS)](./procfs.md)
  - `/proc/<pid>/status` の生成/同期/cleanup、状態遷移の観測、既知制約と次段階
- [Kernel Operation Walkthrough](./kernel-operation-walkthrough.md)
  - shell操作と kernel 内部処理（fork/exec/wait/cwd/procfs）の対応イメージ
- [RTC / Time Syscall](./rtc.md)
  - Goldfish RTCドライバ、`gettime` syscall、`date` コマンド、64-bit秒対応
- [Memory Map](./memory-map.md)
  - カーネル/ユーザ/MMIO(virtio, RTC)のASCIIメモリマップ
- [SV32 Paging](./sv32.md)
  - 2段ページテーブル、アドレス計算、マッピング、QEMU/GDB での VA/PA 追跡
- [Page Table Mapping Path](./page-table-path.md)
  - page_tableの配置、kernel/user/MMIOマップ、`satp` 切替とCPUページウォーク経路
- [Troubleshoot: Page Table Walk GDB手順](./troubleshoot/page_table_walk_gdb.md)
  - `stval/sepc/satp` からの手動ページウォーク、コマンド一覧、実例
