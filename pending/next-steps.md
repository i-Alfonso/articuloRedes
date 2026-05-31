# Pasos Pendientes — Experimento Completo

Estado al 2026-05-28. Lo que sigue es para la próxima sesión.

## Decisión de diseño — 7 algoritmos (taxonomía Capozzi 2-3-2)

| Cat | Algoritmos | Paradigma |
|-----|-----------|-----------|
| (i) Channel-unaware | RR + BET | tiempo igual / throughput igual |
| (ii) Ch-aware/QoS-unaware | MT (TD) + TTA + PF | máx. throughput / intermedio / balance |
| (iii) Ch-aware/QoS-aware | M-LWDF + PSS | delay-aware / híbrido |

- **3 bases requeridos** por doc de proyecto: RR ✅, MT ✅, PF ✅
- **4 adicionales** nativos en ns-3.47: BET, TTA, PSS + M-LWDF (implementado)
- **Total simulaciones:** 7 × 8 grupos × 20 runs = **1,120**
- Script actualizado: parámetro `--scheduler` acepta: `rr | bet | mt | tta | pf | mlwdf | pss`

---

## 1. Analizar resultados ITER 5 ya generados

Los runs ITER 5 ya están en `results/raw/`. Falta procesarlos:

```bash
cd /ruta/al/proyecto

for sched in rr mt pf; do
  for spatial in uniform clustered; do
    python3 sim/scripts/analyze_run.py \
      --raw-dir results/raw/iter5_${sched}_${spatial}_h_n20_r1 \
      --warmup 5.0 --simtime 30.0 --nues 20
  done
done
```

Verificar orden esperado (Fase 1 del plan):

| Métrica       | Orden esperado              |
|---------------|-----------------------------|
| Throughput    | MT ≥ PF > RR (por scheduler) |
| Jain uniforme | PF ≥ RR > MT                |
| Jain clust.   | MT cae más que en uniforme  |

Si MT throughput < PF en runs de 30s: revisar `FdMtFfMacScheduler` vs `TdMtFfMacScheduler`.

---

## 2. Completar ITER 5: mlwdf y pss en ambas distribuciones

```bash
for sched in mlwdf pss; do
  for spatial in uniform clustered; do
    tag="iter5_${sched}_${spatial}_h_n20_r1"
    mkdir -p results/raw/${tag}
    cd sim/ns-3.47
    ./ns3 run "ofdma-scheduler-eval --scheduler=${sched} --nUEs=20 \
        --spatial=${spatial} --traffic=homogeneous \
        --runId=1 --simTime=30 \
        --outputDir=../../results/raw/${tag}"
    cd ../..
  done
done
```

---

## 3. Escribir `run_all.sh` — 1,120 simulaciones en paralelo

Ubicación: `sim/scripts/run_all.sh`

Parámetros del diseño experimental:
- Schedulers: rr mt pf mlwdf pss
- Grupo A (heterogeneidad espacial, tráfico homogéneo):
  - nUEs: 10 20 40
  - spatial: uniform clustered
  - 20 runs cada combo → 5×2×3×20 = **600 corridas**
- Grupo B (heterogeneidad de tráfico):
  - nUEs: 20
  - spatial: uniform clustered
  - traffic: heterogeneous
  - 20 runs → 5×2×20 = **200 corridas**
- Total: **800 corridas**

Template del script (ver dev-plan/04-plan-pruebas.md Fase 4 para la versión completa):

```bash
#!/bin/bash
# run_all.sh — ejecutar desde sim/ns-3.47/
# Requiere: GNU parallel  (brew install parallel)

NS3=./ns3
RESULTS=../../results/raw
SIM_TIME=30

gen_cmd() {
    local sched=$1 nues=$2 spatial=$3 traffic=$4 run=$5
    local tag="${sched}_${spatial}_${traffic}_n${nues}_r${run}"
    echo "${NS3} run \"ofdma-scheduler-eval \
        --scheduler=${sched} --nUEs=${nues} \
        --spatial=${spatial} --traffic=${traffic} \
        --runId=${run} --simTime=${SIM_TIME} \
        --outputDir=${RESULTS}/${tag}\""
}

# Grupo A
for sched in rr bet mt tta pf mlwdf pss; do
  for spatial in uniform clustered; do
    for nues in 10 20 40; do
      for run in $(seq 1 20); do
        gen_cmd $sched $nues $spatial homogeneous $run
      done
    done
  done
done > /tmp/cmds_A.txt

# Grupo B
for sched in rr bet mt tta pf mlwdf pss; do
  for spatial in uniform clustered; do
    for run in $(seq 1 20); do
      gen_cmd $sched 20 $spatial heterogeneous $run
    done
  done
done > /tmp/cmds_B.txt

cat /tmp/cmds_A.txt /tmp/cmds_B.txt > /tmp/all_cmds.txt
wc -l /tmp/all_cmds.txt   # debe ser 800

# Crear directorios de salida
awk '{match($0, /outputDir=([^ "]+)/, a); print a[1]}' /tmp/all_cmds.txt | \
    xargs -I{} mkdir -p {}

# Lanzar con 8 workers (ajustar según núcleos)
parallel --progress -j 8 < /tmp/all_cmds.txt
echo "Completadas todas las simulaciones."
```

