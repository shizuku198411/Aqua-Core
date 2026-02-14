# VFS / RAMFS / VirtIO Block Storage

対象:

- `src/kernel/fs/fs.c`
- `src/kernel/fs/blockdev_virtio.c`
- `src/include/fs.h`
- `src/include/fs_internal.h`
- `src/include/blockdev.h`
- `src/kernel/trap/syscall_fs.c`
- `scripts/start.sh`

関連:

- [Syscall](./syscall.md)
- [Process Management](./process-management.md)
- [Kernel Bootstrap](./kernel-bootstrap.md)
- [Troubleshoot: VFS Write Failed](./troubleshoot/vfs_write_failed_rootcause.md)

## 概要

現在のファイルシステムは VFS 化されており、以下の2つを共存させています。

- `/` : 永続バックエンド（PFS on block device）
- `/tmp` : 揮発バックエンド（RAMFS）

ブート時 `fs_init()` で以下を実施します。

1. `blockdev_init()`（virtio-blk 初期化）
2. 永続ルートFSインスタンス初期化（PFS読み込み/フォーマット）
3. 揮発tmpfsインスタンス初期化
4. `vfs_mount("/", ...)` と `vfs_mount("/tmp", ...)`

## VFSレイヤ

実装: `src/kernel/fs/fs.c`

主要要素:

- `struct vfs_mount mounts[VFS_MOUNT_MAX]`
  - マウントポイントと各FS実装（ops + ctx）を保持
- `struct vfs_fd fd_table[PROCS_MAX][FS_FD_MAX]`
  - PIDごとのFDテーブル
- `vfs_resolve_mount(path, &mount, &subpath)`
  - 最長一致でマウントを解決
  - 例: `/tmp/a` は `/tmp` マウントへ、`/a` は `/` マウントへ

### VFSの格納形式（実体）

VFSは「マウントテーブル + FDテーブル」で管理しています。

1. マウントテーブル: `struct vfs_mount mounts[VFS_MOUNT_MAX]`

- `used`: エントリ使用中か
- `path`: マウントポイント（例: `/`, `/tmp`）
- `path_len`: マウントポイント長
- `ops`: FS実装の関数テーブル（open/read/write/...）
- `ctx`: FS実装コンテキスト（`struct nodefs *`）

2. FDテーブル: `struct vfs_fd fd_table[PROCS_MAX][FS_FD_MAX]`

- PID単位でFD空間を分離
- 各FDは以下を保持
  - `mount_idx`: どのマウントに属するか
  - `node_index`: バックエンドFS内ノード番号
  - `offset`: 現在オフセット
  - `flags`: open時フラグ

この構造により、`fs_read/fs_write` は「mount_idx から backend ops を引く」だけで、PFS/RAMFSを透過的に扱えます。

```
process(pid)
  └─ fd_table[pid][fd]  (vfs_fd: プロセスごとのFDスロット)
       ├─ used
       ├─ mount_idx  -----------+
       ├─ node_index ---------+ |
       ├─ offset              | |
       └─ flags               | |
                              | |
mount_table[mount_idx]  <-----+ |
  ├─ mount point ("/", "/tmp")  |
  ├─ fs ops (open/read/write...)|
  └─ backend private data       |
                                |
backend filesystem (PFS/RAMFS)<-+
  └─ node[node_index]  (実ファイル/ディレクトリ実体)
```

### マウント解決の処理

`vfs_resolve_mount()` は「最長一致」を使います。

判定手順:

1. `mounts[]` を走査して `vfs_match_mount()` で候補抽出
2. `path_len` が最大の候補を採用
3. 採用マウントに対する `subpath` を生成

`subpath` 生成例:

- 入力 `/tmp/log/a.txt`
- 採用 mount `/tmp`
- backendへ渡す subpath `/log/a.txt`

ルート特例:

- mount が `/` の場合は subpath は元pathそのまま（`/a`, `/tmp/x` 等）

境界条件:

- `/tmp2` は `/tmp` マウントにマッチしない
  - `vfs_match_mount()` が「次文字は `\\0` か `/`」を要求するため

### open/read/write時のVFS処理

open:

1. `vfs_resolve_mount(path)` で mount と subpath 決定
2. `mount->ops->open(mount->ctx, subpath, ...)`
3. 返ってきた `node_index/offset` を `vfs_fd` に格納
4. ユーザへFD番号返却

read/write:

