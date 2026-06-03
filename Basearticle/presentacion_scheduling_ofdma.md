# Evaluación de Schedulers en Redes LTE/OFDMA
### Guía de presentación — 25 diapositivas
**Alfonso García · Maestría en Ciencias de la Computación · BUAP**

---

> **Cómo usar este archivo:**
> Cada sección `---` es una diapositiva. El nombre del archivo de imagen
> aparece entre corchetes `[imagen: NOMBRE_ARCHIVO.pdf]` — búscala en la
> carpeta de figuras y colócala en esa posición. Las notas del presentador
> están en bloque `> 💬`.

---

## DIAPOSITIVA 1 · Portada

# ¿A quién le toca usar el canal?
## Cómo decide una antena celular a quién darle velocidad

**Alfonso García**
Maestría en Ciencias de la Computación — BUAP, 2026

> 💬 **Nota del presentador:**
> El título es intencional: la pregunta más simple resume todo el proyecto.
> Cada milisegundo, una antena LTE tiene que decidir quién de todos
> los usuarios conectados recibe datos. Esa decisión, repetida 1,000
> veces por segundo, es lo que estudiamos.

---

## DIAPOSITIVA 2 · El problema en palabras simples

# Imagina esto

Una sala de espera hospitalaria conectada por LTE:

- 👨‍⚕️ Un médico transmite una videollamada de telemedicina
  → Necesita velocidad estable y poca espera
- 📋 Una enfermera descarga expedientes clínicos
  → No le urge tanto, puede esperar
- 📱 Todos están lejos de la antena, agrupados en la misma sala

**La antena tiene que decidir, cada milisegundo: ¿a quién le doy recursos?**

> 💬 **Nota del presentador:**
> Este ejemplo viene directo del paper. Sirve para mostrar que el problema
> no es teórico: los algoritmos que estudiamos afectan experiencias reales.
> Si el scheduler está mal elegido, el médico puede perder la videollamada
> o el paciente recibir un diagnóstico equivocado por mala conexión.

---

## DIAPOSITIVA 3 · ¿Qué es un scheduler?

# El árbitro del canal de radio

```
Cada 1 ms (un TTI), la antena pregunta:
¿Quién tiene mejor señal ahora?
¿Quién lleva más tiempo sin recibir nada?
¿Quién tiene una videollamada urgente en espera?

→ Según las respuestas, asigna bloques de frecuencia (RBs)
```

**Un scheduler es el algoritmo que toma esa decisión.**

| Si el scheduler prioriza... | Gana... | Pierde... |
|---|---|---|
| La mejor señal | Velocidad total | Los usuarios lejos |
| La equidad | Todos reciben algo | Velocidad total |
| Las urgencias de QoS | Videollamadas | Datos en segundo plano |

> 💬 **Nota del presentador:**
> RB = Resource Block = bloque mínimo de frecuencia que se puede asignar.
> Con 10 MHz de ancho de banda hay 50 RBs disponibles cada milisegundo.
> El scheduler decide cuál de los usuarios conectados se lleva cada uno.

---

## DIAPOSITIVA 4 · El contexto técnico: LTE y OFDMA

# Cómo funciona el canal en LTE

```
Espectro de radio (10 MHz)
┌──────────────────────────────────────────────┐
│  RB₁  │  RB₂  │  RB₃  │  ...  │  RB₅₀    │  frecuencia →
└──────────────────────────────────────────────┘
   ↑                                    ↑
   1 ms = 1 TTI                    cada RB = 180 kHz
```

- **OFDMA** divide el espectro en 50 bloques de frecuencia (RBs)
- Cada usuario reporta con qué calidad recibe cada RB → **CQI** (valor 1-15)
- Mejor CQI = más bits por RB (hasta 14 bits) → el scheduler explota eso

**El canal varía por usuario, por frecuencia, y por milisegundo → ahí está la oportunidad**

> 💬 **Nota del presentador:**
> OFDMA es la técnica de acceso múltiple que usan LTE y 5G.
> La clave es que el canal no es igual para todos: el usuario que está
> cerca de la antena recibe muy bien, el que está lejos recibe mal.
> Y esa calidad cambia constantemente (fading). El scheduler inteligente
> aprovecha esos picos momentáneos de buena señal.

