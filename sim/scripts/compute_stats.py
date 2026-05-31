#!/usr/bin/env python3
"""
compute_stats.py — Calcula media, std e IC95% por grupo experimental.

Lee results/processed/master.csv y produce results/processed/stats.csv
con una fila por combinación (phase, scheduler, n_ues, spatial, traffic).

Uso (desde sim/):
  python3 scripts/compute_stats.py

También invocado automáticamente por:
  python3 scripts/orchestrator.py --phase analyze
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
from scipy import stats as scipy_stats

# ---------------------------------------------------------------------------
# Rutas
# ---------------------------------------------------------------------------
SIM_ROOT = Path(__file__).parent.parent.resolve()
MASTER   = SIM_ROOT / "results" / "processed" / "master.csv"
OUT      = SIM_ROOT / "results" / "processed" / "stats.csv"

METRICS  = ["cell_throughput_mbps", "jain_index", "mean_delay_ms", "plr"]
GROUPBY  = ["phase", "scheduler", "n_ues", "spatial", "traffic"]


def ci95(series: pd.Series) -> tuple[float, float]:
    """IC 95% con t-Student. Requiere n >= 2."""
    n = series.dropna().count()
    if n < 2:
        return float("nan"), float("nan")
    m  = series.mean()
    se = series.std(ddof=1) / np.sqrt(n)
    t  = scipy_stats.t.ppf(0.975, df=n - 1)
    return m - t * se, m + t * se


def main() -> None:
    if not MASTER.exists():
        sys.exit(
            f"ERROR: {MASTER} no encontrado.\n"
            "Ejecuta primero: python3 scripts/process_all.py"
        )

    df = pd.read_csv(MASTER)
    records = []

    for keys, grp in df.groupby(GROUPBY, sort=True):
        row = dict(zip(GROUPBY, keys))
        row["n_runs"] = len(grp)

        for metric in METRICS:
            if metric not in grp.columns:
                continue
            m    = grp[metric].dropna()
            lo, hi = ci95(m)
            row[f"{metric}_mean"]    = round(m.mean(), 6)
            row[f"{metric}_std"]     = round(m.std(ddof=1), 6)
            row[f"{metric}_ci95_lo"] = round(lo, 6)
            row[f"{metric}_ci95_hi"] = round(hi, 6)

        records.append(row)

    stats_df = pd.DataFrame(records)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    stats_df.to_csv(OUT, index=False)
    print(f"  stats.csv guardado: {OUT} ({len(stats_df)} grupos)")

    # Resumen en pantalla para validación rápida
    ref = stats_df[
        (stats_df["n_ues"]   == 20) &
        (stats_df["spatial"] == "uniform") &
        (stats_df["traffic"] == "homogeneous")
    ]
    if not ref.empty:
        cols_show = ["phase", "scheduler",
                     "cell_throughput_mbps_mean", "jain_index_mean",
                     "mean_delay_ms_mean", "n_runs"]
        available = [c for c in cols_show if c in ref.columns]
        print("\n  Referencia (N=20, D1=uniform, T1=homogeneous):")
        print(ref[available].to_string(index=False))


if __name__ == "__main__":
    main()
