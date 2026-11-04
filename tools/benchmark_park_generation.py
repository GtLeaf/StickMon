"""Alternate fixed-seed desktop generation runs; never interpret these as device FPS."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-source', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'.stick-local/artifacts/park-generation-benchmark')
    parser.add_argument('--seeds', type=int, default=1024)
    parser.add_argument('--rounds', type=int, default=3)
    args = parser.parse_args()
    if args.seeds < 1 or args.rounds < 1:
        parser.error('seeds and rounds must be positive')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sources = {'before': args.reference_source.resolve(), 'after': ROOT/'src/game/ExploreMapGenerator.cpp'}
    for label, source in sources.items():
        subprocess.run(['c++', '-std=c++17', '-O2', '-I' + str(ROOT/'src'),
                        str(ROOT/'tools/park_generation_benchmark_host.cpp'), str(source),
                        '-o', str(out/label)], check=True, capture_output=True, text=True)
    expected = None
    stats = {'before': [], 'after': []}
    for index in range(args.rounds):
        for label in (('before', 'after') if index % 2 == 0 else ('after', 'before')):
            run = subprocess.check_output([str(out/label), str(args.seeds)], text=True)
            (out/f'{label}-{index}.txt').write_text(run)
            records = [row.split() for row in run.splitlines()]
            maps = [(row[0], row[1], row[3], row[4]) for row in records]
            if expected is None:
                expected = maps
            assert maps == expected, (label, index, 'map output changed')
            times = sorted(float(row[2]) for row in records)
            stats[label].append(dict(medianUs=statistics.median(times),
                                     p95Us=times[int((len(times)-1)*.95)], maxUs=max(times),
                                     totalUs=sum(times)))
    totals = {label: statistics.median(r['totalUs'] for r in rounds) for label, rounds in stats.items()}
    report = dict(platform='desktop C++ -O2, generation only; not ESP32 timing',
                  mapsChecked=len(expected), rounds=args.rounds, exactMapsEqual=True,
                  sourceHashes={k: hashlib.sha256(v.read_bytes()).hexdigest() for k, v in sources.items()},
                  runs=stats, medianTotalReductionPercent=(1-totals['after']/totals['before'])*100)
    (out/'benchmark.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