---

## DIAPOSITIVA 5 · La pregunta de investigación

# ¿Qué estudiamos exactamente?

**Pregunta central:**
> *¿Cómo afectan la distribución espacial de los usuarios
> y el tipo de tráfico al comportamiento de los schedulers?*

**Lo que no existe en la literatura:**
Una comparación sistemática que cruce **ambas** dimensiones al mismo tiempo:
- 📍 Usuarios agrupados vs dispersos
- 📦 Un solo tipo de tráfico vs mezcla de servicios

**Lo que hicimos:**
1,680 simulaciones en ns-3.47 · 7 algoritmos · 4 condiciones · 20 repeticiones cada una

> 💬 **Nota del presentador:**
> La literatura previa evalúa schedulers en condiciones ideales:
> usuarios distribuidos uniformemente, todos con el mismo tipo de tráfico.
> Nosotros preguntamos: ¿qué pasa cuando la realidad es más complicada?
> Eso nadie lo había cruzado sistemáticamente sobre la taxonomía completa.

---

## DIAPOSITIVA 6 · Los 4 objetivos (hipótesis)

# Lo que esperábamos encontrar

| # | Hipótesis | Resultado |
|---|---|---|
| **H1** | Los schedulers de throughput pierden equidad cuando los usuarios están agrupados | ✅ Confirmada (parcialmente, TTA más que MT) |
| **H2** | Agrupar usuarios geográficamente daña más la equidad que tener muchos usuarios | ❌ Rechazada — la carga importa más |
| **H3** | PF pierde eficiencia cuando hay mezcla de video + datos | ✅ Confirmada |
| **H4** | M-LWDF y PSS mejoran el balance equidad-velocidad frente a PF | ✅ Confirmada (pero con costo) |

> 💬 **Nota del presentador:**
> H2 es la más interesante porque salió al revés de lo esperado.
> Anticipábamos que poner a los usuarios en clusters arruinaría la equidad.
> Resultó que lo que más la arruina es tener demasiados usuarios compitiendo,
> no dónde están parados. Eso tiene implicaciones prácticas importantes.

---

## DIAPOSITIVA 7 · Los 7 algoritmos estudiados

# Clasificación según Capozzi et al. (2013)

| Categoría | Algoritmo | En palabras simples |
|---|---|---|
| **(i) Sin consciencia del canal** | **RR** (Round Robin) | "Turno igualitario, uno por uno" |
| | **BET** (Blind Equal Throughput) | "Todos reciben la misma cantidad acumulada" |
| **(ii) Consciente del canal** | **MT** (Maximum Throughput) | "Siempre al que mejor señal tiene" |
| | **TTA** (Throughput to Average) | "Al que más se beneficia en este momento" |
| | **PF** (Proportional Fair) | "Balance entre señal y equidad histórica" |
| **(iii) Consciente del canal + QoS** | **M-LWDF** | "Prioriza los que llevan más tiempo esperando" |
| | **PSS** (Priority Set Scheduler) | "Separación: primero urgentes, luego el resto" |

> 💬 **Nota del presentador:**
> La taxonomía de Capozzi organiza 12 años de propuestas de la literatura
> en 5 categorías. Este trabajo activa las 3 más relevantes para redes
> con usuarios reales: sin canal, con canal, y con canal+QoS.
> También evaluamos como extensión dos implementaciones propias: CQA y TBFQ.

---

## DIAPOSITIVA 8 · Cómo se hicieron las simulaciones

# El laboratorio virtual: ns-3.47

```
ns-3 = Network Simulator 3 = programa de código abierto
       que simula redes con todos sus protocolos reales
```

**Configuración fija en todas las simulaciones:**

| Qué | Valor |
|---|---|
| Antena (eNodeB) | 1 sola, potencia 46 dBm |
| Ancho de banda | 10 MHz → 50 bloques de frecuencia |
| Frecuencia | 2.1 GHz (banda LTE estándar) |
| Modelo de señal | Pérdida por distancia + fading EPA 3 km/h |
| Protocolo | UDP extremo a extremo |
| Duración por run | 30 s (se descartan los primeros 5 s) |
| Repeticiones | **20 por configuración** → estadísticas confiables |

