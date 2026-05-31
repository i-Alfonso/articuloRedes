# Decisiones y definiciones del proyecto

> Registro acumulativo de todo lo que se va definiendo, descartando y justificando.

---

## 1. Título y pregunta central

**Título:** Evaluación de paradigmas de scheduling en sistemas OFDMA bajo tráfico y condiciones espaciales heterogéneas

**Pregunta central:**
> ¿Cómo afectan la heterogeneidad espacial y la heterogeneidad del tráfico al desempeño estructural de distintos paradigmas de scheduling en sistemas OFDMA?

---

## 2. Hipótesis

| ID | Hipótesis |
|----|-----------|
| H1 | Los schedulers orientados a throughput presentan degradación no lineal de fairness bajo distribuciones espaciales heterogéneas. |
| H2 | La heterogeneidad espacial tiene mayor impacto sobre fairness que el número total de usuarios. |
| H3 | Los schedulers balanceados pierden eficiencia espectral cuando coexisten múltiples tipos de tráfico con distintos requisitos de QoS. |
| H4 | Los schedulers híbridos mejoran el compromiso fairness-throughput, aunque introducen mayor complejidad de coordinación. |

---

## 3. Taxonomía de scheduling adoptada

**Fuente:** Capozzi et al., *"Downlink Packet Scheduling in LTE Cellular Networks: Key Design Issues and a Survey"*, IEEE Communications Surveys & Tutorials, 2012.

Archivo local: `Basearticle/DL_PacketSchedLTE_andASurveyCOMST-00100-2011.R2.pdf`

El artículo propone una clasificación en 5 categorías. Se adopta como base de taxonomía porque:
- Propone la clasificación explícita que el proyecto necesita.
- Cubre los 3 algoritmos clásicos de referencia del proyecto (RR, PF, MT).
- Usa exactamente las mismas métricas: Jain fairness index, throughput agregado, PLR y delay.
- El vacío que deja (heterogeneidad espacial no evaluada) es el espacio de contribución del proyecto.

### Categorías de la taxonomía

| # | Categoría | Descripción | Estado |
|---|-----------|-------------|--------|
| 1 | Channel-unaware | No considera calidad de canal. Diseñados para redes cableadas. | ✅ Activa |
| 2 | Channel-aware / QoS-unaware | Explota CQI pero sin garantías de QoS por flujo. | ✅ Activa |
| 3 | Channel-aware / QoS-aware | Combina CQI con restricciones de delay/bitrate por flujo. | ✅ Activa |
| 4 | Semi-persistent scheduling | Preasigna recursos periódicos para reducir overhead de señalización (diseñado para VoIP). | ❌ Descartada — ver nota |
| 5 | Energy-aware | Optimiza consumo energético del eNB, no throughput/fairness/QoS. | ❌ Descartada — ver nota |

**Justificación de descarte de categorías 4 y 5:**

Las instrucciones del proyecto piden *"seleccionar algoritmos representativos de distintas categorías"* — no requieren cubrir las 5 categorías. Las categorías 4 y 5 se descartan porque:

- **Categoría 4 (Semi-persistent):** Diseñada para tráfico VoIP periódico de baja tasa. Su objetivo es reducir la señalización de control (grants por TTI), no optimizar throughput, fairness ni QoS bajo heterogeneidad. No es comparable con los schedulers de las categorías activas bajo las hipótesis H1–H4.

- **Categoría 5 (Energy-aware):** Optimiza el consumo energético del eNB reduciendo el número de antenas activas o la potencia de transmisión. Su métrica de éxito es Julios/bit, no Jain ni throughput. Incluirlo mezclaría objetivos de optimización incomparables con el resto.

**Algoritmos nativos de ns-3.47 disponibles pero descartados dentro de cat (iii):**

| Algoritmo | Clase ns-3.47 | Razón de descarte |
|-----------|--------------|-------------------|
| CQA (Channel and QoS Aware) | `CqaFfMacScheduler` | Añadiría 3er algoritmo a cat (iii) sin hipótesis adicional; redundante con M-LWDF+PSS |
| FD-TBFQ (Token Bank Fair Queuing) | `FdTbfqFfMacScheduler` | Mecanismo de token bucket: fairness garantizada, pero objetivo diferente al proyecto; sin alineación con H3/H4 |
| TD-TBFQ | `TdTbfqFfMacScheduler` | Variante TD del anterior; mismo argumento |

