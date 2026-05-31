# Escenarios Experimentales — Matriz Completa

---

## Dimensiones del experimento

| Dimensión | Valores | Notas |
|-----------|---------|-------|
| **Distribución espacial** | Uniforme (D1) / Clusterizada (D2) | Eje principal del proyecto |
| **Tipo de tráfico** | Homogéneo full-buffer (T1) / Heterogéneo (T2) | T2: video GBR + gaming GBR + BE full-buffer |
| **Número de usuarios** | 10, 20, 40 | Para curvas de carga — aplica en **ambos** grupos |
| **Algoritmo** | RR, BET, MT, TTA, PF, M-LWDF, PSS | 7 schedulers — taxonomía Capozzi 2-3-2 |
| **Corridas por config** | 20 | Para IC al 95% (t-Student, df=19, t=2.093) |

---

## Parámetros fijos de la simulación (todos los escenarios)

```
Simulador:         ns-3.47, módulo LTE
Script:            sim/ns-3.47/scratch/ofdma-scheduler-eval.cc
Duración:          30 s total (5 s warmup descartado → 25 s de medición)
eNodeBs:           1 (celda única, posición (0,0,30m))
Radio de celda:    500 m  (UEs distribuidos hasta 490 m)
Frecuencia DL:     2.1 GHz (EARFCN 100)
Ancho de banda DL: 10 MHz → 50 Resource Blocks
TX power eNB:      46 dBm (macro eNB estándar; cubre 490 m con SINR > 0 dB)
Path loss:         LogDistancePropagationLossModel
                   Exponent = 3.5 (urbano), RefDist = 1 m, RefLoss = 46.7 dB
Fading:            TraceFadingLossModel — traza EPA 3 km/h (peatonal)
                   Proporciona variación temporal del canal por TTI
                   Sin fading: UEs equidistantes tendrían CQI idéntico
CQI:               Dinámico, full-bandwidth periodic, 1 ms (1 TTI)
EPC:               Habilitado (PointToPointEpcHelper)
Backbone P2P:      100 Gb/s, delay 1 ms (entre PGW y remoteHost)
Protocolo:         UDP IP extremo a extremo
Movilidad:         Estática (ConstantPositionMobilityModel)
FlowMonitor:       Habilitado → FlowStats.csv (delay E2E, PLR por flujo IP)
Semilla RNG:       SetSeed(12345), SetRun(runId) con runId ∈ {1..20}
```

---

## Grupo A — Heterogeneidad Espacial (H1 y H2)

**Tráfico:** T1 — Homogéneo full-buffer (todos los UEs con OnOff UDP always-on, 10 Mbps/UE)
**Propósito:** Aislar el efecto de la distribución espacial y el número de usuarios sobre fairness y throughput

| ID | Distribución | N usuarios | Algoritmos | Corridas | Total runs |
|----|-------------|-----------|-----------|---------|-----------|
| A1 | Uniforme (D1) | 10 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| A2 | Uniforme (D1) | 20 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| A3 | Uniforme (D1) | 40 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| A4 | Clusterizada (D2) | 10 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| A5 | Clusterizada (D2) | 20 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| A6 | Clusterizada (D2) | 40 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |

**Subtotal Grupo A: 840 corridas** (7 alg × 2 spatial × 3 nUEs × 20 runs)

**Análisis H1:** Curvas de Jain(MT) vs Jain(PF) vs Jain(RR) al pasar de D1→D2 con N fijo.
Degradación no lineal esperada: MT cae mucho, PF cae poco, RR cae moderado.

**Análisis H2:** Fijar D1, variar N (A1→A2→A3) → ΔJain_N. Fijar N=20, cambiar D1→D2 (A2 vs A5) → ΔJain_dist.
Comparar |ΔJain_dist| vs |ΔJain_N| → se espera que la heterogeneidad espacial domine.

---

## Grupo B — Heterogeneidad de Tráfico (H3 y H4)

**Tráfico:** T2 — Heterogéneo (video GBR + gaming GBR + BE full-buffer, distribución 1/3:1/3:1/3 interleaved)
**Propósito:** Evaluar cómo el tráfico heterogéneo afecta eficiencia y cumplimiento de QoS bajo distintas cargas y distribuciones espaciales

> **Diseño simétrico con Grupo A:** N varía en {10, 20, 40} y spatial en {uniform, clustered}, igual que el Grupo A. Esto permite comparar directamente A vs B para el mismo (N, spatial) y evaluar si el efecto de H3/H4 depende de la carga.

