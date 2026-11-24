"""Quantitative association analysis for the real C tracker CSV output.

Truth is object *center*. The simulated ultrasonic echo is the front surface
of a circular target, therefore compare target range + radius with truth
center range. This is an approximation at finite scan angle; report it as such.

Metrics:
 - TP/FP/FN on gated, one-to-one per-sweep association
 - angular MAE and center-adjusted range MAE on matched pairs
 - ID switches on *consecutive visible matched sweeps* only
 - reacquisition after a gap: a separate ID change counter
 - missed truth counts, measured detection latency, continuity
 - 0-target scenes retain meaningful false positive counts, precision N/A

No external packages required.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
from functools import lru_cache
from pathlib import Path

STATES = {"NEW", "TRACKING"}
# The simulator currently builds circular objects; radius has not been
# embedded in truth.csv yet. Using 230mm for legacy two-target scenarios,
# 185mm for the 5-target scenario.
LEGACY_RADIUS_MM = {"five": 185.0}


def load_csv(path):
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def best_assignment(truth, detected, angle_gate, range_gate, radius):
    """Max-cardinality assignment, then min normalized distance (n<=5).

    Include skip branch: a poor track never forces an invalid match.
    Pair cost is normalized to the caller's configured gate; report errors
    from raw data instead of the normalized matching cost.
    """
    valid = {}
    for i, a in enumerate(truth):
        for j, b in enumerate(detected):
            angle_err = abs(float(a["angle_deg"]) - float(b["angle_deg"]))
            # Convert observed *surface* distance into approximate center.
            range_err = abs(float(a["distance_mm"]) -
                            (float(b["distance_mm"]) + radius))
            if angle_err <= angle_gate and range_err <= range_gate:
                valid[i, j] = (angle_err / angle_gate) ** 2 + (range_err / range_gate) ** 2

    @lru_cache(None)
    def solve(i, used):
        if i == len(truth):
            return (0, 0.0, ())
        best = solve(i + 1, used)
        for j in range(len(detected)):
            if used & (1 << j) or (i, j) not in valid:
                continue
            count, cost, pairs = solve(i + 1, used | (1 << j))
            attempt = (count + 1, cost + valid[i, j], ((i, j),) + pairs)
            if attempt[0] > best[0] or (attempt[0] == best[0] and attempt[1] < best[1]):
                best = attempt
        return best

    return solve(0, 0)[2]


def evaluate(folder, angle_gate=12.0, range_gate=450.0, target_radius=None):
    folder = Path(folder)
    if angle_gate <= 0 or range_gate <= 0:
        raise ValueError("Positive angle and range gates are required")
    summary_path = folder / "summary.json"
    metadata = json.loads(summary_path.read_text(encoding="utf-8"))
    radius = float(target_radius if target_radius is not None else
                   LEGACY_RADIUS_MM.get(metadata["scenario"], 230.0))
    if radius < 0:
        raise ValueError("target_radius must be non-negative")
    truth = load_csv(folder / "truth.csv")
    tracks = load_csv(folder / "tracks.csv")
    frames = load_csv(folder / "frames.csv")
    if not frames:
        raise ValueError("No completed sweep frames")
    truth_by_sweep = {}
    tracks_by_sweep = {}
    for row in truth:
        truth_by_sweep.setdefault(int(row["sweep"]), []).append(row)
    for row in tracks:
        tracks_by_sweep.setdefault(int(row["sweep"]), []).append(row)

    tp = fp = fn = idswitch = reacq_changed = 0
    angle_errors = []
    range_errors = []
    associations = []
    per_sweep = []
    last_match = {}  # name -> (last sweep, tracker ID)
    first_seen = {}
    first_matched = {}
    continuity_frames = 0
    for frame in frames:
        sweep = int(frame["sweep"])
        # No expected objects on background calibration sweep.
        scene = truth_by_sweep.get(sweep, [])
        predicted = [row for row in tracks_by_sweep.get(sweep, [])
                     if row["state"] in STATES]
        pairs = best_assignment(scene, predicted, angle_gate, range_gate, radius)
        tp += len(pairs)
        fp += len(predicted) - len(pairs)
        fn += len(scene) - len(pairs)
        per_sweep.append({"sweep": sweep, "truth": len(scene),
                          "active_tracks": len(predicted), "matched": len(pairs),
                          "false_positive": len(predicted) - len(pairs),
                          "missed": len(scene) - len(pairs)})
        for row in scene:
            name = row["name"]
            first_seen.setdefault(name, sweep)
        for i, j in pairs:
            a, b = scene[i], predicted[j]
            name, track_id = a["name"], int(b["id"])
            angle_err = abs(float(a["angle_deg"]) - float(b["angle_deg"]))
            range_err = abs(float(a["distance_mm"]) -
                            (float(b["distance_mm"]) + radius))
            angle_errors.append(angle_err)
            range_errors.append(range_err)
            if name in last_match:
                last_sweep, last_id = last_match[name]
                if last_sweep == sweep - 1:
                    continuity_frames += 1
                    if last_id != track_id:
                        idswitch += 1
                elif last_id != track_id:
                    reacq_changed += 1
            last_match[name] = (sweep, track_id)
            first_matched.setdefault(name, sweep)
            associations.append({
                "sweep": sweep, "truth_name": name, "tracker_id": track_id,
                "truth_angle_deg": float(a["angle_deg"]),
                "track_angle_deg": float(b["angle_deg"]),
                "angle_error_deg": round(angle_err, 4),
                "truth_center_range_mm": float(a["distance_mm"]),
                "track_surface_range_mm": float(b["distance_mm"]),
                "approx_center_range_error_mm": round(range_err, 4),
            })

    truth_count = tp + fn
    pred_count = tp + fp
    # undefined denominators are null, not misleading 100%.
    precision = tp / pred_count if pred_count else None
    recall = tp / truth_count if truth_count else None
    f1 = ((2 * tp) / (2 * tp + fp + fn)) if 2 * tp + fp + fn else None
    result = {
        "scenario": metadata["scenario"],
        "sweeps": len(frames),
        "configuration": {
            "angle_gate_deg": angle_gate,
            "center_range_gate_mm": range_gate,
            "assumed_target_radius_mm": radius,
        },
        "matches": {"true_positive": tp, "false_positive": fp, "false_negative": fn},
        "precision": precision, "recall": recall, "f1": f1,
        "angle_mae_deg": sum(angle_errors)/len(angle_errors) if angle_errors else None,
        "approx_center_range_mae_mm": sum(range_errors)/len(range_errors) if range_errors else None,
        "id_switch_consecutive": idswitch,
        "id_change_after_gap": reacq_changed,
        "consecutive_matched_transitions": continuity_frames,
        "first_detection_delay_sweeps": {
            name: first_matched[name] - start if name in first_matched else None
            for name, start in first_seen.items()
        },
        "notes": [
            "Truth distances denote target centers; C returns nearest reflecting surface.",
            "Range MAE adds the assumed circle radius, not an exact ray-circle correction.",
            "Assignment maximizes matches then minimizes error within angle/range gates.",
            "ID switches count only consecutive matched sweeps; gaps are separate.",
            "Ground-truth association uses hindsight and does not improve the live tracker.",
            "This is a deterministic synthetic model, not physical sensor accuracy.",
        ],
    }
    with (folder / "metrics.json").open("w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
        f.write("\n")
    with (folder / "associations.csv").open("w", newline="", encoding="utf-8") as f:
        fields = ["sweep", "truth_name", "tracker_id", "truth_angle_deg",
                  "track_angle_deg", "angle_error_deg", "truth_center_range_mm",
                  "track_surface_range_mm", "approx_center_range_error_mm"]
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(associations)
    with (folder / "sweep_metrics.csv").open("w", newline="", encoding="utf-8") as f:
        fields = ["sweep", "truth", "active_tracks", "matched", "false_positive", "missed"]
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(per_sweep)
    return result


def main():
    parser = argparse.ArgumentParser(description="Ground-truth metrics for C radar tracking")
    parser.add_argument("result_dir", help="Path to sim/results/<scenario>")
    parser.add_argument("--angle-gate", type=float, default=12.0)
    parser.add_argument("--range-gate", type=float, default=450.0)
    parser.add_argument("--target-radius", type=float, default=None)
    args = parser.parse_args()
    report = evaluate(args.result_dir, args.angle_gate, args.range_gate, args.target_radius)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print(f"[saved] {Path(args.result_dir) / 'metrics.json'}")


if __name__ == "__main__":
    main()
