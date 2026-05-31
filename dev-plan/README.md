# Plan de Desarrollo — Evaluación de Schedulers OFDMA en ns-3

## Índice

| Archivo | Contenido |
|---------|-----------|
| [01-algoritmos.md](01-algoritmos.md) | Especificación técnica de los 7 algoritmos, fórmulas de métrica e implementación en ns-3 |
| [02-metricas-ns3.md](02-metricas-ns3.md) | Cómo capturar throughput, delay y Jain index en ns-3 con el módulo LTE |
| [03-escenarios.md](03-escenarios.md) | Matriz experimental completa: escenarios, variables y configuración de ns-3 |
| [04-plan-pruebas.md](04-plan-pruebas.md) | Plan de ejecución, automatización, análisis estadístico y estructura de resultados |

---

## Resumen ejecutivo

**Objetivo:** Evaluar 7 paradigmas de scheduling en sistemas OFDMA (LTE como entorno controlado) bajo heterogeneidad espacial y de tráfico para responder 4 hipótesis de investigación.

**Stack tecnológico:**
- Simulador: **ns-3.47** con módulo LTE/EPC (en `sim/ns-3.47/`)
- Análisis estadístico: **Python** (pandas, scipy, matplotlib, seaborn)
- Paralelización: `ProcessPoolExecutor` en `orchestrator.py` (procesos OS reales, reanudable)

**Volumen de simulaciones:**
```
7 algoritmos × 2 spatial × 3 nUEs × 2 traffic × 20 corridas = 1,680 simulaciones
```
Tiempo real observado: ~4.5 horas con 8 núcleos (~75 s/run con fading EPA activo).

---

## Algoritmos — Taxonomía Capozzi (2-3-2)

Selección basada en la taxonomía de Capozzi et al. (2012), Sección IV.
Los 3 marcados como **base** son los clásicos explícitamente requeridos por el documento de proyecto.

| # | Nombre | Clase ns-3.47 | Categoría Capozzi | Base | Estado |
|---|--------|--------------|-------------------|------|--------|
| 1 | Round Robin (RR) | `RrFfMacScheduler` | (i) Channel-unaware | ✅ base | Nativo |
| 2 | Blind Equal Throughput (BET) | `FdBetFfMacScheduler` | (i) Channel-unaware | — | Nativo |
| 3 | Maximum Throughput (MT) | `TdMtFfMacScheduler` | (ii) Ch-aware/QoS-unaware | ✅ base | Nativo |
| 4 | Throughput To Average (TTA) | `TtaFfMacScheduler` | (ii) Ch-aware/QoS-unaware | — | Nativo |
| 5 | Proportional Fair (PF) | `PfFfMacScheduler` | (ii) Ch-aware/QoS-unaware | ✅ base | Nativo |
| 6 | Modified LWDF (M-LWDF) | `MlwdfFfMacScheduler` | (iii) Ch-aware/QoS-aware | — | Implementado |
| 7 | Priority Set Scheduler (PSS) | `PssFfMacScheduler` | (iii) Ch-aware/QoS-aware | — | Nativo |

**Nota sobre MT:** Se usa `TdMtFfMacScheduler` (variante TD = Time Domain), que corresponde al MT canónico
de la literatura (Capozzi Figs. 8-10). La variante FD (`FdMtFfMacScheduler`) asigna por RB y no
maximiza throughput total de la misma forma.

**Nota sobre M-LWDF:** Implementado e integrado en sesiones previas en `sim/ns-3.47/src/lte/model/`.

---

## Hipótesis que guían el diseño

- **H1:** Los schedulers orientados a throughput (MT) presentan degradación no lineal de fairness bajo distribución clusterizada
- **H2:** La heterogeneidad espacial impacta más el fairness que el número de usuarios
- **H3:** Los schedulers balanceados (PF) pierden eficiencia espectral bajo tráfico heterogéneo; M-LWDF y PSS compensan de forma distinta
- **H4:** Los schedulers híbridos (PSS) mejoran el compromiso fairness-throughput frente a PF simple

| Hipótesis | Algoritmos clave |
|-----------|-----------------|
| H1 | MT (degradación fuerte), TTA (moderada), BET/RR (referencia) |
| H2 | Todos los de cat (i) y (ii) bajo D1 vs D2 |
| H3 | PF vs M-LWDF vs PSS bajo T2 |
| H4 | PSS vs PF vs M-LWDF bajo T2 |

---

## Pipeline de análisis

```
sim/results/raw/          ← 1,680 carpetas, una por corrida (DlRlcStats.txt + FlowStats.csv)
      ↓  process_all.py   leer cada carpeta, extraer 4 métricas por corrida
   master.csv             1,680 filas (phase, scheduler, n_ues, spatial, traffic, run_id, métricas)
      ↓  compute_stats.py agrupar por (scheduler×spatial×N×traffic), calcular media + IC95%
   stats.csv              84 filas — una por grupo, con media/std/ci95_lo/ci95_hi por métrica
      ↓  plot_results.py  leer stats.csv y generar figuras F1–F8 con barras de error
   figures/F1–F8          imágenes listas para el paper
      ↓  tests Welch       t-test sobre master.csv (20 valores) → p-values para H1–H4
```

| Script | Entrada | Salida | Propósito |
|--------|---------|--------|-----------|
| `analyze_run.py` | carpeta de 1 corrida | JSON con 4 métricas | convierte logs ns-3 a números limpios |
| `process_all.py` | todas las carpetas raw/ | `master.csv` | consolida las 1,680 corridas |
| `compute_stats.py` | `master.csv` | `stats.csv` | media + IC95% por grupo (t-Student df=19) |
| `plot_results.py` | `stats.csv` | figuras F1–F8 | visualización para el paper |
| `orchestrator.py` | `experiment.yaml` | ejecución completa | conductor principal del experimento |

## Prerrequisitos del entorno

```bash
# ns-3.47 dentro del proyecto en sim/ns-3.47/
# M-LWDF ya integrado en sim/ns-3.47/src/lte/model/

# Compilar desde la raíz del proyecto:
cd sim/ns-3.47 && ./ns3 build

# Python para análisis
pip3 install pandas scipy matplotlib seaborn numpy pyyaml
```