**Conclusión:** El esquema 2-3-2 (7 algoritmos en 3 categorías activas) satisface completamente los requisitos del proyecto y cubre todos los trade-offs relevantes para H1–H4.

---

## 4. Algoritmos seleccionados — decisión final: 7 algoritmos (2-3-2)

### Justificación del número y distribución

El documento de proyecto pide *"comparar los tres clásicos (RR, PF y MT) contra representativos de distintas categorías"*. El esquema 2-3-2 satisface esto exactamente:

- **2 algoritmos en categoría (i):** RR (clásico base) + BET (contraste dentro de la misma categoría: tiempo igual vs throughput igual)
- **3 algoritmos en categoría (ii):** MT + TTA + PF (los tres que Capozzi evalúa explícitamente en sus Figs. 8-10)
- **2 algoritmos en categoría (iii):** M-LWDF + PSS (uno delay-aware, uno híbrido GBR/non-GBR)

Los 3 algoritmos base requeridos están presentes: **RR ✅, MT ✅, PF ✅**.

Escala de simulaciones: `7 algoritmos × 8 grupos × 20 corridas = 1,120 simulaciones`.

### Tabla de algoritmos

| # | Algoritmo | Categoría | Base | Hipótesis | Clase ns-3.47 | Estado |
|---|-----------|-----------|------|-----------|--------------|--------|
| 1 | **RR** — Round Robin | (i) Channel-unaware | ✅ | H1, H2 | `RrFfMacScheduler` | Nativo |
| 2 | **BET** — Blind Equal Throughput | (i) Channel-unaware | — | H1, H2 | `FdBetFfMacScheduler` | Nativo |
| 3 | **MT** — Maximum Throughput | (ii) Ch-aware/QoS-unaware | ✅ | H1, H2 | `TdMtFfMacScheduler` | Nativo |
| 4 | **TTA** — Throughput To Average | (ii) Ch-aware/QoS-unaware | — | H1, H2 | `TtaFfMacScheduler` | Nativo |
| 5 | **PF** — Proportional Fair | (ii) Ch-aware/QoS-unaware | ✅ | H1, H2, H3, H4 | `PfFfMacScheduler` | Nativo |
| 6 | **M-LWDF** — Modified LWDF | (iii) Ch-aware/QoS-aware | — | H3 | `MlwdfFfMacScheduler` | Implementado |
| 7 | **PSS** — Priority Set Scheduler | (iii) Ch-aware/QoS-aware | — | H3, H4 | `PssFfMacScheduler` | Nativo |

### Rol de cada algoritmo por hipótesis

- **H1:** MT vs TTA vs PF vs BET vs RR bajo D1 (uniforme) → D2 (clusterizada) con N fijo. ¿Cuál degrada más fairness y de qué forma?
- **H2:** Todos los de cat (i) y (ii) variando N con distribución fija vs variando distribución con N fijo. ¿Qué impacta más el Jain?
- **H3:** PF vs M-LWDF vs PSS bajo T2 (video + gaming + BE). ¿Pierde PF eficiencia espectral en flujos GBR? ¿M-LWDF y PSS compensan de forma distinta?
- **H4:** PF (un nivel, QoS-unaware) vs PSS (dos niveles TDPS+FDPS, QoS-aware). ¿Mejora el híbrido el trade-off fairness-throughput-QoS?

### Decisiones clave de implementación

**MT usa TdMtFfMacScheduler (no FdMt):**
La variante TD (Time Domain) asigna todos los RBs del TTI al UE con mejor CQI global — comportamiento canónico de MT en Capozzi. La variante FD (Frequency Domain) asigna por RB individual y con tráfico CBR puede producir throughput total menor que PF porque los UEs de buen canal saturan su CBR y el exceso de capacidad se pierde.

**BET incluido (antes descartado):**
Inicialmente se descartó por considerarlo funcionalmente reemplazado por RR. Se revirtió la decisión porque: (1) BET e RR representan paradigmas distintos dentro de cat (i) (tiempo igual vs throughput igual), (2) BET es el componente de fairness de PF, su inclusión enriquece la narrativa taxonómica, (3) es nativo en ns-3.47.

