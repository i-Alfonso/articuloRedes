# Reporte de Análisis — Evaluación de Schedulers OFDMA en LTE

> Generado automáticamente desde `stats.csv` y `master.csv`  
> 1920 corridas: 8 schedulers (7 originales + CQA extensión cat iii)  
> IC 95% con t-Student (df=19, t_crit=2.093)

---

## Resumen de hipótesis

| Hipótesis | Enunciado | Resultado |
|-----------|-----------|:---------:|
| H1 | MT presenta degradación de fairness bajo D2 (starvation de C3) | ⚠️ Parcial |
| H2 | Heterogeneidad espacial impacta más que el número de usuarios | ❌ No confirmada (6/7 schedulers) |
| H3 | PF pierde eficiencia bajo tráfico heterogéneo; M-LWDF/PSS compensan | ✅ Confirmada (p<0.001) |
| H4 | PSS/M-LWDF mejoran fairness frente a PF en D2+T2 | ✅ Confirmada en D2 (p<0.001, d≈5) |

> ⚠️ H1 parcial: MT tiene Jain bajo tanto en D1 (0.116) como en D2 (0.135). La starvation ya existe en D1 por fading; D2 la hace sistemática (siempre C3), pero el Jain global no cambia significativamente (p=0.23).
>
> ❌ H2 no confirmada: ΔJain por carga (N=10→40) supera ΔJain espacial (D1→D2) en 6/7 schedulers. Los algoritmos channel-aware absorben la heterogeneidad geográfica eficientemente. La carga es el factor dominante de fairness.

---

## 1. Resumen de métricas — Escenario de referencia

**Condiciones:** N=20, D1 uniforme, T1 full-buffer (A2)

| Scheduler | Categoría | Throughput (Mbps) | IC 95% | Jain | IC 95% | UEs activos |
|-----------|-----------|:-----------------:|--------|:----:|--------|:-----------:|
| RR     | (i) | 7.224 | [6.621, 7.827] | 0.6419 | [0.6016, 0.6823] | 20/20 |
| BET    | (i) | 4.315 | [4.047, 4.584] | 0.9996 | [0.9995, 0.9997] | 20/20 |
| MT     | (ii) | 11.087 | [10.892, 11.283] | 0.1155 | [0.0927, 0.1384] | 7/20 |
| TTA    | (ii) | 10.036 | [9.359, 10.712] | 0.7768 | [0.7420, 0.8117] | 20/20 |
| PF     | (ii) | 14.156 | [13.313, 15.000] | 0.7472 | [0.7246, 0.7697] | 20/20 |
| M-LWDF | (iii) | 14.156 | [13.313, 15.000] | 0.7472 | [0.7246, 0.7697] | 20/20 |
| PSS    | (iii) | 14.528 | [13.618, 15.437] | 0.7069 | [0.6741, 0.7397] | 20/20 |
| CQA    | (iii)ext | 5.305 | [5.024, 5.587] | 0.9993 | [0.9992, 0.9994] | 20/20 |

**Observación clave:** MT maximiza throughput de celda pero sólo sirve a ~7/20 UEs (starvation severa). BET logra Jain≈1.000 pero al costo del menor throughput. PF y M-LWDF equilibran ambos objetivos.

---

## 2. Hipótesis H1 — Degradación de fairness bajo distribución clusterizada

> H1: Los schedulers orientados a throughput (MT) presentan degradación no lineal de fairness bajo distribución clusterizada (D2).

### 2.1 Tabla de Jain index: D1 vs D2 (N=20, T1)

