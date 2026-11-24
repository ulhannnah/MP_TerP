"""Deterministic evaluator unit tests; standard library only.

Checks one-to-one matching, false positives, false negatives, ID switches
on consecutive frames, and separate reacquisition ID changes.
"""
from __future__ import annotations
import csv
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from evaluate import evaluate, best_assignment


def write_csv(path, rows, columns):
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def main():
    assert best_assignment(
        [{"angle_deg": "30", "distance_mm": "1000"}],
        [{"angle_deg": "30", "distance_mm": "770"},
         {"angle_deg": "31", "distance_mm": "770"}], 12, 450, 230
    ) == ((0, 0),)
    assert best_assignment(
        [{"angle_deg": "30", "distance_mm": "1000"}],
        [{"angle_deg": "75", "distance_mm": "770"}], 12, 450, 230
    ) == ()

    with TemporaryDirectory() as tmp:
        folder = Path(tmp)
        (folder / "summary.json").write_text(
            json.dumps({"scenario": "static"}), encoding="utf-8")
        write_csv(folder / "frames.csv", [{"sweep": i} for i in range(1, 6)], ["sweep"])
        truth = [
            {"sweep": i, "name": "A", "angle_deg": 60, "distance_mm": 1500}
            for i in (2, 3, 4, 5)
        ]
        tracks = [
            {"sweep": 2, "id": 1, "state": "NEW", "angle_deg": 60, "distance_mm": 1270},
            {"sweep": 3, "id": 2, "state": "TRACKING", "angle_deg": 60, "distance_mm": 1270},
            # sweep 4 missing; count false negative, not a switch
            {"sweep": 5, "id": 3, "state": "NEW", "angle_deg": 60, "distance_mm": 1270},
            {"sweep": 5, "id": 4, "state": "NEW", "angle_deg": 100, "distance_mm": 1270},
        ]
        write_csv(folder / "truth.csv", truth,
                  ["sweep", "name", "angle_deg", "distance_mm"])
        write_csv(folder / "tracks.csv", tracks,
                  ["sweep", "id", "state", "angle_deg", "distance_mm"])
        result = evaluate(folder)
        assert result["matches"] == {
            "true_positive": 3, "false_positive": 1, "false_negative": 1
        }, result
        assert result["id_switch_consecutive"] == 1, result
        assert result["id_change_after_gap"] == 1, result
        assert result["precision"] == 0.75, result
        assert result["recall"] == 0.75, result
        assert result["angle_mae_deg"] == 0, result
        assert result["approx_center_range_mae_mm"] == 0, result
        assert (folder / "metrics.json").exists()
        assert len(list(csv.DictReader((folder / "associations.csv").open(
            encoding="utf-8", newline="")))) == 3
    print("[PASS] evaluator: TP/FP/FN, association, consecutive ID switch, reacquisition")


if __name__ == "__main__":
    main()
