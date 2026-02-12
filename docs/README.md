# AquaCore Documentation

本ディレクトリは、現在の AquaCore (RV32) 実装を機能単位で整理したドキュメントです。

- `docs/linker-script.md`
  - カーネル配置とリンカシンボル
- `docs/kernel-bootstrap.md`
  - `boot` から `kernel_main`、`stvec`/timer 初期化
- `docs/trap-handler.md`
  - trap 分岐と `ecall`/timer 処理
- `docs/mode-transition.md`
  - S-Mode <-> U-Mode 遷移 (`sret`, `ecall`, `sscratch`)
- `docs/syscall.md`
  - syscall ABI、ディスパッチ、各 syscall 実装
- `docs/memory-process.md`
  - bitmap メモリアロケータ、プロセス生成/解放、スケジューリング

## ソース構成 (抜粋)

- `src/kernel/` : カーネル本体
- `src/kernel/mm/` : メモリ管理
- `src/kernel/proc/` : プロセス管理
- `src/kernel/trap/` : trap/syscall
- `src/kernel/time/` : timer
- `src/kernel/platform/` : SBI ラッパ
- `src/lib/` : 共通ライブラリ
- `src/user/runtime/` : ユーザランタイム/ユーザ syscall ラッパ
- `src/user/apps/` : ユーザアプリ
  - `shell`: 対話実行、`ps`/`bitmap`/`exit`
  - `ps`: プロセス一覧表示