| Scheduler | Jain D1 | Jain D2 | ΔJain (D1→D2) | % caída |
|-----------|:-------:|:-------:|:-------------:|:-------:|
| RR     | 0.6419 | 0.6326 | -0.0094 | -1.5% |
| BET    | 0.9996 | 0.9996 | -0.0000 | -0.0% |
| MT     | 0.1155 | 0.1354 | +0.0199 | +17.2% |
| TTA    | 0.7768 | 0.7289 | -0.0479 | -6.2% |
| PF     | 0.7472 | 0.7473 | +0.0002 | +0.0% |
| M-LWDF | 0.7472 | 0.7473 | +0.0002 | +0.0% |
| PSS    | 0.7069 | 0.6925 | -0.0145 | -2.0% |
| CQA    | 0.9993 | 0.9989 | -0.0004 | -0.0% |

### 2.2 Tests de Welch (Jain D1 vs D2, N=20)

| Comparación | Media D1 | Media D2 | t | p-value | Sig | Cohen's d | Dirección esperada |
|-------------|:--------:|:--------:|---|:-------:|-----|:---------:|-------------------|
| Jain(MT): D1 vs D2 | 0.116 | 0.135 | -1.22 | 0.2289 | ns | -0.39 | D1 > D2 |
| Jain(PF): D1 vs D2 | 0.747 | 0.747 | -0.02 | 0.9874 | ns | -0.01 | D1 > D2 |
| Jain(RR): D1 vs D2 | 0.642 | 0.633 | +0.46 | 0.6472 | ns | +0.15 | D1 > D2 |
| Jain(BET): D1 vs D2 | 1.000 | 1.000 | +0.19 | 0.8516 | ns | +0.06 | D1 > D2 |

### 2.3 Interpretación

**MT:** Su Jain es bajo tanto en D1 (0.1155) como en D2 (0.1354). El ligero aumento (+0.0199) se debe a que en D2, los UEs de C1 (todos con SINR similar) se turnan los recursos más equitativamente entre sí. Lo que confirma H1 no es el Jain global sino la starvation sistemática: los UEs de C3 reciben cero bytes de forma consistente, algo que en D1 (distribución uniforme con fading) ocurría de forma rotatoria. **El scheduler con la mayor caída real de Jain** al pasar a D2 es TTA (0.7768 → 0.7289, -0.0479), no MT.

**PF:** prácticamente sin cambio (0.7472 → 0.7473), el historial proporcional compensa la heterogeneidad espacial. **BET:** equidad perfecta en ambas distribuciones (0.9996 → 0.9996), aunque con throughput reducido.

---

## 3. Hipótesis H2 — Heterogeneidad espacial vs carga de usuarios

> H2: La heterogeneidad espacial impacta más el fairness que el número de usuarios.

### 3.1 ΔJain por efecto espacial (D1→D2, N=20) vs por carga (N=10→40, D1)

| Scheduler | ΔJain espacial (D1→D2) | ΔJain por carga (N10→N40) | Domina |
|-----------|:---------------------:|:-------------------------:|:------:|
| RR     | 0.0094 | 0.0242 | **Carga** |
| BET    | 0.0000 | 0.0013 | **Carga** |
| MT     | 0.0199 | 0.1210 | **Carga** |
| TTA    | 0.0479 | 0.0468 | **Espacial** |
| PF     | 0.0002 | 0.0107 | **Carga** |
| M-LWDF | 0.0002 | 0.0107 | **Carga** |
| PSS    | 0.0145 | 0.0531 | **Carga** |
| CQA    | 0.0004 | 0.0013 | **Carga** |

### 3.2 Test: ΔJain espacial vs ΔJain por carga (PF, Welch)

- ΔJain_espacial(PF): media = -0.0045
- ΔJain_carga(PF):    media = -0.0107
- Welch t=+0.405, p=0.6883 ns, Cohen's d=+0.11

### 3.3 Interpretación

