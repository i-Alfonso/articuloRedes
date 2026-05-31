**Evaluación de paradigmas de ****scheduling**** en sistemas OFDMA bajo tráfico y condiciones espaciales heterogéneas**
Las redes inalámbricas modernas basadas en OFDMA, como LTE y 5G, requieren mecanismos de asignación dinámica de recursos capaces de operar bajo condiciones heterogéneas de canal, movilidad y tráfico. El scheduler de la estación base debe equilibrar simultáneamente múltiples objetivos:
maximizar throughput,
mantener fairness,
cumplir requisitos de QoS,
minimizar retardo,
y adaptarse dinámicamente a variaciones del canal.
Sin embargo, estos objetivos son inherentemente conflictivos y los algoritmos clásicos presentan limitaciones importantes cuando la red opera bajo escenarios heterogéneos.
**Problema de investigación**
La asignación de recursos radio en sistemas OFDMA depende fuertemente de: la heterogeneidad espacial de los usuarios, las condiciones variables del canal y la coexistencia de tráfico con distintos requisitos de QoS.
Bajo estas condiciones, los schedulers clásicos presentan trade-offs significativos entre eficiencia espectral, fairness y latencia.
La pregunta central es:
¿Cómo afectan la heterogeneidad espacial y la heterogeneidad del tráfico al desempeño estructural de distintos paradigmas de scheduling en sistemas OFDMA?
El objetivo es evaluar el comportamiento de distintos paradigmas de scheduling en sistemas OFDMA bajo condiciones heterogéneas de canal y tráfico, identificando sus ventajas, limitaciones y trade-offs fundamentales.
**3. Hipótesis**
Se plantean las siguientes hipótesis:
H1: Los schedulers orientados a throughput presentan degradación no lineal de fairness bajo distribuciones espaciales heterogéneas.
H2: La heterogeneidad espacial tiene mayor impacto sobre fairness que el número total de usuarios.
H3: Los schedulers balanceados pierden eficiencia espectral cuando coexisten múltiples tipos de tráfico con distintos requisitos de QoS.
H4: Los schedulers híbridos mejoran el compromiso fairness-throughput, aunque introducen mayor complejidad de coordinación.
**4. Metodología**
Utilizará ns-3 con:
- módulo LTE (LTE no es el objetivo del estudio, solo el entorno controlado para analizar problemas fundamentales de asignación de recursos en redes inalámbricas modernas.)
EPC habilitado
tráfico IP extremo a extremo
Configuración base:
1 eNodeB
N UEs (variable)
fading Rayleigh
path loss log-distance
canal heterogéneo
CQI dinámico
**Variables independientes**
Número de usuarios
Distribución espacial:
uniforme
clusterizada
Tipo de tráfico:
homogéneo (best-effort)
heterogéneo:
video
best-effort
baja latencia
Condiciones de canal:
homogéneas
heterogéneas (distancia al eNodeB)
**Algoritmos evaluados**
Revisar la literatura para identificar si existe una taxonomía de técnicas de scheduling, si existiera entonces seleccionar uno como representativo del enfoque. Si no existiera, entonces proponer una y determinar algoritmos representativos. 
Comparar los tres clásicos (RR, PF y MT) contra los algoritmos representativos de distintas categorías de scheduling identificadas en la literatura
**Métricas**
- Throughput total
- Throughput por usuario
- Retardo
- Fairness (Índice de Jain)

**Diseño experimental**
Cada configuración debe ejecutarse múltiples veces (≥20) para obtener:
media
desviación estándar
intervalos de confianza
Se debe mantener constante:
topología base
parámetros de simulación
modelo de canal
**Resultados esperados**
La discusión deberá abordar:
trade-off throughput vs fairness,
impacto de la heterogeneidad espacial,
impacto del tráfico heterogéneo,
implicaciones para eMBB,
implicaciones para URLLC,
implicaciones para mMTC,
limitaciones estructurales de los paradigmas de scheduling.
**Conclusión**
Las conclusiones deberán centrarse en:
- la naturaleza multiobjetivo del scheduling en sistemas OFDMA,
- las limitaciones de los algoritmos clásicos bajo escenarios heterogéneos,
- la necesidad de mecanismos adaptativos,
- y la viabilidad de enfoques híbridos.
Y responder de acuerdo con lo analizado: ¿Es posible diseñar un scheduler que satisfaga simultáneamente throughput, fairness y QoS en un entorno heterogéneo?

**12. Producto final**
El estudiante deberá entregar:
reporte en formato tipo paper, siguiendo el estándar: Introducción, Marco teórico, Estado del arte, Métodos y herramientas, Resultados, Discusión, Conclusiones, Bibliografía.  
Dentro de los resultados: gráficas comparativas, análisis estadístico.
discusión crítica y evaluación de los algoritmos implementados.