1. `fd_table[pid][fd]` を参照
2. `mount_idx` から `mounts[mount_idx]` を取得
3. `mount->ops->read/write(...)` を呼ぶ
4. `vfs_fd.offset` を backend が更新

close:

- `fd_table[pid][fd]` を未使用状態へ戻す

## NodeFS実装（RAMFS/PFS共通）

`nodefs` は単一実装で、`persistent` フラグにより動作を切り替えます。

- `persistent=0` : RAMFS（メモリのみ）
- `persistent=1` : PFS（変更時に blockdev へ同期）

### PFS/RAMFSとの関係

現在の実装では backend はどちらも `nodefs` です。違いは `persistent` のみです。

- `rootfs`:
  - `struct nodefs rootfs;`
  - `persistent=1`
  - mount先: `/`
- `tmpfs`:
  - `struct nodefs tmpfs;`
  - `persistent=0`
  - mount先: `/tmp`

`nodefs_write()` などの共通処理内で、`persistent` を見て同期有無を分岐します。

- `persistent=1`:
  - 変更後 `pfs_sync()` を実行して blockdev へ反映
- `persistent=0`:
  - メモリ更新のみで完了

### ブート時のマウント組み立て

`fs_init()` の実処理は以下です。

1. `blockdev_init()`（virtio-blk初期化）
2. `nodefs_init_instance(&rootfs, 1)`（PFSロード）
3. `nodefs_init_instance(&tmpfs, 0)`（RAMFS初期化）
4. `vfs_mount("/", &nodefs_ops, &rootfs)`
5. ルートFSに `/tmp` ノードがなければ作成
6. `vfs_mount("/tmp", &nodefs_ops, &tmpfs)`

内部構造:

- `struct fs_node nodes[FS_MAX_NODES]`
  - `used`, `type`, `parent`, `name`, `size`, `data[FS_FILE_MAX_SIZE]`

実装済み操作:

- open/close/read/write
- mkdir/readdir
- unlink/rmdir

削除制約:

- open中のファイルは `unlink` 不可
- 非空ディレクトリは `rmdir` 不可

## 永続化フォーマット（PFS）

`persistent` な nodefs は `pfs_image` をブロックデバイスへ保存します。

- `struct pfs_image { magic, nodes[] }`
- マジック: `PFS_MAGIC`
- 同期: `pfs_sync()`
  - `pfs_image` を 512B ブロック単位で `blockdev_write()`
- 復元: `nodefs_init_instance(..., persistent=1)`
  - 先頭から全ブロック読込
  - magic 不一致時は初期化して再同期

### PFSの保存単位

保存は「ファイル単位」ではなく「FSイメージ全体単位」です。

- `nodes[]` 全体を `pfs_image` として直列化
- 512B block へ分割して `blockdev_write()` 連続実行

## VirtIO Block 実装

実装: `src/kernel/fs/blockdev_virtio.c`

方式:

- QEMU `virtio-mmio` modern(v1.0+) を使用
- デバイス探索:
  - base: `0x10001000 + n * 0x1000`
  - magic/version/device/vendor を検証（`version >= 2`）
- キュー:
  - split virtqueue
  - modern queue register を使用
  - `QUEUE_DESC_LOW/HIGH`, `QUEUE_AVAIL_LOW/HIGH`, `QUEUE_USED_LOW/HIGH`, `QUEUE_READY`
- feature negotiation:
  - `DEVICE_FEATURES_SEL` / `DRIVER_FEATURES_SEL` を使用
  - `VIRTIO_F_VERSION_1` を必須でネゴ
  - `FEATURES_OK` セット後に再読込で受理確認
- I/O:
  - `virtio_do_io(VIRTIO_BLK_T_IN/OUT, block, buf)`

QEMU起動条件（`scripts/start.sh`）:

- `-global virtio-mmio.force-legacy=false`
- `-device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0`

## Syscall 経路

- user: `fs_open/fs_read/fs_write/...` (`src/user/runtime/user_syscall.c`)
- kernel trap: `syscall_handle_open/read/write/...` (`src/kernel/trap/syscall_fs.c`)
- fs core: `fs_open/read/write/...` (`src/kernel/fs/fs.c`)

`syscall_fs.c` では user pointer を扱うため `sstatus.SUM` を一時的に有効化しています。

## 今後の拡張候補

- ディレクトリエントリの永続化最適化（全量同期から差分同期へ）
- ジャーナリング/CRC導入
- lock導入（将来マルチコア・並行syscall向け）
- ルートFSをより一般的なオンディスクフォーマットへ置換