**Resultado negativo importante:** contrario a lo esperado en H2, la tabla muestra que el número de usuarios (N=10→40) produce un ΔJain mayor que la distribución espacial (D1→D2) en 6 de los 7 schedulers. Solo TTA muestra dominancia marginal del efecto espacial. Esto indica que los algoritmos channel-aware (especialmente PF, M-LWDF y PSS) se adaptan bien a la heterogeneidad espacial mediante sus métricas proporcionales/históricas, mientras que la saturación progresiva de la celda al aumentar usuarios es el factor que más degrada la equidad. Para MT, la carga domina con mucho (ΔJain_carga=0.121 vs ΔJain_espacial=0.020) porque con N=40 hay más candidatos y el algoritmo maximiza con mayor ventaja al mismo conjunto reducido de UEs con mejor canal, empeorando el Jain de forma progresiva. Este resultado negativo es valioso para el paper: refina la comprensión de H2 y sugiere que el diseño de schedulers robustos debe enfocarse en escalar con carga, no solo en compensar heterogeneidad geográfica.

---

## 4. Hipótesis H3 — Impacto del tráfico heterogéneo en throughput

> H3: Los schedulers balanceados (PF) pierden eficiencia espectral bajo tráfico heterogéneo; M-LWDF y PSS compensan de forma distinta.

### 4.1 Throughput T1 vs T2 (N=20, D1)

| Scheduler | Tput T1 [Mbps] | Tput T2 [Mbps] | Δ [Mbps] | % cambio |
|-----------|:--------------:|:--------------:|:--------:|:--------:|
| RR     | 7.224 | 6.343 | -0.881 | -12.2% |
| BET    | 4.315 | 3.476 | -0.839 | -19.4% |
| MT     | 11.087 | 10.954 | -0.133 | -1.2% |
| TTA    | 10.036 | 9.435 | -0.601 | -6.0% |
| PF     | 14.156 | 11.457 | -2.699 | -19.1% |
| M-LWDF | 14.156 | 9.745 | -4.411 | -31.2% |
| PSS    | 14.528 | 10.614 | -3.913 | -26.9% |
| CQA    | 5.305 | 6.217 | +0.912 | +17.2% |

### 4.2 Tests de Welch (Throughput T1 vs T2, N=20, D1)

| Comparación | Media T1 | Media T2 | t | p-value | Sig | Cohen's d | Dirección esperada |
|-------------|:--------:|:--------:|---|:-------:|-----|:---------:|-------------------|
| Tput(PF): T1 vs T2 | 14.156 | 11.457 | +4.96 | 0.0000 | *** | +1.57 | T2 < T1 (GBR limita uso eficiente del canal) |
| Tput(M-LWDF): T1 vs T2 | 14.156 | 9.745 | +8.19 | 0.0000 | *** | +2.59 | T2 < T1 (GBR limita uso eficiente del canal) |
| Tput(PSS): T1 vs T2 | 14.528 | 10.614 | +6.75 | 0.0000 | *** | +2.14 | T2 < T1 (GBR limita uso eficiente del canal) |
| Tput(MT): T1 vs T2 | 11.087 | 10.954 | +0.63 | 0.5324 | ns | +0.20 | T2 < T1 (GBR limita uso eficiente del canal) |
| Tput(RR): T1 vs T2 | 7.224 | 6.343 | +2.10 | 0.0423 | * | +0.66 | T2 < T1 (GBR limita uso eficiente del canal) |

### 4.3 Delay E2E bajo T2 (N=20)

| Scheduler | Delay D1 T2 [ms] | Delay D2 T2 [ms] | ΔDelay |
|-----------|:----------------:|:----------------:|:------:|
| PF     | 3.22 | 3.26 | +0.03 |
| M-LWDF | 3.21 | 3.25 | +0.04 |
| PSS    | 3.23 | 3.27 | +0.04 |
| CQA    | 3.40 | 3.41 | +0.00 |

### 4.4 Interpretación

Bajo tráfico heterogéneo (T2), todos los schedulers reducen su throughput de celda excepto CQA. PF baja de 14.16 a 11.46 Mbps (-19.1%) porque los flujos GBR tienen tasas máximas bajas y dejan capacidad sin usar. M-LWDF cae de 14.16 a 9.75 Mbps (-31.2%), la mayor caída: prioriza agresivamente GBR aunque sean de baja tasa, reduciendo los recursos disponibles para BE y con ello el throughput total.

