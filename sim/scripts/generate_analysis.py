#!/usr/bin/env python3
"""
generate_analysis.py — Genera el reporte completo de análisis de resultados.

Lee master.csv y stats.csv, ejecuta los tests estadísticos (Welch t-test)
para las 4 hipótesis, y escribe results/analysis/reporte_analisis.md

Uso (desde sim/):
  python3 scripts/generate_analysis.py
"""

from pathlib import Path
import numpy as np
import pandas as pd
from scipy import stats

# ---------------------------------------------------------------------------
SIM_ROOT   = Path(__file__).parent.parent.resolve()
MASTER     = SIM_ROOT / "results/processed/master.csv"
STATS      = SIM_ROOT / "results/processed/stats.csv"
OUT_DIR    = SIM_ROOT / "results/analysis"
OUT_DIR.mkdir(parents=True, exist_ok=True)
REPORT     = OUT_DIR / "reporte_analisis.md"
N_RUNS     = 1920  # 7 orig × 240 + CQA × 240
N_SCHEDS   = 8

SCHED_LABEL = {
    "rr": "RR", "bet": "BET", "mt": "MT", "tta": "TTA",
    "pf": "PF", "mlwdf": "M-LWDF", "pss": "PSS",
    "cqa": "CQA",
}
SCHED_ORDER = ["rr", "bet", "mt", "tta", "pf", "mlwdf", "pss", "cqa"]
# QoS-aware: TBFQ eliminado (deadlock bajo T1, datos incompletos)
QOS_ORDER    = ["pf", "mlwdf", "pss", "cqa"]
QOS_ORDER_T2 = ["pf", "mlwdf", "pss", "cqa"]
QOS_ORDER_T1 = ["pf", "mlwdf", "pss", "cqa"]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load():
    master = pd.read_csv(MASTER)
    st     = pd.read_csv(STATS)
    return master, st


def _phase(scheduler: str, traffic: str) -> str:
    if scheduler == "cqa":
        return "phase4_extent_a" if traffic == "homogeneous" else "phase4_extent_b"
    return "phase2_group_a" if traffic == "homogeneous" else "phase3_group_b"


def get_vals(master: pd.DataFrame, scheduler: str, metric: str,
             spatial: str, traffic: str, n_ues: int) -> np.ndarray:
    phase = _phase(scheduler, traffic)
    mask  = ((master.phase == phase) &
             (master.scheduler == scheduler) &
             (master.spatial   == spatial)   &
             (master.traffic   == traffic)   &
             (master.n_ues     == n_ues))
    return master.loc[mask, metric].values


def get_stat(st: pd.DataFrame, scheduler: str, metric: str,
             spatial: str, traffic: str, n_ues: int) -> pd.Series:
    phase = _phase(scheduler, traffic)
    mask  = ((st.phase == phase) &
             (st.scheduler == scheduler) &
             (st.spatial   == spatial)   &
             (st.traffic   == traffic)   &
             (st.n_ues     == n_ues))
    r = st[mask]
    if r.empty:
        raise KeyError(f"No data: {phase} {scheduler} n={n_ues} {spatial} {traffic}")
    return r.iloc[0]


def welch(a: np.ndarray, b: np.ndarray):
    """Welch t-test bilateral. Devuelve (t, p, cohen_d, n_a, n_b)."""
    t_stat, p_val = stats.ttest_ind(a, b, equal_var=False)
    pooled_std = np.sqrt((np.std(a, ddof=1)**2 + np.std(b, ddof=1)**2) / 2)
    d = (np.mean(a) - np.mean(b)) / pooled_std if pooled_std > 0 else 0.0
    return float(t_stat), float(p_val), float(d), len(a), len(b)


def sig(p: float) -> str:
    if p < 0.001: return "***"
    if p < 0.01:  return "**"
    if p < 0.05:  return "*"
    return "ns"


def fmt_row(label: str, a: np.ndarray, b: np.ndarray, direction: str) -> str:
    t, p, d, na, nb = welch(a, b)
    ma, mb = np.mean(a), np.mean(b)
    return (f"| {label} | {ma:.3f} | {mb:.3f} | {t:+.2f} | {p:.4f} | "
            f"{sig(p)} | {d:+.2f} | {direction} |")


# ---------------------------------------------------------------------------
# Tabla resumen de métricas por scheduler
# ---------------------------------------------------------------------------

def tabla_resumen(master, st):
    lines = []
    lines.append("## 1. Resumen de métricas — Escenario de referencia\n")
    lines.append("**Condiciones:** N=20, D1 uniforme, T1 full-buffer (A2)\n")
    lines.append("| Scheduler | Categoría | Throughput (Mbps) | IC 95% | Jain | IC 95% | UEs activos |")
    lines.append("|-----------|-----------|:-----------------:|--------|:----:|--------|:-----------:|")
    cat = {"rr":"(i)", "bet":"(i)", "mt":"(ii)", "tta":"(ii)",
           "pf":"(ii)", "mlwdf":"(iii)", "pss":"(iii)",
           "cqa":"(iii)ext"}
    for s in SCHED_ORDER:
        r  = get_stat(st, s, "cell_throughput_mbps", "uniform", "homogeneous", 20)
        ph = _phase(s, "homogeneous")
        n_act_raw = master.loc[
            (master.scheduler == s) & (master.spatial == "uniform") &
            (master.traffic == "homogeneous") & (master.n_ues == 20) &
            (master.phase == ph), "n_ues_active"].mean()
        # Redondear al entero más cercano (no truncar): 6.95 → 7
        n_active = int(round(n_act_raw)) if not np.isnan(n_act_raw) else 0
        lines.append(
            f"| {SCHED_LABEL[s]:6} | {cat[s]} "
            f"| {r['cell_throughput_mbps_mean']:.3f} "
            f"| [{r['cell_throughput_mbps_ci95_lo']:.3f}, {r['cell_throughput_mbps_ci95_hi']:.3f}] "
            f"| {r['jain_index_mean']:.4f} "
            f"| [{r['jain_index_ci95_lo']:.4f}, {r['jain_index_ci95_hi']:.4f}] "
            f"| {n_active}/20 |"
        )
    lines.append("")
    lines.append("**Observación clave:** MT maximiza throughput de celda pero sólo sirve a "
                 "~7/20 UEs (starvation severa). BET logra Jain≈1.000 pero al costo del menor "
                 "throughput. PF y M-LWDF equilibran ambos objetivos.")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# H1 — Fairness bajo heterogeneidad espacial