**TTA incluido (antes descartado):**
Inicialmente se descartó por similitud con PF. Se revirtió porque: (1) Capozzi lo evalúa explícitamente como punto intermedio entre MT y PF, (2) su métrica es conceptualmente distinta (normaliza por throughput wideband del mismo TTI, no por historial), (3) es nativo en ns-3.47 y completa la cat (ii) como lo hace Capozzi.

**PSS mantiene su lugar (reemplazó a PF-PF desde el inicio):**
PF-PF solo reduce complejidad computacional de PF sin cambiar el paradigma. PSS introduce distinción GBR/non-GBR en el TDPS, lo que lo convierte en un paradigma genuinamente diferente. Es puente entre H3 y H4.

### Algoritmos considerados y descartados definitivamente

| Algoritmo | Categoría | Razón de descarte |
|-----------|-----------|-------------------|
| PF-PF | (ii) | Reemplazado por PSS desde el inicio: misma complejidad de implementación, menor valor científico |
| FD-MT | (ii) | Variante FD del MT; comportamiento contraintuitivo con full-buffer (throughput < PF). TD-MT es el MT canónico de Capozzi |
| EXP rule | (iii) | Fortalecería H3 pero convierte el análisis en comparación intra-categoría (iii) sin nueva hipótesis; requiere implementación en C++ |
| FLS (Frame Level Scheduler) | (iii) | Más complejo que PSS con el mismo rol teórico; mayor costo de implementación sin aporte diferencial |
| CQA (Channel and QoS Aware) | (iii) | Nativo en ns-3.47 (`CqaFfMacScheduler`); añadiría 3er algoritmo a cat (iii) sin hipótesis adicional |
| FD-TBFQ / TD-TBFQ | (iii) | Nativos en ns-3.47; mecanismo token bank distinto pero objetivo (fairness con buffer) ya cubierto por M-LWDF y PSS |
| Semi-persistent (cat iv) | (iv) | Diseñado para VoIP periódico; objetivo de reducir señalización, incompatible con H1–H4 |
| Energy-aware (cat v) | (v) | Optimiza Julios/bit, no throughput/fairness/QoS; métricas incomparables con el resto del experimento |

---

## 5. Métricas de evaluación

| Métrica | Descripción | Fuente ns-3 |
|---------|-------------|-------------|
| Throughput total (celda) | Eficiencia espectral del sistema | `DlRlcStats.txt` — RxBytes |
| Throughput por usuario | Distribución de recursos, detecta starvation | `DlRlcStats.txt` — por RNTI |
| Retardo (delay) | Head-of-line packet delay | `DlRlcStats.txt` — columna delay |
| Fairness — Índice de Jain | `J = (Σxᵢ)² / (n · Σxᵢ²)`, rango [0,1] | Calculado en `analyze_run.py` |
| PLR — Packet Loss Ratio | `(nTxPDUs - nRxPDUs) / nTxPDUs` | `DlRlcStats.txt` |

**Nota sobre Jain y starvation:** `analyze_run.py` acepta `--nues N` para incluir UEs con throughput=0 en el cálculo del Jain. Sin este parámetro, MT infla artificialmente su Jain al excluir UEs starved.

---

## 6. Entorno de simulación

- **Simulador:** ns-3.47, módulo LTE (en `sim/ns-3.47/`)
- **Nota:** LTE es el entorno controlado, no el objeto de estudio. El objetivo es analizar problemas fundamentales de asignación de recursos OFDMA.

| Parámetro | Valor | Notas |
|-----------|-------|-------|
| eNodeBs | 1 | Celda única |
| UEs | 10 / 20 / 40 | Variable independiente |
| Potencia TX eNB | 46 dBm | Macro eNB estándar; cubre 490 m con SINR > 0 dB |
| Frecuencia DL | 2.1 GHz (EARFCN 100) | |
| BW DL | 10 MHz → 50 RBs | |
| Path loss | LogDistancePropagationLossModel, exp=3.5, refLoss=46.7 dB | |
| Fading | No modelado explícitamente (canal quasi-estático) | Variación por posición |
| CQI | Dinámico, full-bandwidth, 1 ms | |
| EPC | Habilitado (PointToPointEpcHelper) | |
| Tráfico | UDP IP extremo a extremo | |
| Movilidad | Estática (ConstantPositionMobilityModel) | |
| Semilla | `RngSeedManager::SetSeed(12345); SetRun(runId)` | |
| Warm-up descartado | 5 s | |
| Duración total | 30 s | |

