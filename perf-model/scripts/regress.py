#!/usr/bin/env python3
"""Deterministic golden-file regression runner for translation_sim."""

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, List


def diff_json(expected: Any, actual: Any, path: str = "$") -> List[str]:
    diffs: List[str] = []
    if type(expected) is not type(actual):
        diffs.append(f"{path}: type mismatch expected={type(expected).__name__} actual={type(actual).__name__}")
        return diffs
    if isinstance(expected, dict):
        all_keys = sorted(set(expected.keys()) | set(actual.keys()))
        for k in all_keys:
            if k not in expected:
                diffs.append(f"{path}.{k}: unexpected key in actual")
            elif k not in actual:
                diffs.append(f"{path}.{k}: missing key in actual")
            else:
                diffs.extend(diff_json(expected[k], actual[k], f"{path}.{k}"))
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            diffs.append(f"{path}: list length expected={len(expected)} actual={len(actual)}")
        else:
            for i, (e, a) in enumerate(zip(expected, actual)):
                diffs.extend(diff_json(e, a, f"{path}[{i}]"))
    else:
        if expected != actual:
            diffs.append(f"{path}: expected={expected!r} actual={actual!r}")
    return diffs


def main() -> int:
    parser = argparse.ArgumentParser(description="Golden-file regression check for translation_sim")
    parser.add_argument("--binary", type=Path, required=True, help="Path to translation_sim binary")
    parser.add_argument("--configs-dir", type=Path, required=True, help="Directory with golden_*.json configs")
    parser.add_argument("--golden-dir", type=Path, required=True, help="Directory with checked-in golden outputs")
    parser.add_argument("--update", action="store_true", help="Regenerate golden JSON files instead of comparing")
    args = parser.parse_args()

    configs = sorted(args.configs_dir.glob("golden_*.json"))
    if not configs:
        print(f"Error: no golden_*.json configs found in {args.configs_dir}", file=sys.stderr)
        return 1

    args.golden_dir.mkdir(parents=True, exist_ok=True)
    failures = 0

    for cfg_path in configs:
        golden_path = args.golden_dir / cfg_path.name
        proc = subprocess.run(
            [str(args.binary), "--config", str(cfg_path)],
            capture_output=True,
            text=True,
            check=True,
        )
        actual_text = proc.stdout
        actual_obj = json.loads(actual_text)

        if args.update:
            golden_path.write_text(actual_text)
            print(f"[UPDATED] {golden_path.name}")
            continue

        if not golden_path.exists():
            print(f"[FAIL] Missing golden file: {golden_path}", file=sys.stderr)
            failures += 1
            continue

        expected_obj = json.loads(golden_path.read_text())
        diffs = diff_json(expected_obj, actual_obj)
        if diffs:
            print(f"[FAIL] {cfg_path.name} ({len(diffs)} mismatches):", file=sys.stderr)
            for d in diffs[:20]:
                print(f"  - {d}", file=sys.stderr)
            failures += 1
        else:
            m = actual_obj["metrics"]
            print(
                f"[PASS] {cfg_path.name}: cycles={m['total_cycles']} "
                f"hit_rate={m['dtlb_hit_rate']:.4f} "
                f"mean_comp={m['completion_latency']['mean']:.2f} "
                f"hol_stalls={m['hol_stall_cycles']}"
            )

    if failures > 0:
        print(f"\nRegression FAILED ({failures} config(s) mismatched).", file=sys.stderr)
        return 1
    print(f"\nAll {len(configs)} golden regression checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
