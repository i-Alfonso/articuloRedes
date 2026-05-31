#!/usr/bin/env python3
"""
plot_results.py — Genera las 8 figuras del paper a partir de stats.csv y FlowStats.csv.

Uso (desde sim/):
  python3 scripts/plot_results.py
  # o bien:
  python3 scripts/orchestrator.py --phase plot

Figuras generadas en results/processed/figures/:
  F1: Throughput vs N (D1, T1) — baseline H1
  F2: Jain vs N (D1, T1) — fairness uniforme H1
  F3: Jain vs N (D2, T1) — fairness clusterizado H1
  F4: Jain D1 vs D2 (N=20, T1) — efecto espacial H2
  F5: Throughput T1 vs T2 (N=20, D1) — efecto tráfico H3
  F6: Throughput GBR vs BE (N=20, D1, T2) — PF/M-LWDF/PSS H3+H4
  F7: Delay E2E D1 vs D2 (N=20, T2) — PF/M-LWDF/PSS H3+H4
  F8: Jain D1 vs D2 (N=20, T2) — PF/M-LWDF/PSS H4
"""

import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import pandas as pd

# ---------------------------------------------------------------------------
# Rutas
# ---------------------------------------------------------------------------
SIM_ROOT  = Path(__file__).parent.parent.resolve()
STATS_CSV = SIM_ROOT / "results/processed/stats.csv"
RAW_DIR   = SIM_ROOT / "results/raw"
OUT_DIR   = SIM_ROOT / "results/processed/figures"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Estilo global
# ---------------------------------------------------------------------------
plt.rcParams.update({
    "figure.dpi":        150,
    "savefig.dpi":       300,
    "font.family":       "sans-serif",
    "font.size":         11,
    "axes.titlesize":    12,
    "axes.labelsize":    11,
    "legend.fontsize":   9,
    "xtick.labelsize":   10,
    "ytick.labelsize":   10,
    "axes.grid":         True,
    "grid.alpha":        0.3,
    "grid.linestyle":    "--",
    "axes.spines.top":   False,
    "axes.spines.right": False,
})

# Paleta: colores por categoría Capozzi
# (i) azules, (ii) cálidos, (iii) verdes/violeta
SCHED_COLOR = {
    "rr":    "#4878CF",
    "bet":   "#6BAED6",
    "mt":    "#E6550D",
    "tta":   "#FDAE6B",
    "pf":    "#74C476",
    "mlwdf": "#B47CC7",
    "pss":   "#C4A634",
}
SCHED_LABEL = {
    "rr":    "RR",
    "bet":   "BET",
    "mt":    "MT",
    "tta":   "TTA",
    "pf":    "PF",
    "mlwdf": "M-LWDF",
    "pss":   "PSS",
}
SCHED_ORDER = ["rr", "bet", "mt", "tta", "pf", "mlwdf", "pss"]
QOS_ORDER   = ["pf", "mlwdf", "pss"]
N_VALS      = [10, 20, 40]

# ---------------------------------------------------------------------------
# Helpers de datos
# ---------------------------------------------------------------------------

def load_stats() -> pd.DataFrame:
    return pd.read_csv(STATS_CSV)


def _phase(traffic: str) -> str:
    return "phase2_group_a" if traffic == "homogeneous" else "phase3_group_b"


def row(df: pd.DataFrame, sched: str, n: int,
        spatial: str, traffic: str) -> pd.Series:
    ph = _phase(traffic)
    mask = ((df.phase == ph) & (df.scheduler == sched) &
            (df.n_ues == n) & (df.spatial == spatial) &
            (df.traffic == traffic))
    r = df[mask]
    if r.empty:
        raise KeyError(f"Sin datos: {ph} {sched} n={n} {spatial} {traffic}")
    return r.iloc[0]


def errbar(ax, x, y_arr, lo_arr, hi_arr, color, label,
           marker="o", lw=1.8, ms=5):
    yerr = [
        [yi - li for yi, li in zip(y_arr, lo_arr)],
        [hi - yi for yi, hi in zip(y_arr, hi_arr)],
    ]
    ax.errorbar(x, y_arr, yerr=yerr, marker=marker, color=color,
                label=label, linewidth=lw, capsize=4, markersize=ms,
                markerfacecolor="white", markeredgewidth=1.5)


def grouped_bar(ax, pos, height, err_lo, err_hi,
                color, hatch=None, label=None, w=0.35):
    yerr = [[height - err_lo], [err_hi - height]]
    ax.bar(pos, height, w, color=color, hatch=hatch,
           label=label, alpha=0.85,
           yerr=yerr, capsize=4, error_kw={"linewidth": 1.2})


def save(fig, name: str):
    path = OUT_DIR / name
    fig.savefig(path, bbox_inches="tight")
    print(f"  {name}")
    plt.close(fig)