---

## 7. Variables independientes

| Variable | Valores |
|----------|---------|
| Número de usuarios | 10, 20, 40 |
| Distribución espacial | Uniforme D1 (disco 490 m) / Clusterizada D2 (3 clusters a distancias distintas: cerca 100 m · medio 250 m · lejos 400 m, radio 70 m c/u) |
| Tipo de tráfico | T1 homogéneo (UDP CBR 1 Mbps/UE) / T2 heterogéneo (video GBR + gaming GBR + BE) |

---

## 8. Diseño experimental

- **Corridas por configuración:** 20 (para IC al 95% con t-Student, df=19)
- **Total simulaciones:** 7 alg × 8 grupos × 20 corridas = **1,120**
- Mantener constante entre corridas: topología, parámetros, modelo de canal
- PF actúa como **referencia central** en todas las comparaciones

| Grupo | Escenarios | Runs | Hipótesis |
|-------|-----------|------|-----------|
| A — Heterogeneidad espacial | D1+D2 × N={10,20,40}, T1 | 840 | H1, H2 |
| B — Heterogeneidad de tráfico | D1+D2, N=20, T2 | 280 | H3, H4 |
| **Total** | | **1,120** | |

---

## 9. Diseño de experimentos — qué medimos, por qué y para qué

> Esta sección describe cada experimento concreto: su configuración, las métricas que captura,
> la hipótesis que pretende responder y lo que se espera observar.

---

### Experimento E1 — Efecto del scheduler bajo distribución espacial uniforme

**Configuración:** D1 uniforme × T1 homogéneo × N ∈ {10, 20, 40} × 20 corridas × 7 schedulers  
**Grupos de escenario:** A1, A2, A3

**¿Qué medimos?**
- Throughput agregado de celda [Mbps]
- Jain index sobre throughput por usuario
- Throughput por usuario (detectar starvation)

**¿Por qué?**  
Con distribución uniforme, los UEs se distribuyen homogéneamente en el área de cobertura. Este escenario es el punto de referencia *neutral*: ningún grupo de usuarios está sistemáticamente más cerca o más lejos del eNB. Es la condición base que Capozzi usa en sus Figs. 8-10.

**Objetivo:**  
Establecer la línea base del comportamiento de cada scheduler antes de introducir heterogeneidad. Permite verificar que los ordenamientos teóricos se cumplen: MT > PF > TTA > RR en throughput y PF/TTA > RR > BET > MT en fairness.

**Resultado esperado:**  
Al aumentar N, el throughput de MT crece por diversidad multiusuario (más candidatos con buen canal disponible). PF y TTA también crecen pero menos. RR y BET son relativamente insensibles a N porque no explotan el canal. El Jain de MT es el más bajo y cae a medida que N aumenta (más usuarios con mal canal son ignorados).

---

### Experimento E2 — Efecto del scheduler bajo distribución espacial clusterizada

**Configuración:** D2 clusterizada × T1 homogéneo × N ∈ {10, 20, 40} × 20 corridas × 7 schedulers  
**Grupos de escenario:** A4, A5, A6

**¿Qué medimos?**
- Jain index (central para H1)
- Throughput por usuario — especialmente la diferencia entre usuarios de clusters cercanos vs lejanos
- Throughput agregado de celda

**¿Por qué?**  
Con distribución clusterizada, los usuarios de distintos clusters tienen condiciones de canal sistemáticamente distintas (los clusters más lejanos tienen menor SINR). Esto exacerba el comportamiento discriminatorio de MT y pone a prueba la robustez de PF y TTA bajo heterogeneidad estructural del canal.

