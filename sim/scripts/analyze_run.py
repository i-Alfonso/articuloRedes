#!/usr/bin/env python3
"""
analyze_run.py  —  Procesa DlRlcStats.txt de una corrida y extrae métricas.

Uso:
    python analyze_run.py --raw-dir results/raw/pf_u_h_n20_r1 \
                          --warmup 5.0 --simtime 30.0

Salida (stdout JSON):
    {
      "cell_throughput_mbps": ...,
      "jain_index": ...,
      "mean_delay_ms": ...,
      "plr": ...,
      "per_ue_throughput": [...]
    }
"""

import argparse
import json
import math
import os
import sys

import numpy as np


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

DL_RLC_COLS = [
    "start", "end", "cellId", "IMSI", "RNTI", "LCID",
    "nTxPDUs", "TxBytes", "nRxPDUs", "RxBytes",
    "delay", "stdDev", "minDelay", "maxDelay", "PduCorruptRatio",
]


def load_dl_rlc(path: str) -> list[dict]:
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("%"):
                continue
            parts = line.split()
            if len(parts) < len(DL_RLC_COLS):
                continue
            row = {}
            for i, col in enumerate(DL_RLC_COLS):
                try:
                    row[col] = float(parts[i])
                except ValueError:
                    row[col] = parts[i]
            rows.append(row)
    return rows


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------

def jain_index(values: np.ndarray) -> float:
    """Jain fairness index J = (sum x_i)^2 / (n * sum x_i^2)."""
    n = len(values)
    if n == 0:
        return float("nan")
    s1 = float(np.sum(values))
    s2 = float(np.sum(values ** 2))
    if s2 == 0:
        return 1.0
    return (s1 ** 2) / (n * s2)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw-dir",  required=True,  help="Directory with DlRlcStats.txt")
    parser.add_argument("--warmup",   type=float, default=5.0,  help="Warmup seconds to discard")
    parser.add_argument("--simtime",  type=float, default=30.0, help="Total simulation time [s]")
    parser.add_argument("--nues",     type=int,   default=0,    help="Total UEs in simulation (0 = auto from data)")
    args = parser.parse_args()

    rlc_path = os.path.join(args.raw_dir, "DlRlcStats.txt")
    if not os.path.exists(rlc_path):
        sys.exit(f"ERROR: file not found: {rlc_path}")

    rows = load_dl_rlc(rlc_path)
    if not rows:
        sys.exit("ERROR: DlRlcStats.txt is empty or has no data rows")

    # Filter out warmup period
    active = [r for r in rows if r["start"] >= args.warmup]
    if not active:
        sys.exit(f"ERROR: no rows after warmup ({args.warmup} s). "
                 "Reduce --warmup or increase --simtime.")

    duration = args.simtime - args.warmup  # seconds of active measurement

    # Aggregate per UE (RNTI) — sum over all epochs and LCIDs
    ue_rxbytes: dict[int, float] = {}
    ue_txpdus:  dict[int, float] = {}
    ue_rxpdus:  dict[int, float] = {}
    delay_list: list[float]      = []

    for r in active:
        rnti = int(r["RNTI"])
        ue_rxbytes[rnti] = ue_rxbytes.get(rnti, 0.0) + r["RxBytes"]
        ue_txpdus[rnti]  = ue_txpdus.get(rnti, 0.0)  + r["nTxPDUs"]
        ue_rxpdus[rnti]  = ue_rxpdus.get(rnti, 0.0)  + r["nRxPDUs"]
        # Delay is in seconds; only include epochs with actual traffic
        if r["nRxPDUs"] > 0 and r["delay"] > 0:
            delay_list.append(r["delay"] * 1000.0)  # → ms

    # Per-UE throughput in Mbps
    ue_tput = {rnti: (b * 8) / duration / 1e6 for rnti, b in ue_rxbytes.items()}
    tput_arr = np.array(list(ue_tput.values()), dtype=float)

    # Cell throughput
    cell_tput_mbps = float(np.sum(tput_arr))

    # Jain index: pad with zeros for any UEs that never received data (scheduler starvation).
    # This gives a fair cross-scheduler comparison — starved UEs count as 0 throughput.
    n_active = len(tput_arr)
    if args.nues > 0 and args.nues > n_active:
        padding = np.zeros(args.nues - n_active)
        tput_for_jain = np.concatenate([tput_arr, padding])
    else:
        tput_for_jain = tput_arr
    j = jain_index(tput_for_jain)

    # Mean DL RLC delay
    mean_delay_ms = float(np.mean(delay_list)) if delay_list else float("nan")

    # Packet Loss Ratio (cell-wide)
    total_tx = sum(ue_txpdus.values())
    total_rx = sum(ue_rxpdus.values())
    plr = (total_tx - total_rx) / total_tx if total_tx > 0 else float("nan")

    result = {
        "raw_dir":             args.raw_dir,
        "n_ues_active":        n_active,
        "n_ues_total":         args.nues if args.nues > 0 else n_active,
        "duration_s":          duration,
        "cell_throughput_mbps": round(cell_tput_mbps, 4),
        "jain_index":          round(j, 6),
        "mean_delay_ms":       round(mean_delay_ms, 4),
        "plr":                 round(plr, 6),
        "per_ue_throughput_mbps": {str(k): round(v, 4) for k, v in sorted(ue_tput.items())},
    }

    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