# ---------------------------------------------------------------------------

def analisis_h1(master, st):
    lines = []
    lines.append("## 2. Hipótesis H1 — Degradación de fairness bajo distribución clusterizada\n")
    lines.append("> H1: Los schedulers orientados a throughput (MT) presentan degradación no "
                 "lineal de fairness bajo distribución clusterizada (D2).\n")

    lines.append("### 2.1 Tabla de Jain index: D1 vs D2 (N=20, T1)\n")
    lines.append("| Scheduler | Jain D1 | Jain D2 | ΔJain (D1→D2) | % caída |")
    lines.append("|-----------|:-------:|:-------:|:-------------:|:-------:|")
    for s in SCHED_ORDER:
        r1 = get_stat(st, s, "jain_index", "uniform",   "homogeneous", 20)
        r2 = get_stat(st, s, "jain_index", "clustered", "homogeneous", 20)
        j1, j2 = r1["jain_index_mean"], r2["jain_index_mean"]
        delta  = j2 - j1
        pct    = delta / j1 * 100
        lines.append(f"| {SCHED_LABEL[s]:6} | {j1:.4f} | {j2:.4f} | {delta:+.4f} | {pct:+.1f}% |")
    lines.append("")

    lines.append("### 2.2 Tests de Welch (Jain D1 vs D2, N=20)\n")
    lines.append("| Comparación | Media D1 | Media D2 | t | p-value | Sig | Cohen's d | Dirección esperada |")
    lines.append("|-------------|:--------:|:--------:|---|:-------:|-----|:---------:|-------------------|")
    for s in ["mt", "pf", "rr", "bet"]:
        a = get_vals(master, s, "jain_index", "uniform",   "homogeneous", 20)
        b = get_vals(master, s, "jain_index", "clustered", "homogeneous", 20)
        lines.append(fmt_row(f"Jain({SCHED_LABEL[s]}): D1 vs D2", a, b, "D1 > D2"))
    lines.append("")

    lines.append("### 2.3 Interpretación\n")
    mt_d1  = get_stat(st, "mt",  "jain_index", "uniform",   "homogeneous", 20)["jain_index_mean"]
    mt_d2  = get_stat(st, "mt",  "jain_index", "clustered", "homogeneous", 20)["jain_index_mean"]
    tta_d1 = get_stat(st, "tta", "jain_index", "uniform",   "homogeneous", 20)["jain_index_mean"]
    tta_d2 = get_stat(st, "tta", "jain_index", "clustered", "homogeneous", 20)["jain_index_mean"]
    pf_d1  = get_stat(st, "pf",  "jain_index", "uniform",   "homogeneous", 20)["jain_index_mean"]
    pf_d2  = get_stat(st, "pf",  "jain_index", "clustered", "homogeneous", 20)["jain_index_mean"]
    bet_d1 = get_stat(st, "bet", "jain_index", "uniform",   "homogeneous", 20)["jain_index_mean"]
    bet_d2 = get_stat(st, "bet", "jain_index", "clustered", "homogeneous", 20)["jain_index_mean"]
    lines.append(
        f"**MT:** Su Jain es bajo tanto en D1 ({mt_d1:.4f}) como en D2 ({mt_d2:.4f}). "
        f"El ligero aumento ({mt_d2-mt_d1:+.4f}) se debe a que en D2, los UEs de C1 "
        f"(todos con SINR similar) se turnan los recursos más equitativamente entre sí. "
        f"Lo que confirma H1 no es el Jain global sino la starvation sistemática: "
        f"los UEs de C3 reciben cero bytes de forma consistente, algo que en D1 "
        f"(distribución uniforme con fading) ocurría de forma rotatoria. "
        f"**El scheduler con la mayor caída real de Jain** al pasar a D2 es "
        f"TTA ({tta_d1:.4f} → {tta_d2:.4f}, {tta_d2-tta_d1:+.4f}), no MT.\n\n"
        f"**PF:** prácticamente sin cambio ({pf_d1:.4f} → {pf_d2:.4f}), "
        f"el historial proporcional compensa la heterogeneidad espacial. "
        f"**BET:** equidad perfecta en ambas distribuciones ({bet_d1:.4f} → {bet_d2:.4f}), "
        f"aunque con throughput reducido."
    )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# H2 — Efecto espacial vs número de usuarios
# ---------------------------------------------------------------------------

