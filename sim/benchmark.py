"""Run controlled noise/dropout/seed sweeps, using real C tracker for every run."""
from __future__ import annotations
import argparse
import csv
import json
from pathlib import Path
from run import execute, SCENARIOS
from evaluate import evaluate


def run_matrix(root: str, scenarios, seeds, noise_levels, dropouts, sweeps):
    root = Path(root)
    out_rows = []
    for scenario in scenarios:
        for noise in noise_levels:
            for dropout in dropouts:
                for seed in seeds:
                    run_name = f"noise{noise:g}_drop{dropout:g}_seed{seed}"
                    directory = root / run_name
                    detail = execute(scenario=scenario, sweeps=sweeps, seed=seed,
                                     noise=noise, dropout=dropout,
                                     output=str(directory), quiet=True)
                    result = evaluate(directory / scenario)
                    row = {
                        "scenario": scenario, "noise_sigma_mm": noise,
                        "dropout_probability": dropout, "seed": seed,
                        "sweeps": sweeps, "tp": result["matches"]["true_positive"],
                        "fp": result["matches"]["false_positive"],
                        "fn": result["matches"]["false_negative"],
                        "precision": result["precision"], "recall": result["recall"],
                        "f1": result["f1"], "angle_mae_deg": result["angle_mae_deg"],
                        "center_range_mae_mm": result["approx_center_range_mae_mm"],
                        "id_switch_consecutive": result["id_switch_consecutive"],
                        "id_change_after_gap": result["id_change_after_gap"],
                        "calibration_pass": int(detail["calibration_pass"]),
                        "result_dir": str(directory / scenario),
                    }
                    out_rows.append(row)
                    print(f"{scenario:9s} seed={seed:3d} noise={noise:5.1f} "
                          f"drop={dropout:.2f} F1={str(row['f1']):>6s} "
                          f"IDS={row['id_switch_consecutive']:2d} "
                          f"TP/FP/FN={row['tp']}/{row['fp']}/{row['fn']}")
    root.mkdir(parents=True, exist_ok=True)
    if out_rows:
        with (root / "matrix.csv").open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(out_rows[0]))
            writer.writeheader()
            writer.writerows(out_rows)
    print(f"\n[saved] {(root / 'matrix.csv').resolve()}")
    return out_rows


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--scenario", nargs="+", choices=SCENARIOS,
                   default=["static", "moving", "occlusion", "crossing", "five", "empty"])
    p.add_argument("--seeds", nargs="+", type=int, default=[11, 42, 73])
    p.add_argument("--noise", nargs="+", type=float, default=[0, 15, 40])
    p.add_argument("--dropout", nargs="+", type=float, default=[0.0, 0.08])
    p.add_argument("--sweeps", type=int, default=16)
    p.add_argument("--output", default="sim/results/matrix")
    args = p.parse_args()
    run_matrix(args.output, args.scenario, args.seeds,
               args.noise, args.dropout, args.sweeps)


if __name__ == "__main__":
    main()