| ID | Distribución | N usuarios | Algoritmos | Corridas | Total runs |
|----|-------------|-----------|-----------|---------|-----------|
| B1 | Uniforme (D1) | 10 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| B2 | Uniforme (D1) | 20 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| B3 | Uniforme (D1) | 40 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| B4 | Clusterizada (D2) | 10 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| B5 | Clusterizada (D2) | 20 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |
| B6 | Clusterizada (D2) | 40 | RR, BET, MT, TTA, PF, M-LWDF, PSS | 20 | 140 |

**Subtotal Grupo B: 840 corridas** (7 alg × 2 spatial × 3 nUEs × 20 runs)

**Análisis H3:** Comparar PF en T1 (A2) vs PF en T2 (B2) → ¿cae throughput total? Repetir para N=10 (A1 vs B1) y N=40 (A3 vs B3).

**Análisis H4:** PSS vs PF vs M-LWDF en B1-B6: ¿mejora throughput GBR? ¿a qué costo en BE y Jain total?

---

## Total de simulaciones

| Grupo | Escenarios | Runs | Propósito |
|-------|-----------|------|-----------|
| A (6 escenarios) | A1–A6 | 840 | H1 + H2 |
| B (6 escenarios) | B1–B6 | 840 | H3 + H4 |
| **Total producción** | **12** | **1,680** | — |
| Validación (Fase 1) | 2 | 10 | No cuenta para paper |

**Tiempo estimado:** ~2.3 h con 8 cores paralelos (~40 s/run con fading)

---

## Configuración de distribuciones espaciales

### D1 — Distribución Uniforme

UEs distribuidos uniformemente en un disco de radio 490 m centrado en el eNB.
Todos los UEs tienen SINR similar en promedio — referencia de canal homogéneo.

```cpp
Ptr<UniformDiscPositionAllocator> pa = CreateObject<UniformDiscPositionAllocator>();
pa->SetX(0.0);
pa->SetY(0.0);
pa->SetRho(490.0);  // radio = 490 m (margen de 10 m respecto al límite de celda)
ueMob.SetPositionAllocator(pa);
ueMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
ueMob.Install(ueNodes);
```

### D2 — Distribución Clusterizada (cerca / medio / lejos)

3 clusters a **distancias distintas** del eNB. Requerimiento explícito del proyecto:
*"condiciones de canal heterogéneas (distancia al eNodeB)"*.

| Cluster | Distancia centro | Ángulo | Coordenadas | Rango UEs | Canal |
|---------|-----------------|--------|-------------|-----------|-------|
| C1 — cerca | 100 m | 0° | (100, 0) | 30–170 m | SINR alto |
| C2 — medio | 250 m | 120° | (-125, 216) | 180–320 m | SINR moderado |
| C3 — lejos | 400 m | 240° | (-200, -346) | 330–470 m | SINR bajo |

Radio de cada cluster: **70 m**. Ángulos separados 120° para distribución espacial simétrica.

```cpp
// 3 clusters a distancias DISTINTAS — cerca / medio / lejos
const std::vector<std::pair<double, double>> centres = {
    { 100.0,    0.0},   // C1 cerca:  100 m al Este    (0°)
    {-125.0,  216.0},   // C2 medio:  250 m Noroeste  (120°)
    {-200.0, -346.0}};  // C3 lejos:  400 m Suroeste  (240°)
const double clusterR = 70.0;
uint32_t base = nUEs / 3;
// C1 y C2 reciben base UEs, C3 recibe el resto (nUEs - 2*base)
```

**Comportamiento esperado por scheduler:**

| Scheduler | C1 (cerca) | C2 (medio) | C3 (lejos) |
|-----------|-----------|-----------|-----------|
| MT | Monopoliza (alta SINR) | Parcial | Starvation total |
| PF | Sirve a todos | Equitativo | Sirve con penalización |
| RR | Tiempo igual | Tiempo igual | Tiempo igual (throughput bajo) |
| BET | Más tiempo a C3 | Medio | Mucho tiempo (compensa SINR bajo) |

---

## Configuración de modelos de tráfico

### T1 — Homogéneo Full-Buffer

**Fix3 aplicado:** Se cambió de CBR (1 Mbps fijo) a full-buffer. Con CBR, el buffer de cada UE
se vacía rápido y los schedulers channel-aware no pueden explotar diversidad multiusuario.
Full-buffer mantiene siempre demanda en cola: el scheduler debe elegir activamente en cada TTI.

