# shellリダイレクト(`<`)時の入力停止 調査記録

## 概要
`stdin_test < /tmp/test.txt` 実行時に、入力が止まったように見える事象が発生した。  
調査の結果、`dup2` 適用後の一時FDクローズ処理が `fd0` 自体を閉じるケースを考慮していなかったことが原因だった。

## 発生事象
- `stdin_test < /tmp/test.txt` 実行時に処理が進まないように見える
- 同様の症状は RAMFS(`/tmp`) / PFS(`/`) のどちらでも再現

```text
aqua-core:$ stdin_test < /tmp/test.txt
# 停止したように見える（実際はコンソール入力待ち）
```

## 調査
`main.c` のリダイレクト適用処理と `fd_table` の挙動を中心に確認。

- `<` 適用時の `fs_open -> dup2 -> fs_close` の順序
- `dup2(old_fd, 0)` で `old_fd == 0` となるケース
- `getchar` が最終的に参照する `fd0` の状態

## 原因特定に至った観測結果
1. `fs_open(in_path, O_RDONLY)` が `fd=0` を返す場合がある  
   -> `fd_table[pid][0]` が未使用なら先頭から割り当てるため、`0` は正常値

2. `dup2(0, 0)` は no-op 成功  
   -> 参照先は変わらない

3. その直後に `fs_close(fd_in)` を無条件実行していた  
   -> `fd_in == 0` のとき `fd0` を閉じてしまう

4. `stdin_test` は `getchar()`（= `fs_read(fd=0,...)`）を呼ぶ  
   -> `fd0` が閉じられているため、ファイル入力ではなくコンソール側待機に見える挙動になる

### 参照イメージ（不具合発生時）

```text
before "<":
  fd0: (unbound) -> console fallback

open("/tmp/test.txt") returns 0:
  fd0 -> /tmp/test.txt

dup2(0,0):
  fd0 -> /tmp/test.txt   (no-op)

fs_close(0):   <-- bug
  fd0: closed/unbound

stdin_test -> getchar() -> read(fd0)
  => fileではなく console 側待機に見える
```

## 根本原因
`dup2` 後の後始末で「`old_fd == new_fd` かつそれが標準FD（0/1）の場合」を考慮せず、  
無条件で一時FDを `close` していた実装不備。

## 対策
### 1. `fd_in == 0` / `fd_out == 1` のときは追加 close しない
対象: `src/user/apps/shell/main.c`

```c
if (fd_in != 0) {
    if (dup2(fd_in, 0) < 0) { ... }
    fs_close(fd_in);
}
```

```c
if (fd_out != 1) {
    if (dup2(fd_out, 1) < 0) { ... }
    fs_close(fd_out);
}
```

### 2. 実行後の復帰は `fs_close(0/1)` で統一
対象: `src/user/apps/shell/main.c`

- リダイレクト実行が終わったら `fs_close(0)` / `fs_close(1)` を実行
- 次コマンドは未割当標準FDとして console fallback に戻る

### 参照イメージ（対策後）

```text
redirect "< file":
  open(file) -> fdX
  dup2(fdX,0)
  if fdX != 0: close(fdX)
  run command (read from fd0=file)
  close(0)  # restore to unbound console fallback
```

## 結果
- `stdin_test < /tmp/test.txt` が期待どおり完了
- RAMFS/PFS いずれでも同様に動作
- `fd0/1` を基準とした標準入出力リダイレクトが安定化