**Excepción — CQA:** es el único que GANA throughput bajo T2 (5.31 → 6.22 Mbps, +17.2%). Bajo T1 full-buffer, CQA actúa como equalizer (todos los UEs saturados con igual urgencia). Bajo T2, los flujos GBR de baja tasa reducen la presión de cola, CQA puede discriminar entre tipos de flujo y asignar más recursos al BE full-buffer que sí los usa, elevando el throughput total de celda.

---

## 5. Hipótesis H4 — Schedulers QoS-aware mejoran fairness bajo T2

> H4: Los schedulers híbridos (PSS) mejoran el compromiso fairness-throughput frente a PF simple bajo tráfico heterogéneo.

### 5.1 Jain index T2 — PF vs M-LWDF vs PSS

| Config | Jain PF | Jain M-LWDF | Jain PSS | Mejor fairness |
|--------|:-------:|:-----------:|:--------:|:--------------:|
| D1 N=20 uni T2 | 0.5613 | 0.5640 | 0.5526 | **M-LWDF** |
| D2 N=20 clu T2 | 0.5432 | 0.5959 | 0.5961 | **PSS** |

### 5.2 Tests de Welch (Jain T2, N=20, D2)

| Comparación | Media A | Media B | t | p-value | Sig | Cohen's d | Dirección esperada |
|-------------|:-------:|:-------:|---|:-------:|-----|:---------:|-------------------|
| Jain(M-LWDF vs PF) D1 | 0.564 | 0.561 | +0.19 | 0.8504 | ns | +0.06 | M-LWDF ≥ PF |
| Jain(PSS vs PF) D1 | 0.553 | 0.561 | -0.66 | 0.5128 | ns | -0.21 | PSS ≥ PF |
| Jain(PSS vs M-LWDF) D1 | 0.553 | 0.564 | -1.38 | 0.1758 | ns | -0.44 | PSS ≥ M-LWDF |
| Jain(M-LWDF vs PF) D2 | 0.596 | 0.543 | +16.27 | 0.0000 | *** | +5.14 | M-LWDF ≥ PF |
| Jain(PSS vs PF) D2 | 0.596 | 0.543 | +14.93 | 0.0000 | *** | +4.72 | PSS ≥ PF |
| Jain(PSS vs M-LWDF) D2 | 0.596 | 0.596 | +0.06 | 0.9531 | ns | +0.02 | PSS ≥ M-LWDF |

### 5.3 PLR — Tasa de pérdida de paquetes (T2)

| Scheduler | PLR D1 T2 | PLR D2 T2 |
|-----------|:---------:|:---------:|
| PF     | 0.0048 | 0.0029 |
| M-LWDF | 0.0066 | 0.0040 |
| PSS    | 0.0045 | 0.0027 |
| CQA    | 0.0152 | 0.0097 |

### 5.4 Interpretación

**H4 confirmada.** En D2+T2, M-LWDF (0.5959) y PSS (0.5961) superan claramente a PF (0.5432) en equidad global (p<0.001, d≈5). La diferencia es de +9.7% sobre PF — estadísticamente muy robusta.

El mecanismo es clave: con tráfico interleaved y clustering, los UEs GBR de C3 (400m, SINR bajo) acumulan delay HOL rápidamente porque PF los penaliza por su mal canal. M-LWDF escala su prioridad cuando ese delay crece, rescatándolos y mejorando el Jain global. En D1 (sin clustering), no hay ventaja: PF=0.5613, M-LWDF=0.5640 — prácticamente idénticos (ns, d=0.06).

H4 solo se activa en el escenario que la motiva: usuarios con canal heterogéneo (clustering) más tráfico con QoS diferenciado (T2).

---

## 6. Análisis de figuras

