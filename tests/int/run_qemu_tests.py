#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import threading
import time


class Harness:
    def __init__(self, cmd):
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        self.buf = bytearray()
        self.cv = threading.Condition()
        self.reader = threading.Thread(target=self._reader_main, daemon=True)
        self.reader.start()

    def _reader_main(self):
        assert self.proc.stdout is not None
        while True:
            data = self.proc.stdout.read(1)
            if not data:
                with self.cv:
                    self.cv.notify_all()
                return
            with self.cv:
                self.buf.extend(data)
                self.cv.notify_all()

    def mark(self) -> int:
        with self.cv:
            return len(self.buf)

    def wait_for(self, token: bytes, timeout_sec: float, start: int = 0):
        deadline = time.time() + timeout_sec
        with self.cv:
            while True:
                if token in self.buf[start:]:
                    return
                if self.proc.poll() is not None:
                    raise RuntimeError(
                        f"process exited early while waiting for {token!r}\n"
                        f"--- output tail ---\n{self.tail(4000)}"
                    )
                remain = deadline - time.time()
                if remain <= 0:
                    raise TimeoutError(
                        f"timeout waiting for {token!r}\n"
                        f"--- output tail ---\n{self.tail(4000)}"
                    )
                self.cv.wait(timeout=remain)

    def send_line(self, s: str):
        if self.proc.stdin is None:
            raise RuntimeError("stdin is not available")
        self.proc.stdin.write((s + "\n").encode("ascii"))
        self.proc.stdin.flush()

    def tail(self, size: int) -> str:
        b = bytes(self.buf[-size:])
        return b.decode("utf-8", errors="replace")

    def text_from(self, start: int) -> str:
        with self.cv:
            b = bytes(self.buf[start:])
        return b.decode("utf-8", errors="replace")

    def terminate(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=5)


def build_qemu_cmd(args):
    return [
        args.qemu,
        "-machine",
        "virt",
        "-bios",
        "default",
        "-nographic",
        "-serial",
        "mon:stdio",
        "--no-reboot",
        "-global",
        "virtio-mmio.force-legacy=false",
        "-drive",
        f"file={args.disk},if=none,format=raw,id=hd0",
        "-device",
        "virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0",
        "-netdev",
        "user,id=net0",
        "-device",
        "virtio-net-device,bus=virtio-mmio-bus.1,netdev=net0",
        "-kernel",
        args.kernel,
    ]