**Total: 1,680 simulaciones · ~30 horas de cómputo**

> 💬 **Nota del presentador:**
> ns-3 implementa el stack completo de LTE: física, MAC, RLC, PDCP.
> Es el mismo simulador que usan grupos de investigación de Ericsson y Nokia.
> Las 20 repeticiones con semillas diferentes son clave: el canal varía
> aleatoriamente, y necesitamos promedios estadísticamente confiables.

---

## DIAPOSITIVA 9 · Las 4 condiciones del experimento

# Qué variamos y por qué

**Variable 1: Distribución espacial de los usuarios**

```
D1 — UNIFORME                    D2 — CLUSTERIZADA
●  ●   ● ●                       ●●●        (cerca: 100m)
  ● ●●   ●    Antena ▲             ▲
●   ● ●  ●                            ●●●   (medio: 250m)
  ●   ●●                                       ●●●  (lejos: 400m)
```

**Variable 2: Tipo de tráfico**

| T1 — Homogéneo | T2 — Heterogéneo |
|---|---|
| Todos transmiten datos a máxima velocidad (full-buffer) | 1/3 video HD (1.5 Mbps, garantizado) |
| Simula saturación total de la celda | 1/3 gaming (64 kbps, muy sensible a latencia) |
| | 1/3 best-effort (descarga normal) |

> 💬 **Nota del presentador:**
> El "interleaved" en T2 significa que en cada cluster hay los 3 tipos
> mezclados. Esto es intencional: así separamos el efecto del tráfico
> del efecto de la posición geográfica. Si no lo hacemos, no sabemos
> si un scheduler favorece el video porque sabe de QoS o simplemente
> porque esos usuarios están más cerca de la antena.

---

## DIAPOSITIVA 10 · Cómo medimos los resultados

# Las 4 métricas que usamos

**1. Throughput de celda (Mbps)**
> Velocidad total que entrega la antena a todos los usuarios juntos.
> *Más alto = mejor eficiencia espectral.*

**2. Índice de Jain (0 a 1)**
> Mide qué tan igualitario es el reparto. Si vale 1.0 = perfecto.
> Si vale 0.1 = unos pocos monopolizan el canal.
> Fórmula: $J = \frac{(\sum x_i)^2}{n \cdot \sum x_i^2}$

**3. Retardo E2E (ms)**
> Cuánto tiempo tarda un paquete en llegar. Crítico para videollamadas y gaming.

**4. PLR — Tasa de pérdida de paquetes**
> Qué fracción de paquetes se pierde. Para video en tiempo real, debe ser mínima.

> 💬 **Nota del presentador:**
> El Índice de Jain es el más importante para este trabajo.
> Imaginemos 3 usuarios con velocidades 10, 10, 10 Mbps → Jain = 1.0 (perfecto).
> Usuarios con 30, 0, 0 Mbps → Jain = 0.33 (injusto).
> Los SLA de latencia que usamos: gaming ≤ 50ms, video ≤ 300ms.
> Todos los schedulers evaluados los cumplen en las condiciones simuladas.

---

## DIAPOSITIVA 11 · El punto de partida: los extremos del trade-off

# Resultado de referencia: N=20 usuarios, distribución uniforme, tráfico homogéneo

`[imagen: F1_throughput_vs_N_D1.pdf]`

| Scheduler | Velocidad total | Equidad (Jain) | ¿Quién pierde? |
|---|---|---|---|
| **MT** | 11.09 Mbps | 0.116 (pésima) | 13 de 20 usuarios reciben 0 |
| **BET** | 4.32 Mbps | 0.999 (perfecta) | Todos reciben poco |
| **PF** | 14.16 Mbps | 0.747 (buena) | Nadie — balance real |
| **PSS** | 14.53 Mbps | 0.707 (buena) | Nadie — el más rápido |

**El dilema fundamental: más velocidad total = menos equidad**

