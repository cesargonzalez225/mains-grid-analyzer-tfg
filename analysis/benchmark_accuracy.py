
"""Accuracy benchmarking for the mains grid analyzer.

Takes a CSV of (reference, measured) pairs - reference being a trusted
instrument (multimeter, signal generator, lab PQ analyzer) and measured
being what the Pico reported for the same instant/setting - and produces
the error statistics and plots a TFG "Experimental Validation" chapter
needs: per-point error, MAE, RMSE, bias, and an error-vs-actual-value plot.

Usage:
    python benchmark_accuracy.py data.csv --quantity Voltage --unit V
    python benchmark_accuracy.py freq_data.csv --quantity Frequency --unit Hz

CSV format (header required, columns can be in any order):
    reference,measured
    230.0,229.4
    220.0,219.1
    ...

Run with no arguments to see this on a bundled synthetic example:
    python benchmark_accuracy.py sample_data/voltage_accuracy_sample.csv --quantity Voltage --unit V
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

def load_data(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    required = {"reference", "measured"}
    missing = required - set(df.columns)
    if missing:
        sys.exit(f"CSV is missing required column(s): {sorted(missing)}. "
                  f"Found columns: {list(df.columns)}")
    return df

def compute_stats(df: pd.DataFrame) -> dict:
    error = df["measured"] - df["reference"]
    error_pct = 100.0 * error / df["reference"]

    return {
        "n": len(df),
        "mae": float(np.mean(np.abs(error))),
        "rmse": float(np.sqrt(np.mean(error ** 2))),
        "bias": float(np.mean(error)),
        "std": float(np.std(error)),
        "max_abs_error": float(np.max(np.abs(error))),
        "max_abs_error_pct": float(np.max(np.abs(error_pct))),
        "mean_abs_error_pct": float(np.mean(np.abs(error_pct))),
    }

def print_report(stats: dict, quantity: str, unit: str) -> str:
    lines = [
        f"--- Accuracy benchmark: {quantity} ---",
        f"Samples:                 {stats['n']}",
        f"Mean Absolute Error:      {stats['mae']:.4f} {unit}",
        f"RMS Error:                {stats['rmse']:.4f} {unit}",
        f"Bias (mean signed error): {stats['bias']:+.4f} {unit}"
        f"  ({'reads high' if stats['bias'] > 0 else 'reads low' if stats['bias'] < 0 else 'no bias'})",
        f"Std dev of error:         {stats['std']:.4f} {unit}",
        f"Max |error|:              {stats['max_abs_error']:.4f} {unit} "
        f"({stats['max_abs_error_pct']:.2f}%)",
        f"Mean |error| %:           {stats['mean_abs_error_pct']:.3f}%",
    ]
    report = "\n".join(lines)
    print(report)
    return report

def make_plots(df: pd.DataFrame, quantity: str, unit: str, out_prefix: Path):
    error = df["measured"] - df["reference"]
    error_pct = 100.0 * error / df["reference"]

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.5))

    lo = min(df["reference"].min(), df["measured"].min())
    hi = max(df["reference"].max(), df["measured"].max())
    axes[0].plot([lo, hi], [lo, hi], "--", color="gray", label="ideal (y = x)")
    axes[0].scatter(df["reference"], df["measured"], color="#2b6cb0")
    axes[0].set_xlabel(f"Reference {quantity} ({unit})")
    axes[0].set_ylabel(f"Measured {quantity} ({unit})")
    axes[0].set_title("Measured vs. reference")
    axes[0].legend()
    axes[0].grid(alpha=0.3)

    axes[1].axhline(0, color="gray", linewidth=1)
    axes[1].scatter(df["reference"], error_pct, color="#c05621")
    axes[1].set_xlabel(f"Reference {quantity} ({unit})")
    axes[1].set_ylabel("Error (%)")
    axes[1].set_title("Error % vs. reference value")
    axes[1].grid(alpha=0.3)

    fig.tight_layout()
    plot_path = out_prefix.with_suffix(".png")
    fig.savefig(plot_path, dpi=150)
    print(f"\nPlot saved to {plot_path}")

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", nargs="?",
                         default=str(Path(__file__).parent / "sample_data" / "voltage_accuracy_sample.csv"),
                         help="CSV file with 'reference' and 'measured' columns "
                              "(defaults to the bundled synthetic example)")
    parser.add_argument("--quantity", default="Voltage",
                         help="What's being measured, for labels (default: Voltage)")
    parser.add_argument("--unit", default="V",
                         help="Unit for labels (default: V)")
    parser.add_argument("--out", default=None,
                         help="Output file prefix for the plot and report "
                              "(default: same name as the CSV)")
    args = parser.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        sys.exit(f"File not found: {csv_path}")

    out_prefix = Path(args.out) if args.out else csv_path.with_suffix("")

    df = load_data(csv_path)
    stats = compute_stats(df)
    report = print_report(stats, args.quantity, args.unit)
    make_plots(df, args.quantity, args.unit, out_prefix)

    report_path = out_prefix.with_suffix(".report.txt")
    report_path.write_text(report + "\n")
    print(f"Report saved to {report_path}")

if __name__ == "__main__":
    main()
