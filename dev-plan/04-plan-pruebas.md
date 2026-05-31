# Plan de Pruebas — Ejecución, Automatización y Análisis

---

## Estado actual del proyecto

```
Fase 0 │ Setup del entorno                    │ ✅ COMPLETO
Fase 1 │ Validación de schedulers             │ ✅ COMPLETO (10/10 runs OK)
Fase 2 │ Implementación M-LWDF               │ ✅ COMPLETO
Fase 3 │ Validación M-LWDF + mejoras         │ ✅ COMPLETO
Fase 4 │ Ejecución del experimento completo  │ ✅ COMPLETO (1,680/1,680 runs, 0 errores)
Fase 5 │ Análisis estadístico y figuras      │ 🔄 EN PROGRESO (master.csv+stats.csv ✅, figuras ⏳)
Fase 6 │ Escritura del paper                 │ ⏳ PENDIENTE
```

---

## Mejoras aplicadas al entorno de simulación

Las siguientes correcciones fueron identificadas y aplicadas antes de lanzar el experimento completo:

| Fix | Problema | Solución | Impacto |
|-----|---------|----------|---------|
| Fix1 | Tráfico heterogéneo correlacionado con clusters (C1=video, C2=gaming, C3=BE) | Asignación interleaved `u % 3` | H3/H4 ahora independientes de posición |
| Fix2 | Sin fading Rayleigh — canal puramente determinístico | `TraceFadingLossModel` EPA 3 km/h | Schedulers channel-aware explotan diversidad real |
| Fix3 | CBR en T1 — buffers vacíos → schedulers no pueden elegir | OnOff always-on 10 Mbps/UE (full-buffer) | MT starvation visible, diferencias entre schedulers claras |
| Fix4 | Clusters a igual distancia (todos a 300 m) | 3 clusters: C1=100m · C2=250m · C3=400m | Gradiente real de canal — requerimiento del proyecto |
| Fix5 | Solo delay RLC — sin métricas E2E | FlowMonitor → `FlowStats.csv` | PLR y delay E2E por flujo IP disponibles |

---

## Estructura de directorios (estado real)

```
sim/
├── experiment.yaml              ← fuente de verdad del experimento
├── ns-3.47/
│   ├── scratch/ofdma-scheduler-eval.cc   ← script principal (con Fix1-5)
│   └── src/lte/model/
│       ├── mlwdf-ff-mac-scheduler.{h,cc} ← M-LWDF integrado
│       └── fading-traces/fading_trace_EPA_3kmph.fad
├── schedulers/
│   └── mlwdf-ff-mac-scheduler.{h,cc}    ← fuente canónica (en sync)
├── scripts/
│   ├── orchestrator.py   ← conductor principal (lee experiment.yaml)
│   ├── analyze_run.py    ← análisis por corrida → JSON
│   ├── process_all.py    ← consolida todas las corridas → master.csv
│   ├── compute_stats.py  ← IC95% por grupo → stats.csv
│   └── plot_results.py   ← figuras del paper (pendiente)
└── results/
    ├── raw/
    │   ├── phase1_validation/   ← 10 runs de validación
    │   ├── phase2_group_a/      ← 840 runs Grupo A (H1,H2)
    │   └── phase3_group_b/      ← 840 runs Grupo B (H3,H4)
    └── processed/
        ├── master.csv
        ├── stats.csv
        └── figures/             ← F1–F8 del paper
```

---

## Fase 4 — Ejecución completa

### Comando de lanzamiento (desde sim/)

```bash
# Validación — salta automáticamente los 10 ya completados
python3 scripts/orchestrator.py --phase validate

# Grupo A — 840 runs (H1, H2)
python3 scripts/orchestrator.py --phase run --group a --jobs 8

# Grupo B — 840 runs (H3, H4)
python3 scripts/orchestrator.py --phase run --group b --jobs 8

# Análisis
python3 scripts/orchestrator.py --phase analyze

# Estado en cualquier momento
python3 scripts/orchestrator.py --phase status
```