# ---------------------------------------------------------------------------
# F1 — Throughput vs N (D1, T1)
# ---------------------------------------------------------------------------
def plot_f1(df):
    fig, ax = plt.subplots(figsize=(6, 4.2))
    for s in SCHED_ORDER:
        means = [row(df, s, n, "uniform", "homogeneous")["cell_throughput_mbps_mean"] for n in N_VALS]
        lo    = [row(df, s, n, "uniform", "homogeneous")["cell_throughput_mbps_ci95_lo"] for n in N_VALS]
        hi    = [row(df, s, n, "uniform", "homogeneous")["cell_throughput_mbps_ci95_hi"] for n in N_VALS]
        errbar(ax, N_VALS, means, lo, hi, SCHED_COLOR[s], SCHED_LABEL[s])
    ax.set_xlabel("Número de UEs (N)")
    ax.set_ylabel("Throughput de celda [Mbps]")
    ax.set_title("F1 — Throughput vs carga (D1 uniforme, T1 full-buffer)")
    ax.set_xticks(N_VALS)
    ax.legend(ncol=2, loc="center right")
    save(fig, "F1_throughput_vs_N_D1.pdf")


# ---------------------------------------------------------------------------
# F2 — Jain vs N (D1, T1)
# ---------------------------------------------------------------------------
def plot_f2(df):
    fig, ax = plt.subplots(figsize=(6, 4.2))
    for s in SCHED_ORDER:
        means = [row(df, s, n, "uniform", "homogeneous")["jain_index_mean"] for n in N_VALS]
        lo    = [row(df, s, n, "uniform", "homogeneous")["jain_index_ci95_lo"] for n in N_VALS]
        hi    = [row(df, s, n, "uniform", "homogeneous")["jain_index_ci95_hi"] for n in N_VALS]
        errbar(ax, N_VALS, means, lo, hi, SCHED_COLOR[s], SCHED_LABEL[s])
    ax.set_xlabel("Número de UEs (N)")
    ax.set_ylabel("Jain Fairness Index")
    ax.set_title("F2 — Fairness vs carga (D1 uniforme, T1 full-buffer)")
    ax.set_xticks(N_VALS)
    ax.set_ylim(0, 1.08)
    ax.legend(ncol=2, loc="lower center")
    save(fig, "F2_jain_vs_N_D1.pdf")


# ---------------------------------------------------------------------------
# F3 — Jain vs N (D2, T1)
# ---------------------------------------------------------------------------
def plot_f3(df):
    fig, ax = plt.subplots(figsize=(6, 4.2))
    for s in SCHED_ORDER:
        means = [row(df, s, n, "clustered", "homogeneous")["jain_index_mean"] for n in N_VALS]
        lo    = [row(df, s, n, "clustered", "homogeneous")["jain_index_ci95_lo"] for n in N_VALS]
        hi    = [row(df, s, n, "clustered", "homogeneous")["jain_index_ci95_hi"] for n in N_VALS]
        errbar(ax, N_VALS, means, lo, hi, SCHED_COLOR[s], SCHED_LABEL[s])
    ax.set_xlabel("Número de UEs (N)")
    ax.set_ylabel("Jain Fairness Index")
    ax.set_title("F3 — Fairness vs carga (D2 clusterizado, T1 full-buffer)")
    ax.set_xticks(N_VALS)
    ax.set_ylim(0, 1.08)
    ax.legend(ncol=2, loc="lower center")
    save(fig, "F3_jain_vs_N_D2.pdf")


# ---------------------------------------------------------------------------
# F4 — Jain: D1 vs D2 (N=20, T1) — barras agrupadas
# ---------------------------------------------------------------------------
def plot_f4(df):
    fig, ax = plt.subplots(figsize=(8, 4.5))
    x = np.arange(len(SCHED_ORDER))
    w = 0.35
    for i, s in enumerate(SCHED_ORDER):
        r1 = row(df, s, 20, "uniform",   "homogeneous")
        r2 = row(df, s, 20, "clustered", "homogeneous")
        grouped_bar(ax, x[i]-w/2, r1["jain_index_mean"],
                    r1["jain_index_ci95_lo"], r1["jain_index_ci95_hi"],
                    SCHED_COLOR[s], w=w)
        grouped_bar(ax, x[i]+w/2, r2["jain_index_mean"],
                    r2["jain_index_ci95_lo"], r2["jain_index_ci95_hi"],
                    SCHED_COLOR[s], hatch="//", w=w)
    legend_elems = [
        mpatches.Patch(facecolor="grey", alpha=0.85, label="D1 Uniforme"),
        mpatches.Patch(facecolor="grey", alpha=0.85, hatch="//", label="D2 Clusterizado"),
    ]
    ax.legend(handles=legend_elems, loc="upper right")
    ax.set_xticks(x)
    ax.set_xticklabels([SCHED_LABEL[s] for s in SCHED_ORDER])
    ax.set_ylabel("Jain Fairness Index")
    ax.set_title("F4 — Efecto espacial en fairness (N=20, T1 full-buffer)")
    ax.set_ylim(0, 1.15)
    save(fig, "F4_jain_D1vsD2_N20_T1.pdf")