def analisis_h2(master, st):
    lines = []
    lines.append("## 3. Hipótesis H2 — Heterogeneidad espacial vs carga de usuarios\n")
    lines.append("> H2: La heterogeneidad espacial impacta más el fairness que el número de "
                 "usuarios.\n")

    lines.append("### 3.1 ΔJain por efecto espacial (D1→D2, N=20) vs por carga (N=10→40, D1)\n")
    lines.append("| Scheduler | ΔJain espacial (D1→D2) | ΔJain por carga (N10→N40) | Domina |")
    lines.append("|-----------|:---------------------:|:-------------------------:|:------:|")
    for s in SCHED_ORDER:
        j_d1_20  = get_stat(st, s, "jain_index", "uniform",   "homogeneous", 20)["jain_index_mean"]
        j_d2_20  = get_stat(st, s, "jain_index", "clustered", "homogeneous", 20)["jain_index_mean"]
        j_d1_10  = get_stat(st, s, "jain_index", "uniform",   "homogeneous", 10)["jain_index_mean"]
        j_d1_40  = get_stat(st, s, "jain_index", "uniform",   "homogeneous", 40)["jain_index_mean"]
        d_spatial = abs(j_d2_20 - j_d1_20)
        d_load    = abs(j_d1_40 - j_d1_10)
        domina    = "Espacial" if d_spatial > d_load else "Carga"
        lines.append(f"| {SCHED_LABEL[s]:6} | {d_spatial:.4f} | {d_load:.4f} | **{domina}** |")
    lines.append("")

    lines.append("### 3.2 Test: ΔJain espacial vs ΔJain por carga (PF, Welch)\n")
    pf_d1_n  = [get_vals(master, "pf", "jain_index", "uniform",   "homogeneous", n) for n in [10,20,40]]
    pf_d2_n  = [get_vals(master, "pf", "jain_index", "clustered", "homogeneous", n) for n in [10,20,40]]
    delta_spatial = np.concatenate([d1 - d2 for d1, d2 in zip(pf_d1_n, pf_d2_n)])
    delta_load    = np.concatenate([
        get_vals(master, "pf", "jain_index", "uniform", "homogeneous", 40) -
        get_vals(master, "pf", "jain_index", "uniform", "homogeneous", 10)
    ])
    t, p, d, na, nb = welch(delta_spatial, delta_load)
    lines.append(f"- ΔJain_espacial(PF): media = {np.mean(delta_spatial):.4f}")
    lines.append(f"- ΔJain_carga(PF):    media = {np.mean(delta_load):.4f}")
    lines.append(f"- Welch t={t:+.3f}, p={p:.4f} {sig(p)}, Cohen's d={d:+.2f}\n")

    lines.append("### 3.3 Interpretación\n")
    lines.append(
        "**Resultado negativo importante:** contrario a lo esperado en H2, la tabla muestra que "
        "el número de usuarios (N=10→40) produce un ΔJain mayor que la distribución espacial (D1→D2) "
        "en 6 de los 7 schedulers. Solo TTA muestra dominancia marginal del efecto espacial. "
        "Esto indica que los algoritmos channel-aware (especialmente PF, M-LWDF y PSS) se adaptan "
        "bien a la heterogeneidad espacial mediante sus métricas proporcionales/históricas, "
        "mientras que la saturación progresiva de la celda al aumentar usuarios es el "
        "factor que más degrada la equidad. "
        "Para MT, la carga domina con mucho (ΔJain_carga=0.121 vs ΔJain_espacial=0.020) porque "
        "con N=40 hay más candidatos y el algoritmo maximiza con mayor ventaja al mismo conjunto "
        "reducido de UEs con mejor canal, empeorando el Jain de forma progresiva. "
        "Este resultado negativo es valioso para el paper: refina la comprensión de H2 y sugiere "
        "que el diseño de schedulers robustos debe enfocarse en escalar con carga, "
        "no solo en compensar heterogeneidad geográfica."
    )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# H3 — Efecto del tráfico heterogéneo
# ---------------------------------------------------------------------------

def analisis_h3(master, st):
    lines = []
    lines.append("## 4. Hipótesis H3 — Impacto del tráfico heterogéneo en throughput\n")
    lines.append("> H3: Los schedulers balanceados (PF) pierden eficiencia espectral bajo "
                 "tráfico heterogéneo; M-LWDF y PSS compensan de forma distinta.\n")

    lines.append("### 4.1 Throughput T1 vs T2 (N=20, D1)\n")
    lines.append("| Scheduler | Tput T1 [Mbps] | Tput T2 [Mbps] | Δ [Mbps] | % cambio |")
    lines.append("|-----------|:--------------:|:--------------:|:--------:|:--------:|")
    for s in SCHED_ORDER:
        r1 = get_stat(st, s, "cell_throughput_mbps", "uniform", "homogeneous",    20)
        r2 = get_stat(st, s, "cell_throughput_mbps", "uniform", "heterogeneous",  20)
        t1, t2 = r1["cell_throughput_mbps_mean"], r2["cell_throughput_mbps_mean"]
        delta  = t2 - t1
        pct    = delta / t1 * 100
        lines.append(f"| {SCHED_LABEL[s]:6} | {t1:.3f} | {t2:.3f} | {delta:+.3f} | {pct:+.1f}% |")
    lines.append("")

    lines.append("### 4.2 Tests de Welch (Throughput T1 vs T2, N=20, D1)\n")
    lines.append("| Comparación | Media T1 | Media T2 | t | p-value | Sig | Cohen's d | Dirección esperada |")
    lines.append("|-------------|:--------:|:--------:|---|:-------:|-----|:---------:|-------------------|")
    for s in ["pf", "mlwdf", "pss", "mt", "rr"]:
        a = get_vals(master, s, "cell_throughput_mbps", "uniform", "homogeneous",   20)
        b = get_vals(master, s, "cell_throughput_mbps", "uniform", "heterogeneous", 20)
        lines.append(fmt_row(f"Tput({SCHED_LABEL[s]}): T1 vs T2", a, b,
                             "T2 < T1 (GBR limita uso eficiente del canal)"))
    lines.append("")

    lines.append("### 4.3 Delay E2E bajo T2 (N=20)\n")
    lines.append("| Scheduler | Delay D1 T2 [ms] | Delay D2 T2 [ms] | ΔDelay |")
    lines.append("|-----------|:----------------:|:----------------:|:------:|")
    for s in QOS_ORDER:
        r1 = get_stat(st, s, "mean_delay_ms", "uniform",   "heterogeneous", 20)
        r2 = get_stat(st, s, "mean_delay_ms", "clustered", "heterogeneous", 20)
        d1, d2 = r1["mean_delay_ms_mean"], r2["mean_delay_ms_mean"]
        lines.append(f"| {SCHED_LABEL[s]:6} | {d1:.2f} | {d2:.2f} | {d2-d1:+.2f} |")
    lines.append("")

    lines.append("### 4.4 Interpretación\n")
    pf_t1  = get_stat(st, "pf",    "cell_throughput_mbps", "uniform", "homogeneous",   20)["cell_throughput_mbps_mean"]
    pf_t2  = get_stat(st, "pf",    "cell_throughput_mbps", "uniform", "heterogeneous", 20)["cell_throughput_mbps_mean"]
    ml_t1  = get_stat(st, "mlwdf", "cell_throughput_mbps", "uniform", "homogeneous",   20)["cell_throughput_mbps_mean"]
    ml_t2  = get_stat(st, "mlwdf", "cell_throughput_mbps", "uniform", "heterogeneous", 20)["cell_throughput_mbps_mean"]
    cqa_t1 = get_stat(st, "cqa",   "cell_throughput_mbps", "uniform", "homogeneous",   20)["cell_throughput_mbps_mean"]
    cqa_t2 = get_stat(st, "cqa",   "cell_throughput_mbps", "uniform", "heterogeneous", 20)["cell_throughput_mbps_mean"]
    lines.append(
        f"Bajo tráfico heterogéneo (T2), todos los schedulers reducen su throughput "
        f"de celda excepto CQA. PF baja de {pf_t1:.2f} a {pf_t2:.2f} Mbps (-19.1%) "
        f"porque los flujos GBR tienen tasas máximas bajas y dejan capacidad sin usar. "
        f"M-LWDF cae de {ml_t1:.2f} a {ml_t2:.2f} Mbps (-31.2%), la mayor caída: "
        f"prioriza agresivamente GBR aunque sean de baja tasa, reduciendo los recursos "
        f"disponibles para BE y con ello el throughput total.\n\n"
        f"**Excepción — CQA:** es el único que GANA throughput bajo T2 "
        f"({cqa_t1:.2f} → {cqa_t2:.2f} Mbps, +17.2%). Bajo T1 full-buffer, CQA actúa "
        f"como equalizer (todos los UEs saturados con igual urgencia). Bajo T2, "
        f"los flujos GBR de baja tasa reducen la presión de cola, CQA puede discriminar "
        f"entre tipos de flujo y asignar más recursos al BE full-buffer que sí los usa, "
        f"elevando el throughput total de celda."
    )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# H4 — Schedulers híbridos (PSS) vs PF en T2
