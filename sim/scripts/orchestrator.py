#!/usr/bin/env python3
"""
orchestrator.py — Conductor principal del experimento OFDMA scheduling.

Lee experiment.yaml y gestiona todo el ciclo: validación → simulaciones → análisis → figuras.

Uso (ejecutar desde sim/):
  python3 scripts/orchestrator.py --phase validate
  python3 scripts/orchestrator.py --phase run --group a --jobs 8
  python3 scripts/orchestrator.py --phase run --group b --jobs 4
  python3 scripts/orchestrator.py --phase analyze
  python3 scripts/orchestrator.py --phase plot
  python3 scripts/orchestrator.py --phase status
  python3 scripts/orchestrator.py --phase run --group a --dry-run   # ver comandos sin ejecutar

Fases en orden:
  1. validate  → smoke test, criterios de aceptación
  2. run -g a  → 840 corridas Grupo A (H1, H2)
  3. run -g b  → 280 corridas Grupo B (H3, H4)
  4. analyze   → master.csv + stats.csv
  5. plot      → 8 figuras del paper
"""

import argparse
import json
import multiprocessing
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from datetime import datetime, timedelta
from pathlib import Path

import yaml

# ---------------------------------------------------------------------------
# Rutas — todo relativo a sim/ donde se ejecuta el script
# ---------------------------------------------------------------------------
SIM_ROOT    = Path(__file__).parent.parent.resolve()
CONFIG_FILE = SIM_ROOT / "experiment.yaml"
SCRIPTS_DIR = SIM_ROOT / "scripts"
NS3_DIR     = SIM_ROOT / "ns-3.47"


def load_config(path: Path = CONFIG_FILE) -> dict:
    with open(path) as f:
        return yaml.safe_load(f)


def ns3_binary(cfg: dict) -> Path:
    return SIM_ROOT / cfg["paths"]["ns3_binary"]


def results_root(cfg: dict) -> Path:
    return SIM_ROOT / cfg["paths"]["results_raw"]


# ---------------------------------------------------------------------------
# Generación de runs
# ---------------------------------------------------------------------------

def _make_run(sched: str, spatial: str, traffic: str, n_ues: int,
              run_id: int, sim_time: int, output_dir: Path) -> dict:
    tag = f"{sched}_{spatial}_{traffic}_n{n_ues}_r{run_id:02d}"
    return {
        "tag":        tag,
        "output_dir": str(output_dir / tag),
        "ns3_args": [
            f"--scheduler={sched}",
            f"--nUEs={n_ues}",
            f"--spatial={spatial}",
            f"--traffic={traffic}",
            f"--runId={run_id}",
            f"--simTime={sim_time}",
            f"--outputDir={output_dir / tag}",
        ],
        "n_ues": n_ues,
    }


GROUP_KEY = {
    "a":  "phase2_group_a",
    "b":  "phase3_group_b",
    "ea": "phase4_extent_a",
    "eb": "phase4_extent_b",
}


def generate_runs(cfg: dict, phase: str, group: str | None = None) -> list[dict]:
    runs = []

    if phase == "validate":
        exp    = cfg["experiments"]["phase1_validation"]
        outdir = SIM_ROOT / exp["output_dir"]
        for _sc_name, sc in exp["scenarios"].items():
            for sched in sc["schedulers"]:
                runs.append(_make_run(
                    sched, sc["spatial"], sc["traffic"],
                    sc["n_ues"], 1, exp["sim_time_s"], outdir
                ))

    elif phase == "run":
        key = GROUP_KEY[group]
        exp = cfg["experiments"][key]
        outdir   = SIM_ROOT / exp["output_dir"]
        sim_time = exp["sim_time_s"]
        n_runs   = exp["runs"]
        scheds   = exp["schedulers"]
        traffic  = exp["traffic"]

        for _sc_name, sc in exp["scenarios"].items():
            # n_ues puede estar en el escenario (grupo A y B simétrico)
            # o fijo en el nivel de experimento (diseño anterior)
            n_ues = sc.get("n_ues") or exp.get("n_ues")
            for sched in scheds:
                for run_id in range(1, n_runs + 1):
                    runs.append(_make_run(
                        sched, sc["spatial"], traffic,
                        n_ues, run_id, sim_time, outdir
                    ))

    return runs


# ---------------------------------------------------------------------------
# Ejecución de una simulación (se llama en proceso hijo)
# ---------------------------------------------------------------------------

