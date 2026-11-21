#!/usr/bin/env python3
"""Independent Python LRU TLB reference simulator for validating the C++ model.

Supports two verification modes:
1. Pure trace verification (InOrderFifo, 1 walker): feeds the transaction vaddr trace
   directly into an independent OrderedDict LRU TLB and asserts exact hit/miss counts.
2. Chronological event-log verification (all policies including IdAwareBypass): replays
   LOOKUP and INSTALL events through the Python LRU TLB and asserts every individual
   lookup's hit/miss outcome matches the C++ simulator bit-for-bit.
"""

import argparse
import csv
import json
import math
import subprocess
import sys
import tempfile
from collections import OrderedDict
from pathlib import Path


class RefLruTlb:
    """Clean-room true-LRU TLB implemented independently with OrderedDict."""

    def __init__(self, entries: int, ways: int, page_bytes: int) -> None:
        if entries <= 0 or ways <= 0 or entries % ways != 0:
            raise ValueError(f"invalid TLB geometry: entries={entries}, ways={ways}")
        self.entries = entries
        self.ways = ways
        self.num_sets = entries // ways
        if self.num_sets & (self.num_sets - 1) != 0:
            raise ValueError("num_sets must be a power of 2")
        self.page_shift = int(math.log2(page_bytes))
        self.set_bits = int(math.log2(self.num_sets))
        self.set_mask = self.num_sets - 1
        self.sets = [OrderedDict() for _ in range(self.num_sets)]
        self.hits = 0
        self.misses = 0

    def _split(self, vaddr: int):
        vpn = vaddr >> self.page_shift
        sidx = vpn & self.set_mask
        tag = vpn >> self.set_bits
        return sidx, tag

    def lookup(self, vaddr: int) -> bool:
        sidx, tag = self._split(vaddr)
        cache = self.sets[sidx]
        if tag in cache:
            cache.move_to_end(tag)
            self.hits += 1
            return True
        self.misses += 1
        return False

    def install(self, vaddr: int) -> None:
        sidx, tag = self._split(vaddr)
        cache = self.sets[sidx]
        if tag in cache:
            cache.move_to_end(tag)
            return
        if len(cache) >= self.ways:
            cache.popitem(last=False)
        cache[tag] = True

    def access_sequential(self, vaddr: int) -> bool:
        """Lookup and immediately install on miss (exact match to 1-walker InOrderFifo)."""
        hit = self.lookup(vaddr)
        if not hit:
            self.install(vaddr)
        return hit


def verify_trace(trace_csv: Path, entries: int, ways: int, page_bytes: int,
                 expected_hits: int, expected_misses: int) -> None:
    tlb = RefLruTlb(entries, ways, page_bytes)
    with trace_csv.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            vaddr = int(row["vaddr"], 0)
            cpp_hit = int(row["tlb_hit"]) == 1
            py_hit = tlb.access_sequential(vaddr)
            if py_hit != cpp_hit:
                raise AssertionError(
                    f"Per-txn mismatch at id={row['id']} vaddr={row['vaddr']}: "
                    f"C++ hit={cpp_hit} vs Python ref hit={py_hit}"
                )
    if tlb.hits != expected_hits or tlb.misses != expected_misses:
        raise AssertionError(
            f"Aggregate count mismatch: C++=({expected_hits}, {expected_misses}) "
            f"vs Python=({tlb.hits}, {tlb.misses})"
        )


def verify_events(events_csv: Path, entries: int, ways: int, page_bytes: int,
                  expected_hits: int, expected_misses: int) -> None:
    tlb = RefLruTlb(entries, ways, page_bytes)
    with events_csv.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for idx, row in enumerate(reader):
            ev = row["event"]
            vaddr = int(row["vaddr"], 0)
            if ev == "INSTALL":
                tlb.install(vaddr)
            elif ev == "LOOKUP":
                cpp_hit = int(row["hit"]) == 1
                py_hit = tlb.lookup(vaddr)
                if py_hit != cpp_hit:
                    raise AssertionError(
                        f"Event mismatch at row={idx} cycle={row['cycle']} txn={row['txn_id']} "
                        f"vaddr={row['vaddr']}: C++ hit={cpp_hit} vs Python hit={py_hit}"
                    )
            else:
                raise ValueError(f"Unknown event type: {ev}")
    if tlb.hits != expected_hits or tlb.misses != expected_misses:
        raise AssertionError(
            f"Event-log count mismatch: C++=({expected_hits}, {expected_misses}) "
            f"vs Python=({tlb.hits}, {tlb.misses})"
        )


