#!/usr/bin/env python3
"""Measure fixed AMOLED scrolling paths on the host, optionally against a prior binary."""
import argparse
import json
import re
import statistics
import subprocess
from datetime import date
from pathlib import Path

from amoled_host import ROOT, build_amoled_host


def measure(binary):
    output = subprocess.check_output([str(binary), str(ROOT / "data")],
                                     cwd=ROOT, text=True)
    rows = re.findall(r"SCROLL area=(\d+) median_us=([\d.]+) p95_us=([\d.]+) hash=([0-9a-f]+)", output)
    if len(rows) != 6:
        raise RuntimeError("Expected all six scrolling maps")
    return {area: {"median_us": float(median), "p95_us": float(p95), "hash": digest}
            for area, median, p95, digest in rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".stick-local/artifacts" /
                        f"amoled-scroll-benchmark-{date.today():%Y%m%d}")
    parser.add_argument("--reference-binary", type=Path)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("--repeats must be positive")
    destination = args.output_dir.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    binary = build_amoled_host(destination / "scroll-current",
                              ["tools/amoled_explore_scroll_host.cpp"])
    binaries = {"current": binary}
    if args.reference_binary:
        binaries["reference"] = args.reference_binary.resolve()
    runs = {name: [] for name in binaries}
    for repeat in range(args.repeats):
        # Alternate order to reduce systematic thermal/scheduling bias.
        order = list(binaries.items())
        for name, executable in (order if repeat % 2 == 0 else order[::-1]):
            runs[name].append(measure(executable))
    results = {}
    for area in runs["current"][0]:
        hashes = {run[area]["hash"] for group in runs.values() for run in group}
        if len(hashes) != 1:
            raise RuntimeError(f"Pixel output changed for area {area}: {hashes}")
        results[area] = {"hash": hashes.pop()}
        for name, group in runs.items():
            results[area][name] = {
                metric: statistics.median(run[area][metric] for run in group)
                for metric in ("median_us", "p95_us")}
        if "reference" in runs:
            results[area]["median_reduction_percent"] = round(100 * (
                1 - results[area]["current"]["median_us"] /
                results[area]["reference"]["median_us"]), 1)
    report = {"scope": "Host rendering only; excludes SPI, PSRAM and device scheduling costs.",
              "repeats": args.repeats, "areas": results, "runs": runs}
    (destination / "scroll-benchmark.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