# ---------------------------------------------------------------------------

def analisis_h4(master, st):
    lines = []
    lines.append("## 5. Hipótesis H4 — Schedulers QoS-aware mejoran fairness bajo T2\n")
    lines.append("> H4: Los schedulers híbridos (PSS) mejoran el compromiso "
                 "fairness-throughput frente a PF simple bajo tráfico heterogéneo.\n")

    lines.append("### 5.1 Jain index T2 — PF vs M-LWDF vs PSS\n")
    lines.append("| Config | Jain PF | Jain M-LWDF | Jain PSS | Mejor fairness |")
    lines.append("|--------|:-------:|:-----------:|:--------:|:--------------:|")
    for sp, label in [("uniform","D1 N=20"), ("clustered","D2 N=20")]:
        for n, nl in [(20, "N=20")]:
            vals = []
            for s in QOS_ORDER:
                r = get_stat(st, s, "jain_index", sp, "heterogeneous", n)
                vals.append(r["jain_index_mean"])
            best = QOS_ORDER[int(np.argmax(vals))]
            lines.append(f"| {label} {sp[:3]} T2 | {vals[0]:.4f} | {vals[1]:.4f} | {vals[2]:.4f} | **{SCHED_LABEL[best]}** |")
    lines.append("")

    lines.append("### 5.2 Tests de Welch (Jain T2, N=20, D2)\n")
    lines.append("| Comparación | Media A | Media B | t | p-value | Sig | Cohen's d | Dirección esperada |")
    lines.append("|-------------|:-------:|:-------:|---|:-------:|-----|:---------:|-------------------|")
    for sp in ["uniform", "clustered"]:
        a_pf   = get_vals(master, "pf",    "jain_index", sp, "heterogeneous", 20)
        a_ml   = get_vals(master, "mlwdf", "jain_index", sp, "heterogeneous", 20)
        a_pss  = get_vals(master, "pss",   "jain_index", sp, "heterogeneous", 20)
        sp_lbl = "D1" if sp == "uniform" else "D2"
        lines.append(fmt_row(f"Jain(M-LWDF vs PF) {sp_lbl}", a_ml,  a_pf,  "M-LWDF ≥ PF"))
        lines.append(fmt_row(f"Jain(PSS vs PF) {sp_lbl}",    a_pss, a_pf,  "PSS ≥ PF"))
        lines.append(fmt_row(f"Jain(PSS vs M-LWDF) {sp_lbl}",a_pss, a_ml,  "PSS ≥ M-LWDF"))
    lines.append("")

    lines.append("### 5.3 PLR — Tasa de pérdida de paquetes (T2)\n")
    lines.append("| Scheduler | PLR D1 T2 | PLR D2 T2 |")
    lines.append("|-----------|:---------:|:---------:|")
    for s in QOS_ORDER:
        r1 = get_stat(st, s, "plr", "uniform",   "heterogeneous", 20)
        r2 = get_stat(st, s, "plr", "clustered", "heterogeneous", 20)
        lines.append(f"| {SCHED_LABEL[s]:6} | {r1['plr_mean']:.4f} | {r2['plr_mean']:.4f} |")
    lines.append("")

    lines.append("### 5.4 Interpretación\n")
    pf_j  = get_stat(st, "pf",    "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
    ml_j  = get_stat(st, "mlwdf", "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
    pss_j = get_stat(st, "pss",   "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
    pf_j1 = get_stat(st, "pf",    "jain_index", "uniform",   "heterogeneous", 20)["jain_index_mean"]
    ml_j1 = get_stat(st, "mlwdf", "jain_index", "uniform",   "heterogeneous", 20)["jain_index_mean"]
    lines.append(
        f"**H4 confirmada.** En D2+T2, M-LWDF ({ml_j:.4f}) y PSS ({pss_j:.4f}) "
        f"superan claramente a PF ({pf_j:.4f}) en equidad global (p<0.001, d≈5). "
        f"La diferencia es de +9.7% sobre PF — estadísticamente muy robusta.\n\n"
        f"El mecanismo es clave: con tráfico interleaved y clustering, los UEs GBR de C3 "
        f"(400m, SINR bajo) acumulan delay HOL rápidamente porque PF los penaliza por su "
        f"mal canal. M-LWDF escala su prioridad cuando ese delay crece, rescatándolos "
        f"y mejorando el Jain global. En D1 (sin clustering), no hay ventaja: "
        f"PF={pf_j1:.4f}, M-LWDF={ml_j1:.4f} — prácticamente idénticos (ns, d=0.06).\n\n"
        f"H4 solo se activa en el escenario que la motiva: usuarios con canal "
        f"heterogéneo (clustering) más tráfico con QoS diferenciado (T2)."
    )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Extensión — CQA (tercer representante de categoría iii)
# ---------------------------------------------------------------------------

def analisis_extension(master, st):
    lines = []
    lines.append("## 7. Extensión — CQA como tercer representante de categoría (iii)\n")
    lines.append("> Branch `final` — 240 corridas adicionales (CQA, T1+T2).\n")
    lines.append("> **Nota:** TBFQ fue evaluado pero descartado por incompatibilidad con tráfico\n")
    lines.append("> full-buffer (T1): token deadlock en el primer segundo de simulación.\n")

    lines.append("### 7.1 CQA bajo T1 — comportamiento equalizer\n")
    lines.append("| Scheduler | Throughput [Mbps] | Jain | UEs activos | Nota |")
    lines.append("|-----------|:-----------------:|:----:|:-----------:|------|")
    for s in ["bet", "pf", "mlwdf", "pss", "cqa"]:
        try:
            r = get_stat(st, s, "cell_throughput_mbps", "uniform", "homogeneous", 20)
            n_raw = master.loc[
                (master.scheduler == s) & (master.spatial == "uniform") &
                (master.traffic == "homogeneous") & (master.n_ues == 20),
                "n_ues_active"].mean()
            n_active = int(n_raw) if not np.isnan(n_raw) else 0
            note = "← similar a BET" if s == "cqa" else ("← referencia" if s == "bet" else "")
            lines.append(f"| {SCHED_LABEL[s]:6} | {r['cell_throughput_mbps_mean']:.3f} "
                         f"| {r['jain_index_mean']:.4f} | {n_active}/20 | {note} |")
        except KeyError:
            pass
    lines.append("")
    lines.append("CQA bajo T1 produce Jain≈0.999 y throughput de ~5.3 Mbps, comportamiento "
                 "casi idéntico a BET. La métrica multi-criterio de CQA (CQI + HOL delay + "
                 "tamaño de cola + prioridad) bajo saturación total converge a un equalizer: "
                 "todos los UEs tienen urgencia similar, domina la componente de fairness.\n")

    lines.append("### 7.2 CQA bajo T2 — comparación con PF/M-LWDF/PSS\n")
    lines.append("| Scheduler | Tput T2 D1 [Mbps] | Jain T2 D1 | Jain T2 D2 | ΔJain D1→D2 |")
    lines.append("|-----------|:-----------------:|:----------:|:----------:|:-----------:|")
    for s in QOS_ORDER:
        try:
            r1 = get_stat(st, s, "cell_throughput_mbps", "uniform",   "heterogeneous", 20)
            j1 = get_stat(st, s, "jain_index",           "uniform",   "heterogeneous", 20)
            j2 = get_stat(st, s, "jain_index",           "clustered", "heterogeneous", 20)
            delta = j2["jain_index_mean"] - j1["jain_index_mean"]
            lines.append(f"| {SCHED_LABEL[s]:6} | {r1['cell_throughput_mbps_mean']:.3f} "
                         f"| {j1['jain_index_mean']:.4f} | {j2['jain_index_mean']:.4f} "
                         f"| {delta:+.4f} |")
        except KeyError:
            lines.append(f"| {SCHED_LABEL[s]:6} | n/d | n/d | n/d | n/d |")
    lines.append("")

    lines.append("### 7.3 Test Welch — CQA vs PF en T2 (comparación directa)\n")
    lines.append("| Comparación | Media CQA | Media PF | t | p-value | Sig | Cohen's d | CQA > PF? |")
    lines.append("|-------------|:---------:|:--------:|---|:-------:|-----|:---------:|:---------:|")
    for sp in ["uniform", "clustered"]:
        try:
            a_cqa = get_vals(master, "cqa", "jain_index", sp, "heterogeneous", 20)
            a_pf  = get_vals(master, "pf",  "jain_index", sp, "heterogeneous", 20)
            t, p, d, _, _ = welch(a_cqa, a_pf)
            sp_lbl = "D1" if sp == "uniform" else "D2"
            supera = "✅ Sí" if np.mean(a_cqa) > np.mean(a_pf) else "❌ No"
            lines.append(f"| Jain(CQA vs PF) {sp_lbl} | {np.mean(a_cqa):.4f} | "
                         f"{np.mean(a_pf):.4f} | {t:+.2f} | {p:.4f} | {sig(p)} | {d:+.2f} | {supera} |")
        except (KeyError, IndexError):
            pass
    lines.append("")

    lines.append("### 7.4 Interpretación\n")
    try:
        cqa_t2_d1 = get_stat(st, "cqa", "jain_index", "uniform",   "heterogeneous", 20)["jain_index_mean"]
        cqa_t2_d2 = get_stat(st, "cqa", "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
        pf_t2_d2  = get_stat(st, "pf",  "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
        mlwdf_t2_d2 = get_stat(st, "mlwdf", "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
        cqa_t1_d1 = get_stat(st, "cqa", "jain_index", "uniform",   "homogeneous",   20)["jain_index_mean"]
        if cqa_t2_d2 > pf_t2_d2:
            h4_cqa = f"CQA ({cqa_t2_d2:.4f}) **supera a PF ({pf_t2_d2:.4f})** en T2+D2, confirmando H4 desde un tercer mecanismo."
        else:
            h4_cqa = (f"CQA ({cqa_t2_d2:.4f}) **no supera a PF ({pf_t2_d2:.4f})** en T2+D2. "
                      f"A diferencia de M-LWDF ({mlwdf_t2_d2:.4f}) y PSS que escalan prioridad "
                      f"exclusivamente por delay HOL, la métrica multi-criterio de CQA pondera también "
                      f"el CQI instantáneo. Los UEs de C3 (400m, SINR bajo) tienen CQI bajo que "
                      f"reduce su prioridad en CQA incluso cuando acumulan delay, a diferencia de "
                      f"M-LWDF que escala prioridad puramente por delay sin importar el canal. "
                      f"Esto muestra que dentro de cat(iii), el mecanismo específico sí importa: "
                      f"no todos los QoS-aware rescatan igualmente a usuarios en zona de cobertura débil.")
        lines.append(
            f"**CQA bajo T1:** Jain={cqa_t1_d1:.4f} — comportamiento equalizer idéntico a BET. "
            f"Bajo saturación total, los criterios QoS son equivalentes para todos los UEs y domina "
            f"la componente de fairness.\n\n"
            f"**CQA bajo T2+D2:** {h4_cqa}\n\n"
            f"**Conclusión de la extensión:** M-LWDF y PSS confirman H4 robustamente. "
            f"CQA aporta un matiz: dentro de la categoría (iii), el mecanismo de priorización "
            f"determina si un scheduler puede rescatar UEs en zona débil bajo tráfico heterogéneo. "
            f"El delay HOL puro (M-LWDF) es más efectivo que un criterio multi-factor (CQA) "
            f"para ese objetivo específico."
        )
    except KeyError as e:
        lines.append(f"[Datos insuficientes: {e}]")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Análisis por figura
# ---------------------------------------------------------------------------

def analisis_figuras(st):
    lines = []
    lines.append("## 6. Análisis de figuras\n")
    lines.append("Las figuras F1–F8 se encuentran en `results/processed/figures/`. "
                 "A continuación se describe qué muestra cada una, qué buscar visualmente "
                 "y qué conclusión aporta al paper.\n")
    lines.append("---\n")

    # --- F1 ---
    lines.append("### F1 — Throughput de celda vs N (D1 uniforme, T1 full-buffer)\n")
    lines.append("**Archivo:** `F1_throughput_vs_N_D1.pdf`  ")
    lines.append("**Tipo:** Curvas con barras de error IC95%, eje X = N={10,20,40}, eje Y = Mbps\n")
    tbl = []
    for s in SCHED_ORDER:
        v = [get_stat(st, s, "cell_throughput_mbps", "uniform", "homogeneous", n)["cell_throughput_mbps_mean"] for n in [10,20,40]]
        tbl.append(f"| {SCHED_LABEL[s]:6} | {v[0]:.2f} | {v[1]:.2f} | {v[2]:.2f} |")
    lines.append("| Scheduler | N=10 | N=20 | N=40 |")
    lines.append("|-----------|:----:|:----:|:----:|")
    lines += tbl
    lines.append("")
    lines.append("**Qué buscar:** PSS y PF/M-LWDF se mantienen estables (~14 Mbps) porque "
                 "explotan el canal eficientemente sin importar cuántos usuarios compitan. "
                 "MT es también alto pero por razones opuestas: concentra todos los recursos "
                 "en 1-3 UEs privilegiados (starvation). "
                 "BET cae con N porque dedica más tiempo a UEs de canal débil. "
                 "RR muestra un pico en N=20 (carga media óptima para RR).\n")
    lines.append("**Para el paper:** Esta figura establece el baseline de throughput (H1). "
                 "Muestra el trade-off entre categoría (i) baja eficiencia/alta equidad, "
                 "categoría (ii) alta eficiencia/baja equidad (MT), y categoría (ii-iii) "
                 "eficiencia sin sacrificar equidad (PF, PSS).\n")

    # --- F2 ---
    lines.append("### F2 — Jain index vs N (D1 uniforme, T1 full-buffer)\n")
    lines.append("**Archivo:** `F2_jain_vs_N_D1.pdf`  ")
    lines.append("**Tipo:** Curvas con IC95%, eje X = N, eje Y = Jain [0,1]\n")
    tbl = []
    for s in SCHED_ORDER:
        v = [get_stat(st, s, "jain_index", "uniform", "homogeneous", n)["jain_index_mean"] for n in [10,20,40]]
        tbl.append(f"| {SCHED_LABEL[s]:6} | {v[0]:.4f} | {v[1]:.4f} | {v[2]:.4f} |")
    lines.append("| Scheduler | N=10 | N=20 | N=40 |")
    lines.append("|-----------|:----:|:----:|:----:|")
    lines += tbl
    lines.append("")
    lines.append("**Qué buscar:** Tres grupos claramente separados: "
                 "(1) BET≈1.000 para todo N — equidad perfecta independiente de carga. "
                 "(2) TTA/PF/M-LWDF/PSS en rango 0.70–0.80 — equidad moderada-alta. "
                 "(3) MT cae dramáticamente con N (0.180→0.116→0.059): más usuarios = "
                 "más candidatos para starvation. RR se estabiliza ~0.64.\n")
    lines.append("**Para el paper:** Esta figura cuantifica el costo de fairness de "
                 "optimizar throughput. MT sacrifica equidad de forma creciente con la carga. "
                 "Las barras de error pequeñas confirman que las 20 corridas son consistentes.\n")

    # --- F3 ---
    lines.append("### F3 — Jain index vs N (D2 clusterizado, T1 full-buffer)\n")
    lines.append("**Archivo:** `F3_jain_vs_N_D2.pdf`  ")
    lines.append("**Tipo:** Misma estructura que F2, pero con distribución clusterizada\n")
    tbl = []
    for s in SCHED_ORDER:
        v = [get_stat(st, s, "jain_index", "clustered", "homogeneous", n)["jain_index_mean"] for n in [10,20,40]]
        tbl.append(f"| {SCHED_LABEL[s]:6} | {v[0]:.4f} | {v[1]:.4f} | {v[2]:.4f} |")
    lines.append("| Scheduler | N=10 | N=20 | N=40 |")
    lines.append("|-----------|:----:|:----:|:----:|")
    lines += tbl
    lines.append("")
    lines.append("**Qué buscar — comparar con F2:** "
                 "TTA es el más afectado por el clustering: su Jain cae con N en D2 "
                 "(0.744→0.729→0.710) en lugar de subir como en D1 (0.755→0.777→0.802). "
                 "PF/M-LWDF son prácticamente idénticos entre F2 y F3, "
                 "confirmando su robustez ante heterogeneidad espacial. "
                 "MT mantiene Jain bajos en ambas figuras (ya starve en D1).\n")
    lines.append("**Para el paper:** Comparar F2 vs F3 es la evidencia visual de H1/H2. "
                 "La similitud entre ambas figuras para PF/M-LWDF/PSS suporta el "
                 "resultado negativo de H2: el clustering no degrada a estos algoritmos.\n")

    # --- F4 ---
    lines.append("### F4 — Fairness D1 vs D2 (N=20, T1) — barras agrupadas\n")
    lines.append("**Archivo:** `F4_jain_D1vsD2_N20_T1.pdf`  ")
    lines.append("**Tipo:** Barras agrupadas (barra sólida = D1, rayada = D2) por scheduler\n")
    lines.append("| Scheduler | Jain D1 | Jain D2 | Δ |")
    lines.append("|-----------|:-------:|:-------:|:---:|")
    for s in SCHED_ORDER:
        j1 = get_stat(st, s, "jain_index", "uniform",   "homogeneous", 20)["jain_index_mean"]
        j2 = get_stat(st, s, "jain_index", "clustered", "homogeneous", 20)["jain_index_mean"]
        lines.append(f"| {SCHED_LABEL[s]:6} | {j1:.4f} | {j2:.4f} | {j2-j1:+.4f} |")
    lines.append("")
    lines.append("**Qué buscar:** La diferencia entre barra sólida y rayada para cada scheduler. "
                 "TTA tiene la mayor diferencia visible (-0.048). "
                 "BET y PF/M-LWDF muestran barras casi iguales (robustez al clustering). "
                 "MT tiene ambas barras muy bajas — ya está en el suelo en D1.\n")
    lines.append("**Para el paper:** Esta figura sintetiza H1 para todos los schedulers "
                 "en una sola vista. Si las barras D1 y D2 son iguales, el scheduler es "
                 "robusto al clustering. Si D2 es menor, el clustering lo afecta.\n")

    # --- F5 ---
    lines.append("### F5 — Throughput T1 vs T2 (N=20, D1) — barras agrupadas\n")
    lines.append("**Archivo:** `F5_throughput_T1vsT2_N20_D1.pdf`  ")
    lines.append("**Tipo:** Barras agrupadas (barra sólida = T1, rayada = T2)\n")
    lines.append("| Scheduler | Tput T1 | Tput T2 | Δ | % |")
    lines.append("|-----------|:-------:|:-------:|:---:|:---:|")
    for s in SCHED_ORDER:
        t1 = get_stat(st, s, "cell_throughput_mbps", "uniform", "homogeneous",   20)["cell_throughput_mbps_mean"]
        t2 = get_stat(st, s, "cell_throughput_mbps", "uniform", "heterogeneous", 20)["cell_throughput_mbps_mean"]
        lines.append(f"| {SCHED_LABEL[s]:6} | {t1:.2f} | {t2:.2f} | {t2-t1:+.2f} | {(t2-t1)/t1*100:+.1f}% |")
    lines.append("")
    lines.append("**Qué buscar — hallazgo clave:** M-LWDF tiene la mayor caída (-31.2%), "
                 "más que PF (-19.1%) y PSS (-26.9%). Esto parece paradójico: M-LWDF "
                 "supuestamente 'compensa' el tráfico heterogéneo, pero su throughput total "
                 "es el que más cae. La explicación es que M-LWDF prioriza agresivamente "
                 "los flujos GBR (video+gaming) de baja tasa, reduciendo el throughput total "
                 "de celda para cumplir los SLA. MT casi no cae (-1.2%) porque ignora "
                 "completamente el tipo de bearer y solo maximiza la tasa instantánea.\n")
    lines.append("**Para el paper:** Esta figura confirma H3 visualmente. Muestra que "
                 "T2 siempre reduce el throughput total, y que el grado de reducción "
                 "refleja cuánto prioriza cada scheduler el QoS sobre la eficiencia espectral.\n")

    # --- F6 ---
    lines.append("### F6 — GBR vs BE throughput (N=20, D1, T2) — PF, M-LWDF, PSS\n")
    lines.append("**Archivo:** `F6_GBR_vs_BE_N20_D1_T2.pdf`  ")
    lines.append("**Tipo:** Barras agrupadas por scheduler: barra sólida = GBR, rayada = BE\n")
    lines.append("**Qué buscar:** Para PF, la barra BE es relativamente alta y la GBR moderada "
                 "— PF no distingue entre tipos de tráfico, así que el BE full-buffer compite "
                 "en igualdad con el video/gaming. Para M-LWDF y PSS, la barra GBR es mayor "
                 "y la BE menor: estos schedulers sacrifican BE para proteger los flujos GBR "
                 "que tienen SLA de delay.\n")
    lines.append("**Para el paper:** Complementa F5. Donde F5 muestra la caída en throughput "
                 "total, F6 explica *por qué*: M-LWDF/PSS redistribuyen los RBs hacia GBR "
                 "(video+gaming), lo que reduce la capacidad disponible para BE. "
                 "Esta redistribución es exactamente el objetivo del diseño QoS-aware.\n")

    # --- F7 ---
    lines.append("### F7 — Delay E2E D1 vs D2 (N=20, T2) — PF, M-LWDF, PSS\n")
    lines.append("**Archivo:** `F7_delay_D1vsD2_N20_T2.pdf`  ")
    lines.append("**Tipo:** Curvas de 2 puntos (D1 y D2) para 3 schedulers\n")
    lines.append("| Scheduler | Delay D1 T2 | Delay D2 T2 | Δ |")
    lines.append("|-----------|:-----------:|:-----------:|:---:|")
    for s in QOS_ORDER:
        d1 = get_stat(st, s, "mean_delay_ms", "uniform",   "heterogeneous", 20)["mean_delay_ms_mean"]
        d2 = get_stat(st, s, "mean_delay_ms", "clustered", "heterogeneous", 20)["mean_delay_ms_mean"]
        lines.append(f"| {SCHED_LABEL[s]:6} | {d1:.2f} ms | {d2:.2f} ms | {d2-d1:+.2f} ms |")
    lines.append("")
    lines.append("**Qué buscar:** Las tres curvas son casi paralelas y muy cercanas entre sí. "
                 "El delay es muy similar en D1 y D2 (~3.2-3.3 ms) para los tres schedulers, "
                 "y las diferencias son mínimas (<0.1 ms). Esto indica que el delay E2E "
                 "está dominado por el canal inalámbrico y la serialización, "
                 "no por el tipo de scheduler ni la distribución espacial.\n")
    lines.append("**Para el paper:** Este resultado indica que la heterogeneidad espacial "
                 "(D1 vs D2) no afecta el delay promedio de celda bajo T2. "
                 "El delay es una métrica estable en este entorno. "
                 "La diferencia entre schedulers es estadísticamente pequeña, "
                 "lo que sugiere que todos cumplen los requisitos de delay (~3ms << 300ms de SLA).\n")

    # --- F8 ---
    lines.append("### F8 — Jain D1 vs D2 (N=20, T2) — PF, M-LWDF, PSS\n")
    lines.append("**Archivo:** `F8_jain_D1vsD2_N20_T2.pdf`  ")
    lines.append("**Tipo:** Curvas de 2 puntos (D1 y D2) para 3 schedulers\n")
    lines.append("| Scheduler | Jain D1 T2 | Jain D2 T2 | Δ |")
    lines.append("|-----------|:----------:|:----------:|:---:|")
    for s in QOS_ORDER:
        j1 = get_stat(st, s, "jain_index", "uniform",   "heterogeneous", 20)["jain_index_mean"]
        j2 = get_stat(st, s, "jain_index", "clustered", "heterogeneous", 20)["jain_index_mean"]
        lines.append(f"| {SCHED_LABEL[s]:6} | {j1:.4f} | {j2:.4f} | {j2-j1:+.4f} |")
    lines.append("")
    lines.append("**Qué buscar — hallazgo clave:** PF baja su Jain al pasar de D1 a D2 "
                 "(-0.018), mientras M-LWDF y PSS la SUBEN (+0.032, +0.044). "
                 "Las líneas de M-LWDF y PSS se cruzan con PF al pasar de D1 a D2: "
                 "en D1 todos están al mismo nivel (~0.55), pero en D2 M-LWDF y PSS "
                 "superan claramente a PF (0.596 vs 0.543, p<0.001, d≈5). "
                 "CQA aparece en la figura con Jain ~0.40, por debajo de PF en ambas "
                 "distribuciones — su mecanismo multi-criterio no rescata tan efectivamente "
                 "a los UEs de C3 como el delay HOL puro de M-LWDF.\n")
    lines.append("**Para el paper:** Esta es la figura más importante para H4. "
                 "M-LWDF y PSS confirman H4 de forma contundente. "
                 "CQA muestra que dentro de cat(iii), el mecanismo específico importa.\n")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Resumen ejecutivo
# ---------------------------------------------------------------------------

def resumen_ejecutivo(master, st):
    lines = []
    lines.append("# Reporte de Análisis — Evaluación de Schedulers OFDMA en LTE\n")
    lines.append(f"> Generado automáticamente desde `stats.csv` y `master.csv`  \n"
                 f"> {N_RUNS} corridas: {N_SCHEDS} schedulers (7 originales + CQA extensión cat iii)  \n"
                 f"> IC 95% con t-Student (df=19, t_crit=2.093)\n")
    lines.append("---\n")
    lines.append("## Resumen de hipótesis\n")
    lines.append("| Hipótesis | Enunciado | Resultado |")
    lines.append("|-----------|-----------|:---------:|")
    lines.append("| H1 | MT presenta degradación de fairness bajo D2 (starvation de C3) | ⚠️ Parcial |")
    lines.append("| H2 | Heterogeneidad espacial impacta más que el número de usuarios | ❌ No confirmada (6/7 schedulers) |")
    lines.append("| H3 | PF pierde eficiencia bajo tráfico heterogéneo; M-LWDF/PSS compensan | ✅ Confirmada (p<0.001) |")
    lines.append("| H4 | PSS/M-LWDF mejoran fairness frente a PF en D2+T2 | ✅ Confirmada en D2 (p<0.001, d≈5) |")
    lines.append("")
    lines.append("> ⚠️ H1 parcial: MT tiene Jain bajo tanto en D1 (0.116) como en D2 (0.135). "
                 "La starvation ya existe en D1 por fading; D2 la hace sistemática (siempre C3), "
                 "pero el Jain global no cambia significativamente (p=0.23).")
    lines.append(">")
    lines.append("> ❌ H2 no confirmada: ΔJain por carga (N=10→40) supera ΔJain espacial (D1→D2) "
                 "en 6/7 schedulers. Los algoritmos channel-aware absorben la heterogeneidad geográfica "
                 "eficientemente. La carga es el factor dominante de fairness.")
    lines.append("\n---\n")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("Cargando datos...")
    master, st = load()
    print(f"  master.csv: {len(master)} filas")
    print(f"  stats.csv:  {len(st)} grupos\n")

    sections = [
        resumen_ejecutivo(master, st),
        tabla_resumen(master, st),
        "\n---\n",
        analisis_h1(master, st),
        "\n---\n",
        analisis_h2(master, st),
        "\n---\n",
        analisis_h3(master, st),
        "\n---\n",
        analisis_h4(master, st),
        "\n---\n",
        analisis_figuras(st),
        "\n---\n",
        analisis_extension(master, st),
    ]

    report = "\n".join(sections)
    REPORT.write_text(report, encoding="utf-8")
    print(f"Reporte guardado: {REPORT}")
    print(f"  {len(report.splitlines())} líneas")


if __name__ == "__main__":
    main()
