# AquaCore

AquaCoreは、RISC-V 32bit向けに開発しているマイクロカーネルです。
QEMU + OpenSBI 環境での動作を前提に実装しています。

## 現在の実装機能

- ブートストラップ
  - `stvec` 設定
  - S-mode trap 入口 (`kernel_entry`) 実装
  - Supervisor Timer Interrupt 有効化
- Trap/割り込み
  - U-mode `ecall` 処理
  - timer 割り込みでの時限再設定 (`stimecmp`) とスケジューリング起動
- Syscall
  - `putchar`, `getchar`, `exit`, `ps`, `clone/spawn`, `bitmap`
- 入力処理
  - コンソール入力リングバッファ
  - `getchar` の待機/起床制御（busy loop回避）
- メモリ管理
  - ページ単位 bitmap allocator (`alloc_pages` / `free_pages`)
  - SV32 ページテーブル構築とマッピング
- プロセス管理/スケジューリング
  - プロセス作成 (`create_process`)
  - ラウンドロビンスケジューリング (`yield`)
  - 終了プロセスの回収 (`reap_exited_processes`)
- ユーザアプリ
  - `shell` アプリ
  - `ps` アプリ（新規プロセスとして起動）

## ドキュメント

詳細設計・実装メモは `docs/` 配下にあります。

- `docs/README.md`
- `docs/linker-script.md`
- `docs/kernel-bootstrap.md`
- `docs/trap-handler.md`
- `docs/mode-transition.md`
- `docs/syscall.md`
- `docs/memory-process.md`