Tiempo estimado: ~4-6 h con 8 núcleos (MacBook M-series).

---

## 4. Escribir `process_all.py` — CSV maestro

Ubicación: `sim/scripts/process_all.py`

```python
#!/usr/bin/env python3
"""
Recorre results/raw/, llama a analyze_run para cada directorio,
consolida en un CSV maestro con columnas:
  scheduler, nUEs, spatial, traffic, run_id,
  cell_throughput_mbps, jain_index, mean_delay_ms, plr
"""
import subprocess, json, csv, re, sys
from pathlib import Path

RAW = Path("results/raw")
OUT = Path("results/processed/master.csv")
OUT.parent.mkdir(parents=True, exist_ok=True)

# Nombre esperado: {sched}_{spatial}_{traffic}_n{N}_r{R}
pattern = re.compile(r'^([a-z]+)_([a-z]+)_([a-z]+)_n(\d+)_r(\d+)$')

rows = []
for d in sorted(RAW.iterdir()):
    m = pattern.match(d.name)
    if not m: continue
    sched, spatial, traffic, nues, run = m.groups()
    result = subprocess.run(
        ["python3", "sim/scripts/analyze_run.py",
         "--raw-dir", str(d),
         "--warmup", "5.0", "--simtime", "30.0",
         "--nues", nues],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(f"SKIP {d.name}: {result.stderr.strip()}", file=sys.stderr)
        continue
    data = json.loads(result.stdout)
    rows.append({
        "scheduler": sched,
        "nUEs": int(nues),
        "spatial": spatial,
        "traffic": traffic,
        "run_id": int(run),
        **{k: data[k] for k in
           ["cell_throughput_mbps","jain_index","mean_delay_ms","plr"]},
    })

with open(OUT, "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=rows[0].keys())
    writer.writeheader()
    writer.writerows(rows)

print(f"Guardado: {OUT} ({len(rows)} filas)")
```

---

## 5. Escribir `compute_stats.py` — estadísticas por grupo

Ubicación: `sim/scripts/compute_stats.py`

```python
#!/usr/bin/env python3
import pandas as pd
import numpy as np
from scipy import stats as scipy_stats
from pathlib import Path

df = pd.read_csv("results/processed/master.csv")
groupby = ["scheduler","nUEs","spatial","traffic"]

def ci95(x):
    n = len(x)
    se = x.std(ddof=1) / np.sqrt(n)
    t = scipy_stats.t.ppf(0.975, df=n-1)
    return x.mean() - t*se, x.mean() + t*se

records = []
for keys, grp in df.groupby(groupby):
    row = dict(zip(groupby, keys))
    for metric in ["cell_throughput_mbps","jain_index","mean_delay_ms","plr"]:
        m = grp[metric]
        lo, hi = ci95(m)
        row[f"{metric}_mean"] = round(m.mean(), 4)
        row[f"{metric}_std"]  = round(m.std(ddof=1), 4)
        row[f"{metric}_ci95_lo"] = round(lo, 4)
        row[f"{metric}_ci95_hi"] = round(hi, 4)
    records.append(row)

out = pd.DataFrame(records)
out.to_csv("results/processed/stats.csv", index=False)
print(f"stats.csv: {len(out)} filas")
```

---

## 6. Checklist final antes de ejecutar las 800 corridas

- [ ] ITER 5 completo: mlwdf y pss corren en uniform y clustered (30 s)
- [ ] Resultados ITER 5 analizados — órdenes relativos correctos
- [ ] `run_all.sh` escrito y probado (verificar `wc -l` = 800)
- [ ] Espacio en disco: estimar ~800 × 5 MB = ~4 GB
- [ ] `process_all.py` y `compute_stats.py` listos
- [ ] Instalar dependencias: `pip3 install pandas scipy matplotlib seaborn`
- [ ] Iniciar con `parallel -j $(nproc) < /tmp/all_cmds.txt`

---

## Notas técnicas acumuladas

| Problema | Solución aplicada |
|----------|-------------------|
| eNB TX power 30 dBm → cubre solo ~302 m | `Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(46.0))` |
| Jain inflado para MT (UEs starved ausentes) | `--nues N` en analyze_run.py para padding con cero |
| DlRlcStats.txt tiene 18 columnas (no 15) | OK — columnas extra son PduSize stats, no usadas |
| FD-MT throughput < PF (CBR + starved UEs) | Documentar en paper: FdMtFfMacScheduler ≠ TD-MT de la literatura |
| outputDir relativo desde sim/ns-3.47/ | Paths como `../../results/raw/...` resuelven a `proyecto/results/raw/` |
