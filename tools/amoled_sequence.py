"""Compare typed native sequence fields without hashing float bit patterns."""
import json
import math
import re

FLOAT_TOLERANCE = 0.001


def parse_frames(output):
    return [json.loads(line[len("SequenceFrame "):])
            for line in output.splitlines() if line.startswith("SequenceFrame ")]


def frame_key(row):
    return row["seed"], row["scenario"], row["frame"]


def reference_frames(reference):
    columns = reference["columns"]
    if len(set(columns)) != len(columns):
        raise AssertionError("duplicate reference columns")
    result = []
    for seed, scenario, frame, *values in reference["frames"]:
        if len(values) != len(columns):
            raise AssertionError("reference row has incorrect field count")
        result.append({"seed": seed, "scenario": scenario, "frame": frame,
                       "fields": dict(zip(columns, values))})
    return result


def parse_summary(output):
    pattern = (r"(?:Home|Pair|Visitor)Sequence seed=(\d+) (action|scenario)=(\d+) "
               r"frames=(\d+) hash=[0-9a-f]+ rng=([0-9a-f]+)")
    return [{"seed": int(seed), kind: int(value), "frames": int(frames), "rng": rng}
            for seed, kind, value, frames, rng in re.findall(pattern, output)]


def compare_frames(expected, actual):
    if len(expected) != len(actual):
        raise AssertionError(f"frame count: expected {len(expected)}, got {len(actual)}")
    for before, after in zip(expected, actual):
        context = "seed=%d scenario=%d frame=%d" % frame_key(before)
        if frame_key(before) != frame_key(after):
            raise AssertionError(f"{context}: frame identity differs: {frame_key(after)}")
        left, right = before["fields"], after["fields"]
        if left.keys() != right.keys():
            raise AssertionError(f"{context}: field schema differs")
        for name, expected_value in left.items():
            value = right[name]
            if isinstance(expected_value, float):
                equal = (type(value) is float and math.isfinite(expected_value)
                         and math.isfinite(value)
                         and math.isclose(expected_value, value, rel_tol=0,
                                          abs_tol=FLOAT_TOLERANCE))
            else:
                equal = type(value) is type(expected_value) and value == expected_value
            if not equal:
                raise AssertionError(
                    f"{context}: {name}: expected {expected_value!r}, got {value!r}")


def select_reference_frames(frames):
    """Retain transitions on both sides, endpoints and one-second samples."""
    selected = set()
    previous = None
    previous_signature = None
    for index, row in enumerate(frames):
        group = frame_key(row)[:2]
        signature = {name: value for name, value in row["fields"].items()
                     if name.split(".")[-1] in
                     ("phase", "action", "activity", "task", "motion", "index",
                      "count", "active", "hidden", "mainArrived", "companionArrived")}
        if group != previous:
            selected.add(index)
            if index:
                selected.add(index - 1)
        elif signature != previous_signature:
            selected.update((index - 1, index))
        if row["frame"] % 20 == 0:
            selected.add(index)
        previous, previous_signature = group, signature
    if frames:
        selected.add(len(frames) - 1)
    return [frames[index] for index in sorted(selected)]
