# AquaCore Kernel Docs (Draft)

このディレクトリは、AquaCore カーネル実装の機能別ドキュメントです。
仕様策定用ではなく、現行コード追従の実装メモとして管理します。

## 目次
- [Boot / Trap](./boot-trap.md)
- [Memory](./memory.md)
- [Process / Scheduler](./process-scheduler.md)
- [Syscall](./syscall.md)
- [Filesystem / VFS](./fs-vfs.md)
- [User Runtime / App Launch](./user-runtime.md)
- [Network](./network.md)

## ドキュメント対応表
- Boot/Trap:
  - `src/kernel/kernel.c`
  - `src/kernel/trap/trap_handler.c`
  - `src/kernel/trap/syscall_handler.c`
- Memory:
  - `src/kernel/mm/memory.c`
  - `src/kernel/trap/page_access.c`
- Process/Scheduler:
  - `src/kernel/proc/process.c`
  - `src/include/proc/process.h`
- Syscall:
  - `src/kernel/trap/syscall_*.c`
  - `src/user/runtime/user_syscall.c`
- Filesystem/VFS:
  - `src/kernel/fs/fs.c`
  - `src/kernel/fs/pfs.c`
  - `src/kernel/fs/ramfs.c`
  - `src/kernel/fs/procfs.c`
- User Runtime:
  - `src/user/runtime/user.c`
  - `src/user/runtime/user_syscall.c`
- Network:
  - `src/kernel/net/net_virtio.c`
  - `src/kernel/net/net_dev.c`
  - `src/kernel/net/ping.c`
  - `src/kernel/net/udp.c`
