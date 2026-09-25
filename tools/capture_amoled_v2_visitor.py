#!/usr/bin/env python3
"""Exercise the V2 visitor prompt, pair talk, and exit; save raw serial logs."""

import argparse
from datetime import datetime
from pathlib import Path
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem2101")
    parser.add_argument("--timeout", type=int, default=100)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.timeout < 45:
        parser.error("--timeout must be at least 45 seconds")
    output = args.output or (Path(__file__).resolve().parent / "out" /
                             f"amoled-v2-visitor-{datetime.now():%Y%m%d-%H%M%S}.log")
    output.parent.mkdir(parents=True, exist_ok=True)

    device = serial.Serial(port=None, baudrate=115200, timeout=0.2,
                           write_timeout=1)
    device.dtr = False
    device.rts = False
    device.port = args.port
    try:
        device.open()
    except serial.SerialException as exc:
        print(f"Cannot open {args.port}: {exc}", file=sys.stderr)
        return 2

    print(f"Capturing {args.port} to {output}", flush=True)
    phase = "ping"
    deadline = time.monotonic() + 15
    next_send = 0.0
    seen = set()
    milestone_order = []
    line_buffer = bytearray()
    try:
        with output.open("wb") as log:
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_send:
                    command = {"ping": "diag ping\n",
                               "prompt": "diag prompt 1\n",
                               "accept": "diag accept\n"}.get(phase)
                    if command:
                        device.write(command.encode("ascii"))
                        next_send = now + 2
                        if phase in ("prompt", "accept"):
                            phase += "-waiting"
                chunk = device.read(device.in_waiting or 1)
                if not chunk:
                    continue
                log.write(chunk)
                log.flush()
                for byte in chunk:
                    if byte not in (10, 13):
                        if len(line_buffer) < 4096:
                            line_buffer.append(byte)
                        continue
                    if not line_buffer:
                        continue
                    line = line_buffer.decode("utf-8", errors="replace")
                    line_buffer.clear()
                    if "[DiagCmd] pong" in line and phase == "ping":
                        phase = "prompt"
                        deadline = time.monotonic() + args.timeout
                    if "[DiagCmd] prompt kind=1 result=rejected" in line or \
                            "[DiagCmd] accept result=rejected" in line:
                        print(f"Diagnostic request rejected: {line}", file=sys.stderr)
                        return 3
                    if "[FriendDiag]" in line:
                        print(line[line.index("[FriendDiag]"):], flush=True)
                    if "[FriendDiag] prompt ready kind=1" in line:
                        seen.add("prompt")
                        if phase == "prompt-waiting":
                            phase = "accept"
                    if "[FriendDiag] accepted kind=1" in line:
                        seen.add("accepted")
                        milestone_order.append("accepted")
                        phase = "entry"
                    if "[FriendDiag] arrival host-approach kind=1" in line:
                        seen.add("host-approach")
                        milestone_order.append("host-approach")
                    if "[FriendDiag] arrival host-ready kind=1" in line:
                        seen.add("host-ready")
                        milestone_order.append("host-ready")
                    if "[FriendDiag] arrival visitor-visible kind=1" in line:
                        seen.add("visitor-visible")
                        milestone_order.append("visitor-visible")
                    if "[FriendDiag] arrival visitor-entered kind=1" in line:
                        seen.add("entered")
                        milestone_order.append("entered")
                        if phase == "entry":
                            # The firmware starts the arrival conversation as
                            # part of finishVisitorEntry(). Sending another
                            # pair command here would race and be rejected.
                            phase = "waiting-talk"
                    if "[FriendDiag] talk positioned kind=1" in line:
                        seen.add("talk")
                    if "[FriendDiag] arrival talk-ready kind=1" in line:
                        seen.add("talk-ready")
                        milestone_order.append("talk-ready")
                        phase = "exit"
                    if "[FriendDiag] visitor-exit begin kind=1" in line:
                        seen.add("exit")
                    if "[FriendDiag] visitor-exit finish kind=1" in line:
                        seen.add("finish")
                    if "[FriendDiag] complete kind=1" in line:
                        seen.add("complete")
                        required = {"prompt", "accepted", "host-approach",
                                    "host-ready", "visitor-visible", "entered",
                                    "talk", "talk-ready", "exit", "finish",
                                    "complete"}
                        missing = required - seen
                        if missing:
                            print(f"Lifecycle incomplete: missing={sorted(missing)}",
                                  file=sys.stderr)
                            return 4
                        expected = ["accepted", "host-approach", "host-ready",
                                    "visitor-visible", "entered", "talk-ready"]
                        positions = [milestone_order.index(name)
                                     for name in expected]
                        if positions != sorted(positions):
                            print("Lifecycle order invalid: "
                                  f"milestones={milestone_order}",
                                  file=sys.stderr)
                            return 4
                        print("Visitor lifecycle captured", flush=True)
                        return 0
    except (OSError, serial.SerialException) as exc:
        print(f"Serial capture stopped: {exc}", file=sys.stderr)
        return 5
    finally:
        device.close()
        print(f"Log: {output}", flush=True)
    print(f"Timed out in phase={phase}, seen={sorted(seen)}", file=sys.stderr)
    return 6


if __name__ == "__main__":
    raise SystemExit(main())
