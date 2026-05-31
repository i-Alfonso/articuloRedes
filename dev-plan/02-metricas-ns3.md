# Métricas en ns-3 — Captura y Cálculo

> Cómo instrumentar las simulaciones para obtener throughput, delay y Jain index por usuario y por celda.

---

## 1. Throughput total (agregado de celda)

**Definición:** Suma de bytes recibidos correctamente por todos los UEs por unidad de tiempo.

### Método A — FlowMonitor (recomendado)

```cpp
#include "ns3/flow-monitor-module.h"

// En el script principal, después de instalar aplicaciones:
FlowMonitorHelper flowmon;
Ptr<FlowMonitor> monitor = flowmon.InstallAll();

Simulator::Stop(Seconds(simTime));
Simulator::Run();

// Post-simulación:
monitor->CheckForLostPackets();
Ptr<Ipv4FlowClassifier> classifier =
    DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

double totalRxBytes = 0.0;
for (auto& flow : stats) {
    totalRxBytes += flow.second.rxBytes;
}
double cellThroughputMbps = (totalRxBytes * 8.0) / (simTime * 1e6);
```

### Método B — RadioBearerStatsCalculator (nativo LTE)

```cpp
lteHelper->EnableDlRlcTraces();
lteHelper->EnableDlPdcpTraces();
// Genera archivos: DlRlcStats.txt, DlPdcpStats.txt
// Columnas relevantes: RNTI, LCID, TxBytes, RxBytes, Delay
```

**Archivo de salida:** `DlRlcStats.txt`  
Columnas: `%time  cellId  IMSI  RNTI  LCID  nTxPDUs  TxBytes  nRxPDUs  RxBytes  Delay  StdDev  MinDelay  MaxDelay  ...`

### Cálculo del throughput por usuario desde DlRlcStats

```python
import pandas as pd

df = pd.read_csv('DlRlcStats.txt', sep='\t', comment='%')
# Agrupar por RNTI, sumar RxBytes
per_user = df.groupby('RNTI')['RxBytes'].sum()
# Throughput en Mbps
sim_time = 30  # segundos
per_user_throughput_mbps = per_user * 8 / (sim_time * 1e6)
cell_throughput_mbps = per_user_throughput_mbps.sum()
```

---

## 2. Throughput por usuario

**Por qué es importante:** Detecta starvation (usuarios con throughput ≈ 0 bajo MT con clustering).

```python
# Continuación del bloque anterior
per_user_throughput = per_user * 8 / (sim_time * 1e6)  # Mbps por usuario
print(per_user_throughput.describe())  # min, max, media, std
```

**Traza alternativa — callback en aplicación:**

```cpp
// Para cada UE, conectar un sink con callback:
void PacketSinkRxCallback(Ptr<const Packet> packet, const Address& addr) {
    g_rxBytes[GetRNTI(addr)] += packet->GetSize();
}
// Conectar: sink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketSinkRxCallback));
```

---

## 3. Delay (retardo de entrega)

**Definición usada:** End-to-end packet delay = timestamp de recepción − timestamp de envío.

### Desde FlowMonitor

```cpp
for (auto& flow : stats) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
    double meanDelay = flow.second.delaySum.GetSeconds() /
                       flow.second.rxPackets;
    double maxDelay  = flow.second.lastDelay.GetSeconds();
    // Guardar por flujo: source IP, dest IP → RNTI
}
```

### Desde DlRlcStats

El archivo ya incluye columna `Delay` (mean) y `MaxDelay` por bearer.

```python
delay_df = df.groupby('RNTI')[['Delay', 'MaxDelay']].mean()
```

### Para M-LWDF: Head-of-Line Delay (D_HOL)

El D_HOL no está disponible directamente en las trazas — es interno al scheduler.  
**Dentro de la implementación de M-LWDF** se obtiene así:

```cpp
// En DoSchedDlTriggerReq, el parámetro params incluye:
// params.m_rlcBufferReq → lista de RlcPduListElement_s
// Cada elemento tiene: m_logicalChannelIdentity, m_rlcRetransmissionHolDelay,
//                      m_rlcTransmissionQueueHolDelay

for (auto& bufReq : params.m_rlcBufferReq) {
    uint16_t rnti = bufReq.m_rnti;
    // D_HOL en milisegundos:
    double hol_ms = bufReq.m_rlcTransmissionQueueHolDelay;
    double hol_s  = hol_ms / 1000.0;
}
```

---

## 4. Jain Fairness Index

