# AquaCore

<p>
  <img src="docs/assets/aquacore_logo.png" alt="Project Icon" width="190">
</p>

AquaCoreは、RISC-V 32bit向けに開発しているマイクロカーネルです。  
QEMU + OpenSBI 環境での動作を前提に実装しています。

## 現在の実装機能

- ブートストラップ
  - `stvec` 設定
  - S-mode trap 入口 (`kernel_entry`) 実装
  - Supervisor Timer Interrupt 有効化
- Trap/割り込み
  - U-mode `ecall` 処理
  - timer 割り込みでの再スケジュール判定
- Syscall
  - `putchar`, `getchar`, `exit`, `ps`, `clone/spawn`, `waitpid`, `kill`, `ipc_send`, `ipc_recv`, `bitmap`
- 入力処理
  - コンソール入力リングバッファ
  - `getchar` の待機/起床制御（busy loop回避）
- メモリ管理
  - ページ単位 bitmap allocator (`alloc_pages` / `free_pages`)
  - SV32 2段ページテーブル構築とマッピング
- プロセス管理/スケジューリング
  - プロセス作成 (`create_process`)
  - タイムスライス付きラウンドロビン (`yield`)
  - 終了プロセスの回収 (`reap_exited_processes`, `waitpid`)
- IPC
  - プロセスごとの単一 mailbox
  - `ipc_send` / `ipc_recv` による送受信
- ユーザアプリ
  - `shell`
  - `ps`
  - `ipc_rx`（受信待機アプリ）
- カーネル終了
  - init プロセス終了時の shutdown 処理

## ドキュメント

- [Documentation Index](./docs/README.md)
- [Linker Script](./docs/linker-script.md)
- [Kernel Bootstrap](./docs/kernel-bootstrap.md)
- [Trap Handler](./docs/trap-handler.md)
- [Mode Transition](./docs/mode-transition.md)
- [Syscall](./docs/syscall.md)
- [Memory / Process](./docs/memory-process.md)
- [Process Management](./docs/process-management.md)
- [SV32 Paging](./docs/sv32.md)
