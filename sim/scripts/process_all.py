#!/usr/bin/env python3
"""
process_all.py — Consolida todas las corridas de producción en master.csv.

Escanea results/raw/phase2_group_a/ y results/raw/phase3_group_b/,
llama a analyze_run.py por cada directorio con DlRlcStats.txt,
y consolida los resultados en results/processed/master.csv.

Uso (desde sim/):
  python3 scripts/process_all.py

También invocado automáticamente por:
  python3 scripts/orchestrator.py --phase analyze
"""

import json
import re
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Rutas
# ---------------------------------------------------------------------------
SIM_ROOT       = Path(__file__).parent.parent.resolve()
ANALYZE_SCRIPT = SIM_ROOT / "scripts" / "analyze_run.py"
PHASES = [
    ("phase2_group_a",   SIM_ROOT / "results" / "raw" / "phase2_group_a"),
    ("phase3_group_b",   SIM_ROOT / "results" / "raw" / "phase3_group_b"),
    ("phase4_extent_a",  SIM_ROOT / "results" / "raw" / "phase4_extent_a"),
    ("phase4_extent_b",  SIM_ROOT / "results" / "raw" / "phase4_extent_b"),
]
OUT = SIM_ROOT / "results" / "processed" / "master.csv"

# Convención: {sched}_{spatial}_{traffic}_n{N}_r{RR}
TAG_PATTERN = re.compile(
    r"^(?P<scheduler>[a-z]+)"
    r"_(?P<spatial>[a-z]+)"
    r"_(?P<traffic>[a-z]+)"
    r"_n(?P<n_ues>\d+)"
    r"_r(?P<run_id>\d+)$"
)

WARMUP  = "5.0"
SIMTIME = "30.0"

# Columnas del CSV de salida (orden fijo para reproducibilidad)
COLUMNS = [
    "phase", "scheduler", "n_ues", "spatial", "traffic", "run_id",
    "cell_throughput_mbps", "jain_index", "mean_delay_ms", "plr",
    "n_ues_active", "n_ues_total",
]


def process_run_dir(phase_name: str, run_dir: Path) -> dict | None:
    rlc = run_dir / "DlRlcStats.txt"
    if not rlc.exists():
        return None

    m = TAG_PATTERN.match(run_dir.name)
    if not m:
        print(f"  SKIP (nombre no reconocido): {run_dir.name}", file=sys.stderr)
        return None

    g       = m.groupdict()
    n_ues   = int(g["n_ues"])
    run_id  = int(g["run_id"])

    result = subprocess.run(
        [sys.executable, str(ANALYZE_SCRIPT),
         "--raw-dir", str(run_dir),
         "--warmup",  WARMUP,
         "--simtime", SIMTIME,
         "--nues",    str(n_ues)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"  ERROR {run_dir.name}: {result.stderr.strip()[:200]}", file=sys.stderr)
        return None

    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError:
        print(f"  ERROR JSON {run_dir.name}: {result.stdout[:120]}", file=sys.stderr)
        return None

    row = {
        "phase":     phase_name,
        "scheduler": g["scheduler"],
        "n_ues":     n_ues,
        "spatial":   g["spatial"],
        "traffic":   g["traffic"],
        "run_id":    run_id,
        "cell_throughput_mbps": data.get("cell_throughput_mbps"),
        "jain_index":           data.get("jain_index"),
        "mean_delay_ms":        data.get("mean_delay_ms"),
        "plr":                  data.get("plr"),
        "n_ues_active":         data.get("n_ues_active"),
        "n_ues_total":          data.get("n_ues_total"),
    }
    return row


def main() -> None:
    # Verificar dependencia
    if not ANALYZE_SCRIPT.exists():
        sys.exit(f"ERROR: {ANALYZE_SCRIPT} no encontrado")

    all_rows = []
    for phase_name, phase_dir in PHASES:
        if not phase_dir.exists():
            print(f"  WARNING: {phase_dir} no existe — saltando", file=sys.stderr)
            continue

        run_dirs = sorted(d for d in phase_dir.iterdir() if d.is_dir())
        phase_rows = []
        for rd in run_dirs:
            row = process_run_dir(phase_name, rd)
            if row:
                phase_rows.append(row)

        print(f"  {phase_name}: {len(phase_rows)}/{len(run_dirs)} runs procesados")
        all_rows.extend(phase_rows)

    if not all_rows:
        sys.exit("ERROR: No se encontraron runs procesables. ¿Se ejecutaron las simulaciones?")

    # Escribir CSV
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w") as f:
        f.write(",".join(COLUMNS) + "\n")
        for row in all_rows:
            line = ",".join(str(row.get(col, "")) for col in COLUMNS)
            f.write(line + "\n")

    print(f"\n  master.csv guardado: {OUT} ({len(all_rows)} filas)")


if __name__ == "__main__":
    main()