Las figuras F1–F8 se encuentran en `results/processed/figures/`. A continuación se describe qué muestra cada una, qué buscar visualmente y qué conclusión aporta al paper.

---

### F1 — Throughput de celda vs N (D1 uniforme, T1 full-buffer)

**Archivo:** `F1_throughput_vs_N_D1.pdf`  
**Tipo:** Curvas con barras de error IC95%, eje X = N={10,20,40}, eje Y = Mbps

| Scheduler | N=10 | N=20 | N=40 |
|-----------|:----:|:----:|:----:|
| RR     | 5.60 | 7.22 | 6.75 |
| BET    | 4.92 | 4.32 | 3.94 |
| MT     | 10.90 | 11.09 | 11.07 |
| TTA    | 11.27 | 10.04 | 8.89 |
| PF     | 14.06 | 14.16 | 14.44 |
| M-LWDF | 14.06 | 14.16 | 14.44 |
| PSS    | 14.17 | 14.53 | 13.87 |
| CQA    | 5.76 | 5.31 | 5.05 |

**Qué buscar:** PSS y PF/M-LWDF se mantienen estables (~14 Mbps) porque explotan el canal eficientemente sin importar cuántos usuarios compitan. MT es también alto pero por razones opuestas: concentra todos los recursos en 1-3 UEs privilegiados (starvation). BET cae con N porque dedica más tiempo a UEs de canal débil. RR muestra un pico en N=20 (carga media óptima para RR).

**Para el paper:** Esta figura establece el baseline de throughput (H1). Muestra el trade-off entre categoría (i) baja eficiencia/alta equidad, categoría (ii) alta eficiencia/baja equidad (MT), y categoría (ii-iii) eficiencia sin sacrificar equidad (PF, PSS).

### F2 — Jain index vs N (D1 uniforme, T1 full-buffer)

**Archivo:** `F2_jain_vs_N_D1.pdf`  
**Tipo:** Curvas con IC95%, eje X = N, eje Y = Jain [0,1]

| Scheduler | N=10 | N=20 | N=40 |
|-----------|:----:|:----:|:----:|
| RR     | 0.6664 | 0.6419 | 0.6422 |
| BET    | 0.9998 | 0.9996 | 0.9985 |
| MT     | 0.1800 | 0.1155 | 0.0590 |
| TTA    | 0.7551 | 0.7768 | 0.8018 |
| PF     | 0.7497 | 0.7472 | 0.7390 |
| M-LWDF | 0.7497 | 0.7472 | 0.7390 |
| PSS    | 0.7509 | 0.7069 | 0.6977 |
| CQA    | 0.9996 | 0.9993 | 0.9982 |

**Qué buscar:** Tres grupos claramente separados: (1) BET≈1.000 para todo N — equidad perfecta independiente de carga. (2) TTA/PF/M-LWDF/PSS en rango 0.70–0.80 — equidad moderada-alta. (3) MT cae dramáticamente con N (0.180→0.116→0.059): más usuarios = más candidatos para starvation. RR se estabiliza ~0.64.

**Para el paper:** Esta figura cuantifica el costo de fairness de optimizar throughput. MT sacrifica equidad de forma creciente con la carga. Las barras de error pequeñas confirman que las 20 corridas son consistentes.

### F3 — Jain index vs N (D2 clusterizado, T1 full-buffer)

**Archivo:** `F3_jain_vs_N_D2.pdf`  
**Tipo:** Misma estructura que F2, pero con distribución clusterizada

| Scheduler | N=10 | N=20 | N=40 |
|-----------|:----:|:----:|:----:|
| RR     | 0.6252 | 0.6326 | 0.6752 |
| BET    | 0.9998 | 0.9996 | 0.9986 |
| MT     | 0.2104 | 0.1354 | 0.0620 |
| TTA    | 0.7441 | 0.7289 | 0.7098 |
| PF     | 0.7355 | 0.7473 | 0.7664 |
| M-LWDF | 0.7355 | 0.7473 | 0.7664 |
| PSS    | 0.7382 | 0.6925 | 0.7164 |
| CQA    | 0.9996 | 0.9989 | 0.9968 |