> 💬 **Nota del presentador:**
> Esta figura muestra throughput vs número de usuarios.
> PF y M-LWDF son prácticamente indistinguibles en T1 (tráfico homogéneo)
> porque M-LWDF solo se diferencia cuando hay restricciones de latencia
> distintas entre flujos. PSS tiene su pico en N=20 y baja en N=40.
> BET y CQA caen con N porque dedican demasiado tiempo a los usuarios
> con peor señal para mantener la equidad.

---

## DIAPOSITIVA 12 · Figura 1 — Throughput vs número de usuarios

# ¿Más usuarios = más velocidad o menos?

`[imagen: F1_throughput_vs_N_D1.pdf]`

**Qué muestra esta gráfica:**
- Eje X: número de usuarios conectados (10, 20 ó 40)
- Eje Y: velocidad total que entrega la antena (Mbps)
- Cada línea: un scheduler diferente
- Las barras de error: intervalo de confianza del 95%

**Lo que se observa:**
- **PF y M-LWDF** se mantienen estables en ~14 Mbps sin importar cuántos haya
- **PSS** alcanza su pico en 20 usuarios y baja un poco con 40
- **BET** cae conforme hay más usuarios — dedica más tiempo a los más lentos
- **MT** es plano pero nunca sirve a todos — monopolio de los más cercanos
- **CQA** (extensión) cae con N, comportamiento similar a BET

> 💬 **Nota del presentador:**
> El hecho de que PF no caiga con más usuarios se explica por la diversidad
> multiusuario: con más candidatos, siempre hay alguien con buena señal
> en algún bloque de frecuencia, y PF lo aprovecha.
> PSS baja con N=40 porque su mecanismo de dos niveles se satura.

---

## DIAPOSITIVA 13 · Figura 2 — Equidad con usuarios dispersos

# ¿Es justo el reparto de velocidad?

`[imagen: F2_jain_vs_N_D1.pdf]`

**Qué muestra esta gráfica:**
- Eje X: número de usuarios (10, 20, 40)
- Eje Y: índice de Jain (1 = perfecto, 0 = monopolio)
- Distribución uniforme (D1), tráfico homogéneo (T1)

**Lo que se observa:**
- **BET y CQA** en 1.0 siempre — equidad perfecta, sin excepción
- **MT** colapsa de 0.18 a 0.06 — con más usuarios, el monopolio se profundiza
- **PF** estable en ~0.75 — el "punto dulce" de la mayoría de operadores
- **TTA** mejora levemente — más candidatos mejoran su normalización

**Dato clave:** MT con 40 usuarios = solo 2-3 usuarios reciben datos, los demás en 0

> 💬 **Nota del presentador:**
> Jain = 0.06 con 40 usuarios significa que 1-2 usuarios se llevan casi todo.
> CQA logra equidad perfecta pero con throughput muy bajo (5 Mbps).
> PF en 0.75 significa que el peor usuario recibe aproximadamente la mitad
> de lo que recibe el mejor — eso es aceptable en la mayoría de escenarios.

---

## DIAPOSITIVA 14 · Figura 3 — Equidad con usuarios agrupados

# ¿Qué pasa cuando todos están en el mismo lugar?

`[imagen: F3_jain_vs_N_D2.pdf]`

**Qué muestra esta gráfica:**
- Igual que la anterior, pero con distribución **clusterizada (D2)**
- 3 grupos de usuarios: cerca (100m), a media distancia (250m), lejos (400m)

**La sorpresa:**
- **PF y M-LWDF** son casi idénticas a la figura anterior — no les afecta
- **TTA** es el único que empeora al agregar usuarios bajo D2
- **CQA y BET** mantienen su equidad perfecta en cualquier condición

**¿Por qué TTA es diferente?**
TTA normaliza por la velocidad promedio *en ese momento*, sin acumular historial.
Cuando hay usuarios lejos con mala señal, arrastran el promedio hacia abajo
y los demás reciben más recursos de los que deberían.

> 💬 **Nota del presentador:**
> Comparando F2 y F3, la conclusión es sorprendente: la distribución geográfica
> casi no importa para la mayoría de schedulers. Lo que importa es cuántos
> usuarios compiten. Esto responde H2: la carga domina sobre la geometría.

---

## DIAPOSITIVA 15 · Figura 4 — Efecto de la distribución espacial