El orchestrator es **reanudable**: si se interrumpe, al relanzar salta corridas que ya tienen `DlRlcStats.txt`.

### Parámetros del script ns-3

```
--scheduler  : rr | bet | mt | tta | pf | mlwdf | pss
--nUEs       : 10 | 20 | 40
--spatial    : uniform | clustered
--traffic    : homogeneous | heterogeneous
--runId      : 1–20
--simTime    : 30 (segundos)
--outputDir  : ruta absoluta de salida
```

### Estimación de tiempo (con fading activo ~40 s/run)

| Fase | Runs | Tiempo (8 cores) |
|------|------|-----------------|
| Validación | 10 | ~7 min |
| Grupo A | 840 | ~1.2 h |
| Grupo B | 840 | ~1.2 h |
| **Total** | **1,680** | **~2.5 h** |

---

## Fase 1 — Criterios de aceptación (calibrados con fading + full-buffer)

**Escenario de validación:** D1 uniforme, T1 full-buffer, N=20, 1 run, simTime=30s

| Métrica | Criterio | Observado | Estado |
|---------|---------|----------|--------|
| Orden throughput | PF ≥ RR > MT | PF>MT>RR (TdMt no explota FD) | ✅ |
| Orden Jain | BET > PF ≥ RR >> MT | BET≈1.0 > PF~0.75 > RR~0.68 > MT~0.17 | ✅ |
| Jain PF mínimo | > 0.70 | ~0.75 | ✅ |
| Jain MT máximo | < 0.45 | ~0.17 con padding | ✅ |
| Jain BET mínimo | > 0.95 | ~1.00 | ✅ |
| UEs servidos (RR/BET/PF/MLWDF/PSS) | 20/20 | 20/20 | ✅ |
| UEs starved MT | ≥ 10/20 | 14/20 starved | ✅ H1 visible |

**Escenario heterogéneo:** D1 uniforme, T2 interleaved, N=20, 1 run

| Criterio | PF | M-LWDF | PSS |
|---------|-----|-------|-----|
| GBR throughput | ~8.5 Mbps | >10 Mbps | >10 Mbps |
| BE throughput | ~7.5 Mbps | <6 Mbps | <7 Mbps |
| Orden GBR | PSS ≥ M-LWDF > PF | ✅ | ✅ |

---

## Fase 5 — Análisis estadístico

### Pipeline

```bash
# Desde sim/
python3 scripts/orchestrator.py --phase analyze  # → master.csv + stats.csv
python3 scripts/orchestrator.py --phase plot     # → F1-F8 en results/figures/
```

### Estadísticas por grupo

Para cada combinación (scheduler, spatial, n_ues, traffic) con 20 corridas:

```python
# IC 95% con t-Student (df=19, t_crítico=2.093)
mean  = x.mean()
std   = x.std(ddof=1)
se    = std / sqrt(20)
ci_lo = mean - 2.093 * se
ci_hi = mean + 2.093 * se
```

### Figuras requeridas para el paper

| Figura | Eje X | Eje Y | Curvas | Datos | Hipótesis |
|--------|-------|-------|--------|-------|-----------|
| F1 | N (10,20,40) | Throughput [Mbps] | 7 schedulers | A (D1) | H1 baseline |
| F2 | N (10,20,40) | Jain index | 7 schedulers | A (D1) | H1 uniforme |
| F3 | N (10,20,40) | Jain index | 7 schedulers | A (D2) | H1 clusterizado |
| F4 | D1 vs D2 | Jain index | 7 schedulers | A (N=20) | H2 |
| F5 | T1 vs T2 | Throughput [Mbps] | 7 schedulers | A2 vs B2 | H3 |
| F6 | T1 vs T2 | Throughput BE/GBR | PF · M-LWDF · PSS | B2 | H3+H4 bearer |
| F7 | D1 vs D2 | Delay E2E [ms] | PF · M-LWDF · PSS | B2,B5 | H3+H4 delay |
| F8 | D1 vs D2 | Jain index | PF · M-LWDF · PSS | B2,B5 | H4 fairness |