**Objetivo:**  
Responder H1: *¿Los schedulers orientados a throughput degradan fairness de forma no lineal al pasar de D1 a D2?*  
Se comparan los Jain de cada scheduler entre E1 (D1) y E2 (D2) con N fijo. Si MT cae más bruscamente que PF o TTA, H1 se confirma. La no-linealidad se verifica comparando la caída relativa de Jain en {D1→D2} frente a la caída en {N=10→N=40}.

**Resultado esperado:**  
MT concentrará recursos en el cluster más cercano al eNB; usuarios del cluster lejano sufrirán starvation → Jain de MT cae significativamente. BET y PF compensarán mediante el historial de throughput, degradándose menos. TTA, por su ventana de fairness de 1 TTI, se comportará de forma intermedia.

---

### Experimento E3 — Impacto de la distribución espacial vs número de usuarios sobre fairness

**Configuración:** Análisis cruzado sobre los datos de E1 y E2  
**Comparaciones clave:**
- Fija D1, varía N: A1 → A2 → A3 → calcula ΔJain_N para cada scheduler
- Fija N=20, cambia D1→D2: A2 vs A5 → calcula ΔJain_spatial para cada scheduler

**¿Qué medimos?**
- Magnitud del cambio de Jain index al variar N (con D fija)
- Magnitud del cambio de Jain index al cambiar D1→D2 (con N fija)

**¿Por qué?**  
El documento de proyecto plantea que la heterogeneidad espacial podría tener mayor impacto sobre fairness que el número de usuarios. Ningún paper que conozcamos aisla explícitamente estos dos efectos con el mismo conjunto de schedulers y métricas.

**Objetivo:**  
Responder H2: *¿La heterogeneidad espacial impacta más el fairness que el número de usuarios?*  
Si |ΔJain_spatial| > |ΔJain_N| para la mayoría de schedulers, H2 se confirma. El resultado es relevante porque orienta el diseño de redes reales: indica si el operador debe preocuparse más por la distribución geográfica de sus usuarios que por la carga de la celda.

**Resultado esperado:**  
El cambio de D1 a D2 degradará el Jain más que aumentar N de 10 a 40, especialmente para MT. Para PF y TTA el efecto de N será más visible que para MT, porque PF compensa la heterogeneidad de canal mediante el historial.

---

### Experimento E4 — Schedulers bajo tráfico heterogéneo, distribución uniforme

**Configuración:** D1 uniforme × T2 heterogéneo × N=20 × 20 corridas × 7 schedulers  
**Grupo de escenario:** B1  
**Tipos de tráfico en T2:**
- 1/3 UEs: video (GBR_NON_CONV_VIDEO, QCI-4, 1.5 Mbps)
- 1/3 UEs: gaming/baja latencia (GBR_GAMING, QCI-3, 64 kbps)
- 1/3 UEs: best-effort (non-GBR, ~1 Mbps)

**¿Qué medimos?**
- Throughput por LCID: LCID-4 (GBR) vs LCID-3 (BE) — separados
- PLR por tipo de flujo
- Delay medio por tipo de flujo
- Throughput agregado total
- Jain index total y por clase de tráfico

**¿Por qué?**  
Con tráfico heterogéneo, los schedulers que no conocen los requisitos de QoS (RR, BET, MT, TTA, PF) no pueden distinguir un flujo de video que necesita 1.5 Mbps garantizados de un flujo best-effort. Esto genera ineficiencias: PF puede sub-asignar recursos a flujos GBR y over-asignar a BE. M-LWDF y PSS, al ser QoS-aware, deberían corregir esto.

**Objetivo:**  
Responder H3: *¿Pierde PF eficiencia espectral bajo tráfico heterogéneo, y cómo compensan M-LWDF y PSS?*  
Se compara el throughput GBR de PF vs M-LWDF vs PSS en B1. Si M-LWDF y PSS logran mayor throughput GBR a menor PLR GBR que PF, H3 se confirma. También se mide el costo: menor throughput BE.

**Resultado esperado:**  
M-LWDF y PSS redirigen capacidad hacia flujos GBR → su throughput GBR será notablemente mayor que el de PF. PF, al no distinguir flujos, distribuirá el throughput proporcionalmente sin priorizar GBR. RR y BET serán los peores en servicio GBR. TTA ocupará posición intermedia.

