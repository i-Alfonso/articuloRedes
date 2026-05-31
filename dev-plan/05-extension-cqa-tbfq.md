# Fase 4 — Extensión: CQA y TBFQ como complemento de categoría (iii)

> **Branch:** `extent`  
> **Estado:** ⏳ PENDIENTE de ejecución

---

## Motivación

El experimento principal (1,680 corridas) cubre 7 schedulers en las categorías i, ii y iii de Capozzi.
Para la categoría iii (channel-aware / QoS-aware) se seleccionaron M-LWDF y PSS como representantes.

Sin embargo, ns-3.47 incluye dos schedulers adicionales de categoría iii no utilizados:

| Algoritmo | Clase ns-3.47 | Mecanismo QoS |
|-----------|--------------|---------------|
| **CQA** — Channel and QoS Aware | `CqaFfMacScheduler` | Métrica compuesta: CQI + HOL delay + tamaño de cola + clase de prioridad |
| **TBFQ** — Token Bank Fair Queuing | `FdTbfqFfMacScheduler` | Banco de tokens (crédito/deuda): fairness garantizada por presupuesto de bits |

### Justificación científica

**H4 afirma** que los schedulers QoS-aware mejoran el compromiso fairness-throughput frente a PF.
Esta conclusión se apoya en M-LWDF y PSS. Si CQA y TBFQ — con mecanismos QoS completamente distintos —
muestran el mismo patrón de ventaja sobre PF en D2+T2, **H4 se fortalece**.
Si alguno diverge, enriquece el análisis con una matización.

**Diferencias de paradigma dentro de cat (iii):**

| Scheduler | Priorización | Ventana temporal | Tipo de QoS |
|-----------|-------------|-----------------|-------------|
| M-LWDF | Delay HOL lineal × channel gain | Instantánea (1 TTI) | Delay-driven |
| PSS | Separación GBR/non-GBR (TDPS + FDPS) | 2 etapas por TTI | Bearer-driven |
| CQA | CQI + HOL delay + queue occupancy + prioridad | Instantánea | Multi-criterio |
| TBFQ | Token bank: crédito si infraservido, deuda si sobreservido | Ventana larga (acumulativa) | Budget-driven |

Las cuatro implementaciones son nativas en ns-3.47, sin necesidad de código adicional.

---

## Diseño de la extensión

Misma estructura exacta que los Grupos A y B — solo cambian los schedulers evaluados.

### Fase 4a — Grupo A Extensión (heterogeneidad espacial, T1)

| ID | Distribución | N usuarios | Algoritmos | Corridas | Total |
|----|-------------|-----------|-----------|---------|-------|
| A1e | Uniforme (D1) | 10 | CQA, TBFQ | 20 | 40 |
| A2e | Uniforme (D1) | 20 | CQA, TBFQ | 20 | 40 |
| A3e | Uniforme (D1) | 40 | CQA, TBFQ | 20 | 40 |
| A4e | Clusterizada (D2) | 10 | CQA, TBFQ | 20 | 40 |
| A5e | Clusterizada (D2) | 20 | CQA, TBFQ | 20 | 40 |
| A6e | Clusterizada (D2) | 40 | CQA, TBFQ | 20 | 40 |

**Subtotal 4a: 240 corridas**

### Fase 4b — Grupo B Extensión (heterogeneidad de tráfico, T2)

| ID | Distribución | N usuarios | Algoritmos | Corridas | Total |
|----|-------------|-----------|-----------|---------|-------|
| B1e | Uniforme (D1) | 10 | CQA, TBFQ | 20 | 40 |
| B2e | Uniforme (D1) | 20 | CQA, TBFQ | 20 | 40 |
| B3e | Uniforme (D1) | 40 | CQA, TBFQ | 20 | 40 |
| B4e | Clusterizada (D2) | 10 | CQA, TBFQ | 20 | 40 |
| B5e | Clusterizada (D2) | 20 | CQA, TBFQ | 20 | 40 |
| B6e | Clusterizada (D2) | 40 | CQA, TBFQ | 20 | 40 |

**Subtotal 4b: 240 corridas**

**Total extensión: 480 corridas** (~50 min con 8 cores a ~75 s/run)

---

## Parámetros de simulación

Idénticos al experimento principal (Fix1-Fix5 aplicados):

```
simTime:     30 s (5 s warmup descartado)
fading:      TraceFadingLossModel EPA 3 km/h
traffic T1:  OnOff always-on 10 Mbps/UE (full-buffer)
traffic T2:  interleaved u%3 (video GBR + gaming GBR + BE)
clusters:    C1=100m · C2=250m · C3=400m, radio 70m
runs:        20 por configuración (runId 1-20)
```

---

## Comandos de ejecución (desde sim/)

```bash
# Fase 4a — 240 runs (espacial, T1)
python3 scripts/orchestrator.py --phase run --group ea --jobs 8

# Fase 4b — 240 runs (tráfico, T2)
python3 scripts/orchestrator.py --phase run --group eb --jobs 8

# Análisis consolidado (incluye corridas principales + extensión)
python3 scripts/orchestrator.py --phase analyze

# Estado
python3 scripts/orchestrator.py --phase status
```

---

## Análisis esperado

Las corridas de extensión se consolidan en `master.csv` y `stats.csv` junto con las originales.
Los nuevos schedulers aparecerán en todas las figuras F1-F8 con sus propias curvas/barras.

**Preguntas que responde la extensión:**

1. ¿Sigue CQA el mismo patrón que M-LWDF/PSS en D2+T2? → robustez de H4
2. ¿TBFQ (budget-driven) tiene fairness tan alta como BET pero con mejor throughput? → nueva narrativa
3. ¿El mecanismo específico dentro de cat (iii) importa (M-LWDF vs PSS vs CQA) o todos son equivalentes?

---

## Checklist

- [x] CQA y TBFQ añadidos a `ofdma-scheduler-eval.cc` y recompilados
- [x] CQA y TBFQ verificados con test run de 5 s (FlowStats.csv generado)
- [x] `experiment.yaml` actualizado con fase4_extent_a y fase4_extent_b
- [x] `orchestrator.py` actualizado con grupos `ea` y `eb`
- [ ] Lanzar fase 4a (240 runs)
- [ ] Lanzar fase 4b (240 runs)
- [ ] Regenerar master.csv + stats.csv con `--phase analyze`
- [ ] Regenerar figuras con nuevos schedulers (`--phase plot`)
- [ ] Regenerar reporte de análisis (`generate_analysis.py`)
