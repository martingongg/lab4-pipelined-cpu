#!/usr/bin/env python3
"""Generate publication-quality Matplotlib figures from sweep_results.csv."""

import argparse
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

POLICY_ORDER = ["InOrderFifo", "RoundRobin", "IdAwareBypass"]
POLICY_COLORS = {
    "InOrderFifo": "#c0392b",
    "RoundRobin": "#d68910",
    "IdAwareBypass": "#1f618d",
}
POLICY_MARKERS = {
    "InOrderFifo": "o",
    "RoundRobin": "s",
    "IdAwareBypass": "^",
}
POLICY_LINESTYLES = {
    "InOrderFifo": "-",
    "RoundRobin": "--",
    "IdAwareBypass": "-",
}
POLICY_LINEWIDTHS = {
    "InOrderFifo": 2.6,
    "RoundRobin": 1.8,
    "IdAwareBypass": 2.2,
}
WORKLOAD_LABELS = {
    "sequential": "Sequential",
    "strided": "Strided",
    "random": "Random",
    "zipf": "Zipfian",
    "pointer_chase": "Ptr Chase",
    "tiled_gemm": "Tiled GEMM",
    "kv_cache": "KV-Cache",
}


def setup_style() -> None:
    plt.rcParams.update({
        "font.size": 10,
        "axes.titlesize": 11,
        "axes.titleweight": "bold",
        "axes.labelsize": 10,
        "axes.grid": True,
        "grid.alpha": 0.25,
        "grid.linestyle": "--",
        "axes.spines.top": False,
        "axes.spines.right": False,
        "legend.frameon": True,
        "legend.framealpha": 0.9,
        "figure.dpi": 180,
    })


def plot_latency_vs_working_set(df: pd.DataFrame, out_dir: Path) -> None:
    sub = df[df["experiment"] == "working_set_sweep"].copy()
    workloads = ["zipf", "tiled_gemm", "kv_cache"]
    fig, axes = plt.subplots(2, 3, figsize=(13.5, 7.6), sharex=True)

    for col, wl in enumerate(workloads):
        wdf = sub[sub["workload"] == wl]
        ax_mean = axes[0, col]
        ax_p99 = axes[1, col]

        for pol in POLICY_ORDER:
            pdf = wdf[wdf["policy"] == pol].sort_values("working_set_pages")
            ax_mean.plot(
                pdf["working_set_pages"],
                pdf["comp_mean"],
                label=f"{pol} (comp)",
                color=POLICY_COLORS[pol],
                linestyle=POLICY_LINESTYLES[pol],
                marker=POLICY_MARKERS[pol],
                linewidth=POLICY_LINEWIDTHS[pol],
                markersize=6.0 if pol == "InOrderFifo" else 5.0,
            )
            if pol == "IdAwareBypass":
                ax_mean.plot(
                    pdf["working_set_pages"],
                    pdf["ret_mean"],
                    label="IdAwareBypass (retire)",
                    color=POLICY_COLORS[pol],
                    linestyle=":",
                    marker=POLICY_MARKERS[pol],
                    linewidth=1.8,
                    markersize=4.5,
                    alpha=0.8,
                )
            ax_p99.plot(
                pdf["working_set_pages"],
                pdf["comp_p99"],
                label=pol,
                color=POLICY_COLORS[pol],
                linestyle=POLICY_LINESTYLES[pol],
                marker=POLICY_MARKERS[pol],
                linewidth=POLICY_LINEWIDTHS[pol],
                markersize=6.0 if pol == "InOrderFifo" else 5.0,
            )

        for ax in (ax_mean, ax_p99):
            ax.set_xscale("log", base=2)
            ax.axvline(32, color="#555555", linestyle=":", linewidth=1.3)
            ax.set_xticks([16, 32, 64, 128, 256, 512, 1024])
            ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())

        ax_mean.set_title(WORKLOAD_LABELS[wl])
        ax_mean.text(35, ax_mean.get_ylim()[1] * 0.88, "DTLB reach\n(32 pages)", fontsize=8, color="#444444")
        ax_p99.set_xlabel("Working Set (4 KiB pages)")
        if col == 0:
            ax_mean.set_ylabel("Mean Latency (cycles)")
            ax_p99.set_ylabel("p99 Completion Latency (cycles)")

    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=4, bbox_to_anchor=(0.5, 0.995))
    fig.suptitle("Translation Latency vs. Working-Set Size (32-Entry Full-Assoc DTLB, 1 Walker)", y=1.03, fontsize=13, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig(out_dir / "latency_vs_working_set.png", bbox_inches="tight")
    plt.close(fig)


def plot_hol_stalls_by_policy(df: pd.DataFrame, out_dir: Path) -> None:
    sub = df[(df["experiment"] == "policy_workload_matrix") & (df["retire_in_order"] == True)].copy()
    workloads = ["zipf", "tiled_gemm", "kv_cache", "sequential", "random", "strided", "pointer_chase"]
    x = np.arange(len(workloads))
    width = 0.25

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13.5, 4.8))
    for i, pol in enumerate(POLICY_ORDER):
        pdf = sub[sub["policy"] == pol].set_index("workload").reindex(workloads)
        ax1.bar(
            x + (i - 1) * width,
            pdf["hol_stall_cycles"] / 1e3,
            width=width,
            label=pol,
            color=POLICY_COLORS[pol],
            edgecolor="white",
            linewidth=0.6,
        )
        ax2.bar(
            x + (i - 1) * width,
            pdf["hol_stall_fraction"] * 100.0,
            width=width,
            label=pol,
            color=POLICY_COLORS[pol],
            edgecolor="white",
            linewidth=0.6,
        )

    for ax in (ax1, ax2):
        ax.set_xticks(x)
        ax.set_xticklabels([WORKLOAD_LABELS[w] for w in workloads], rotation=22, ha="right")

    ax1.set_ylabel("HoL Stall Cycles (thousands)")
    ax1.set_title("Total HoL Stall Cycles (100k txns)")
    ax2.set_ylabel("HoL Stalled Cycles (% of Total Cycles)")
    ax2.set_title("HoL Stall Fraction by Policy & Workload")
    ax1.legend(loc="upper right")
    fig.suptitle("Head-of-Line Blocking Behind Page-Table Walks (256 Pages = 8× DTLB Reach, 1 Walker)", y=1.02, fontsize=12.5, fontweight="bold")
    fig.tight_layout()
    fig.savefig(out_dir / "hol_stalls_by_policy.png", bbox_inches="tight")
    plt.close(fig)