def _run_one(run_info: dict) -> tuple[str, bool, float, str]:
    """
    Retorna (tag, success, elapsed_s, status)
    status: "OK" | "SKIP" | "ERROR"
    """
    tag        = run_info["tag"]
    output_dir = Path(run_info["output_dir"])
    binary     = run_info["binary"]

    # Reanudar: saltar si ya tiene datos
    if (output_dir / "DlRlcStats.txt").exists():
        return tag, True, 0.0, "SKIP"

    output_dir.mkdir(parents=True, exist_ok=True)
    t0 = time.monotonic()

    result = subprocess.run(
        [binary] + run_info["ns3_args"],
        capture_output=True,
        text=True,
        cwd=str(NS3_DIR),
    )
    elapsed = time.monotonic() - t0

    if result.returncode != 0:
        (output_dir / "stderr.log").write_text(result.stderr[-4000:])
        return tag, False, elapsed, "ERROR"

    return tag, True, elapsed, "OK"


def _inject_binary(runs: list[dict], binary: str) -> list[dict]:
    for r in runs:
        r["binary"] = binary
    return runs


# ---------------------------------------------------------------------------
# Ejecutar lista de runs (paralelo o serie)
# ---------------------------------------------------------------------------

def execute_runs(runs: list[dict], cfg: dict,
                 jobs: int = 1, dry_run: bool = False) -> None:
    binary = str(ns3_binary(cfg))
    if not Path(binary).exists():
        sys.exit(
            f"ERROR: binario no encontrado: {binary}\n"
            f"Compila con:  cd sim/ns-3.47 && ./ns3 build"
        )

    if dry_run:
        print(f"  [dry-run] {len(runs)} runs generados — primeros 5:")
        for r in runs[:5]:
            print(f"    {r['tag']}")
        if len(runs) > 5:
            print(f"    ... y {len(runs)-5} más")
        return

    runs = _inject_binary(runs, binary)
    total   = len(runs)
    done    = errors = skipped = 0
    t_start = datetime.now()

    with ProcessPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(_run_one, r): r for r in runs}
        for fut in as_completed(futures):
            tag, ok, elapsed, status = fut.result()
            done += 1

            if status == "SKIP":
                skipped += 1
                continue

            elapsed_total = (datetime.now() - t_start).total_seconds()
            runs_left     = total - done
            eta_s         = (elapsed_total / done * runs_left) if done > 0 else 0
            eta_str       = str(timedelta(seconds=int(eta_s)))

            if not ok:
                errors += 1
                print(f"  ERROR  [{done:5d}/{total}] {tag}")
            else:
                pct = done * 100 / total
                print(
                    f"  OK     [{done:5d}/{total}] {pct:5.1f}%  "
                    f"{elapsed:5.1f}s/run  ETA {eta_str}  {tag}"
                )

    print(f"\n  Completado: {done} — OK {done-errors-skipped}  "
          f"SKIP {skipped}  ERROR {errors}")
    if errors:
        print(f"  Revisa los archivos stderr.log en los directorios de salida.")


# ---------------------------------------------------------------------------
# Comandos de fase
# ---------------------------------------------------------------------------

def phase_validate(cfg: dict, args: argparse.Namespace) -> None:
    runs = generate_runs(cfg, "validate")
    print(f"\n[validate] {len(runs)} escenarios de validación (1 run c/u)")
    execute_runs(runs, cfg, jobs=1, dry_run=args.dry_run)

    if not args.dry_run:
        print("\n  Criterios de aceptación (verificar manualmente):")
        exp = cfg["experiments"]["phase1_validation"]
        for name, sc in exp["scenarios"].items():
            if "acceptance_criteria" in sc:
                print(f"  {name}:")
                for k, v in sc["acceptance_criteria"].items():
                    print(f"    {k}: {v}")


def phase_run(cfg: dict, args: argparse.Namespace) -> None:
    if not args.group:
        sys.exit("ERROR: --group a|b requerido con --phase run")

    group_key = GROUP_KEY.get(args.group)
    if not group_key or group_key not in cfg["experiments"]:
        sys.exit(f"ERROR: grupo '{args.group}' no definido en experiment.yaml")

    exp  = cfg["experiments"][group_key]
    runs = generate_runs(cfg, "run", group=args.group)

    print(f"\n[run] Grupo {args.group.upper()} — {exp['label']}")
    print(f"  {len(runs)} runs × ~30 s = estimado {len(runs)*30/args.jobs/3600:.1f} h con {args.jobs} workers")

    execute_runs(runs, cfg, jobs=args.jobs, dry_run=args.dry_run)


def phase_analyze(cfg: dict, args: argparse.Namespace) -> None:
    process_script = SCRIPTS_DIR / "process_all.py"
    stats_script   = SCRIPTS_DIR / "compute_stats.py"

    if not process_script.exists():
        sys.exit(f"ERROR: {process_script} no encontrado")
    if not stats_script.exists():
        sys.exit(f"ERROR: {stats_script} no encontrado")

    print("\n[analyze] Paso 1/2 — Consolidar corridas → master.csv")
    if not args.dry_run:
        subprocess.run([sys.executable, str(process_script)],
                       cwd=str(SIM_ROOT), check=True)

    print("\n[analyze] Paso 2/2 — Calcular estadísticas → stats.csv")
    if not args.dry_run:
        subprocess.run([sys.executable, str(stats_script)],
                       cwd=str(SIM_ROOT), check=True)
    else:
        print("  [dry-run] proceso_all.py + compute_stats.py")