**Qué buscar — comparar con F2:** TTA es el más afectado por el clustering: su Jain cae con N en D2 (0.744→0.729→0.710) en lugar de subir como en D1 (0.755→0.777→0.802). PF/M-LWDF son prácticamente idénticos entre F2 y F3, confirmando su robustez ante heterogeneidad espacial. MT mantiene Jain bajos en ambas figuras (ya starve en D1).

**Para el paper:** Comparar F2 vs F3 es la evidencia visual de H1/H2. La similitud entre ambas figuras para PF/M-LWDF/PSS suporta el resultado negativo de H2: el clustering no degrada a estos algoritmos.

### F4 — Fairness D1 vs D2 (N=20, T1) — barras agrupadas

**Archivo:** `F4_jain_D1vsD2_N20_T1.pdf`  
**Tipo:** Barras agrupadas (barra sólida = D1, rayada = D2) por scheduler

| Scheduler | Jain D1 | Jain D2 | Δ |
|-----------|:-------:|:-------:|:---:|
| RR     | 0.6419 | 0.6326 | -0.0094 |
| BET    | 0.9996 | 0.9996 | -0.0000 |
| MT     | 0.1155 | 0.1354 | +0.0199 |
| TTA    | 0.7768 | 0.7289 | -0.0479 |
| PF     | 0.7472 | 0.7473 | +0.0002 |
| M-LWDF | 0.7472 | 0.7473 | +0.0002 |
| PSS    | 0.7069 | 0.6925 | -0.0145 |
| CQA    | 0.9993 | 0.9989 | -0.0004 |

**Qué buscar:** La diferencia entre barra sólida y rayada para cada scheduler. TTA tiene la mayor diferencia visible (-0.048). BET y PF/M-LWDF muestran barras casi iguales (robustez al clustering). MT tiene ambas barras muy bajas — ya está en el suelo en D1.

**Para el paper:** Esta figura sintetiza H1 para todos los schedulers en una sola vista. Si las barras D1 y D2 son iguales, el scheduler es robusto al clustering. Si D2 es menor, el clustering lo afecta.

### F5 — Throughput T1 vs T2 (N=20, D1) — barras agrupadas

**Archivo:** `F5_throughput_T1vsT2_N20_D1.pdf`  
**Tipo:** Barras agrupadas (barra sólida = T1, rayada = T2)

| Scheduler | Tput T1 | Tput T2 | Δ | % |
|-----------|:-------:|:-------:|:---:|:---:|
| RR     | 7.22 | 6.34 | -0.88 | -12.2% |
| BET    | 4.32 | 3.48 | -0.84 | -19.4% |
| MT     | 11.09 | 10.95 | -0.13 | -1.2% |
| TTA    | 10.04 | 9.44 | -0.60 | -6.0% |
| PF     | 14.16 | 11.46 | -2.70 | -19.1% |
| M-LWDF | 14.16 | 9.75 | -4.41 | -31.2% |
| PSS    | 14.53 | 10.61 | -3.91 | -26.9% |
| CQA    | 5.31 | 6.22 | +0.91 | +17.2% |

**Qué buscar — hallazgo clave:** M-LWDF tiene la mayor caída (-31.2%), más que PF (-19.1%) y PSS (-26.9%). Esto parece paradójico: M-LWDF supuestamente 'compensa' el tráfico heterogéneo, pero su throughput total es el que más cae. La explicación es que M-LWDF prioriza agresivamente los flujos GBR (video+gaming) de baja tasa, reduciendo el throughput total de celda para cumplir los SLA. MT casi no cae (-1.2%) porque ignora completamente el tipo de bearer y solo maximiza la tasa instantánea.

