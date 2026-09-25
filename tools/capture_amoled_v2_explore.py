#!/usr/bin/env python3
"""Drive a V2 debug expedition over USB Serial/JTAG and capture raw logs."""

import argparse
from datetime import datetime
from pathlib import Path
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem2101")
    parser.add_argument("--area", type=int, choices=range(6), default=0)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    if args.timeout < 15:
        parser.error("--timeout must be at least 15 seconds")
    output = args.output or (
        Path(__file__).resolve().parent / "out" /
        f"amoled-v2-explore-{datetime.now():%Y%m%d-%H%M%S}.log"
    )
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
    deadline = time.monotonic() + args.timeout
    next_ping = 0.0
    command_sent = False
    started = False
    completed_at = None
    line_buffer = bytearray()
    try:
        with output.open("wb") as log:
            while time.monotonic() < deadline:
                now = time.monotonic()
                if not command_sent and now >= next_ping:
                    device.write(b"diag ping\n")
                    next_ping = now + 2
                chunk = device.read(device.in_waiting or 1)
                if not chunk:
                    if completed_at is not None and now - completed_at >= 1:
                        print("Encounter frame captured", flush=True)
                        return 0
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
                    if "[DiagCmd] pong" in line and not command_sent:
                        device.write(f"diag explore {args.area}\n".encode("ascii"))
                        command_sent = True
                    elif "[DiagCmd] explore area=" in line:
                        if "result=started" in line:
                            started = True
                            print("Automatic exploration started", flush=True)
                        elif "result=rejected" in line:
                            print("Device rejected exploration; check home scene, "
                                  "team HP and area unlock", file=sys.stderr)
                            return 3
                    elif "[EncounterPerf]" in line and started:
                        completed_at = time.monotonic()
                if completed_at is not None and time.monotonic() - completed_at >= 1:
                    print("Encounter frame captured", flush=True)
                    return 0
    except (OSError, serial.SerialException) as exc:
        print(f"Serial capture stopped: {exc}", file=sys.stderr)
        return 4
    finally:
        device.close()
        print(f"Log: {output}", flush=True)

    print("Timed out before first encounter frame" if started
          else "Timed out waiting for the V2 debug command response",
          file=sys.stderr)
    return 5


if __name__ == "__main__":
    raise SystemExit(main())
