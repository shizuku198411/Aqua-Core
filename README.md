# AquaCore

<p>
  <img src="docs/assets/aquacore_logo.png" alt="Project Icon" width="190">
</p>

AquaCore は、RISC-V 32bit (`qemu-system-riscv32`) 上で動作するマイクロカーネルです。  
現在の実装は、プロセス/トラップ/システムコール/ファイルシステム/ネットワーク/ユーザアプリ実行までを一通り含みます。

## 実装機能（現行）

- ブート/トラップ
- タイマ割り込みとプリエンプティブなスケジューリング
- プロセス管理（`fork` / `exec_path` / `execv_path` / `exit` / `waitpid` / `kill`）
- ユーザメモリアクセス保護（`copyin` / `copyout` / `copyinstr` と `SSTATUS_SUM` ラップ）
- VFS
- 永続FS: `pfs`（virtio-blk上、dirty block同期）
- 揮発FS: `ramfs` (`/tmp`)
- `procfs` (`/proc/<pid>/status`)
- AppFS（`/bin/*` 実行イメージをディスクからロード）
- ネットワーク（virtio-net、`/dev/net0`、ping/UDP送信）
- RTC時刻取得

## 同梱ユーザアプリ

- `shell`, `ps`, `date`, `ls`, `mkdir`, `rmdir`, `touch`, `rm`, `write`, `cat`
- `kill`, `kernel_info`, `bitmap`, `ipc_rx`, `ping`, `udp_send`, `nslookup`, `echo`

## リポジトリ構成（主要）

- `src/kernel/` カーネル本体
- `src/user/` ユーザランタイム/ユーザアプリ
- `src/lib/` 共通ライブラリ
- `tests/unit/` ホスト実行のunit test
- `tests/int/` QEMU実行のintegration test
- `docs/kernel/` カーネル機能ドキュメント

## 必要ツール

- `clang`（RISC-Vターゲット付き）
- `lld`
- `llvm-objcopy`
- `qemu-system-riscv32`
- `python3`
- （TAP/NAT運用時）`ip`, `iptables`, `sudo`
- OpenSBIバイナリ(リポジトリトップへの設置が必要です)  
  ```
  cd Aqua-Core
  curl -LO https://github.com/qemu/qemu/raw/v8.0.4/pc-bios/opensbi-riscv32-generic-fw_dynamic.bin
  ```

## ビルド

```bash
make build
```

生成物:

- カーネルELF: `bin/kernel.elf`
- ディスクイメージ: `bin/disk.img`（`make disk` または `run/start/test-int` で自動作成）

## 実行

### 1. 通常実行（TAPネットワーク）

```bash
make run
```

### 2. user-mode networking で実行

```bash
make run-usernet
```

### 3. GDB待ち受けで起動

```bash
make qemu-debug
```

## テスト

### Unit test

```bash
make test-unit
```

- `tests/unit/test_commonlibs.c`
- `tests/unit/test_user_path.c`

### Integration test (QEMU)

```bash
make test-int
```

実行スクリプト: `tests/int/run_qemu_tests.py`

### 全テスト

```bash
make test
```

## Make ターゲット早見

- `make build` カーネルとユーザアプリをビルド
- `make disk` `bin/disk.img` を作成し appfs をパック
- `make run` QEMU実行（TAP）
- `make start` `run` と同等（既存運用向けエイリアス）
- `make run-usernet` usernetでQEMU実行
- `make qemu-debug` GDB待ち受け付き起動
- `make test-unit` unit test実行
- `make test-int` integration test実行
- `make test` 全テスト実行
- `make clean` ビルド成果物を削除
- `make distclean` `clean` + `bin/disk.img` を削除

## ネットワーク補助ターゲット（TAP/NAT）

- `make tap-up` / `make tap-down` / `make tap-status`
- `make nat-up` / `make nat-down` / `make nat-status`

環境変数で変更可能:

- `TAP_DEV`（default: `tap0`）
- `TAP_ADDR`（default: `10.0.2.1/24`）
- `TAP_CIDR`（default: `10.0.2.0/24`）
- `WAN_DEV`（default: `wlan0`）

## 関連ドキュメント

- カーネル機能ドキュメント: `docs/kernel/README.md`
- 実行フロー（AppFS）: `docs/appfs-exec-flow.md`
- `execv` 引数関連: `docs/execv-args.md`

## ライセンス

`LICENSE` を参照してください。