# ¿Cuánto daña realmente agrupar a los usuarios?

`[imagen: F4_jain_D1vsD2_N20_T1.pdf]`

**Qué muestra esta gráfica:**
- Barras pareadas para cada scheduler
- Azul sólido = D1 (distribución uniforme)
- Azul rayado = D2 (distribución clusterizada)
- Fijado en N=20 usuarios, tráfico homogéneo

**La respuesta visual es clara:**
Para RR, BET, PF, M-LWDF y CQA → las barras son **idénticas**

Solo TTA muestra una diferencia visible entre D1 y D2.
MT tiene equidad pésima en ambas condiciones — ya no hay más que perder.

**Tabla de impacto real:**
| Scheduler | Cambio por distribución | Cambio por más usuarios |
|---|---|---|
| PF | 0.000 | 0.011 |
| MT | 0.020 | 0.121 |
| TTA | **0.048** | 0.047 |

> 💬 **Nota del presentador:**
> Esta figura es la evidencia visual de que H2 fue rechazada.
> Si la distribución espacial importara más, veríamos barras muy distintas
> entre D1 y D2 para todos los schedulers. Solo TTA muestra eso.
> Para MT, triplicar usuarios (N=10 a N=40) es 6 veces más dañino
> que pasar de usuarios dispersos a agrupados.

---

## DIAPOSITIVA 16 · El hallazgo que no anticipamos

# Agrupar usuarios en realidad SUBE la velocidad

**¿Cómo es posible?**

En distribución uniforme (D1):
- Hay usuarios a 400-490m de la antena con señal casi nula
- Cada bloque de frecuencia asignado a ellos entrega solo **2 bits**
- El scheduler "desperdicia" recursos en usuarios que poco pueden aprovecharlos

En distribución clusterizada (D2):
- La mayoría de usuarios están a 100m con excelente señal
- Los mismos bloques de frecuencia entregan **6-8 bits**
- El scheduler asigna los recursos donde rinden más

| Scheduler | D1 (dispersos) | D2 (agrupados) | Ganancia |
|---|---|---|---|
| PF | 14.16 Mbps | 19.36 Mbps | **+37%** |
| PSS | 14.53 Mbps | 20.30 Mbps | **+40%** |
| RR | 7.22 Mbps | 11.35 Mbps | +57% |

> 💬 **Nota del presentador:**
> PSS con D2 alcanza 20.30 Mbps — prácticamente el techo teórico
> de la celda con 10 MHz de ancho de banda.
> MT es la excepción: no cambia porque ya elegía al mejor usuario
> en D1, y en D2 sigue eligiendo a los mismos (los de C1).
> La conclusión práctica: el clustering no es el problema. El problema es la carga.

---

## DIAPOSITIVA 17 · Figura 5 — Qué pasa con tráfico mezclado

# Cuando hay video, gaming y descargas al mismo tiempo

`[imagen: F5_throughput_T1vsT2_N20_D1.pdf]`

**Qué muestra esta gráfica:**
- Barras pareadas: sólido = T1 (todos igual), rayado = T2 (mezcla de servicios)
- Fijado en N=20, distribución uniforme

**La paradoja más importante del trabajo:**

| Scheduler | Caída al pasar a T2 | ¿Por qué? |
|---|---|---|
| **M-LWDF** | **−31.2%** | Reserva recursos para flujos lentos de video/gaming |
| **PSS** | **−26.9%** | Similar, prioridad a GBR con tasas bajas |
| MT | −1.2% | Ignora completamente el tipo de tráfico |
| PF | −19.1% | No distingue, pero tampoco sobre-protege |
| **CQA** | **+17%** ⬆️ | Único que sube — comportamiento singular |

> 💬 **Nota del presentador:**
> M-LWDF hace exactamente lo que fue diseñado para hacer:
> proteger los flujos con garantía de servicio (GBR). El costo es que
> dedica recursos a flujos de video (1.5 Mbps) y gaming (64 kbps) que
> son mucho más lentos que los de best-effort (10 Mbps). El canal queda
> sub-utilizado porque nadie puede "heredar" los recursos no usados.
> MT no cae porque para él todos los paquetes son iguales — no le importa
> si es video urgente o descarga en segundo plano.