def plot_throughput_and_rob(df: pd.DataFrame, out_dir: Path) -> None:
    sub_io = df[(df["experiment"] == "policy_workload_matrix") & (df["retire_in_order"] == True)].copy()
    sub_ooo = df[(df["experiment"] == "policy_workload_matrix") & (df["retire_in_order"] == False)].copy()
    workloads = ["sequential", "zipf", "kv_cache", "tiled_gemm", "random", "strided", "pointer_chase"]
    x = np.arange(len(workloads))
    width = 0.25

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13.5, 4.9))
    for i, pol in enumerate(POLICY_ORDER):
        pdf = sub_io[sub_io["policy"] == pol].set_index("workload").reindex(workloads)
        ax1.bar(
            x + (i - 1) * width,
            pdf["throughput"],
            width=width,
            label=pol,
            color=POLICY_COLORS[pol],
            edgecolor="white",
            linewidth=0.6,
        )
        ax2.bar(
            x + (i - 1) * width,
            pdf["rob_max_occupancy"],
            width=width,
            label=pol,
            color=POLICY_COLORS[pol],
            edgecolor="white",
            linewidth=0.6,
        )

    # Overlay IdAwareBypass out-of-order retire throughput markers on ax1.
    ooo_bypass = sub_ooo[sub_ooo["policy"] == "IdAwareBypass"].set_index("workload").reindex(workloads)
    ax1.scatter(
        x + width,
        ooo_bypass["throughput"],
        color="#111111",
        marker="D",
        s=32,
        zorder=5,
        label="IdAwareBypass (OoO retire)",
    )

    for ax in (ax1, ax2):
        ax.set_xticks(x)
        ax.set_xticklabels([WORKLOAD_LABELS[w] for w in workloads], rotation=22, ha="right")

    ax1.set_ylabel("Throughput (txns / cycle)")
    ax1.set_title("Translation Throughput (In-Order vs. OoO Retire)")
    ax1.legend(loc="upper right", fontsize=8.5)
    ax2.set_ylabel("Max Reorder-Buffer Occupancy (entries)")
    ax2.set_title("Reorder-Buffer Cost Under In-Order Retirement")
    ax2.axhline(64, color="#555555", linestyle=":", linewidth=1.2)
    ax2.text(0.05, 65.5, "ROB limit (64)", fontsize=8, color="#444444")

    fig.suptitle("Throughput Gains vs. Reorder-Buffer Occupancy (256 Pages, 1 Walker)", y=1.02, fontsize=12.5, fontweight="bold")
    fig.tight_layout()
    fig.savefig(out_dir / "throughput_and_rob_by_policy.png", bbox_inches="tight")
    plt.close(fig)


