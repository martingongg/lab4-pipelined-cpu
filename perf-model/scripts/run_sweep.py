#!/usr/bin/env python3
"""Parallel parameter-sweep runner for translation_sim.

Expands configs/sweep_grid.json across policy x workload x working-set x walkers x page-size,
executes the C++ simulator in parallel, and writes consolidated CSV and JSON outputs.
"""

import argparse
import concurrent.futures
import copy
import csv
import itertools
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict, List, Tuple


def expand_grid(grid: Dict[str, Any]) -> List[Tuple[str, Dict[str, Any]]]:
    base = grid["base"]
    jobs: List[Tuple[str, Dict[str, Any]]] = []

    for exp in grid["experiments"]:
        exp_name = exp["name"]
        policies = exp["policies"]
        workloads = exp["workloads"]
        walkers_list = exp.get("walkers", [base["walker"]["count"]])
        retire_list = exp.get("retire_in_order", [base.get("retire_in_order", True)])
        page_sizes = exp.get("page_bytes", [base["dtlb"]["page_bytes"]])
        txns = exp.get("transactions", 100000)

        if "working_set_bytes" in exp:
            ws_items = [("bytes", b) for b in exp["working_set_bytes"]]
        else:
            ws_items = [("pages", p) for p in exp["working_set_pages"]]

        for policy, wl, (ws_kind, ws_val), walkers, retire_io, pb in itertools.product(
            policies, workloads, ws_items, walkers_list, retire_list, page_sizes
        ):
            cfg = copy.deepcopy(base)
            cfg["policy"] = policy
            cfg["retire_in_order"] = retire_io
            cfg["walker"]["count"] = walkers
            cfg["dtlb"]["page_bytes"] = pb
            cfg["workload"] = {
                "type": wl,
                "transactions": txns,
            }
            if ws_kind == "bytes":
                cfg["workload"]["working_set_bytes"] = ws_val
                cfg["workload"]["working_set_pages"] = max(1, ws_val // pb)
            else:
                cfg["workload"]["working_set_pages"] = ws_val
            jobs.append((exp_name, cfg))
    return jobs


def run_one(args_tuple: Tuple[str, str, Dict[str, Any]]) -> Dict[str, Any]:
    binary, exp_name, cfg = args_tuple
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(cfg, tf)
        cfg_path = tf.name
    try:
        proc = subprocess.run(
            [binary, "--config", cfg_path],
            capture_output=True,
            text=True,
            check=True,
        )
        result = json.loads(proc.stdout)
    finally:
        os.unlink(cfg_path)

    m = result["metrics"]
    c = result["config"]
    wl = c["workload"]
    row: Dict[str, Any] = {
        "experiment": exp_name,
        "policy": c["policy"],
        "workload": wl["type"],
        "working_set_pages": wl["working_set_pages"],
        "working_set_bytes": wl.get("working_set_bytes", wl["working_set_pages"] * c["dtlb"]["page_bytes"]),
        "page_bytes": c["dtlb"]["page_bytes"],
        "dtlb_entries": c["dtlb"]["entries"],
        "walkers": c["walker"]["count"],
        "levels": c["walker"]["levels"],
        "retire_in_order": c["retire_in_order"],
        "transactions": m["transactions_completed"],
        "total_cycles": m["total_cycles"],
        "throughput": m["throughput_txns_per_cycle"],
        "dtlb_hits": m["dtlb_hits"],
        "dtlb_misses": m["dtlb_misses"],
        "dtlb_hit_rate": m["dtlb_hit_rate"],
        "walks_started": m["walks_started"],
        "walks_completed": m["walks_completed"],
        "coalesced_misses": m["coalesced_misses"],
        "walker_utilization": m["walker_utilization"],
        "hol_stall_cycles": m["hol_stall_cycles"],
        "hol_stall_fraction": m["hol_stall_fraction"],
        "hol_txn_wait_cycles": m["hol_txn_wait_cycles"],
        "rob_max_occupancy": m["rob_max_occupancy"],
        "rob_mean_occupancy": m["rob_mean_occupancy"],
        "comp_mean": m["completion_latency"]["mean"],
        "comp_p50": m["completion_latency"]["p50"],
        "comp_p95": m["completion_latency"]["p95"],
        "comp_p99": m["completion_latency"]["p99"],
        "comp_max": m["completion_latency"]["max"],
        "ret_mean": m["retirement_latency"]["mean"],
        "ret_p50": m["retirement_latency"]["p50"],
        "ret_p95": m["retirement_latency"]["p95"],
        "ret_p99": m["retirement_latency"]["p99"],
        "ret_max": m["retirement_latency"]["max"],
    }
    return row


def main() -> int:
    parser = argparse.ArgumentParser(description="Run translation_sim parameter sweep")
    parser.add_argument("--binary", type=Path, required=True, help="Path to translation_sim binary")
    parser.add_argument("--grid", type=Path, required=True, help="Path to sweep_grid.json")
    parser.add_argument("--out-dir", type=Path, required=True, help="Output directory for CSV/JSON")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4, help="Parallel worker count")
    args = parser.parse_args()

    grid = json.loads(args.grid.read_text())
    expanded = expand_grid(grid)
    print(f"Launching {len(expanded)} simulation runs across {args.jobs} workers...")

    work_items = [(str(args.binary), exp_name, cfg) for exp_name, cfg in expanded]
    rows: List[Dict[str, Any]] = []
    with concurrent.futures.ProcessPoolExecutor(max_workers=args.jobs) as pool:
        for idx, row in enumerate(pool.map(run_one, work_items), start=1):
            rows.append(row)
            if idx % 20 == 0 or idx == len(work_items):
                print(f"  completed {idx}/{len(work_items)} runs")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    csv_path = args.out_dir / "sweep_results.csv"
    json_path = args.out_dir / "sweep_results.json"

    fieldnames = list(rows[0].keys())
    with csv_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    json_path.write_text(json.dumps(rows, indent=2) + "\n")
    print(f"Saved sweep results to:\n  {csv_path}\n  {json_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