---

## DIAPOSITIVA 18 · Figura 6 — Video vs Datos normales

# ¿Quién se lleva los recursos cuando hay mezcla?

`[imagen: F6_GBR_vs_BE_N20_D1_T2.pdf]`

**Qué muestra esta gráfica:**
- Barras por scheduler, divididas en dos partes:
  - **Sólido** = throughput de flujos GBR (video + gaming garantizados)
  - **Rayado** = throughput de flujos best-effort (datos normales)

**Lo que revela:**

| Scheduler | GBR recibe | BE recibe | Filosofía |
|---|---|---|---|
| PF | 7.1 Mbps | 6.0 Mbps | "No sé la diferencia, reparto proporcional" |
| M-LWDF | 8.4 Mbps | 2.7 Mbps | "El video urgente primero, siempre" |
| PSS | 9.1 Mbps | 2.9 Mbps | "Separación total: GBR va primero" |

> 💬 **Nota del presentador:**
> Esta figura muestra la transferencia de recursos: M-LWDF y PSS
> básicamente toman velocidad de los flujos de descarga normal y se
> la dan a los flujos de video y gaming.
> La pregunta es si ese trade-off vale la pena: los flujos GBR están
> satisfechos, pero el throughput total de la celda cae.
> CQA y TBFQ no tienen datos desglosados por tipo de flujo con
> la instrumentación actual.

---

## DIAPOSITIVA 19 · H4: ¿Quién es más justo con tráfico mezclado?

# La figura más importante del trabajo

`[imagen: F8_jain_D1vsD2_N20_T2.pdf]`

**Qué muestra esta gráfica:**
- Eje X: distribución uniforme (izq.) vs clusterizada (der.)
- Eje Y: índice de Jain bajo tráfico heterogéneo (T2)
- Solo schedulers QoS-aware y extensiones

**El cruce de líneas que confirma H4:**

| Condición | PF | M-LWDF | PSS |
|---|---|---|---|
| Usuarios dispersos (D1) | 0.561 | 0.564 | 0.553 |
| Usuarios agrupados (D2) | **0.543** ↓ | **0.596** ↑ | **0.596** ↑ |

**¿Por qué sube M-LWDF con D2?**
Los usuarios de video en el grupo lejano (400m) acumulan paquetes en espera.
M-LWDF detecta esa urgencia y les da recursos aunque su señal sea mala.
PF no tiene ese mecanismo → los usuarios lejanos quedan rezagados.

> 💬 **Nota del presentador:**
> La diferencia es estadísticamente muy sólida: p < 0.001, d ≈ 5.
> Un "effect size" de d=5 es enorme — significa que el efecto es real
> y no es ruido estadístico. CQA y TBFQ quedan por debajo de PF,
> lo que demuestra que no todo scheduler QoS-aware mejora la equidad.

---

## DIAPOSITIVA 20 · Figura 7 — Latencia: ¿viola alguien los límites?

`[imagen: F7_delay_D1vsD2_N20_T2.pdf]`

**Qué muestra esta gráfica:**
- Retardo promedio en milisegundos
- Tráfico heterogéneo (T2), N=20, distribución uniforme vs clusterizada

**Los límites de QoS que deben cumplirse:**
- Gaming: ≤ 50 ms
- Video: ≤ 300 ms

**Resultado: todos cumplen con margen enorme**

| Scheduler | Delay D1 | Delay D2 |
|---|---|---|
| TBFQ | **3.013 ms** (más bajo) | 3.016 ms |
| M-LWDF | 3.214 ms | 3.251 ms |
| PF | 3.223 ms | 3.258 ms |
| CQA | 3.405 ms | 3.408 ms (más alto) |

**El cuello de botella está en el canal, no en el scheduler.**
Esto podría cambiar con N > 40 usuarios o con interferencia entre celdas.

> 💬 **Nota del presentador:**
> Este resultado significa que para el número de usuarios evaluados
> (hasta 40), la latencia no es el problema. Todos los schedulers
> están muy por debajo de los límites de QoS.
> TBFQ tiene el delay más bajo porque transmite en ráfagas compactas
> y libera el canal rápido. CQA el más alto porque gestiona recursos
> más conservadoramente para mantener la equidad.