# ---------------------------------------------------------------------------
# F5 — Throughput: T1 vs T2 (N=20, D1)
# ---------------------------------------------------------------------------
def plot_f5(df):
    fig, ax = plt.subplots(figsize=(8, 4.5))
    x = np.arange(len(SCHED_ORDER))
    w = 0.35
    for i, s in enumerate(SCHED_ORDER):
        r1 = row(df, s, 20, "uniform", "homogeneous")
        r2 = row(df, s, 20, "uniform", "heterogeneous")
        grouped_bar(ax, x[i]-w/2, r1["cell_throughput_mbps_mean"],
                    r1["cell_throughput_mbps_ci95_lo"], r1["cell_throughput_mbps_ci95_hi"],
                    SCHED_COLOR[s], w=w)
        grouped_bar(ax, x[i]+w/2, r2["cell_throughput_mbps_mean"],
                    r2["cell_throughput_mbps_ci95_lo"], r2["cell_throughput_mbps_ci95_hi"],
                    SCHED_COLOR[s], hatch="//", w=w)
    legend_elems = [
        mpatches.Patch(facecolor="grey", alpha=0.85, label="T1 Full-buffer"),
        mpatches.Patch(facecolor="grey", alpha=0.85, hatch="//", label="T2 Heterogéneo"),
    ]
    ax.legend(handles=legend_elems, loc="upper right")
    ax.set_xticks(x)
    ax.set_xticklabels([SCHED_LABEL[s] for s in SCHED_ORDER])
    ax.set_ylabel("Throughput de celda [Mbps]")
    ax.set_title("F5 — Efecto del tráfico heterogéneo en throughput (N=20, D1 uniforme)")
    save(fig, "F5_throughput_T1vsT2_N20_D1.pdf")


# ---------------------------------------------------------------------------
# F6 — GBR vs BE throughput (N=20, D1, T2) para PF, M-LWDF, PSS
# ---------------------------------------------------------------------------

def _gbr_be_from_runs(phase: str, sched: str, n: int, spatial: str):
    """
    Lee FlowStats.csv de las 20 corridas del grupo indicado.
    Identifica flujos por volumen de tx_pkts:
      > 20 000 paquetes  → BE full-buffer
      2 000-20 000       → video GBR (1.5 Mbps, 1000 B/pkt)
      100-2 000          → gaming GBR (64 kbps, 160 B/pkt)
      < 100              → señalización / EPC (ignorar)
    Devuelve (gbr_mbps_mean, be_mbps_mean) sobre las corridas disponibles.
    """
    pattern = f"{sched}_{spatial}_heterogeneous_n{n}_r*"
    dirs = sorted((RAW_DIR / phase).glob(pattern))
    gbr_runs, be_runs = [], []
    for d in dirs:
        fp = d / "FlowStats.csv"
        if not fp.exists():
            continue
        gbr_b = be_b = 0
        with open(fp) as f:
            for r in csv.DictReader(f):
                tx = int(r["tx_pkts"])
                rb = int(r["rx_bytes"])
                if tx > 20_000:
                    be_b  += rb
                elif tx > 2_000:
                    gbr_b += rb          # video
                elif tx > 100:
                    gbr_b += rb          # gaming
        meas = 25.0
        gbr_runs.append(gbr_b * 8 / meas / 1e6)
        be_runs.append(be_b   * 8 / meas / 1e6)
    if not gbr_runs:
        return 0.0, 0.0, 0.0, 0.0
    return (float(np.mean(gbr_runs)),
            float(np.std(gbr_runs, ddof=1)),
            float(np.mean(be_runs)),
            float(np.std(be_runs, ddof=1)))


