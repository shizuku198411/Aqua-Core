# getcwd syscallでの Store/AMO page fault 調査記録

## 概要
`getcwd` syscall を追加し、shellプロンプトを `aqua-core:<cwd>$` 表示へ変更したところ、起動後に `Store/AMO page fault` が発生した。  
調査の結果、`getcwd` ハンドラでユーザポインタへ書き込む際に `SSTATUS_SUM` を有効化していなかったことが原因だった。

## 発生事象
- `getcwd` 呼び出し直後に PANIC
- エラー内容:

    ```text
    PANIC: src/kernel/trap/trap_handler.c:88: Store/AMO page fault.
    scause=0000000f, stval=01013024, sepc=8020023e
    ```

- `stval` はユーザ空間アドレス (`0x01013024`) を指していた

## 調査
GDBで `scause/stval/sepc/satp` と対象VAのページテーブルを手動ウォークして確認。

- `stval` の VA を `vpn1/vpn0` 分解
- `satp` から root table を求めて `pte1/pte0` を確認
- `V/R/W/X/U` ビットを個別に確認

## 原因特定に至った観測結果
1. `stval=0x01013024` はユーザアドレス  
   -> `getcwd` の出力バッファ（ユーザ側 `char cwd_path[]`）と整合

2. 該当PTEは有効で、`U/R/W/X/V` すべて立っていた  
   ```text
    (gdb) p $pte0 & 1           # V
    $9 = 1
    (gdb) p ($pte0 >> 1) & 1    # R
    $10 = 1
    (gdb) p ($pte0 >> 2) & 1    # W
    $11 = 1
    (gdb) p ($pte0 >> 3) & 1    # X
    $12 = 1
    (gdb) p ($pte0 >> 4) & 1    # U
    $13 = 1
   ```
   -> ページ未マップ・PTE権限不足ではない

3. `syscall_handle_getcwd()` だけが `SSTATUS_SUM` を立てずにユーザポインタへ `strcpy_s` していた  
   -> S-modeからUページへの store が禁止され、`Store/AMO page fault` が発生

    ```c
    // 修正前イメージ
    void syscall_handle_getcwd(struct trap_frame *f) {
        char *cwd_path = (char *) f->a0;
        strcpy_s(cwd_path, FS_PATH_MAX, current_proc->cwd_path); // <- user ptr write
    }
    ```

## 根本原因
S-modeハンドラでユーザメモリへアクセスする際の `SSTATUS_SUM` 制御漏れ。  
`open/read/write` 系ハンドラは `SUM` を有効化していたが、`getcwd` ハンドラには同等処理がなかった。

## 対策
`syscall_handle_getcwd()` に `SSTATUS_SUM` の save/restore を追加。

対象: `src/kernel/trap/syscall_fs.c`

```c
void syscall_handle_getcwd(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    char *cwd_path = (char *) f->a0;
    if (!cwd_path) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    strcpy_s(cwd_path, FS_PATH_MAX, current_proc->cwd_path);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = 0;
}
```

## 結果
- `getcwd` syscall 実行時の `Store/AMO page fault` は解消
- shell の `aqua-core:<cwd>$` 表示が動作
- 同種の syscall 追加時は「ユーザポインタアクセス時の `SSTATUS_SUM` 有効化」を確認観点として明確化できた