def plot_walker_and_pagesize(df: pd.DataFrame, out_dir: Path) -> None:
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13.2, 4.8))

    # Left panel: Walker scaling on Zipf (256 pages)
    wsub = df[(df["experiment"] == "walker_scaling") & (df["workload"] == "zipf")].copy()
    for pol in POLICY_ORDER:
        pdf = wsub[wsub["policy"] == pol].sort_values("walkers")
        ax1.plot(
            pdf["walkers"],
            pdf["hol_stall_cycles"] / 1e3,
            label=f"{pol} — HoL stalls",
            color=POLICY_COLORS[pol],
            linestyle=POLICY_LINESTYLES[pol],
            marker=POLICY_MARKERS[pol],
            linewidth=POLICY_LINEWIDTHS[pol],
            markersize=7.0 if pol == "InOrderFifo" else 5.5,
        )
    ax1.set_xticks([1, 2, 4])
    ax1.set_xlabel("Hardware Page-Table Walker Count")
    ax1.set_ylabel("HoL Stall Cycles (thousands)")
    ax1.set_title("Walker Count Scaling on Zipf (256 Pages)")
    ax1.legend(loc="upper right", fontsize=9)

    # Right panel: 4 KiB vs 2 MiB page size impact on 32 MiB footprint (IdAwareBypass)
    psub = df[
        (df["experiment"] == "pagesize_comparison")
        & (df["policy"] == "IdAwareBypass")
        & (df["working_set_bytes"] == 33554432)
    ].copy()
    wls = ["tiled_gemm", "kv_cache"]
    x = np.arange(len(wls))
    width = 0.34
    p4k = psub[psub["page_bytes"] == 4096].set_index("workload").reindex(wls)
    p2m = psub[psub["page_bytes"] == 2097152].set_index("workload").reindex(wls)

    b1 = ax2.bar(x - width / 2, p4k["comp_mean"], width=width, label="4 KiB pages (8192 pages)", color="#c0392b", edgecolor="white")
    b2 = ax2.bar(x + width / 2, p2m["comp_mean"], width=width, label="2 MiB hugepages (16 pages)", color="#1f618d", edgecolor="white")
    for bars, hits in [(b1, p4k["dtlb_hit_rate"]), (b2, p2m["dtlb_hit_rate"])]:
        for bar, hr in zip(bars, hits):
            h = bar.get_height()
            ax2.text(
                bar.get_x() + bar.get_width() / 2,
                h + max(1.0, h * 0.03),
                f"{h:.1f} cyc\n({hr * 100:.2f}% hit)",
                ha="center",
                va="bottom",
                fontsize=8.5,
            )

    ax2.set_xticks(x)
    ax2.set_xticklabels([WORKLOAD_LABELS[w] for w in wls])
    ax2.set_ylabel("Mean Completion Latency (cycles)")
    ax2.set_title("4 KiB vs. 2 MiB Pages at 32 MiB Footprint (IdAwareBypass)")
    ax2.set_ylim(0, max(p4k["comp_mean"].max(), p2m["comp_mean"].max()) * 1.28)
    ax2.legend(loc="upper right", fontsize=9)

    fig.suptitle("Architectural Levers: Multi-Walker Scaling & 2 MiB Hugepage Reach", y=1.02, fontsize=12.5, fontweight="bold")
    fig.tight_layout()
    fig.savefig(out_dir / "walker_and_pagesize_scaling.png", bbox_inches="tight")
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot translation_sim sweep results")
    parser.add_argument("--input", type=Path, required=True, help="Path to sweep_results.csv")
    parser.add_argument("--out-dir", type=Path, required=True, help="Output directory for PNG figures")
    args = parser.parse_args()

    setup_style()
    df = pd.read_csv(args.input)
    args.out_dir.mkdir(parents=True, exist_ok=True)

    plot_latency_vs_working_set(df, args.out_dir)
    plot_hol_stalls_by_policy(df, args.out_dir)
    plot_throughput_and_rob(df, args.out_dir)
    plot_walker_and_pagesize(df, args.out_dir)

    print(f"Generated 4 figures in {args.out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
