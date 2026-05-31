# Algoritmos — Especificación Técnica e Implementación ns-3

> Fuente de fórmulas: Capozzi et al. (2012), Secciones III–IV.
> Taxonomía adoptada: 3 categorías activas, 2-3-2 algoritmos por categoría.

---

## Notación común

| Símbolo | Significado |
|---------|-------------|
| `m_i,k` | Métrica del usuario i en el RB k (determina la asignación) |
| `d_i^k(t)` | Throughput esperado del usuario i en el RB k en el TTI t (del CQI) |
| `d_i(t)` | Throughput esperado wideband del usuario i en el TTI t |
| `R_i(t)` | Throughput promedio pasado del usuario i hasta el TTI t |
| `R_i^sch(t)` | Promedio solo actualizado cuando el usuario es servido (variante PSS) |
| `r_i(t)` | Throughput instantáneo del usuario i en el TTI t |
| `D_HOL,i` | Head-of-Line delay: delay del primer paquete en cola del usuario i |
| `τ_i` | Delay budget máximo del usuario i (umbral de QoS) |
| `δ_i` | Tasa de pérdida de paquetes aceptable para el usuario i |
| `β` | Factor de suavizado del promedio móvil (típico: 0.9–0.99) |

**Actualización del promedio pasado:**
```
R_i(t) = β · R_i(t-1) + (1 - β) · r_i(t)
```

**Asignación de RB** (regla universal): el RB k se asigna al usuario j tal que:
```
j = argmax_i { m_i,k }
```

---

## Categoría (i) — Channel-unaware

> Schedulers que no usan información de canal (CQI). Originalmente diseñados para redes cableadas.

---

## 1. Round Robin (RR)

**Clase ns-3:** `ns3::RrFfMacScheduler` — **Nativo, base requerido**  
**Objetivo:** Fairness temporal — cada usuario recibe el mismo tiempo de canal  
**Hipótesis:** H1, H2 (referencia inferior)

### Métrica

```
m_i,k^RR = t - T_i
```
donde `T_i` es el último TTI en que el usuario i fue servido. La métrica es **independiente del canal**.

### Limitación clave

No explota CQI: usuarios en periferia reciben igual tiempo pero menor throughput efectivo.
Bajo clustering, el Jain de tiempo ≈ 1 pero el Jain de throughput puede degradarse significativamente.

---

## 2. Blind Equal Throughput (BET)

**Clase ns-3:** `ns3::FdBetFfMacScheduler` — **Nativo**  
**Objetivo:** Igualdad de throughput a largo plazo (no de tiempo)  
**Hipótesis:** H1, H2 (contraste con RR en categoría channel-unaware)

### Métrica

```
m_i,k^BET = 1 / R_i(t-1)
```

El usuario con menor throughput acumulado obtiene prioridad. A diferencia de RR (que iguala tiempo),
BET iguala el throughput recibido: usuarios con peor canal reciben más recursos para compensar.

### Diferencia clave RR vs BET

| Aspecto | RR | BET |
|---------|----|----|
| ¿Qué iguala? | Tiempo de canal | Throughput recibido |
| Reacción a canal heterogéneo | Ignora (asigna igual tiempo) | Compensa (asigna más tiempo al peor canal) |
| Usa CQI | No | No (solo R_i histórico) |
| Jain throughput bajo clustering | Puede degradarse | Más estable |

**Nota:** BET es el componente de fairness de PF. La métrica PF = MT × BET, por eso conecta las categorías (i) y (ii) en la narrativa del paper.

---

## Categoría (ii) — Channel-aware / QoS-unaware

> Schedulers que explotan el CQI pero no tienen en cuenta requisitos de QoS de los flujos.

---

## 3. Maximum Throughput (MT)

**Clase ns-3:** `ns3::TdMtFfMacScheduler` — **Nativo, base requerido**  
**Objetivo:** Maximizar throughput agregado de celda  
**Hipótesis:** H1 (polo de throughput extremo), H2

### Métrica

```
m_i,k^MT = d_i^k(t)
```

Cada TTI, **todos los RBs** se asignan al usuario con mayor CQI. Favorece sistemáticamente
a usuarios con buen canal (cerca del eNB). Maximiza la eficiencia espectral instantánea.

### Por qué TdMt y no FdMt

`TdMtFfMacScheduler` (TD = Time Domain) asigna todos los recursos del TTI al mejor usuario,
que es el comportamiento canónico de MT en la literatura (Capozzi Figs. 8-10, Jain < 0.1 con muchos UEs).
`FdMtFfMacScheduler` asigna por RB individualmente y con tráfico CBR puede dar throughput total
**menor** que PF porque los UEs de mejor canal alcanzan su tasa máxima rápido y no usan el exceso.

### Comportamiento esperado

- Distribución uniforme: favorece usuarios cercanos → Jain bajo
- Distribución clusterizada: si clusters están lejos, starvation de esos UEs → **degradación no lineal del Jain** (H1)

---

## 4. Throughput To Average (TTA)

**Clase ns-3:** `ns3::TtaFfMacScheduler` — **Nativo**  
**Objetivo:** Fairness a corto plazo dentro de cada TTI; intermedio entre MT y PF  
**Hipótesis:** H1, H2 (punto intermedio en el espectro throughput-fairness)

