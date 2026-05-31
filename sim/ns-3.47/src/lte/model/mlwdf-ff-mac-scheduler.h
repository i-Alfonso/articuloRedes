/*
 * M-LWDF (Modified Largest Weighted Delay First) Downlink Scheduler for ns-3 LTE
 *
 * Based on Capozzi et al. (2012), eq. 17:
 *   RT flows:  m_i,k = alpha_i * D_HOL,i * d_i^k(t) / R_i(t-1)
 *   BE flows:  m_i,k = d_i^k(t) / R_i(t-1)   (PF metric)
 *   alpha_i  = -log(delta_i) / tau_i
 *
 * Derived from PfFfMacScheduler (ns-3.47), CTTC.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MLWDF_FF_MAC_SCHEDULER_H
#define MLWDF_FF_MAC_SCHEDULER_H

#include "ff-mac-csched-sap.h"
#include "ff-mac-sched-sap.h"
#include "ff-mac-scheduler.h"
#include "lte-amc.h"
#include "lte-common.h"
#include "lte-ffr-sap.h"

#include "ns3/nstime.h"

#include <map>
#include <vector>

namespace ns3
{

/// Per-UE flow performance stats (mirrors pfsFlowPerf_t from PF scheduler)
struct mlwdfFlowPerf_t
{
    Time flowStart;
    unsigned long totalBytesTransmitted;
    unsigned int lastTtiBytesTransmitted;
    double lastAveragedThroughput;
};

/// QoS parameters for a single logical channel (M-LWDF specific)
struct MlwdfLcQosInfo
{
    bool isRealTime;   ///< true for GBR flows that get M-LWDF metric
    double delayBudget; ///< tau_i in seconds
    double targetPLR;   ///< delta_i (packet loss ratio target)
};

/**
 * @ingroup ff-api
 * @brief Implements the SCHED SAP and CSCHED SAP for the M-LWDF scheduler.
 *
 * Real-time (GBR) flows use the M-LWDF metric; best-effort flows fall
 * back to PF.  QoS parameters (tau_i, delta_i) are derived from the
 * QCI carried in CschedLcConfigReq.
 */
class MlwdfFfMacScheduler : public FfMacScheduler
{
  public:
    MlwdfFfMacScheduler();
    ~MlwdfFfMacScheduler() override;

    void DoDispose() override;

    static TypeId GetTypeId();

    // inherited from FfMacScheduler
    void SetFfMacCschedSapUser(FfMacCschedSapUser* s) override;
    void SetFfMacSchedSapUser(FfMacSchedSapUser* s) override;
    FfMacCschedSapProvider* GetFfMacCschedSapProvider() override;
    FfMacSchedSapProvider* GetFfMacSchedSapProvider() override;

    // FFR SAPs
    void SetLteFfrSapProvider(LteFfrSapProvider* s) override;
    LteFfrSapUser* GetLteFfrSapUser() override;

    friend class MemberCschedSapProvider<MlwdfFfMacScheduler>;
    friend class MemberSchedSapProvider<MlwdfFfMacScheduler>;

    void TransmissionModeConfigurationUpdate(uint16_t rnti, uint8_t txMode);

  private:
    void DoCschedCellConfigReq(
        const FfMacCschedSapProvider::CschedCellConfigReqParameters& params);
    void DoCschedUeConfigReq(
        const FfMacCschedSapProvider::CschedUeConfigReqParameters& params);
    void DoCschedLcConfigReq(
        const FfMacCschedSapProvider::CschedLcConfigReqParameters& params);
    void DoCschedLcReleaseReq(
        const FfMacCschedSapProvider::CschedLcReleaseReqParameters& params);
    void DoCschedUeReleaseReq(
        const FfMacCschedSapProvider::CschedUeReleaseReqParameters& params);
    void DoSchedDlRlcBufferReq(
        const FfMacSchedSapProvider::SchedDlRlcBufferReqParameters& params);
    void DoSchedDlPagingBufferReq(
        const FfMacSchedSapProvider::SchedDlPagingBufferReqParameters& params);
    void DoSchedDlMacBufferReq(
        const FfMacSchedSapProvider::SchedDlMacBufferReqParameters& params);
    void DoSchedDlTriggerReq(
        const FfMacSchedSapProvider::SchedDlTriggerReqParameters& params);
    void DoSchedDlRachInfoReq(
        const FfMacSchedSapProvider::SchedDlRachInfoReqParameters& params);
    void DoSchedDlCqiInfoReq(
        const FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params);
    void DoSchedUlTriggerReq(
        const FfMacSchedSapProvider::SchedUlTriggerReqParameters& params);
    void DoSchedUlNoiseInterferenceReq(
        const FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters& params);
    void DoSchedUlSrInfoReq(
        const FfMacSchedSapProvider::SchedUlSrInfoReqParameters& params);
    void DoSchedUlMacCtrlInfoReq(
        const FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters& params);
    void DoSchedUlCqiInfoReq(
        const FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params);

