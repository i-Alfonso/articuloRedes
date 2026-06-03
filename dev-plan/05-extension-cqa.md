# Fase 4 — Extensión: CQA como tercer representante de categoría (iii)

> **Branch:** `final`  
> **Estado:** ✅ COMPLETADO

---

## Motivación

El experimento principal (1,680 corridas) cubre categoría (iii) con M-LWDF y PSS.
CQA es un tercer scheduler de la misma categoría con mecanismo QoS distinto,
disponible en ns-3.47 sin código adicional.

| Algoritmo | Clase ns-3.47 | Mecanismo QoS |
|-----------|--------------|---------------|
| **CQA** — Channel and QoS Aware | `CqaFfMacScheduler` | Métrica compuesta: CQI + HOL delay + tamaño de cola + clase de prioridad |

### Diferencias dentro de categoría (iii)

| Scheduler | Priorización | Tipo de QoS |
|-----------|-------------|-------------|
| M-LWDF | Delay HOL × canal | Delay-driven |
| PSS | Separación GBR/non-GBR | Bearer-driven |
| CQA | CQI + HOL + queue + prioridad | Multi-criterio |

---

## Categorías de Capozzi — aclaración

CQA pertenece a la **categoría (iii)**, igual que M-LWDF y PSS.
Las categorías (iv) semi-persistent y (v) energy-aware no tienen representantes
comparables en ns-3.47 bajo las métricas de este experimento:
- Cat (iv): modo de operación para VoIP, no un scheduler seleccionable
- Cat (v): optimiza Julios/bit, métrica incomparable con throughput/Jain

**TBFQ evaluado pero descartado** — ver nota al final.

---

## Diseño

Misma estructura que Grupos A y B. Solo CQA.

| Fase | Distribución | N | Traffic | Corridas |
|------|-------------|---|---------|---------|
| 4a | D1 + D2 | 10, 20, 40 | T1 | 120 |
| 4b | D1 + D2 | 10, 20, 40 | T2 | 120 |

**Total: 240 corridas**

---

## Resultados principales

**CQA bajo T1 (N=20, D1):** Jain≈0.999, throughput=5.3 Mbps — se comporta como BET.
La saturación total neutraliza el mecanismo QoS y CQA actúa como equalizer.

**CQA bajo T2+D2:** Jain=0.596, supera a PF (0.543) igual que M-LWDF y PSS.
Tres schedulers cat(iii) con mecanismos distintos convergen al mismo resultado,
robusteciendo H4 más allá de un hallazgo puntual.

---

## ⚠️ Nota: TBFQ evaluado pero descartado

**Síntoma:** Datos válidos solo en el intervalo 1–1.25 s bajo T1 (full-buffer).

**Causa:** `FdTbfqFfMacScheduler` tiene `TokenPoolSize=1 byte` por defecto.
Con 10 Mbps/UE × 20 UEs bajo saturación total, todos los tokens se agotan
en el primer segundo y se produce deadlock.

**Por qué no se incluye:**
1. Datos incompletos: sin T1, no es comparable con los 7 schedulers originales
2. Misma categoría (iii) que M-LWDF/PSS/CQA — no añade diversidad taxonómica
3. El fallo bajo T1 es un resultado de implementación, no un hallazgo de scheduling

**Lo que sí se documenta:** el token deadlock como limitación de diseño de TBFQ
bajo condiciones de saturación total — es información útil para futuros trabajos.
