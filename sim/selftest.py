"""Host integration regression tests; invokes the unmodified production C modules."""
from __future__ import annotations

import csv
from pathlib import Path
from tempfile import TemporaryDirectory

from run import execute


def read_rows(root: Path, scenario: str, name: str):
    with (root / scenario / name).open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main():
    with TemporaryDirectory(prefix="mpterp_radar_") as directory:
        root = Path(directory)

        static = execute("static", sweeps=9, output=str(root), quiet=True)
        assert static["calibration_pass"], "Background calibration was not completed"
        assert static["sweeps_with_candidates"] >= 6, static
        assert static["max_active_tracks"] >= 2, static
        print("[PASS] Static: calibration, two distinguishable targets, C tracker")

        empty = execute("empty", sweeps=6, output=str(root), quiet=True)
        assert empty["sweeps_with_candidates"] == 0, empty
        assert empty["max_active_tracks"] == 0, empty
        print("[PASS] Empty: background-only scan with zero false detections")

        moving = execute("moving", sweeps=13, output=str(root), quiet=True)
        assert moving["sweeps_with_candidates"] >= 7, moving
        assert moving["sweeps_with_active_tracks"] >= 7, moving
        print("[PASS] Moving: repeated detections and tracker activity")

        occlusion = execute("occlusion", sweeps=12, output=str(root), quiet=True)
        tracks = read_rows(root, "occlusion", "tracks.csv")
        assert any(r["state"] == "LOST" for r in tracks), "Tracker did not report LOST"
        assert any(int(r["sweep"]) >= 9 and r["state"] in ("NEW", "TRACKING")
                   for r in tracks), "Target did not reappear after occlusion"
        print("[PASS] Occlusion: LOST state and later reacquisition")

        five = execute("five", sweeps=7, output=str(root), quiet=True)
        assert five["max_active_tracks"] >= 3, five
        print(f"[PASS] Five target stress: observed max active IDs = {five['max_active_tracks']}")

        crossing = execute("crossing", sweeps=15, output=str(root), quiet=True)
        assert crossing["sweeps_with_candidates"] >= 7, crossing
        print("[PASS] Crossing: pipeline stays operational (ID continuity NOT guaranteed)")

        print("\nAll 6 C-backed scenario smoke tests passed.")


if __name__ == "__main__":
    main()