def plot_f6():
    fig, ax = plt.subplots(figsize=(6, 4.2))
    x   = np.arange(len(QOS_ORDER))
    w   = 0.35
    ph  = "phase3_group_b"
    t   = 2.093
    n   = 20

    gbr_m, gbr_s, be_m, be_s = [], [], [], []
    for s in QOS_ORDER:
        gm, gs, bm, bs = _gbr_be_from_runs(ph, s, n, "uniform")
        gbr_m.append(gm); gbr_s.append(gs)
        be_m.append(bm);  be_s.append(bs)

    n_obs = 20
    for i, s in enumerate(QOS_ORDER):
        ci_g = t * gbr_s[i] / np.sqrt(n_obs)
        ci_b = t * be_s[i]  / np.sqrt(n_obs)
        ax.bar(x[i]-w/2, gbr_m[i], w, color=SCHED_COLOR[s],
               alpha=0.9, label=f"{SCHED_LABEL[s]} GBR" if i == 0 else "_",
               yerr=ci_g, capsize=4, error_kw={"linewidth": 1.2})
        ax.bar(x[i]+w/2, be_m[i],  w, color=SCHED_COLOR[s],
               alpha=0.45, hatch="//",
               label=f"{SCHED_LABEL[s]} BE" if i == 0 else "_",
               yerr=ci_b, capsize=4, error_kw={"linewidth": 1.2})

    # Leyenda manual
    gbr_patch = mpatches.Patch(facecolor="grey", alpha=0.9, label="GBR (video+gaming)")
    be_patch  = mpatches.Patch(facecolor="grey", alpha=0.45, hatch="//", label="Best-Effort")
    color_patches = [mpatches.Patch(facecolor=SCHED_COLOR[s],
                                    label=SCHED_LABEL[s]) for s in QOS_ORDER]
    ax.legend(handles=[gbr_patch, be_patch] + color_patches,
              loc="upper right", fontsize=8)
    ax.set_xticks(x)
    ax.set_xticklabels([SCHED_LABEL[s] for s in QOS_ORDER])
    ax.set_ylabel("Throughput [Mbps]")
    ax.set_title("F6 — GBR vs BE: efecto del scheduler QoS-aware (N=20, D1, T2)")
    save(fig, "F6_GBR_vs_BE_N20_D1_T2.pdf")


# ---------------------------------------------------------------------------
# F7 — Delay E2E: D1 vs D2 (N=20, T2) para PF, M-LWDF, PSS
# ---------------------------------------------------------------------------
def plot_f7(df):
    fig, ax = plt.subplots(figsize=(5, 4.2))
    x_pos  = [0, 1]
    xlabs  = ["D1 Uniforme", "D2 Clusterizado"]
    spatials = ["uniform", "clustered"]
    for s in QOS_ORDER:
        means = [row(df, s, 20, sp, "heterogeneous")["mean_delay_ms_mean"] for sp in spatials]
        lo    = [row(df, s, 20, sp, "heterogeneous")["mean_delay_ms_ci95_lo"] for sp in spatials]
        hi    = [row(df, s, 20, sp, "heterogeneous")["mean_delay_ms_ci95_hi"] for sp in spatials]
        errbar(ax, x_pos, means, lo, hi, SCHED_COLOR[s], SCHED_LABEL[s])
    ax.set_xticks(x_pos)
    ax.set_xticklabels(xlabs)
    ax.set_ylabel("Delay E2E promedio [ms]")
    ax.set_title("F7 — Delay E2E: D1 vs D2 (N=20, T2 heterogéneo)")
    ax.legend()
    save(fig, "F7_delay_D1vsD2_N20_T2.pdf")


# ---------------------------------------------------------------------------
# F8 — Jain: D1 vs D2 (N=20, T2) para PF, M-LWDF, PSS
# ---------------------------------------------------------------------------
def plot_f8(df):
    fig, ax = plt.subplots(figsize=(5, 4.2))
    x_pos  = [0, 1]
    xlabs  = ["D1 Uniforme", "D2 Clusterizado"]
    spatials = ["uniform", "clustered"]
    for s in QOS_ORDER:
        means = [row(df, s, 20, sp, "heterogeneous")["jain_index_mean"] for sp in spatials]
        lo    = [row(df, s, 20, sp, "heterogeneous")["jain_index_ci95_lo"] for sp in spatials]
        hi    = [row(df, s, 20, sp, "heterogeneous")["jain_index_ci95_hi"] for sp in spatials]
        errbar(ax, x_pos, means, lo, hi, SCHED_COLOR[s], SCHED_LABEL[s])
    ax.set_xticks(x_pos)
    ax.set_xticklabels(xlabs)
    ax.set_ylabel("Jain Fairness Index")
    ax.set_title("F8 — Fairness bajo tráfico heterogéneo: D1 vs D2 (N=20, T2)")
    ax.set_ylim(0, 1.08)
    ax.legend()
    save(fig, "F8_jain_D1vsD2_N20_T2.pdf")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    print("Cargando stats.csv...")
    df = load_stats()
    print(f"  {len(df)} grupos cargados\n")
    print("Generando figuras:")
    plot_f1(df)
    plot_f2(df)
    plot_f3(df)
    plot_f4(df)
    plot_f5(df)
    plot_f6()
    plot_f7(df)
    plot_f8(df)
    print(f"\nFiguras guardadas en: {OUT_DIR}")


if __name__ == "__main__":
    main()
