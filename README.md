# AquaCore

<p>
  <img src="docs/assets/aquacore_logo.png" alt="Project Icon" width="190">
</p>

AquaCoreは、RISC-V 32bit向けに開発しているマイクロカーネルです。  
QEMU + OpenSBI 環境での動作を前提に実装しています。

<p>
  <img src="docs/assets/aquacore_terminal.png" alt="terminal">
</p>

## ビルド/起動
### BIOSダウンロード
以下リンクよりルート直下にBIOS(OpenSBI)をダウンロード、本リポジトリのルート直下に設置してください。
```bash
curl -LO https://github.com/qemu/qemu/blob/v8.0.4/pc-bios/opensbi-riscv32-generic-fw_dynamic.bin
```

### Makefile
```bash
# build (kernel + user apps)
make

# run on qemu (create ./bin/disk.img when missing)
make run
```

主要ターゲット:

- `make` or `make build` : ビルド
- `make start`: QEMU起動
- `make run` : ビルド + QEMU起動
- `make clean` : 生成物削除（disk imageは残す）
- `make distclean` : 生成物 + `./bin/disk.img` 削除

## 現在の実装機能

- ブートストラップ
  - `stvec` 設定
  - S-mode trap 入口 (`kernel_entry`) 実装
  - Supervisor Timer Interrupt 有効化
- Trap/割り込み
  - U-mode `ecall` 処理 (syscall)
  - timer 割り込みでの再スケジュール判定
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
- ファイルシステム (VFS)
  - `/` : PFS（virtio-blk上の永続ストレージ）
  - `/tmp` : RAMFS（揮発ストレージ）
  - `ls` / `cat` / `write` / `mkdir` / `rm` などのファイル操作
  - `dup2` と shell リダイレクト（`<`, `>`）
- RTC/時刻
  - Goldfish RTC ドライバによる現在時刻取得
  - `gettime` syscall と shell `date` コマンド
- shell UX
  - コマンド履歴（保存/復元）
  - 履歴の上下キー参照
  - 左右キーでカーソル移動、途中挿入/削除（Backspace/Delete）
  - Tab 補完（App名）
- ユーザアプリ
  - `shell`, `ps`, `date`, `ls`, `mkdir`, `rmdir`, `touch`, `rm`, `write`, `cat`, `kill`, `kernel_info`, `bitmap`
  - `ipc_rx`（`receiver`/`sender` モード）
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
- [Fork / Exec](./docs/fork-exec.md)
- [Execv Argument Passing](./docs/execv-args.md)
- [Shell Redirection](./docs/shell-redirection.md)
- [VFS / RAMFS / VirtIO Block Storage](./docs/vfs.md)
- [RTC / Time Syscall](./docs/rtc.md)
- [Memory Map](./docs/memory-map.md)
- [SV32 Paging](./docs/sv32.md)
- [Page Table Mapping Path](./docs/page-table-path.md)