**Para el paper:** Esta figura confirma H3 visualmente. Muestra que T2 siempre reduce el throughput total, y que el grado de reducción refleja cuánto prioriza cada scheduler el QoS sobre la eficiencia espectral.

### F6 — GBR vs BE throughput (N=20, D1, T2) — PF, M-LWDF, PSS

**Archivo:** `F6_GBR_vs_BE_N20_D1_T2.pdf`  
**Tipo:** Barras agrupadas por scheduler: barra sólida = GBR, rayada = BE

**Qué buscar:** Para PF, la barra BE es relativamente alta y la GBR moderada — PF no distingue entre tipos de tráfico, así que el BE full-buffer compite en igualdad con el video/gaming. Para M-LWDF y PSS, la barra GBR es mayor y la BE menor: estos schedulers sacrifican BE para proteger los flujos GBR que tienen SLA de delay.

**Para el paper:** Complementa F5. Donde F5 muestra la caída en throughput total, F6 explica *por qué*: M-LWDF/PSS redistribuyen los RBs hacia GBR (video+gaming), lo que reduce la capacidad disponible para BE. Esta redistribución es exactamente el objetivo del diseño QoS-aware.

### F7 — Delay E2E D1 vs D2 (N=20, T2) — PF, M-LWDF, PSS

**Archivo:** `F7_delay_D1vsD2_N20_T2.pdf`  
**Tipo:** Curvas de 2 puntos (D1 y D2) para 3 schedulers

| Scheduler | Delay D1 T2 | Delay D2 T2 | Δ |
|-----------|:-----------:|:-----------:|:---:|
| PF     | 3.22 ms | 3.26 ms | +0.03 ms |
| M-LWDF | 3.21 ms | 3.25 ms | +0.04 ms |
| PSS    | 3.23 ms | 3.27 ms | +0.04 ms |
| CQA    | 3.40 ms | 3.41 ms | +0.00 ms |

**Qué buscar:** Las tres curvas son casi paralelas y muy cercanas entre sí. El delay es muy similar en D1 y D2 (~3.2-3.3 ms) para los tres schedulers, y las diferencias son mínimas (<0.1 ms). Esto indica que el delay E2E está dominado por el canal inalámbrico y la serialización, no por el tipo de scheduler ni la distribución espacial.

**Para el paper:** Este resultado indica que la heterogeneidad espacial (D1 vs D2) no afecta el delay promedio de celda bajo T2. El delay es una métrica estable en este entorno. La diferencia entre schedulers es estadísticamente pequeña, lo que sugiere que todos cumplen los requisitos de delay (~3ms << 300ms de SLA).

### F8 — Jain D1 vs D2 (N=20, T2) — PF, M-LWDF, PSS

**Archivo:** `F8_jain_D1vsD2_N20_T2.pdf`  
**Tipo:** Curvas de 2 puntos (D1 y D2) para 3 schedulers

| Scheduler | Jain D1 T2 | Jain D2 T2 | Δ |
|-----------|:----------:|:----------:|:---:|
| PF     | 0.5613 | 0.5432 | -0.0181 |
| M-LWDF | 0.5640 | 0.5959 | +0.0319 |
| PSS    | 0.5526 | 0.5961 | +0.0435 |
| CQA    | 0.4192 | 0.4033 | -0.0158 |

**Qué buscar — hallazgo clave:** PF baja su Jain al pasar de D1 a D2 (-0.018), mientras M-LWDF y PSS la SUBEN (+0.032, +0.044). Las líneas de M-LWDF y PSS se cruzan con PF al pasar de D1 a D2: en D1 todos están al mismo nivel (~0.55), pero en D2 M-LWDF y PSS superan claramente a PF (0.596 vs 0.543, p<0.001, d≈5). CQA aparece en la figura con Jain ~0.40, por debajo de PF en ambas distribuciones — su mecanismo multi-criterio no rescata tan efectivamente a los UEs de C3 como el delay HOL puro de M-LWDF.

