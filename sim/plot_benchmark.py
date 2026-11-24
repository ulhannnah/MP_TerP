"""Visualize quantitative radar benchmark matrix (optional matplotlib).

No modification to any C modules. Run after python sim/benchmark.py.
"""
from __future__ import annotations
import argparse
import csv
from collections import defaultdict
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--matrix", default="sim/results/matrix/matrix.csv")
    p.add_argument("--out", default=None)
    args = p.parse_args()

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit("matplotlib required: python -m pip install matplotlib") from exc

    matrix = Path(args.matrix)
    with matrix.open(newline="", encoding="utf-8") as file:
        records = list(csv.DictReader(file))
    if not records:
        raise SystemExit("matrix.csv is empty")

    groups = defaultdict(list)
    for row in records:
        groups[(row["scenario"], float(row["dropout_probability"]))].append(row)

    # Separate figures for F1 and ID switch count.
    for metric, name, ylabel in [
        ("f1", "f1_vs_noise.png", "F1 (matched tracker output)"),
        ("id_switch_consecutive", "id_switch_vs_noise.png", "Consecutive ID switches"),
    ]:
        fig, ax = plt.subplots(figsize=(10, 6))
        for key, rows in sorted(groups.items()):
            per_noise = defaultdict(list)
            for row in rows:
                if row[metric] == "":
                    continue
                per_noise[float(row["noise_sigma_mm"])].append(float(row[metric]))
            xs = sorted(per_noise)
            if not xs:
                continue
            ys = [sum(per_noise[x]) / len(per_noise[x]) for x in xs]
            label = f"{key[0]} / dropout {key[1]:.0%}"
            ax.plot(xs, ys, marker="o", label=label)
        ax.set(xlabel="Ultrasonic noise sigma (mm)", ylabel=ylabel,
               title="Synthetic-world C tracker benchmark (mean over seeds)")
        ax.grid(True)
        if ax.lines:
            ax.legend(fontsize="small")
        out = Path(args.out) if args.out else matrix.parent
        out.mkdir(parents=True, exist_ok=True)
        file_path = out / name
        fig.savefig(file_path, dpi=160, bbox_inches="tight")
        plt.close(fig)
        print(f"[saved] {file_path}")


if __name__ == "__main__":
    main()