def phase_plot(cfg: dict, args: argparse.Namespace) -> None:
    plot_script = SCRIPTS_DIR / "plot_results.py"
    if not plot_script.exists():
        sys.exit(f"ERROR: {plot_script} no encontrado")

    print("\n[plot] Generando figuras F1-F8")
    if not args.dry_run:
        subprocess.run([sys.executable, str(plot_script)],
                       cwd=str(SIM_ROOT), check=True)
    else:
        figs = cfg.get("analysis", {}).get("figures", {})
        for fid, fig in figs.items():
            print(f"  {fid}: {fig['title']}")


def phase_status(cfg: dict, _args: argparse.Namespace) -> None:
    print("\n[status] Estado de corridas por fase:\n")
    phases = [
        ("phase1_validation", cfg["experiments"]["phase1_validation"]["output_dir"]),
        ("phase2_group_a",    cfg["experiments"]["phase2_group_a"]["output_dir"]),
        ("phase3_group_b",    cfg["experiments"]["phase3_group_b"]["output_dir"]),
        ("phase4_extent_a",   cfg["experiments"].get("phase4_extent_a", {}).get("output_dir", "")),
        ("phase4_extent_b",   cfg["experiments"].get("phase4_extent_b", {}).get("output_dir", "")),
    ]
    for name, reldir in phases:
        d = SIM_ROOT / reldir
        if not d.exists():
            print(f"  {name:<30} — directorio no existe")
            continue
        subdirs   = [p for p in d.iterdir() if p.is_dir()]
        completed = sum(1 for p in subdirs if (p / "DlRlcStats.txt").exists())
        errors    = sum(1 for p in subdirs if (p / "stderr.log").exists())
        print(f"  {name:<30}  {completed:4d} completas / {len(subdirs):4d} dirs  ({errors} con errores)")

    # Totales del plan
    totals = cfg.get("totals", {})
    print(f"\n  Planificado: {totals.get('total_production', '?')} runs de producción")
    print(f"  Tiempo estimado con 8 cores: {totals.get('estimated_time_h', '?')} h")
    print(f"  Espacio estimado: {totals.get('disk_estimate_gb', '?')} GB")

    # Verificar binario
    binary = ns3_binary(cfg)
    bstatus = "✅" if binary.exists() else "❌ no compilado"
    print(f"\n  Binario ns-3: {bstatus}  ({binary})")

    # Verificar scripts
    for script in ["analyze_run.py", "process_all.py", "compute_stats.py", "plot_results.py"]:
        p = SCRIPTS_DIR / script
        mark = "✅" if p.exists() else "⏳ pendiente"
        print(f"  {script:<30} {mark}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="orchestrator.py",
        description="Orchestrador del experimento OFDMA scheduling",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Orden de fases:
  1. validate          → smoke test con criterios de aceptación (prerrequisito)
  2. run --group a     → 840 corridas Grupo A (H1, H2)
  3. run --group b     → 280 corridas Grupo B (H3, H4)
  4. analyze           → master.csv + stats.csv
  5. plot              → 8 figuras del paper

Ejemplos:
  python3 scripts/orchestrator.py --phase status
  python3 scripts/orchestrator.py --phase validate --dry-run
  python3 scripts/orchestrator.py --phase run --group a --jobs 8
  python3 scripts/orchestrator.py --phase run --group b --jobs 4
  python3 scripts/orchestrator.py --phase analyze
        """,
    )
    p.add_argument("--phase",  required=True,
                   choices=["validate", "run", "analyze", "plot", "status"])
    p.add_argument("--group",  choices=["a", "b", "ea", "eb"],
                   help="Grupo del experimento (a|b = producción, ea|eb = extensión CQA/TBFQ)")
    p.add_argument("--jobs",   type=int,
                   default=max(1, multiprocessing.cpu_count() // 2),
                   help="Workers paralelos (default: mitad de núcleos del sistema)")
    p.add_argument("--dry-run", action="store_true",
                   help="Imprime lo que se haría sin ejecutar nada")
    p.add_argument("--config", default=str(CONFIG_FILE),
                   help=f"Ruta al YAML de configuración (default: {CONFIG_FILE})")
    return p


def main() -> None:
    args = build_parser().parse_args()
    cfg  = load_config(Path(args.config))

    dispatch = {
        "validate": phase_validate,
        "run":      phase_run,
        "analyze":  phase_analyze,
        "plot":     phase_plot,
        "status":   phase_status,
    }
    dispatch[args.phase](cfg, args)


if __name__ == "__main__":
    main()
