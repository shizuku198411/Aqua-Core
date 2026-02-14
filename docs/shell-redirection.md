# Shell Redirection (`>` / `<`)

## 概要

現在は「shell内コマンド（built-in）」を対象に、`fd0`/`fd1` を使った標準入出力経路で動作します。

---

## 目的

- `cmd > file` で標準出力をファイルへリダイレクトする
- `cmd < file` で標準入力をファイルから読み込む
- 実行後に通常のコンソール入出力へ復帰する

---

## 実装内容

### 1. `>` / `<` のパース

対象:
- `src/user/apps/shell/utils.c`
- `src/user/apps/shell/shell.h`

`parse_redirection()` で以下を実施:
- `argv` から `>` / `<` とパスを抽出
- リダイレクト指定トークンを除去した `exec_argv` を再構築
- 構文エラー（演算子の後にパスがない）を検出して `-1` を返す

### 2. shell実行前後で `fd0` / `fd1` を差し替え

対象:
- `src/user/apps/shell/main.c`

実行前:
- `< path`: `fs_open(path, O_RDONLY)` -> `dup2(fd, 0)` -> 一時fd close
- `> path`: `fs_open(path, O_WRONLY | O_CREAT | O_TRUNC)` -> `dup2(fd, 1)` -> 一時fd close

実行後:
- 入力リダイレクトがあれば `fs_close(0)`
- 出力リダイレクトがあれば `fs_close(1)`

`fd0`/`fd1` が未割り当て状態に戻ることで、次コマンドではコンソールI/Oへ復帰します。

### 3. ユーザランタイムの `putchar/getchar` を `fd1/fd0` 経由化

対象:
- `src/user/runtime/user_syscall.c`

変更:
- `putchar()` は `SYSCALL_WRITE(fd=1)` 経由
- `getchar()` は `SYSCALL_READ(fd=0)` 経由

これにより `printf` など既存出力が `fd1` を通るため、`dup2` によるリダイレクトが有効になります。

### 4. カーネルFSの標準I/Oフォールバック

対象:
- `src/kernel/fs/fs.c`

`fd_table[pid][fd]` 未使用時の特例:
- `fs_read(..., fd=0, ...)` はコンソール入力へフォールバック
- `fs_write(..., fd=1, ...)` はコンソール出力へフォールバック

これにより、通常時は `fd0/1` を明示オープンしていなくても標準入出力が成立します。

---

## 追加テストコマンド

対象:
- `src/user/apps/shell/commands_sys.c`
- `src/user/apps/shell/main.c`

`stdin_test` を追加:
- `fd0` からEOFまで読み込み、内容とバイト数を表示
- `<` の動作確認に利用

例:

```sh
write /tmp/test.txt hello
stdin_test < /tmp/test.txt
```

期待例:

```txt
hello
[stdin_test] bytes=5
```

---

## 関連トラブルシュート

- [shellリダイレクト(`<`)時の入力停止 調査記録](./troubleshoot/shell_redirection_stdin_stall_rootcause.md)
  - `fd0/1` と `dup2` 参照関係の図解、再現条件、原因、対策を記載