def run_tests(h: Harness):
    case_history = []

    def run_case(name, fn):
        case_history.append(f"START {name}")
        try:
            fn()
            print(f"[PASS] int: {name}", flush=True)
            case_history.append(f"PASS  {name}")
        except Exception:
            print(f"[FAIL] int: {name}", flush=True)
            case_history.append(f"FAIL  {name}")
            raise

    def run_cmd_case(name: str, cmd: str, tokens: list[bytes], timeout: float = 10.0):
        def _inner():
            start = h.mark()
            h.send_line(cmd)
            for token in tokens:
                h.wait_for(token, timeout, start=start)
            h.wait_for(b"$ ", timeout, start=start)
        run_case(name, _inner)

    run_case("shell_prompt", lambda: h.wait_for(b"$ ", 30))

    # shell itself is validated by interactive prompt availability.
    run_cmd_case("date_app", "date", [b"UTC"])
    run_cmd_case("date_path_app", "/bin/date", [b"UTC"])
    run_cmd_case("ls_app", "ls /", [b"tmp/", b"proc/"])
    run_cmd_case("mkdir_app", "mkdir /tmp/itest_dir", [])
    run_cmd_case("touch_app", "touch /tmp/itest_dir/file1", [])
    run_cmd_case("write_app", "write /tmp/itest_dir/file1 hello", [])
    run_cmd_case("cat_app", "cat /tmp/itest_dir/file1", [b"hello"])
    run_cmd_case("rm_app", "rm /tmp/itest_dir/file1", [])
    run_cmd_case("rmdir_app", "rmdir /tmp/itest_dir", [])
    run_cmd_case("ps_app", "ps", [b"PID\tPPID\tSTATE\tREASON\tEXIT\tCMD"])
    run_cmd_case("bitmap_app", "bitmap", [b"bitmap: total="])
    run_cmd_case("kill_app", "kill 0", [b"kill failed", b"invalid pid specified"])
    run_cmd_case("ipc_rx_app", "ipc_rx sender 9999 1", [b"ipc_send failed"])
    run_cmd_case("ping_app", "ping bad.ip", [b"invalid ipv4: bad.ip"])
    run_cmd_case("udp_send_app", "udp_send 999.1.1.1 1 1 x", [b"invalid ipv4"])
    run_cmd_case("nslookup_app", "nslookup", [b"usage: nslookup <name> [dns-server-ipv4]"])
    run_cmd_case("exit_status_nonzero", "date --fail", [b"date: forced failure", b"exit status: 42"])
    run_cmd_case("echo_app", "echo hello", [b"hello"])

    def case_kernel_info():
        # Validate that exported kernel parameters are present and populated.
        start = h.mark()
        h.send_line("kernel_info")
        expected_lines = [
            b"version       :",
            b"kernel time   :",
            b"total pages   :",
            b"page size     : 4096 bytes",
            b"kernel base   : 0x",
            b"user base     : 0x",
            b"proc max      :",
            b"kernel stack  :",
            b"time slice    :",
            b"timer interval:",
            b"ramfs node max:",
            b"ramfs size max:",
            b"pfs blk count :",
            b"pfs blk size  :",
            b"pfs img blks  :",
            b"pfs img bytes :",
        ]
        for line in expected_lines:
            h.wait_for(line, 10, start=start)
        h.wait_for(b"$ ", 10, start=start)

        text = h.text_from(start)

        def parse_dec(label: str) -> int:
            m = re.search(rf"^{re.escape(label)}\s*:\s*([0-9]+)", text, re.MULTILINE)
            if not m:
                raise AssertionError(f"missing decimal field: {label}")
            return int(m.group(1))

        def parse_hex(label: str) -> int:
            m = re.search(rf"^{re.escape(label)}\s*:\s*0x([0-9a-fA-F]+)", text, re.MULTILINE)
            if not m:
                raise AssertionError(f"missing hex field: {label}")
            return int(m.group(1), 16)

        # Hard/soft validation of runtime-exported kernel parameters.
        total_pages = parse_dec("total pages")
        page_size = parse_dec("page size")
        kernel_base = parse_hex("kernel base")
        user_base = parse_hex("user base")
        proc_max = parse_dec("proc max")
        kernel_stack = parse_dec("kernel stack")
        time_slice = parse_dec("time slice")
        timer_interval = parse_dec("timer interval")
        ramfs_node_max = parse_dec("ramfs node max")
        ramfs_size_max = parse_dec("ramfs size max")
        pfs_blk_count = parse_dec("pfs blk count")
        pfs_blk_size = parse_dec("pfs blk size")
        pfs_img_blks = parse_dec("pfs img blks")
        pfs_img_bytes = parse_dec("pfs img bytes")

        if total_pages <= 0:
            raise AssertionError("total pages must be > 0")
        if page_size != 4096:
            raise AssertionError(f"unexpected page size: {page_size}")
        if kernel_base != 0x80200000:
            raise AssertionError(f"unexpected kernel base: 0x{kernel_base:x}")
        if user_base != 0x01000000:
            raise AssertionError(f"unexpected user base: 0x{user_base:x}")
        if proc_max <= 0:
            raise AssertionError("proc max must be > 0")
        if kernel_stack <= 0 or time_slice <= 0 or timer_interval <= 0:
            raise AssertionError("scheduler/runtime params must be > 0")
        if ramfs_node_max <= 0 or ramfs_size_max <= 0:
            raise AssertionError("ramfs params must be > 0")
        if pfs_blk_count <= 0 or pfs_blk_size <= 0 or pfs_img_blks <= 0 or pfs_img_bytes <= 0:
            raise AssertionError("pfs params must be > 0")
        if pfs_img_bytes != pfs_img_blks * pfs_blk_size:
            raise AssertionError(
                f"pfs img bytes mismatch: {pfs_img_bytes} != {pfs_img_blks}*{pfs_blk_size}"
            )

    run_case("kernel_info_app", case_kernel_info)

    def case_exit_shutdown():
        h.send_line("exit")
        h.wait_for(b"kernel shutdown requested", 10)

    run_case("exit_shutdown", case_exit_shutdown)
    return case_history


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", default="qemu-system-riscv32")
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--disk", required=True)
    args = parser.parse_args()

    cmd = build_qemu_cmd(args)
    h = None
    fail_log_path = os.path.join(os.path.dirname(__file__), "last_failure.log")
    case_history = []
    try:
        h = Harness(cmd)
        case_history = run_tests(h)
        if os.path.exists(fail_log_path):
            os.remove(fail_log_path)
    except Exception as e:
        tail = ""
        if h is not None:
            tail = h.tail(20000)
        with open(fail_log_path, "w", encoding="utf-8") as fp:
            fp.write("QEMU integration test failed\n")
            fp.write(f"command: {' '.join(cmd)}\n")
            fp.write(f"error: {e}\n")
            fp.write("--- case history ---\n")
            for item in case_history:
                fp.write(item + "\n")
            fp.write("--- output tail ---\n")
            fp.write(tail)
            fp.write("\n")
        print(f"[FAIL] int: see {fail_log_path}", flush=True)
        raise
    finally:
        if h is not None:
            h.terminate()


if __name__ == "__main__":
    main()