---

## DIAPOSITIVA 21 · Los resultados inesperados

# Dos hallazgos que nadie anticipó

### Hallazgo 1: La geometría importa menos de lo esperado

Anticipábamos que agrupar usuarios en clusters destruiría la equidad.
La realidad: para 6 de 7 schedulers, **agregar más usuarios hace más daño que agruparlos**.

→ Antes de rediseñar la cobertura, revisar si la celda está saturada.

### Hallazgo 2: Los clusters mejoran la velocidad total

Tener usuarios agrupados cerca de la antena permite que los schedulers
asignen recursos donde rinden más → **PF +37%, PSS +40%** de velocidad.

→ El clustering no es el problema. El clustering *con saturación* es el problema.

> 💬 **Nota del presentador:**
> Estos dos hallazgos son los más valiosos del trabajo porque van
> contra la intuición inicial. En la práctica significan que un operador
> que detecte que sus usuarios se están agrupando geográficamente
> (por ejemplo, todos entrando a un estadio) debería preocuparse más
> por la carga total de la celda que por rediseñar la distribución de antenas.

---

## DIAPOSITIVA 22 · ¿Para qué sirve cada scheduler en el mundo real?

# Guía de elección por tipo de servicio

### eMBB — Banda ancha móvil (Netflix, videollamadas HD)
**Recomendación: PF o PSS**
- PSS alcanza 20.30 Mbps en cluster (casi el techo teórico con 10 MHz)
- PF ofrece mayor equidad (Jain=0.747) con velocidad similar
- Ventaja: PF no necesita configurar parámetros especiales

### URLLC — Latencia ultrabaja (cirugía remota, vehículos autónomos)
**Recomendación: M-LWDF**
- Escala la prioridad cuando los paquetes llevan mucho tiempo esperando
- Ofrece garantía estadística de latencia que PF no puede dar
- Costo: mayor PLR en flujos de best-effort

### mMTC — Miles de sensores IoT (edificios inteligentes, industria 4.0)
**Recomendación: BET**
- Jain > 0.999 en todas las condiciones — ningún sensor queda olvidado
- No importa si están cerca o lejos de la antena
- El throughput bajo no importa cuando la tasa por sensor es de kilobits

> 💬 **Nota del presentador:**
> Estas tres categorías (eMBB, URLLC, mMTC) son exactamente las que
> define el estándar 5G. El trabajo tiene implicaciones directas para
> cómo se configurarían los schedulers en redes 5G con "network slicing"
> — la capacidad de dividir una red en segmentos con diferentes políticas.

---

## DIAPOSITIVA 23 · La pregunta que abre el futuro

# ¿Y si el scheduler se adaptara en tiempo real?

**El problema identificado:**
- Con tráfico homogéneo → PF es el mejor
- Con tráfico mezclado y usuarios agrupados → M-LWDF y PSS son mejores
- Pero el perfil de tráfico cambia durante el día

**La propuesta:**
Un scheduler adaptativo que detecte la fracción de tráfico GBR activo
y cambie entre lógica PF y M-LWDF según un umbral:

```
Si fracción_GBR > 1/3 del tráfico Y usuarios están agrupados:
    → usar M-LWDF o PSS
Sino:
    → usar PF (más simple, sin parámetros)
```

**¿Es factible en 5G?** Sí — la información sobre tipo de flujo
ya está en la capa MAC, el cambio tomaría milisegundos.

> 💬 **Nota del presentador:**
> Esta es la tercera pregunta de trabajo futuro del paper.
> Los datos de este experimento definen bien los parámetros
> de esa decisión. Es una propuesta concreta y técnicamente viable.

---

## DIAPOSITIVA 24 · Resumen de resultados

# Lo que dicen los datos