def run_suite(binary: Path) -> None:
    # Mode 1: Sequential pure-trace checks across diverse workloads & TLB geometries.
    trace_cases = [
        ("zipf_full32_4k", "InOrderFifo", "zipf", 32, "full", 4096, 128, 1),
        ("zipf_4way32_4k", "InOrderFifo", "zipf", 32, "4", 4096, 128, 1),
        ("strided_dm16_4k", "InOrderFifo", "strided", 16, "1", 4096, 64, 1),
        ("random_full32_2m", "InOrderFifo", "random", 32, "full", 2097152, 64, 1),
        ("gemm_full32_4k", "InOrderFifo", "tiled_gemm", 32, "full", 4096, 128, 1),
        ("kv_cache_4way32", "InOrderFifo", "kv_cache", 32, "4", 4096, 128, 1),
        ("ptr_chase_full32", "InOrderFifo", "pointer_chase", 32, "full", 4096, 64, 1),
    ]

    # Mode 2: Event-log checks under RoundRobin and IdAwareBypass (including multi-walker).
    event_cases = [
        ("rr_zipf_w1", "RoundRobin", "zipf", 32, "full", 4096, 128, 1),
        ("bypass_zipf_w1", "IdAwareBypass", "zipf", 32, "full", 4096, 128, 1),
        ("bypass_gemm_w2", "IdAwareBypass", "tiled_gemm", 32, "4", 4096, 128, 2),
        ("bypass_kv_w2", "IdAwareBypass", "kv_cache", 32, "full", 4096, 128, 2),
    ]

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        for name, policy, workload, entries, assoc, page_bytes, ws, walkers in trace_cases:
            trace_csv = tmp / f"{name}_trace.csv"
            out_json = tmp / f"{name}_out.json"
            cmd = [
                str(binary),
                "--policy", policy,
                "--workload", workload,
                "--dtlb-entries", str(entries),
                "--dtlb-assoc", assoc,
                "--page-bytes", str(page_bytes),
                "--working-set", str(ws),
                "--walkers", str(walkers),
                "--transactions", "4000",
                "--seed", "17",
                "--dump-trace", str(trace_csv),
                "--output", str(out_json),
            ]
            subprocess.run(cmd, check=True)
            data = json.loads(out_json.read_text())
            ways = entries if assoc == "full" else int(assoc)
            verify_trace(
                trace_csv,
                entries,
                ways,
                page_bytes,
                data["metrics"]["dtlb_hits"],
                data["metrics"]["dtlb_misses"],
            )
            print(
                f"[PASS trace] {name}: hits={data['metrics']['dtlb_hits']} "
                f"misses={data['metrics']['dtlb_misses']}"
            )

        for name, policy, workload, entries, assoc, page_bytes, ws, walkers in event_cases:
            events_csv = tmp / f"{name}_events.csv"
            out_json = tmp / f"{name}_out.json"
            cmd = [
                str(binary),
                "--policy", policy,
                "--workload", workload,
                "--dtlb-entries", str(entries),
                "--dtlb-assoc", assoc,
                "--page-bytes", str(page_bytes),
                "--working-set", str(ws),
                "--walkers", str(walkers),
                "--transactions", "4000",
                "--seed", "23",
                "--dump-tlb-events", str(events_csv),
                "--output", str(out_json),
            ]
            subprocess.run(cmd, check=True)
            data = json.loads(out_json.read_text())
            ways = entries if assoc == "full" else int(assoc)
            verify_events(
                events_csv,
                entries,
                ways,
                page_bytes,
                data["metrics"]["dtlb_hits"],
                data["metrics"]["dtlb_misses"],
            )
            print(
                f"[PASS events] {name}: hits={data['metrics']['dtlb_hits']} "
                f"misses={data['metrics']['dtlb_misses']}"
            )

    print(f"All {len(trace_cases) + len(event_cases)} Python LRU reference checks passed exactly.")


def main() -> int:
    parser = argparse.ArgumentParser(description="Independent Python LRU TLB reference verifier")
    parser.add_argument("--binary", type=Path, help="Path to translation_sim binary")
    parser.add_argument("--verify-suite", action="store_true", help="Run full verification suite")
    parser.add_argument("--trace", type=Path, help="Verify a single dumped trace CSV")
    parser.add_argument("--entries", type=int, default=32)
    parser.add_argument("--ways", type=int, default=32)
    parser.add_argument("--page-bytes", type=int, default=4096)
    args = parser.parse_args()

    if args.verify_suite:
        if not args.binary:
            parser.error("--binary is required with --verify-suite")
        run_suite(args.binary)
        return 0

    if args.trace:
        tlb = RefLruTlb(args.entries, args.ways, args.page_bytes)
        with args.trace.open("r", newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                tlb.access_sequential(int(row["vaddr"], 0))
        print(json.dumps({"hits": tlb.hits, "misses": tlb.misses}, indent=2))
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
