"""Run the real C radar algorithm against a deterministic Python-generated world.

Example:
    python sim/run.py --scenario moving --sweeps 16 --plot
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from native import RadarNative
from world import World

SCENARIOS = ("static", "moving", "occlusion", "crossing", "empty")


def save_csv(path: Path, rows: list[dict], columns: list[str]):
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def plot_results(path: Path, frames: list[dict], samples: list[dict],
                 targets: list[dict], truth: list[dict]):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("[plot] matplotlib not installed; CSV files are still available.")
        print("       To plot: python -m pip install matplotlib")
        return False

    fig, axes = plt.subplots(2, 1, figsize=(12, 8), constrained_layout=True)
    # Range-vs-angle: last two sweeps (real C frame samples).
    last = max((int(s["sweep"]) for s in samples), default=-1)
    for sweep in range(max(0, last - 1), last + 1):
        rows = [r for r in samples if int(r["sweep"]) == sweep and r["valid"] == 1]
        if rows:
            axes[0].plot([r["angle_deg"] for r in rows],
                         [r["distance_mm"] for r in rows], marker="o",
                         label=f"measured sweep {sweep}")
    axes[0].set(title="Measured ultrasonic distance (actual C scan path)",
                xlabel="Servo angle (degrees)", ylabel="Distance (mm)", xlim=(30, 150))
    axes[0].grid(True)
    if axes[0].lines:
        axes[0].legend()

    for target_id in range(1, 6):
        rows = [r for r in targets if int(r["id"]) == target_id and r["state"] != "LOST"]
        if rows:
            axes[1].plot([r["sweep"] for r in rows],
                         [r["angle_deg"] for r in rows], marker="o",
                         label=f"C tracker ID {target_id}")
    names = sorted(set(r["name"] for r in truth))
    for name in names:
        rows = [r for r in truth if r["name"] == name]
        axes[1].plot([r["sweep"] for r in rows],
                     [r["angle_deg"] for r in rows], linestyle="--",
                     label=f"truth {name}")
    axes[1].set(title="C tracker ID angles compared with ground truth",
                xlabel="Completed half-sweep", ylabel="Angle (degrees)")
    axes[1].grid(True)
    if axes[1].lines:
        axes[1].legend(loc="best")
    fig.savefig(path, dpi=160)
    plt.close(fig)
    return True


def execute(scenario="static", sweeps=12, seed=42, noise=15.0, dropout=0.0,
            step=5, output="sim/results", plot=False, quiet=False, rebuild=False):
    if sweeps < 2:
        raise ValueError("At least two sweeps are required: calibration + observation")
    if not (0 <= dropout <= 1):
        raise ValueError("dropout must be between 0 and 1")
    native = RadarNative(rebuild=rebuild)
    native.initialize(step_deg=step, average_window=1)
    world = World(scenario=scenario, seed=seed, noise_mm=noise, dropout=dropout)
    output_dir = Path(output) / scenario
    output_dir.mkdir(parents=True, exist_ok=True)
    sample_rows, frame_rows, target_rows, candidate_rows, truth_rows = [], [], [], [], []
    now_ms = 0
    # The real C state machine decides when to advance and request a sample.
    # 10ms servo settle + next tick for echo response, 1ms simulation tick.
    max_ms = sweeps * (int(120 / step) + 4) * 50
    while native.lib.sim_frame_count() < sweeps and now_ms <= max_ms:
        frame_done = bool(native.lib.sim_step(now_ms))
        if native.lib.sim_echo_requested():
            sweep = native.lib.sim_frame_count()
            angle_deg = native.lib.sim_scan_angle() / 1000.0
            measured = world.distance(angle_deg, sweep)
            native.inject(measured)
            sample_rows.append({
                "time_ms": now_ms, "sweep": sweep, "angle_deg": angle_deg,
                "distance_mm": measured if measured is not None else 0,
                "valid": int(measured is not None),
            })
        if frame_done:
            sweep = native.lib.sim_frame_count()
            candidates = native.candidates()
            tracks = native.targets()
            frame_rows.append({
                "sweep": sweep, "time_ms": now_ms,
                "background_ready": int(native.lib.sim_background_ready()),
                "candidate_count": len(candidates),
                "active_tracks": sum(t["state"] in ("NEW", "TRACKING") for t in tracks),
                "lost_tracks": sum(t["state"] == "LOST" for t in tracks),
            })
            for index, c in enumerate(candidates):
                candidate_rows.append({"sweep": sweep, "candidate": index + 1, **c})
            for track in tracks:
                target_rows.append({"sweep": sweep, **track})
            for obj in world.objects(sweep - 1):
                truth_rows.append({"sweep": sweep, "name": obj.name,
                                   "angle_deg": obj.angle_deg,
                                   "distance_mm": obj.distance_mm})
            if not quiet:
                detected = ", ".join(f"ID{t['id']}={t['angle_deg']:.1f}deg/{t['distance_mm']}mm({t['state']})"
                                     for t in tracks if t["state"] in ("NEW", "TRACKING"))
                print(f"[sweep {sweep:02}] candidates={len(candidates)} | {detected or 'no active tracks'}")
        now_ms += 1
    if native.lib.sim_frame_count() != sweeps:
        raise RuntimeError(
            f"Simulation stalled: got {native.lib.sim_frame_count()} of {sweeps} sweeps. "
            f"Last time {now_ms}ms."
        )
    save_csv(output_dir / "frames.csv", frame_rows,
             ["sweep", "time_ms", "background_ready", "candidate_count", "active_tracks", "lost_tracks"])
    save_csv(output_dir / "samples.csv", sample_rows,
             ["time_ms", "sweep", "angle_deg", "distance_mm", "valid"])
    save_csv(output_dir / "candidates.csv", candidate_rows,
             ["sweep", "candidate", "angle_deg", "distance_mm", "samples"])
    save_csv(output_dir / "tracks.csv", target_rows,
             ["sweep", "id", "state", "angle_deg", "distance_mm", "last_seen_ms"])
    save_csv(output_dir / "truth.csv", truth_rows,
             ["sweep", "name", "angle_deg", "distance_mm"])
    details = {
        "scenario": scenario, "seed": seed, "sweeps": sweeps,
        "samples": native.lib.sim_sample_count(),
        "frames": native.lib.sim_frame_count(),
        "calibration_pass": bool(frame_rows[0]["background_ready"]),
        "sweeps_with_candidates": sum(row["candidate_count"] > 0 for row in frame_rows[1:]),
        "sweeps_with_active_tracks": sum(row["active_tracks"] > 0 for row in frame_rows[1:]),
        "max_active_tracks": max(row["active_tracks"] for row in frame_rows),
        "notes": "These are observed counts, not an accuracy/ID continuity guarantee.",
    }
    (output_dir / "summary.json").write_text(
        json.dumps(details, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if plot:
        plot_results(output_dir / "radar_result.png", frame_rows,
                     sample_rows, target_rows, truth_rows)
    if not quiet:
        print(f"\n[done] {output_dir.resolve()}")
        print(json.dumps(details, ensure_ascii=False, indent=2))
    return details


def main():
    parser = argparse.ArgumentParser(description="Python world + actual C radar pipeline simulation")
    parser.add_argument("--scenario", choices=SCENARIOS, default="static")
    parser.add_argument("--sweeps", type=int, default=12)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--noise", type=float, default=15, help="Ultrasonic Gaussian noise sigma, mm")
    parser.add_argument("--dropout", type=float, default=0.0, help="Probability of missing echo [0,1]")
    parser.add_argument("--step", type=int, default=5, help="Scan angular step in degrees")
    parser.add_argument("--output", default="sim/results")
    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--rebuild", action="store_true")
    args = parser.parse_args()
    execute(scenario=args.scenario, sweeps=args.sweeps, seed=args.seed,
            noise=args.noise, dropout=args.dropout, step=args.step,
            output=args.output, plot=args.plot, rebuild=args.rebuild)


if __name__ == "__main__":
    main()