**Fórmula:**
```
J(x_1, x_2, ..., x_n) = (Σ x_i)² / (n · Σ x_i²)
```
Rango: [1/n, 1]. Valor 1 = fairness perfecta.

### Cálculo post-simulación en Python

```python
import numpy as np

def jain_fairness(throughputs):
    """
    throughputs: array-like de throughput por usuario (mismas unidades)
    """
    x = np.array(throughputs, dtype=float)
    n = len(x)
    if n == 0 or np.sum(x**2) == 0:
        return 0.0
    return (np.sum(x))**2 / (n * np.sum(x**2))

# Uso:
jain = jain_fairness(per_user_throughput.values)
print(f"Jain Fairness Index: {jain:.4f}")
```

**Nota:** El índice se calcula sobre throughput (Mbps) por usuario, no sobre tiempo de servicio. Esto es consistente con Capozzi et al. Fig. 10.

---

## 5. Resumen de trazas habilitadas en ns-3

```cpp
// Habilitar en el script antes de Simulator::Run():

lteHelper->EnableDlRlcTraces();     // throughput + delay por bearer
lteHelper->EnableDlPdcpTraces();    // throughput nivel PDCP
lteHelper->EnablePhyTraces();       // CQI, MCS, RBs asignados (opcional)
lteHelper->EnableMacTraces();       // scheduler decisions (opcional)

// FlowMonitor ya configurado arriba
```

**Archivos generados:**

| Archivo | Qué contiene | Usado para |
|---------|-------------|-----------|
| `DlRlcStats.txt` | Throughput y delay por UE/bearer | Throughput, delay, Jain |
| `DlPdcpStats.txt` | Throughput nivel PDCP | Validación cruzada |
| `DlMacStats.txt` | RBs asignados por TTI, MCS | Análisis del scheduler |
| `FlowStats.csv` | E2E delay y PLR por flujo IP (Fix5) | Delay E2E real, PLR por tipo de tráfico |

`FlowStats.csv` columnas: `flow_id, src, dst, tx_pkts, rx_pkts, lost_pkts, mean_e2e_delay_ms, rx_bytes`  
Permite distinguir flujos video/gaming/BE por dirección IP para el escenario T2 heterogéneo.

---

## 6. Scripts de análisis (implementación real)

El análisis se ejecuta en tres pasos desde `sim/`:

```bash
python3 scripts/orchestrator.py --phase analyze
```

Esto llama internamente a:

**`analyze_run.py`** — procesa una carpeta de corrida, devuelve JSON:
```python
# Salida: { "cell_throughput_mbps": 14.16, "jain_index": 0.747,
#            "mean_delay_ms": 3.2, "plr": 0.034,
#            "n_ues_active": 20, "n_ues_total": 20,
#            "per_ue_throughput_mbps": {"1": 0.85, "2": 1.02, ...} }
# Uso:
python3 scripts/analyze_run.py --raw-dir results/raw/phase2_group_a/pf_uniform_homogeneous_n20_r01 --nues 20
```

**`process_all.py`** — llama a analyze_run sobre las 1,680 carpetas y escribe `master.csv`:
```
phase, scheduler, n_ues, spatial, traffic, run_id, cell_throughput_mbps, jain_index, mean_delay_ms, plr
```

**`compute_stats.py`** — agrupa master.csv por (scheduler × spatial × N × traffic) y calcula IC95%:
```python
# IC 95% con t-Student (df=19, t_crítico=2.093)
ci95_lo = mean - 2.093 * std / sqrt(20)
ci95_hi = mean + 2.093 * std / sqrt(20)
```
Salida: `stats.csv` con 84 grupos.

---

## 7. Valores de referencia observados (validación Fase 1)

Escenario de referencia: D1 uniforme, T1 full-buffer, N=20, simTime=30s, fading EPA activo.

| Scheduler | Throughput celda | Jain index | UEs activos |
|-----------|-----------------|-----------|-------------|
| PSS | 12.90 Mbps | 0.733 | 20/20 |
| PF = M-LWDF | 12.74 Mbps | 0.751 | 20/20 |
| MT | 11.56 Mbps | 0.167 | 7/20 ← starvation |
| TTA | 9.11 Mbps | 0.778 | 20/20 |
| RR | 6.09 Mbps | 0.682 | 20/20 |
| BET | 3.73 Mbps | 1.000 | 20/20 ← fairness perfecta |

Nota: con fading EPA y full-buffer el throughput máximo de celda es ~13–14 Mbps (no 20 Mbps teóricos) debido a la variación de canal. MT tiene Jain bajo porque concentra todos los RBs en los UEs con mejor SINR instantáneo.
