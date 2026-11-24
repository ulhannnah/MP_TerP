"""Compile the repository's production C modules and bind them with ctypes.

Requires a native host gcc/clang compiler; S32DS ARM cross-gcc is not a host compiler.
No dependency on Python extension toolkits or third-party Python packages.
"""
from __future__ import annotations

import ctypes as C
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent
SOURCES = [
    "sim/radar_bridge.c",
    "src/radar_scan.c",
    "src/radar_frame.c",
    "src/radar_processing.c",
    "src/distance_filter.c",
    "src/background_filter.c",
    "src/target_detector.c",
    "src/target_tracker.c",
]
SUFFIX = ".dll" if sys.platform == "win32" else ".dylib" if sys.platform == "darwin" else ".so"
LIBRARY = HERE / "build" / ("radar_native" + SUFFIX)


def build_native(force: bool = False) -> Path:
    LIBRARY.parent.mkdir(exist_ok=True)
    dependencies = [ROOT / path for path in SOURCES] + list((ROOT / "src/header").glob("*.h"))
    missing = [str(p) for p in dependencies if not p.is_file()]
    if missing:
        raise RuntimeError("Missing C source/header: " + ", ".join(missing))
    if not force and LIBRARY.exists() and all(p.stat().st_mtime < LIBRARY.stat().st_mtime for p in dependencies):
        return LIBRARY
    compiler = os.environ.get("CC", "gcc")
    if not shutil.which(compiler):
        raise RuntimeError(
            f"Native compiler '{compiler}' not found. Use WSL Ubuntu: "
            "sudo apt install build-essential python3; or install a Windows MinGW-w64 gcc."
        )
    command = [
        compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-shared", "-Isrc/header",
    ]
    if sys.platform != "win32":
        command.append("-fPIC")
    command += SOURCES + ["-o", str(LIBRARY)]
    print("[build] " + " ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)
    return LIBRARY


class RadarNative:
    def __init__(self, rebuild: bool = False):
        self.lib = C.CDLL(str(build_native(force=rebuild)))
        lib = self.lib
        i32, u32, u16, u8 = C.c_int32, C.c_uint32, C.c_uint16, C.c_uint8
        pint, pi32, pu32, pu16 = C.POINTER(C.c_int), C.POINTER(i32), C.POINTER(u32), C.POINTER(u16)
        lib.sim_init.argtypes = [i32, i32, i32, u32, u32, u8, u32, u16, i32, u32, i32, u32, u32, u8]
        lib.sim_init.restype = C.c_int
        lib.sim_start.argtypes = [u32]
        lib.sim_start.restype = C.c_int
        lib.sim_step.argtypes = [u32]
        lib.sim_step.restype = C.c_int
        lib.sim_echo_requested.restype = C.c_int
        lib.sim_inject_echo.argtypes = [u32, C.c_int]
        lib.sim_scan_angle.restype = i32
        lib.sim_frame_count.restype = u32
        lib.sim_sample_count.restype = u32
        lib.sim_background_ready.restype = C.c_int
        lib.sim_candidate_count.restype = u16
        lib.sim_candidate.argtypes = [u16, pi32, pu32, pu16]
        lib.sim_candidate.restype = C.c_int
        lib.sim_target.argtypes = [u8, pint, pi32, pu32, pu32]
        lib.sim_target.restype = C.c_int
        lib.sim_frame_bin.argtypes = [u16, pi32, pu32, pint]
        lib.sim_frame_bin.restype = C.c_int
        lib.sim_frame_bins.restype = u16

    def initialize(self, min_deg=30, max_deg=150, step_deg=5,
                   settle_ms=10, timeout_ms=30, average_window=1,
                   background_threshold_mm=180, min_cluster_samples=2,
                   max_cluster_gap_mdeg=6000, max_cluster_delta_mm=450,
                   max_match_angle_mdeg=20000, max_match_range_mm=650,
                   lost_timeout_ms=5000, max_misses=2):
        ok = self.lib.sim_init(
            round(min_deg * 1000), round(max_deg * 1000), round(step_deg * 1000),
            settle_ms, timeout_ms, average_window, background_threshold_mm,
            min_cluster_samples, max_cluster_gap_mdeg, max_cluster_delta_mm,
            max_match_angle_mdeg, max_match_range_mm, lost_timeout_ms, max_misses
        )
        if not ok:
            raise RuntimeError("C RadarScan/RadarProcessing initialization rejected settings")
        if not self.lib.sim_start(0):
            raise RuntimeError("C RadarScan_Start failed")

    def inject(self, distance_mm: int | None):
        self.lib.sim_inject_echo(0 if distance_mm is None else distance_mm, int(distance_mm is not None))

    def candidates(self):
        rows = []
        for index in range(self.lib.sim_candidate_count()):
            angle, distance, count = C.c_int32(), C.c_uint32(), C.c_uint16()
            if self.lib.sim_candidate(index, C.byref(angle), C.byref(distance), C.byref(count)):
                rows.append({"angle_deg": angle.value / 1000, "distance_mm": distance.value,
                             "samples": count.value})
        return rows

    def targets(self):
        result = []
        states = ["EMPTY", "NEW", "TRACKING", "LOST"]
        for target_id in range(1, 6):
            state, angle, distance, last_seen = C.c_int(), C.c_int32(), C.c_uint32(), C.c_uint32()
            if self.lib.sim_target(target_id, C.byref(state), C.byref(angle),
                                   C.byref(distance), C.byref(last_seen)):
                result.append({"id": target_id, "state": states[state.value],
                               "angle_deg": angle.value / 1000,
                               "distance_mm": distance.value,
                               "last_seen_ms": last_seen.value})
        return result

    def frame_samples(self):
        rows = []
        for index in range(self.lib.sim_frame_bins()):
            angle, distance, valid = C.c_int32(), C.c_uint32(), C.c_int()
            if self.lib.sim_frame_bin(index, C.byref(angle), C.byref(distance), C.byref(valid)):
                rows.append({"angle_deg": angle.value / 1000, "distance_mm": distance.value,
                             "valid": bool(valid.value)})
        return rows
