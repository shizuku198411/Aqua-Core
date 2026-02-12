# shell連続入力での入力停止 調査記録

## 概要
shellで文字を連続入力すると、途中から文字表示/Enter実行ともに反応しない事象が発生した。  
調査の結果、入力自体はUARTから取り込めていたが、カーネル内部の`procs[]`管理領域が破壊され、実行中プロセス状態が不正化していたことが原因だった。

## 発生事象
- shellで一定文字数入力すると、ある時点以降で表示が止まる
- その後、追加入力やEnterが効かないように見える
```
aqua-core:$ ffffffffffffffffffffffffffffffffffffffffffffffffff
-> 以降入力をしてもターミナルの反応が無くなる
```
- CPU100%張り付きといった事象は無し

## 調査
`gdb-multiarch`でQEMU接続、以下を中心に確認。

- `trap_handler`で`scause/stval/sepc`を継続監視
- `input_count/input_head/input_tail`の推移確認
- `current_proc`と`procs[1]`（shellプロセス）の状態確認
- `watch -location procs[1].state`で破壊タイミングの特定

## 原因特定に至った観測結果
1. 入力停止時でも`input_tail`(Consle Buffer末尾)は増加  
   -> `poll_console_input()`は入力を受信できている
   ```bash
   # 正常時: 5文字入力
   (gdb) p input_tail
   $3 = 5
   # 正常時: 追加で5文字入力
   (gdb) p input_tail
   $5 = 10

   # 事象発生時: 42文字入力、表示上は40文字で停止
   (gdb) p input_tail
   $7 = 42
   ```
   ```c
   // src/kernel/trap/syscall_console.c
   input_buf[input_tail] = (char) ch;
   input_tail = (input_tail + 1) % sizeof(input_buf);
   input_count++;
   ```

2. 停止時に`procs[1].state`が`2(PROC_WAITTING)`から`0(PROC_UNUSED)`へ遷移  
   -> 正常な遷移ではない  
   -> `0(PROC_UNUSED)`により、shellプロセスがスケジューラの実行対象から外れたことで、  
      文字表示含むshellプロセスの動作が開始されなくなった
   ```bash
   # 正常時
   (gdb) p procs[1].state
   $2 = 2   # PROC_WAITING

   # 事象発生時
   (gdb) p procs[1].state
   $3 = 0   # PROC_UNUSED
   ```
   ```c
   // src/include/process.h
   #define PROC_UNUSED   0
   #define PROC_WAITTING 2
   ```

3. `watch procs[1].state`のヒット位置は`kernel_entry`内のレジスタ保存命令  
   -> トラップフレーム保存先が`procs[1]`ヘッダに重なって上書き
   ```bash
   (gdb) watch -location procs[1].state
   # 文字連続入力後にヒット
   Old value = 2
   New value = 0
   0x802002a6 in kernel_entry () at ./src/kernel/kernel.c:20
   # -> kernel_entryはtrapへ入る際のスタック一時保管/復元といった処理
   #    プロセスのステータスを含む領域にはアクセスしないが、
   #    kernel_entryにより書き換えが発生
  

   (gdb) x/10i $pc
   => 0x802002a6 <kernel_entry+38>: sw t3,24(sp)
      0x802002a8 <kernel_entry+40>: sw t4,28(sp)
      ...
   ```
   ```c
   // src/kernel/kernel.c (事象発生時のレジスタ退避処理)
   "sw t3,  4 * 6(sp)\n"
   "sw t4,  4 * 7(sp)\n"
   ```

4. 破壊時の`sp`が`procs[1]`近傍  
   -> `sp/sscratch`運用不整合が直接原因
   ```bash
   (gdb) p/x &procs[1]
   $1 = 0x80234fb8
   (gdb) p/x &procs[1].state
   $2 = 0x80234fbc

   # watchヒット時
   (gdb) info reg sp
   sp = 0x80234fa8
   ```
   `sp` が `procs[1]` 先頭直前を指しており、この位置へのレジスタ退避(`sw ... (sp)`)を行ってしまい、`procs[1].state` などの管理情報が破壊された。

## 根本原因
トラップ入口(`kernel_entry`)での`sp`と`sscratch`の扱い、および`sscratch`再設定タイミングの不整合により、  
タイマ割り込み時のトラップフレーム保存先が不正化し、`procs[]`ヘッダを書き潰していた。

特に以下が組み合わさって再現した。
- `kernel_entry`の`sp/sscratch`切替が常時実行される経路
- `sscratch`が一時的な`sp`値で更新される経路
- その状態で次のトラップが発生し、`procs[1].state`/`wait_reason`等を上書き

## 対策
### 1. トラップ入口での`sp/sscratch`切替条件を修正
U-mode由来トラップ時のみ`csrrw sp, sscratch, sp`を実行し、  
S-mode由来トラップ時は現在のカーネル`sp`を維持するよう変更。

対象: `src/kernel/kernel.c`

```c
"csrr t0, sstatus\n"
"andi t0, t0, 0x100\n"
"bnez t0, 1f\n"
"csrrw sp, sscratch, sp\n"
"j 2f\n"
"1:\n"
"csrw sscratch, sp\n"
"2:\n"
```

### 2. `sscratch`を安定したカーネルスタック先頭で維持
`yield()`と`trap_handler`復帰直前で、`sscratch`に
`&current_proc->stack[sizeof(current_proc->stack)]`を設定するよう統一。

対象:
- `src/kernel/proc/process.c`
- `src/kernel/trap/trap_handler.c`

```c
WRITE_CSR(sscratch, (uint32_t) &current_proc->stack[sizeof(current_proc->stack)]);
```

### 3. 初期化タイミングの補強
初回割り込み前に`sscratch`が有効値になるよう、以下で初期化。

対象:
- `src/kernel/kernel.c` (`boot`)
- `src/kernel/kernel.c` (`kernel_bootstrap`内、timer有効化前)

## 結果
- `f`連続入力時に発生していた`PROC_UNUSED`ガードPANICは再発しなくなった
- 調査中に確認したレジストリ/メモリ周りは意図した値を保持し続けることを確認