| Pregunta | Respuesta |
|---|---|
| ¿Hay un scheduler que gane en todo? | **No.** Ninguno domina en todas las métricas |
| ¿Qué afecta más la equidad: posición o cantidad? | La **cantidad** (carga de la celda) en 6/7 casos |
| ¿Agrupar usuarios daña el rendimiento? | Al contrario: **sube el throughput** hasta +40% |
| ¿M-LWDF mejora la equidad con tráfico mezclado? | **Sí**, pero cuesta −31% de velocidad total |
| ¿La latencia es un problema? | **No** — todos están muy por debajo de los SLA |
| ¿CQA y TBFQ son mejores que M-LWDF/PSS? | **No** — su equidad bajo T2 es peor que PF |

**La respuesta a la pregunta central del proyecto:**
> *¿Es posible diseñar un scheduler que satisfaga simultáneamente
> throughput, fairness y QoS en un entorno heterogéneo?*
>
> **La respuesta es no.** Pero sí existen schedulers mejores
> para contextos específicos — y los datos identifican cuáles son.

---

## DIAPOSITIVA 25 · Conclusiones y trabajo futuro

# Lo que aprendimos y lo que sigue

### Las tres conclusiones más importantes:

**1.** PF sigue siendo la elección segura para tráfico homogéneo —
no necesita configuración y absorbe la heterogeneidad espacial.

**2.** M-LWDF y PSS ofrecen ventaja real en equidad cuando hay
mezcla de GBR + best-effort con usuarios agrupados
(*p* < 0.001, *d* ≈ 5) — pero a costo de −27 a −31% en throughput.

**3.** La carga de la celda importa más que la geometría
de los usuarios para 6 de 7 schedulers.

### Las tres preguntas abiertas:

1. ¿Cómo cambia el ranking con interferencia entre celdas?
2. ¿Qué sucede con más de 40 usuarios simultáneos?
3. ¿Es viable un scheduler que cambie entre PF y M-LWDF
   según el tráfico en tiempo real?

---

## APÉNDICE · Índice de figuras

| Diapositiva | Imagen a usar | Nombre del archivo |
|---|---|---|
| 11, 12 | Throughput vs N usuarios (D1 uniforme) | `F1_throughput_vs_N_D1.pdf` |
| 13 | Equidad (Jain) vs N usuarios (D1 uniforme) | `F2_jain_vs_N_D1.pdf` |
| 14 | Equidad (Jain) vs N usuarios (D2 clusterizada) | `F3_jain_vs_N_D2.pdf` |
| 15 | Comparación D1 vs D2 en equidad (barras) | `F4_jain_D1vsD2_N20_T1.pdf` |
| 17 | Throughput T1 vs T2 (efecto del tráfico mezclado) | `F5_throughput_T1vsT2_N20_D1.pdf` |
| 18 | Throughput GBR vs best-effort bajo T2 | `F6_GBR_vs_BE_N20_D1_T2.pdf` |
| 20 | Retardo E2E — D1 vs D2 bajo T2 | `F7_delay_D1vsD2_N20_T2.pdf` |
| 19 | Equidad (Jain) bajo T2 — D1 vs D2 (figura principal H4) | `F8_jain_D1vsD2_N20_T2.pdf` |

---

## APÉNDICE · Glosario rápido

| Término | Significado simple |
|---|---|
| **LTE** | Estándar de red celular 4G (el que usa tu teléfono) |
| **OFDMA** | Técnica de LTE que divide el espectro en bloques de frecuencia |
| **RB (Resource Block)** | El bloque mínimo de frecuencia que se asigna a un usuario |
| **TTI** | Cada 1 ms — la frecuencia con que el scheduler decide |
| **CQI** | Reporte de calidad del canal que manda el teléfono a la antena |
| **Scheduler** | El algoritmo que decide a quién darle cada RB en cada TTI |
| **Throughput** | Velocidad de datos efectiva (Mbps) |
| **Jain Index** | Medida de equidad del reparto (0=monopolio, 1=perfectamente igualitario) |
| **GBR** | Guaranteed Bit Rate — tasa mínima garantizada (para video, gaming) |
| **Best-effort** | Sin garantía — recibe lo que quede |
| **Fading** | Variación aleatoria de la calidad del canal por reflexiones y movimiento |
| **QoS** | Quality of Service — garantías de velocidad y latencia por tipo de servicio |
| **ns-3** | Simulador de redes de código abierto usado para el experimento |

---

*Fin del documento · Alfonso García · BUAP 2026*