### Métrica

```
m_i,k^TTA = d_i^k(t) / d_i(t)
```

Normaliza el throughput esperado en el RB k por el throughput **wideband** del mismo usuario.
Cuantifica la **ventaja relativa** de asignar ese RB específico. Usuarios con peor canal global
tienen métrica más alta en sus mejores RBs → garantiza un mínimo de servicio a todos.

### Diferencia clave MT vs TTA vs PF

| Aspecto | MT | TTA | PF |
|---------|----|----|-----|
| Normaliza por | — | `d_i(t)` wideband (mismo TTI) | `R_i(t-1)` historial |
| Ventana de fairness | Ninguna | 1 TTI | `T_f = 1/(1-β)` TTIs |
| Aprovecha diversidad multiusuario | Máximo | Parcial | Bueno |
| Jain index (literatura) | < 0.2 | ~0.9 | ~0.95 |

TTA fue evaluado explícitamente por Capozzi (Figs. 8-10) y muestra fairness alta sin requerir historial de largo plazo.

---

## 5. Proportional Fair (PF)

**Clase ns-3:** `ns3::PfFfMacScheduler` — **Nativo, base requerido**  
**Objetivo:** Balance óptimo entre eficiencia espectral y fairness a largo plazo  
**Hipótesis:** H1, H2, H3 — **referencia central en todas las comparaciones**

### Métrica

```
m_i,k^PF = d_i^k(t) / R_i(t-1)
```

Combina MT (numerador) con BET (denominador). El historial `R_i` actúa como factor de peso:
usuarios con bajo throughput pasado obtienen métrica más alta → eventualmente son servidos.

### Parámetro β

| β | T_f | Comportamiento |
|---|-----|----------------|
| 0.90 | 10 TTIs | Balance rápido |
| 0.99 | 100 TTIs | Balance estándar |
| 0.999 | 1000 TTIs | Casi BET |

**Valor usado:** β = 0.99 (consistente con Capozzi et al.)

---

## Categoría (iii) — Channel-aware / QoS-aware

> Schedulers que usan CQI **y** requisitos de QoS (delay budget, PLR objetivo, tipo de bearer).

---

## 6. Modified Largest Weighted Delay First (M-LWDF)

**Clase ns-3:** `ns3::MlwdfFfMacScheduler` — **Implementado e integrado**  
**Objetivo:** Garantizar delay acotado bajo tráfico mixto RT + BE  
**Hipótesis:** H3

### Métrica

Para flujos **real-time** (video, baja latencia):
```
m_i,k^MLWDF = α_i · D_HOL,i · d_i^k(t) / R_i(t-1)
```
donde:
```
α_i = -log(δ_i) / τ_i
```

Para flujos **best-effort**:
```
m_i,k^MLWDF = m_i,k^PF = d_i^k(t) / R_i(t-1)
```

### Parámetros QoS por tipo de bearer

| Bearer | QCI | τ_i | δ_i | α_i |
|--------|-----|-----|-----|-----|
| GBR_NON_CONV_VIDEO | 4 | 300 ms | 10⁻⁶ | 46.1 |
| GBR_GAMING | 3 | 50 ms | 10⁻³ | 138.2 |
| non-GBR | — | ∞ | — | usa PF |

---

## 7. Priority Set Scheduler (PSS)

**Clase ns-3:** `ns3::PssFfMacScheduler` — **Nativo**  
**Objetivo:** Garantizar target bitrate a flujos GBR; dos niveles TDPS + FDPS  
**Hipótesis:** H3 + H4

### Estructura

```
TDPS: selecciona N_mux candidatos
  GBR  → BET (prioriza quien no alcanza target bitrate)
  nGBR → PF wideband
        ↓
FDPS: asigna RBs con PFsch = d_i^k(t) / R_i^sch(t-1)
```

### Configuración ns-3

```cpp
lteHelper->SetSchedulerType("ns3::PssFfMacScheduler");
lteHelper->SetSchedulerAttribute("nMux",              UintegerValue(10));
lteHelper->SetSchedulerAttribute("PssFdSchedulerType", StringValue("PFsch"));
```

---

## Resumen comparativo

| # | Algoritmo | Categoría | Base | Clase ns-3 | Estado |
|---|-----------|-----------|------|-----------|--------|
| 1 | RR  | (i) Channel-unaware | ✅ | `RrFfMacScheduler` | Nativo |
| 2 | BET | (i) Channel-unaware | — | `FdBetFfMacScheduler` | Nativo |
| 3 | MT  | (ii) Ch-aware/QoS-unaware | ✅ | `TdMtFfMacScheduler` | Nativo |
| 4 | TTA | (ii) Ch-aware/QoS-unaware | — | `TtaFfMacScheduler` | Nativo |
| 5 | PF  | (ii) Ch-aware/QoS-unaware | ✅ | `PfFfMacScheduler` | Nativo |
| 6 | M-LWDF | (iii) Ch-aware/QoS-aware | — | `MlwdfFfMacScheduler` | Implementado |
| 7 | PSS | (iii) Ch-aware/QoS-aware | — | `PssFfMacScheduler` | Nativo |