```cpp
// OnOff always-on a 10 Mbps/UE — total ofrecido >> capacidad celda (~20 Mbps)
OnOffHelper onoff("ns3::UdpSocketFactory",
                  InetSocketAddress(ueIpIface.GetAddress(u), port));
onoff.SetConstantRate(DataRate("10Mbps"), 1250);  // paquetes de 1250 B
onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
```

Con N=20 UEs: 20 × 10 Mbps = 200 Mbps ofrecidos vs ~20 Mbps de capacidad celda → saturación real.

### T2 — Heterogéneo: Video GBR + Gaming GBR + Best-Effort

**Fix1 aplicado:** Asignación **interleaved** por índice `u % 3` (no secuencial).
Con asignación secuencial: C1=todo video, C2=todo gaming, C3=todo BE → confunde H3/H4
(no se sabe si el scheduler mejora GBR porque es mejor algoritmo o porque los UEs GBR están más cerca).
Con interleaved: cada cluster recibe los 3 tipos mezclados → posición y tipo de tráfico son independientes.

```cpp
// u % 3 == 0 → VIDEO    (GBR QCI-4, 1.5 Mbps)
// u % 3 == 1 → GAMING   (GBR QCI-3, 64 kbps)
// u % 3 == 2 → BEST-EFFORT (non-GBR, full-buffer OnOff 10 Mbps)
for (uint32_t u = 0; u < nUEs; ++u) {
    if      (u % 3 == 0) { /* instalar video bearer GBR_NON_CONV_VIDEO */ }
    else if (u % 3 == 1) { /* instalar gaming bearer GBR_GAMING        */ }
    else                  { /* OnOff full-buffer non-GBR                */ }
}
```

**Parámetros QoS por tipo de tráfico:**

| Tipo | Bearer ns-3 | QCI | Bitrate | Pkt | Intervalo | τ (delay) | δ (PLR) |
|------|------------|-----|---------|-----|-----------|-----------|---------|
| Video | `GBR_NON_CONV_VIDEO` | 4 | 1.5 Mbps | 1000 B | 5.3 ms | 300 ms | 10⁻⁶ |
| Gaming | `GBR_GAMING` | 3 | 64 kbps | 160 B | 20 ms | 50 ms | 10⁻³ |
| Best-effort | `NGBR_DEFAULT` | 9 | full-buffer | 1250 B | ~1 ms | — | — |

**LCID en DlRlcStats.txt:**
- LCID 3 = bearer default non-GBR (BE)
- LCID 4 = bearer dedicado GBR (video + gaming)

### Fading — TraceFadingLossModel (Fix2)

```cpp
// EPA 3 km/h: Extended Pedestrian A, velocidad peatonal — apropiado para UEs estáticos
// Proporciona variación temporal del canal (diferente CQI por RB por TTI)
// Sin fading: schedulers channel-aware no tienen ventaja real sobre RR/BET
lteHelper->SetAttribute("FadingModel", StringValue("ns3::TraceFadingLossModel"));
lteHelper->SetFadingModelAttribute("TraceFilename",
    StringValue("src/lte/model/fading-traces/fading_trace_EPA_3kmph.fad"));
lteHelper->SetFadingModelAttribute("TraceLength",  TimeValue(Seconds(10.0)));
lteHelper->SetFadingModelAttribute("SamplesNum",   UintegerValue(10000));
lteHelper->SetFadingModelAttribute("WindowSize",   TimeValue(Seconds(0.5)));
lteHelper->SetFadingModelAttribute("RbNum",        UintegerValue(100));
```

### FlowMonitor — Métricas E2E (Fix5)

Además de `DlRlcStats.txt` (delay RLC), se genera `FlowStats.csv` con métricas E2E reales:

```
Columnas: flow_id, src, dst, tx_pkts, rx_pkts, lost_pkts, mean_e2e_delay_ms, rx_bytes
```

El delay E2E incluye el backbone P2P de 1 ms sobre el delay RLC. Permite calcular PLR real por flujo IP.

---

## Semillas aleatorias

```cpp
RngSeedManager::SetSeed(12345);
RngSeedManager::SetRun(runId);  // runId de 1 a 20
```

Las 20 corridas por configuración usan runId ∈ {1, 2, ..., 20}. La semilla base garantiza
reproducibilidad; el runId garantiza independencia estadística entre corridas.
