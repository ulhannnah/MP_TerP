"""Deterministic synthetic polar radar world; no C algorithm is reimplemented here."""
from __future__ import annotations

from dataclasses import dataclass
import math
import random


@dataclass(frozen=True)
class Object:
    name: str
    angle_deg: float
    distance_mm: float
    radius_mm: float = 230.0

    @property
    def xy(self):
        angle = math.radians(self.angle_deg)
        return (self.distance_mm * math.cos(angle), self.distance_mm * math.sin(angle))


class World:
    def __init__(self, scenario="static", seed=42, noise_mm=15,
                 background_mm=3500, dropout=0.0):
        if scenario not in ("static", "moving", "occlusion", "crossing", "five", "empty"):
            raise ValueError(f"Unknown scenario: {scenario}")
        self.scenario = scenario
        self.rng = random.Random(seed)
        self.noise_mm = noise_mm
        self.background_mm = background_mm
        self.dropout = dropout

    def objects(self, sweep: int) -> list[Object]:
        # The first (forward) sweep is the empty background calibration pass.
        if sweep == 0 or self.scenario == "empty":
            return []
        phase = sweep - 1
        if self.scenario == "five":
            return [Object(f"T{i+1}", a, 1500, 185) for i, a in enumerate((40, 63, 86, 109, 135))]
        if self.scenario == "static":
            return [Object("A", 65, 1450), Object("B", 110, 2000)]
        if self.scenario == "moving":
            return [Object("A", min(70 + phase * 2.0, 100), 1500 + phase * 20),
                    Object("B", max(115 - phase * 1.0, 90), 2250 - phase * 15)]
        if self.scenario == "occlusion":
            return [] if 5 <= sweep <= 7 else [Object("A", 75, 1550)]
        if self.scenario == "crossing":
            return [Object("A", min(55 + phase * 4.0, 125), 1680),
                    Object("B", max(125 - phase * 4.0, 55), 1720)]
        raise AssertionError("unreachable")

    def distance(self, angle_deg: float, sweep: int) -> int | None:
        # Ray-circle intersection. Return the nearest positive surface point.
        theta = math.radians(angle_deg)
        ux, uy = math.cos(theta), math.sin(theta)
        closest = float(self.background_mm)
        for item in self.objects(sweep):
            cx, cy = item.xy
            projection = cx * ux + cy * uy
            perpendicular2 = cx * cx + cy * cy - projection * projection
            radius2 = item.radius_mm * item.radius_mm
            if projection <= 0 or perpendicular2 > radius2:
                continue
            entry = projection - math.sqrt(max(0.0, radius2 - perpendicular2))
            if 0 < entry < closest:
                closest = entry
        if self.rng.random() < self.dropout:
            return None
        mm = int(round(closest + self.rng.gauss(0, self.noise_mm)))
        return max(1, mm)
