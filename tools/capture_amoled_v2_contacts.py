#!/usr/bin/env python3
"""Trigger V2 debug contact events and capture their full serial lifecycle."""

import argparse
from datetime import datetime
from pathlib import Path
import re
import sys
import time

import serial


EVENTS = {"play": 1, "gift": 2, "explore": 3}
FRIEND_LOG = re.compile(r"\[FriendDiag\] (.+?) kind=(\d+)(.*)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem2101")
    parser.add_argument("--event", choices=("all", *EVENTS), default="all")
    parser.add_argument("--timeout", type=int, default=150,
                        help="maximum seconds per event")
    parser.add_argument("--route-hold", type=int, default=8,
                        help="seconds to observe the route before returning")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.timeout < 40 or args.route_hold < 3:
        parser.error("--timeout must be >= 40 and --route-hold must be >= 3")

    events = list(EVENTS.values()) if args.event == "all" else [EVENTS[args.event]]
    output = args.output or (Path(__file__).resolve().parent / "out" /
                             f"amoled-v2-contacts-{datetime.now():%Y%m%d-%H%M%S}.log")
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
    current = 0
    phase = "ping"
    deadline = time.monotonic() + 15
    next_send = 0.0
    route_at = None
    seen = set()
    early_completion = False
    buffer = bytearray()
    try:
        with output.open("wb") as log:
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_send:
                    command = None
                    if phase == "ping":
                        command = "diag ping\n"
                    elif phase == "starting":
                        command = f"diag contact {events[current]}\n"
                        phase = "running"
                    elif phase == "route" and route_at is not None and \
                            now - route_at >= args.route_hold:
                        command = "diag contact return\n"
                    if command:
                        device.write(command.encode("ascii"))
                        next_send = now + 2
                chunk = device.read(device.in_waiting or 1)
                if not chunk:
                    continue
                log.write(chunk)
                log.flush()
                for byte in chunk:
                    if byte not in (10, 13):
                        if len(buffer) < 4096:
                            buffer.append(byte)
                        continue
                    if not buffer:
                        continue
                    line = buffer.decode("utf-8", errors="replace")
                    buffer.clear()
                    if "[DiagCmd] pong" in line and phase == "ping":
                        phase = "starting"
                        deadline = time.monotonic() + args.timeout
                    if phase not in ("running", "route", "returning"):
                        continue
                    kind = events[current]
                    match = FRIEND_LOG.search(line)
                    if match and int(match.group(2)) == kind:
                        label, detail = match.group(1).split()[0], match.group(3)
                        seen.add(label)
                        print(f"{kind}: {label}{detail}", flush=True)
                        if kind == 3 and label == "route" and "started=1" in detail:
                            route_at = time.monotonic()
                            phase = "route"
                        if label == "complete":
                            expected = {"begin", "accepted", "complete"}
                            if kind == 3:
                                expected |= {"departure", "route", "return", "home-return"}
                            missing = expected - seen
                            if missing or "team=1" not in detail:
                                print(f"Event {kind} incomplete: missing={sorted(missing)}, "
                                      f"result={detail.strip()}", file=sys.stderr)
                                return 3
                            if kind == 3 and "phase=0" not in detail and \
                                    "phase=7" not in detail:
                                # complete fires when RETURN_FADE_OUT finishes
                                # (phase 7); settling there is by design.
                                early_completion = True
                                print("Invitation settled before the return animation finished",
                                      file=sys.stderr, flush=True)
                            print(f"Event {kind} complete", flush=True)
                            current += 1
                            if current == len(events):
                                if early_completion:
                                    print("Events completed, but invitation settlement was early",
                                          file=sys.stderr)
                                    return 3
                                print("All requested contact events completed", flush=True)
                                return 0
                            phase = "starting"
                            seen.clear()
                            route_at = None
                            deadline = time.monotonic() + args.timeout
                    if f"[DiagCmd] contact kind={kind} result=rejected" in line:
                        print(f"Event {kind} rejected; inspect {output}", file=sys.stderr)
                        return 4
                    if "[DiagCmd] contact return result=started" in line:
                        phase = "returning"
                    if "[DiagCmd] contact return result=rejected" in line:
                        print("Return not yet available; retrying", flush=True)
    except (OSError, serial.SerialException) as exc:
        print(f"Serial capture stopped: {exc}", file=sys.stderr)
        return 5
    finally:
        device.close()
        print(f"Log: {output}", flush=True)
    print(f"Timed out during event {events[current]} ({phase})", file=sys.stderr)
    return 6


if __name__ == "__main__":
    raise SystemExit(main())