    int GetRbgSize(int dlbandwidth);
    unsigned int LcActivePerFlow(uint16_t rnti);
    double EstimateUlSinr(uint16_t rnti, uint16_t rb);
    void RefreshDlCqiMaps();
    void RefreshUlCqiMaps();
    void UpdateDlRlcBufferInfo(uint16_t rnti, uint8_t lcid, uint16_t size);
    void UpdateUlRlcBufferInfo(uint16_t rnti, uint16_t size);
    uint8_t UpdateHarqProcessId(uint16_t rnti);
    bool HarqProcessAvailability(uint16_t rnti);
    void RefreshHarqProcesses();

    /**
     * Compute the M-LWDF priority metric for one UE on one RBG.
     *
     * @param rnti            UE identifier
     * @param achievableRate  d_i^k(t) in bytes/s for the RBG
     * @param avgThroughput   R_i(t-1) in bytes/s
     * @return priority metric (higher = more urgent)
     */
    double ComputeMlwdfMetric(uint16_t rnti,
                              double achievableRate,
                              double avgThroughput);

    Ptr<LteAmc> m_amc;

    std::map<LteFlowId_t, FfMacSchedSapProvider::SchedDlRlcBufferReqParameters> m_rlcBufferReq;

    std::map<uint16_t, mlwdfFlowPerf_t> m_flowStatsDl;
    std::map<uint16_t, mlwdfFlowPerf_t> m_flowStatsUl;

    std::map<uint16_t, uint8_t>  m_p10CqiRxed;
    std::map<uint16_t, uint32_t> m_p10CqiTimers;
    std::map<uint16_t, SbMeasResult_s> m_a30CqiRxed;
    std::map<uint16_t, uint32_t> m_a30CqiTimers;

    std::map<uint16_t, std::vector<uint16_t>> m_allocationMaps;
    std::map<uint16_t, std::vector<double>>   m_ueCqi;
    std::map<uint16_t, uint32_t>              m_ueCqiTimers;
    std::map<uint16_t, uint32_t>              m_ceBsrRxed;

    /// QoS classification per logical channel (M-LWDF specific)
    std::map<LteFlowId_t, MlwdfLcQosInfo> m_lcQosInfo;

    // MAC SAPs
    FfMacCschedSapUser*     m_cschedSapUser;
    FfMacSchedSapUser*      m_schedSapUser;
    FfMacCschedSapProvider* m_cschedSapProvider;
    FfMacSchedSapProvider*  m_schedSapProvider;

    // FFR SAPs
    LteFfrSapUser*     m_ffrSapUser;
    LteFfrSapProvider* m_ffrSapProvider;

    FfMacCschedSapProvider::CschedCellConfigReqParameters m_cschedCellConfig;

    double   m_timeWindow;
    uint16_t m_nextRntiUl;
    uint32_t m_cqiTimersThreshold;

    std::map<uint16_t, uint8_t> m_uesTxMode;

    // HARQ
    bool m_harqOn;
    std::map<uint16_t, uint8_t>                    m_dlHarqCurrentProcessId;
    std::map<uint16_t, DlHarqProcessesStatus_t>    m_dlHarqProcessesStatus;
    std::map<uint16_t, DlHarqProcessesTimer_t>     m_dlHarqProcessesTimer;
    std::map<uint16_t, DlHarqProcessesDciBuffer_t> m_dlHarqProcessesDciBuffer;
    std::map<uint16_t, DlHarqRlcPduListBuffer_t>   m_dlHarqProcessesRlcPduListBuffer;
    std::vector<DlInfoListElement_s>               m_dlInfoListBuffered;

    std::map<uint16_t, uint8_t>                    m_ulHarqCurrentProcessId;
    std::map<uint16_t, UlHarqProcessesStatus_t>    m_ulHarqProcessesStatus;
    std::map<uint16_t, UlHarqProcessesDciBuffer_t> m_ulHarqProcessesDciBuffer;

    // RACH
    std::vector<RachListElement_s> m_rachList;
    std::vector<uint16_t>          m_rachAllocationMap;
    uint8_t                        m_ulGrantMcs;
};

} // namespace ns3

#endif /* MLWDF_FF_MAC_SCHEDULER_H */
