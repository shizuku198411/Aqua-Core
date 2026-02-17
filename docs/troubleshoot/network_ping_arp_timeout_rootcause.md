# virtio-net ping failed(-8) 調査記録

## 概要
`ping 10.0.2.1` 実行時に `failed(-8)` で失敗し、ARP 解決がタイムアウトする事象が発生した。  
調査の結果、virtio-net の TX/RX で使う `virtio_net_hdr` サイズ不一致により、Ethernet フレーム先頭が 2 byte ずれていたことが根本原因だった。

## 発生事象
- カーネル側では ARP リトライ後に `ping failed (-8)` で終了
- ホスト側 `tcpdump -ni tap0 'arp or icmp'` では ARP/ICMP が観測できない
- ただし virtio-net 初期化自体は成功し、MAC 取得ログは出る

```text
aqua-core:/$ ping 10.0.2.1
PING 10.0.2.1: 13 data bytes
ping failed (-8)
```

## 調査
以下の順で切り分けを実施。

1. TAP 側セットアップ確認
2. `run-tap` / `run-usernet` の起動オプション差分確認
3. ARP 送信時のカーネルログ追加
4. `run-tap-dump` で pcap を取得して L2 フレームを直接確認
5. virtio-net ヘッダ定義 (`struct virtio_net_hdr`) と RX 長さ計算を確認

## 原因特定に至った観測結果
1. ARP 要求はカーネルから送っているつもりでも、ホスト tcpdump で ARP として認識されない  
2. `run-tap-dump` の pcap では、送信フレームのヘッダが 2 byte ずれて解釈される

```text
06:36:04 ... ethertype Unknown (0x6171)
... payload: "uacore-net-tx-probe..."
```

3. 上記は「Ethernet ヘッダ先頭が正しい位置にない」時の典型パターン  
4. `virtio_net_hdr` を 10 byte 扱いしていた時期があり、デバイス側期待の 12 byte と不一致になっていた  
5. ヘッダ長不一致により、実フレームが 2 byte シフトして TX/RX 双方で崩れる

ASCIIイメージ:

```text
期待(12B hdr):
[virtio hdr 12][eth dst 6][eth src 6][etype 2][payload...]

実際(10B hdrで構築):
[virtio hdr 10][eth dst 6][eth src 6][etype 2][payload...]
               ^ deviceはここから12B読み、以降が2Bずれて解釈される
```

## 根本原因
virtio-net パスにおいて `struct virtio_net_hdr` のサイズがデバイス期待と一致していなかった実装不整合。  
結果として Ethernet フレームが壊れ、ARP 応答を受信できず `NET_ERR_ARP_TIMEOUT(-8)` に到達した。

## 対策
### 1. `virtio_net_hdr` を 12 byte 定義へ統一
`num_buffers` を含む定義に戻し、TX/RX のオフセット計算を一致させた。

```c
struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));
```

### 2. 検証導線の維持
- `Makefile` に `run-tap-dump` を維持し、`/tmp/aquacore-tap.pcap` で再現時の L2 観測を可能化
- 通常運用ではデバッグログを整理し、必要時のみ pcap で確認する運用に変更

## 結果
修正後は ARP 解決が成功し、`ping 10.0.2.1` に応答が返ることを確認。

```text
aqua-core:/$ ping 10.0.2.1
PING 10.0.2.1: 13 data bytes
13 bytes from 10.0.2.1: icmp_seq=1 time=6 ms
```