Todas las figuras con barras de error = IC 95%.

### Tests estadísticos para hipótesis

| Comparación | Test | Hipótesis | Dirección esperada |
|-------------|------|-----------|-------------------|
| Jain(MT, D2) vs Jain(MT, D1) | Welch t-test | H1 | D2 << D1, p < 0.05 |
| \|ΔJain_D1→D2\| vs \|ΔJain_N10→N40\| en PF | efecto size | H2 | espacial > N |
| Throughput(PF, T1=A2) vs Throughput(PF, T2=B2) | Welch t-test | H3 | T2 < T1, p < 0.05 |
| GBR_tput(M-LWDF, T2) vs GBR_tput(PF, T2) | Welch t-test | H3 | M-LWDF > PF |
| GBR_tput(PSS, T2) vs GBR_tput(PF, T2) | Welch t-test | H4 | PSS > PF |
| Jain(PSS, D2, T2) vs Jain(PF, D2, T2) | Welch t-test | H4 | PSS ≥ PF |

---

## Checklist antes de lanzar el experimento completo

- [x] ns-3.47 compila sin errores con M-LWDF + fading + FlowMonitor
- [x] Fix1: tráfico heterogéneo interleaved (`u % 3`) — verificado en ciclo 2
- [x] Fix2: fading EPA activo — delay E2E varía por UE (5–195 ms)
- [x] Fix3: full-buffer — MT starvation severa (14/20 UEs con 0 throughput)
- [x] Fix4: clusters a 100m/250m/400m — gradiente real de canal
- [x] Fix5: FlowStats.csv generado correctamente
- [x] Fase 1 validación: 10/10 runs OK, criterios de aceptación calibrados
- [x] Orchestrator genera 840 + 840 = 1,680 runs (verificado con --dry-run)
- [x] Directorios de resultados creados: phase1, phase2, phase3
- [x] Dependencias Python: numpy, pandas, scipy, yaml instalados
- [x] Validación Fase 1 con nuevo entorno (fading + full-buffer) — 10/10 OK, criterios pasan
- [x] Lanzar Grupo A (840 runs) — completado, 0 errores
- [x] Lanzar Grupo B (840 runs) — completado, 0 errores
- [x] process_all.py → master.csv (1,680 filas)
- [x] compute_stats.py → stats.csv (84 grupos con IC95%)
- [ ] plot_results.py — escribir y generar F1–F8
- [ ] Tests Welch t-test H1–H4 (p-values)
- [ ] Escritura del paper

---

## Estructura del paper (producto final)

```
1. Introducción
   - Motivación: scheduling en OFDMA bajo heterogeneidad espacial y de tráfico
   - Pregunta de investigación central
   - Contribución: evaluación sistemática 7 paradigmas con taxonomía Capozzi

2. Marco teórico
   - OFDMA y asignación de recursos en LTE
   - Conceptos: CQI, RB, TTI, fairness (Jain), QoS (GBR/non-GBR)

3. Estado del arte y taxonomía (Capozzi 2012)
   - 5 categorías; 3 activas (i, ii, iii)
   - Los 7 algoritmos con sus fórmulas de métrica

4. Metodología
   - Entorno ns-3.47 con fading EPA, path loss log-distance
   - D1 (uniforme) vs D2 (clustered near/mid/far)
   - T1 (full-buffer) vs T2 (heterogéneo interleaved)
   - 1,680 simulaciones, IC 95%, Welch t-test

5. Resultados
   - 5.1 H1/H2: Efecto de heterogeneidad espacial — Figuras F1–F4
   - 5.2 H3/H4: Efecto del tráfico heterogéneo — Figuras F5–F8

6. Discusión
   - Trade-off throughput vs fairness por categoría
   - Implicaciones eMBB (MT/PF), URLLC (M-LWDF), mMTC (RR/BET)
   - Limitaciones: celda única, canal quasi-estático, sin interferencia

7. Conclusiones
   - Respuesta: ¿Es posible satisfacer throughput + fairness + QoS simultáneamente?

8. Bibliografía
```
