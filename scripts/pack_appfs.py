#!/usr/bin/env python3
import argparse
import os
import struct
import sys

BLOCK_SIZE = 512
APPFS_MAGIC = 0x41504653
APPFS_START_BLOCK = 192
FS_NAME_MAX = 16

APPS = [
    "shell",
    "ipc_rx",
    "ps",
    "date",
    "ls",
    "mkdir",
    "rmdir",
    "touch",
    "rm",
    "write",
    "cat",
    "kill",
    "kernel_info",
    "bitmap",
    "ping",
    "udp_send",
    "nslookup",
]


def pad_name(name: str) -> bytes:
    b = name.encode("ascii")
    if len(b) >= FS_NAME_MAX:
        raise ValueError(f"name too long for appfs: {name}")
    return b + b"\x00" * (FS_NAME_MAX - len(b))


def build_image(bin_dir: str) -> bytes:
    blobs = []
    for app in APPS:
        path = os.path.join(bin_dir, f"{app}.bin")
        with open(path, "rb") as f:
            data = f.read()
        blobs.append((app, data))

    header_size = 8
    entry_size = FS_NAME_MAX + 4 + 4
    table_size = header_size + entry_size * len(blobs)

    entries = []
    payload = bytearray()
    for app, data in blobs:
        off = table_size + len(payload)
        entries.append((app, off, len(data)))
        payload.extend(data)

    out = bytearray()
    out.extend(struct.pack("<II", APPFS_MAGIC, len(entries)))
    for app, off, size in entries:
        out.extend(pad_name(app))
        out.extend(struct.pack("<II", off, size))
    out.extend(payload)
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--disk", required=True)
    ap.add_argument("--bin-dir", required=True)
    args = ap.parse_args()

    if not os.path.exists(args.disk):
        print(f"disk not found: {args.disk}", file=sys.stderr)
        return 1

    image = build_image(args.bin_dir)
    start = APPFS_START_BLOCK * BLOCK_SIZE

    with open(args.disk, "r+b") as f:
        f.seek(0, os.SEEK_END)
        disk_size = f.tell()
        if start + len(image) > disk_size:
            print("appfs image does not fit in disk.img", file=sys.stderr)
            return 1
        f.seek(start)
        f.write(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