---

### Experimento E5 — Schedulers bajo tráfico heterogéneo, distribución clusterizada

**Configuración:** D2 clusterizada × T2 heterogéneo × N=20 × 20 corridas × 7 schedulers  
**Grupo de escenario:** B2

**¿Qué medimos?**
- Los mismos indicadores que E4
- Adicionalmente: comparación B1 vs B2 para cada scheduler → efecto de la distribución sobre QoS

**¿Por qué?**  
Combinar tráfico heterogéneo con distribución clusterizada es el escenario más exigente. Un scheduler puede funcionar bien con tráfico heterogéneo en distribución uniforme pero fallar cuando los UEs con flujos GBR están en el cluster más lejano (peor canal + mayor exigencia de QoS).

**Objetivo:**  
Responder H4: *¿Mejoran los schedulers híbridos (PSS) el compromiso fairness-throughput frente a PF simple bajo las condiciones más adversas?*  
Se compara PSS vs PF en B2. Si PSS mantiene mejor PLR GBR y menor delay para flujos de video/gaming cuando estos están en el cluster lejano, H4 se confirma.

**Resultado esperado:**  
PSS, al priorizar GBR en el TDPS independientemente de la posición del UE, mantendrá PLR_GBR bajo incluso para UEs del cluster lejano. PF no priorizará esos flujos, resultando en mayor PLR_GBR. M-LWDF compensará mediante D_HOL (los paquetes con más retraso suben en prioridad) pero de forma distinta a PSS.

---

### Resumen de experimentos vs hipótesis

| Experimento | Escenarios | Variable que aísla | Hipótesis |
|-------------|-----------|-------------------|-----------|
| E1 | A1, A2, A3 | N usuarios (D1, T1) | Baseline H1/H2 |
| E2 | A4, A5, A6 | N usuarios (D2, T1) | H1 |
| E3 | A2 vs A5, A1-A3 | Espacial vs N | H2 |
| E4 | B1 | Tráfico heterogéneo (D1) | H3 |
| E5 | B2 | Tráfico heterogéneo (D2) | H4 |

### Análisis estadístico por experimento

Todas las comparaciones entre schedulers se hacen con:
- **Media ± IC95%** (t-Student, n=20 corridas, df=19, t₀.₀₂₅=2.093)
- **Test de Welch** para afirmar diferencias significativas entre pares de schedulers
- Umbral de significancia: p < 0.05

Comparaciones clave a reportar:

| Comparación | Hipótesis | Dirección esperada |
|-------------|-----------|-------------------|
| Jain(MT,D2) vs Jain(MT,D1) | H1 | D2 < D1, p < 0.05 |
| \|ΔJain_spatial\| vs \|ΔJain_N\| para PF | H2 | \|espacial\| > \|N\| |
| Throughput_GBR(M-LWDF,B1) vs Throughput_GBR(PF,B1) | H3 | M-LWDF > PF |
| PLR_GBR(PSS,B2) vs PLR_GBR(PF,B2) | H4 | PSS < PF |

---

## 10. Estado de implementación

| Componente | Estado |
|-----------|--------|
| ns-3.47 en `sim/ns-3.47/` | ✅ Instalado y configurado |
| M-LWDF integrado en ns-3.47 | ✅ Compilado OK |
| Script `ofdma-scheduler-eval.cc` | ✅ Implementado, compila, acepta 7 schedulers |
| Script `analyze_run.py` | ✅ Implementado con `--nues` para Jain correcto |
| TX power eNB corregida a 46 dBm | ✅ Aplicado en script |
| Validación smoke test (PF, 10 UEs, 5 s) | ✅ Jain=0.91, todos los UEs servidos |
| Validación ITER 3 (5 sched, T1 homogéneo) | ✅ Ordenamientos fairness correctos |
| Validación ITER 4 (5 sched, T2 heterogéneo) | ✅ M-LWDF/PSS priorizan GBR correctamente |
| Validación BET y TTA | ⏳ Pendiente (próxima sesión) |
| Ejecución completa 1,120 simulaciones | ⏳ Pendiente |
| Scripts de análisis (process_all, stats, plots) | ⏳ Pendiente — ver `pending/next-steps.md` |