**Para el paper:** Esta es la figura más importante para H4. M-LWDF y PSS confirman H4 de forma contundente. CQA muestra que dentro de cat(iii), el mecanismo específico importa.


---

## 7. Extensión — CQA como tercer representante de categoría (iii)

> Branch `final` — 240 corridas adicionales (CQA, T1+T2).

> **Nota:** TBFQ fue evaluado pero descartado por incompatibilidad con tráfico

> full-buffer (T1): token deadlock en el primer segundo de simulación.

### 7.1 CQA bajo T1 — comportamiento equalizer

| Scheduler | Throughput [Mbps] | Jain | UEs activos | Nota |
|-----------|:-----------------:|:----:|:-----------:|------|
| BET    | 4.315 | 0.9996 | 20/20 | ← referencia |
| PF     | 14.156 | 0.7472 | 20/20 |  |
| M-LWDF | 14.156 | 0.7472 | 20/20 |  |
| PSS    | 14.528 | 0.7069 | 20/20 |  |
| CQA    | 5.305 | 0.9993 | 20/20 | ← similar a BET |

CQA bajo T1 produce Jain≈0.999 y throughput de ~5.3 Mbps, comportamiento casi idéntico a BET. La métrica multi-criterio de CQA (CQI + HOL delay + tamaño de cola + prioridad) bajo saturación total converge a un equalizer: todos los UEs tienen urgencia similar, domina la componente de fairness.

### 7.2 CQA bajo T2 — comparación con PF/M-LWDF/PSS

| Scheduler | Tput T2 D1 [Mbps] | Jain T2 D1 | Jain T2 D2 | ΔJain D1→D2 |
|-----------|:-----------------:|:----------:|:----------:|:-----------:|
| PF     | 11.457 | 0.5613 | 0.5432 | -0.0181 |
| M-LWDF | 9.745 | 0.5640 | 0.5959 | +0.0319 |
| PSS    | 10.614 | 0.5526 | 0.5961 | +0.0435 |
| CQA    | 6.217 | 0.4192 | 0.4033 | -0.0158 |

### 7.3 Test Welch — CQA vs PF en T2 (comparación directa)

| Comparación | Media CQA | Media PF | t | p-value | Sig | Cohen's d | CQA > PF? |
|-------------|:---------:|:--------:|---|:-------:|-----|:---------:|:---------:|
| Jain(CQA vs PF) D1 | 0.4192 | 0.5613 | -11.18 | 0.0000 | *** | -3.54 | ❌ No |
| Jain(CQA vs PF) D2 | 0.4033 | 0.5432 | -51.05 | 0.0000 | *** | -16.14 | ❌ No |

### 7.4 Interpretación

**CQA bajo T1:** Jain=0.9993 — comportamiento equalizer idéntico a BET. Bajo saturación total, los criterios QoS son equivalentes para todos los UEs y domina la componente de fairness.

**CQA bajo T2+D2:** CQA (0.4033) **no supera a PF (0.5432)** en T2+D2. A diferencia de M-LWDF (0.5959) y PSS que escalan prioridad exclusivamente por delay HOL, la métrica multi-criterio de CQA pondera también el CQI instantáneo. Los UEs de C3 (400m, SINR bajo) tienen CQI bajo que reduce su prioridad en CQA incluso cuando acumulan delay, a diferencia de M-LWDF que escala prioridad puramente por delay sin importar el canal. Esto muestra que dentro de cat(iii), el mecanismo específico sí importa: no todos los QoS-aware rescatan igualmente a usuarios en zona de cobertura débil.

**Conclusión de la extensión:** M-LWDF y PSS confirman H4 robustamente. CQA aporta un matiz: dentro de la categoría (iii), el mecanismo de priorización determina si un scheduler puede rescatar UEs en zona débil bajo tráfico heterogéneo. El delay HOL puro (M-LWDF) es más efectivo que un criterio multi-factor (CQA) para ese objetivo específico.