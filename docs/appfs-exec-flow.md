# AppFS User App Deployment And Exec Flow
## 1. 概要

- ユーザアプリをカーネル内の埋め込みアドレス依存ではなく、VFS 経由で実行する
- Linux 風に `/bin/<app>` を `exec_path` で起動できるようにする

## 2. 配置方法（ビルド時）

1. `make` で各ユーザアプリの `.bin` を生成（`bin/*.bin`）。
2. `make disk` で `bin/disk.img` を生成（存在しない場合）。
3. `scripts/pack_appfs.py` が `disk.img` の AppFS 領域へアプリを配置

実行コマンド:

```bash
make
make disk
```

`Makefile` では `disk` ターゲット内で以下を呼び出し

```bash
python3 scripts/pack_appfs.py --disk bin/disk.img --bin-dir bin
```

## 3. AppFS レイアウト

- 格納先: `disk.img` の固定オフセット（`APPFS_START_BLOCK` 起点）
- 構成:
  - header (`magic`, `entry count`)
  - entries (`name`, `data_off`, `size`)
  - payload（各 app のバイナリ本体）

カーネルは起動時に AppFS ヘッダとエントリを読み込み、`/bin` の `open/read/readdir` へ反映

## 4. 実行フロー（カーネル）

1. shell で `/bin/date` のようなパス付きコマンドを入力
2. shell が `execv_path(path, argv)` を syscall
3. kernel trap で `syscall_handle_execv_path` が path/argv を `copyinstr/copyin` で取得
4. `fs_open(current_pid, "/bin/date", O_RDONLY)` を実行
5. `fs_read` が AppFS backend からバイナリを読み出し
6. `process_exec(image, image_size, name, argc, argv)` で `USER_BASE` へ展開
7. trap return 時に `sepc=USER_BASE` から新イメージ実行開始


