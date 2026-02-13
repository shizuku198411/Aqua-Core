# VFS(/) write failed 調査記録

## 概要
`/tmp` には書き込める一方で、`/` への書き込み時に `write failed` となる事象が発生した。  
調査の結果、PFS同期処理で巨大ローカル変数をカーネルスタックへ確保していたことが主因だった。

## 発生事象
- `/tmp` への書き込みは成功
- `/` への書き込みのみ失敗
- shell上では `write failed` / `open failed` と表示される

```text
aqua-core:$ write /tmp/a ok
aqua-core:$ write /a ok
write failed
```

## 調査
`fs.c` の PFS同期経路とカーネルスタックサイズ、トラップ/スケジューラ周辺を中心に確認。

- `pfs_sync()` / `nodefs_init_instance()` のローカル変数サイズ
- `struct process.stack` のサイズ
- `current_proc` / pid の不整合発生有無
- MMIOアクセスに必要なページマップ有無

## 原因特定に至った観測結果
1. 問題は PFS(= `/`) のみで発生
   -> RAMFS(`/tmp`) では同期処理が不要で再現しない

2. `pfs_sync()` と `nodefs_init_instance()` で `struct pfs_image` をローカル確保
   -> `pfs_image` は `nodes[FS_MAX_NODES]` を含み、数十KB規模

3. 1プロセスあたりのカーネルスタックは 8KB
   -> 巨大ローカル変数によりスタックオーバーフローし、`procs` 近傍を破壊

4. メモリ破壊後に `current_proc` / pid などの整合が崩れ、結果として `fs_write` が失敗

## 根本原因
PFS同期処理において、カーネルスタック容量を超えるローカルバッファを確保していた設計不備。  
加えて、trap/MMIO 周辺の防御不足が重なると失敗が顕在化しやすかった。

## 対策
### 1. `pfs_image` の静的ワークバッファ化
- `struct pfs_image` をローカルから静的領域へ移動
- `pfs_sync()` / `nodefs_init_instance()` は同一ワークバッファを利用

```c
// src/kernel/fs/fs.c
static struct pfs_image pfs_work_img;
```

### 2. trap処理の堅牢化
- `kernel_entry` 先頭で SIE をクリア
- `trap_handler` で trap frame 所有プロセス復元と `current_proc/sscratch` 同期を強化

### 3. MMIO領域の明示マッピング
- `create_process()` で `MMIO_BASE..MMIO_END` を `PAGE_R|PAGE_W` で map
- virtio notify 時のページフォルト回避

## 結果
- `/tmp` のみ成功、`/` で失敗する事象は解消
- PFS同期時のメモリ破壊が発生しない構成になった
- VFS(`/` + `/tmp`) 共存での書き込みが安定化
