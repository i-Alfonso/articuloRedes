/*
 * M-LWDF (Modified Largest Weighted Delay First) Downlink Scheduler
 *
 * Metric for real-time (GBR) flows (Capozzi et al. 2012, eq. 17):
 *   m_i,k = alpha_i * D_HOL,i * d_i^k(t) / R_i(t-1)
 *   alpha_i = -log(delta_i) / tau_i
 *
 * Best-effort (non-GBR) flows use the standard PF metric:
 *   m_i,k = d_i^k(t) / R_i(t-1)
 *
 * QoS parameters are derived from the QCI set in CschedLcConfigReq:
 *   QCI 1 (GBR voice):          tau=0.100 s, delta=1e-2
 *   QCI 2 (GBR video):          tau=0.150 s, delta=1e-3
 *   QCI 3 (GBR gaming):         tau=0.050 s, delta=1e-3
 *   QCI 4 (GBR non-conv video): tau=0.300 s, delta=1e-6
 *   Other GBR:                  tau=0.100 s, delta=1e-2
 *
 * Derived from PfFfMacScheduler (ns-3.47), CTTC.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mlwdf-ff-mac-scheduler.h"

#include "lte-amc.h"
#include "lte-vendor-specific-parameters.h"

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/math.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

#include <cfloat>
#include <cmath>
#include <set>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MlwdfFfMacScheduler");

static const int MlwdfType0AllocationRbg[4] = {
    10,  // RBG size 1
    26,  // RBG size 2
    63,  // RBG size 3
    110, // RBG size 4
};

NS_OBJECT_ENSURE_REGISTERED(MlwdfFfMacScheduler);

MlwdfFfMacScheduler::MlwdfFfMacScheduler()
    : m_cschedSapUser(nullptr),
      m_schedSapUser(nullptr),
      m_timeWindow(99.0),
      m_nextRntiUl(0)
{
    m_amc = CreateObject<LteAmc>();
    m_cschedSapProvider = new MemberCschedSapProvider<MlwdfFfMacScheduler>(this);
    m_schedSapProvider = new MemberSchedSapProvider<MlwdfFfMacScheduler>(this);
    m_ffrSapProvider = nullptr;
    m_ffrSapUser = new MemberLteFfrSapUser<MlwdfFfMacScheduler>(this);
}

MlwdfFfMacScheduler::~MlwdfFfMacScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
MlwdfFfMacScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_dlHarqProcessesDciBuffer.clear();
    m_dlHarqProcessesTimer.clear();
    m_dlHarqProcessesRlcPduListBuffer.clear();
    m_dlInfoListBuffered.clear();
    m_ulHarqCurrentProcessId.clear();
    m_ulHarqProcessesStatus.clear();
    m_ulHarqProcessesDciBuffer.clear();
    delete m_cschedSapProvider;
    delete m_schedSapProvider;
    delete m_ffrSapUser;
}

TypeId
MlwdfFfMacScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MlwdfFfMacScheduler")
            .SetParent<FfMacScheduler>()
            .SetGroupName("Lte")
            .AddConstructor<MlwdfFfMacScheduler>()
            .AddAttribute("CqiTimerThreshold",
                          "The number of TTIs a CQI is valid (default 1000 - 1 sec.)",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&MlwdfFfMacScheduler::m_cqiTimersThreshold),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("HarqEnabled",
                          "Activate/Deactivate the HARQ [by default is active].",
                          BooleanValue(true),
                          MakeBooleanAccessor(&MlwdfFfMacScheduler::m_harqOn),
                          MakeBooleanChecker())
            .AddAttribute("UlGrantMcs",
                          "The MCS of the UL grant, must be [0..15] (default 0)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&MlwdfFfMacScheduler::m_ulGrantMcs),
                          MakeUintegerChecker<uint8_t>());
    return tid;
}

void
MlwdfFfMacScheduler::SetFfMacCschedSapUser(FfMacCschedSapUser* s)
{
    m_cschedSapUser = s;
}

void
MlwdfFfMacScheduler::SetFfMacSchedSapUser(FfMacSchedSapUser* s)
{
    m_schedSapUser = s;
}

FfMacCschedSapProvider*
MlwdfFfMacScheduler::GetFfMacCschedSapProvider()
{
    return m_cschedSapProvider;
}

FfMacSchedSapProvider*
MlwdfFfMacScheduler::GetFfMacSchedSapProvider()
{
    return m_schedSapProvider;
}

void
MlwdfFfMacScheduler::SetLteFfrSapProvider(LteFfrSapProvider* s)
{
    m_ffrSapProvider = s;
}

LteFfrSapUser*
MlwdfFfMacScheduler::GetLteFfrSapUser()
{
    return m_ffrSapUser;
}

void
MlwdfFfMacScheduler::DoCschedCellConfigReq(
    const FfMacCschedSapProvider::CschedCellConfigReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_cschedCellConfig = params;
    m_rachAllocationMap.resize(m_cschedCellConfig.m_ulBandwidth, 0);
    FfMacCschedSapUser::CschedUeConfigCnfParameters cnf;
    cnf.m_result = SUCCESS;
    m_cschedSapUser->CschedUeConfigCnf(cnf);
}

void
MlwdfFfMacScheduler::DoCschedUeConfigReq(
    const FfMacCschedSapProvider::CschedUeConfigReqParameters& params)
{
    NS_LOG_FUNCTION(this << " RNTI " << params.m_rnti << " txMode "
                         << (uint16_t)params.m_transmissionMode);
    auto it = m_uesTxMode.find(params.m_rnti);
    if (it == m_uesTxMode.end())
    {
        m_uesTxMode[params.m_rnti] = params.m_transmissionMode;
        // generate HARQ buffers
        m_dlHarqCurrentProcessId[params.m_rnti] = 0;
        DlHarqProcessesStatus_t dlHarqPrcStatus;
        dlHarqPrcStatus.resize(8, 0);
        m_dlHarqProcessesStatus[params.m_rnti] = dlHarqPrcStatus;
        DlHarqProcessesTimer_t dlHarqProcessesTimer;
        dlHarqProcessesTimer.resize(8, 0);
        m_dlHarqProcessesTimer[params.m_rnti] = dlHarqProcessesTimer;
        DlHarqProcessesDciBuffer_t dlHarqdci;
        dlHarqdci.resize(8);
        m_dlHarqProcessesDciBuffer[params.m_rnti] = dlHarqdci;
        DlHarqRlcPduListBuffer_t dlHarqRlcPdu;
        dlHarqRlcPdu.resize(2);
        dlHarqRlcPdu.at(0).resize(8);
        dlHarqRlcPdu.at(1).resize(8);
        m_dlHarqProcessesRlcPduListBuffer[params.m_rnti] = dlHarqRlcPdu;
        m_ulHarqCurrentProcessId[params.m_rnti] = 0;
        UlHarqProcessesStatus_t ulHarqPrcStatus;
        ulHarqPrcStatus.resize(8, 0);
        m_ulHarqProcessesStatus[params.m_rnti] = ulHarqPrcStatus;
        UlHarqProcessesDciBuffer_t ulHarqdci;
        ulHarqdci.resize(8);
        m_ulHarqProcessesDciBuffer[params.m_rnti] = ulHarqdci;
    }
    else
    {
        (*it).second = params.m_transmissionMode;
    }
}

void
MlwdfFfMacScheduler::DoCschedLcConfigReq(
    const FfMacCschedSapProvider::CschedLcConfigReqParameters& params)
{
    NS_LOG_FUNCTION(this << " New LC, rnti: " << params.m_rnti);

    for (std::size_t i = 0; i < params.m_logicalChannelConfigList.size(); i++)
    {
        // Initialize per-UE throughput stats on first LC for this RNTI
        auto itStats = m_flowStatsDl.find(params.m_rnti);
        if (itStats == m_flowStatsDl.end())
        {
            mlwdfFlowPerf_t flowStatsDl;
            flowStatsDl.flowStart = Simulator::Now();
            flowStatsDl.totalBytesTransmitted = 0;
            flowStatsDl.lastTtiBytesTransmitted = 0;
            flowStatsDl.lastAveragedThroughput = 1;
            m_flowStatsDl[params.m_rnti] = flowStatsDl;
            mlwdfFlowPerf_t flowStatsUl;
            flowStatsUl.flowStart = Simulator::Now();
            flowStatsUl.totalBytesTransmitted = 0;
            flowStatsUl.lastTtiBytesTransmitted = 0;
            flowStatsUl.lastAveragedThroughput = 1;
            m_flowStatsUl[params.m_rnti] = flowStatsUl;
        }

        // Classify this LC as RT or BE and record QoS parameters
        const LogicalChannelConfigListElement_s& lc = params.m_logicalChannelConfigList.at(i);
        LteFlowId_t flowId(params.m_rnti, lc.m_logicalChannelIdentity);

        MlwdfLcQosInfo qosInfo;
        qosInfo.isRealTime = (lc.m_qosBearerType ==
                              LogicalChannelConfigListElement_s::QBT_GBR);

        if (qosInfo.isRealTime)
        {
            // Map QCI to (tau_i, delta_i) per 3GPP TS 23.203 / Capozzi Table II
            switch (lc.m_qci)
            {
            case 1: // GBR conversational voice
                qosInfo.delayBudget = 0.100;
                qosInfo.targetPLR = 1e-2;
                break;
            case 2: // GBR conversational video
                qosInfo.delayBudget = 0.150;
                qosInfo.targetPLR = 1e-3;
                break;
            case 3: // GBR real-time gaming  (used for low-latency flows)
                qosInfo.delayBudget = 0.050;
                qosInfo.targetPLR = 1e-3;
                break;
            case 4: // GBR non-conversational video
                qosInfo.delayBudget = 0.300;
                qosInfo.targetPLR = 1e-6;
                break;
            default: // fallback for other GBR types
                qosInfo.delayBudget = 0.100;
                qosInfo.targetPLR = 1e-2;
                break;
            }
            NS_LOG_INFO("MlwdfFfMacScheduler: RNTI " << params.m_rnti
                        << " LC " << (uint16_t)lc.m_logicalChannelIdentity
                        << " QCI " << (uint16_t)lc.m_qci
                        << " -> RT flow tau=" << qosInfo.delayBudget
                        << " delta=" << qosInfo.targetPLR);
        }
        else
        {
            qosInfo.delayBudget = 0.0;
            qosInfo.targetPLR   = 0.0;
        }

        m_lcQosInfo[flowId] = qosInfo;
    }
}

void
MlwdfFfMacScheduler::DoCschedLcReleaseReq(
    const FfMacCschedSapProvider::CschedLcReleaseReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    for (std::size_t i = 0; i < params.m_logicalChannelIdentity.size(); i++)
    {
        // Remove RLC buffer entry
        auto it = m_rlcBufferReq.begin();
        while (it != m_rlcBufferReq.end())
        {
            if (((*it).first.m_rnti == params.m_rnti) &&
                ((*it).first.m_lcId == params.m_logicalChannelIdentity.at(i)))
            {
                auto temp = it;
                it++;
                m_rlcBufferReq.erase(temp);
            }
            else
            {
                it++;
            }
        }
        // Remove QoS info
        LteFlowId_t flowId(params.m_rnti, params.m_logicalChannelIdentity.at(i));
        m_lcQosInfo.erase(flowId);
    }
}

void
MlwdfFfMacScheduler::DoCschedUeReleaseReq(
    const FfMacCschedSapProvider::CschedUeReleaseReqParameters& params)
{
    NS_LOG_FUNCTION(this);

    m_uesTxMode.erase(params.m_rnti);
    m_dlHarqCurrentProcessId.erase(params.m_rnti);
    m_dlHarqProcessesStatus.erase(params.m_rnti);
    m_dlHarqProcessesTimer.erase(params.m_rnti);
    m_dlHarqProcessesDciBuffer.erase(params.m_rnti);
    m_dlHarqProcessesRlcPduListBuffer.erase(params.m_rnti);
    m_ulHarqCurrentProcessId.erase(params.m_rnti);
    m_ulHarqProcessesStatus.erase(params.m_rnti);
    m_ulHarqProcessesDciBuffer.erase(params.m_rnti);
    m_flowStatsDl.erase(params.m_rnti);
    m_flowStatsUl.erase(params.m_rnti);
    m_ceBsrRxed.erase(params.m_rnti);

    auto it = m_rlcBufferReq.begin();
    while (it != m_rlcBufferReq.end())
    {
        if ((*it).first.m_rnti == params.m_rnti)
        {
            auto temp = it;
            it++;
            m_rlcBufferReq.erase(temp);
        }
        else
        {
            it++;
        }
    }

    // Remove all QoS entries for this UE
    auto itQos = m_lcQosInfo.begin();
    while (itQos != m_lcQosInfo.end())
    {
        if (itQos->first.m_rnti == params.m_rnti)
        {
            itQos = m_lcQosInfo.erase(itQos);
        }
        else
        {
            ++itQos;
        }
    }

    if (m_nextRntiUl == params.m_rnti)
    {
        m_nextRntiUl = 0;
    }
}

void
MlwdfFfMacScheduler::DoSchedDlRlcBufferReq(
    const FfMacSchedSapProvider::SchedDlRlcBufferReqParameters& params)
{
    NS_LOG_FUNCTION(this << params.m_rnti << (uint32_t)params.m_logicalChannelIdentity);
    LteFlowId_t flow(params.m_rnti, params.m_logicalChannelIdentity);
    auto it = m_rlcBufferReq.find(flow);
    if (it == m_rlcBufferReq.end())
    {
        m_rlcBufferReq[flow] = params;
    }
    else
    {
        (*it).second = params;
    }
}

void
MlwdfFfMacScheduler::DoSchedDlPagingBufferReq(
    const FfMacSchedSapProvider::SchedDlPagingBufferReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    NS_FATAL_ERROR("method not implemented");
}

void
MlwdfFfMacScheduler::DoSchedDlMacBufferReq(
    const FfMacSchedSapProvider::SchedDlMacBufferReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    NS_FATAL_ERROR("method not implemented");
}

int
MlwdfFfMacScheduler::GetRbgSize(int dlbandwidth)
{
    for (int i = 0; i < 4; i++)
    {
        if (dlbandwidth < MlwdfType0AllocationRbg[i])
        {
            return i + 1;
        }
    }
    return -1;
}

unsigned int
MlwdfFfMacScheduler::LcActivePerFlow(uint16_t rnti)
{
    unsigned int lcActive = 0;
    for (auto it = m_rlcBufferReq.begin(); it != m_rlcBufferReq.end(); it++)
    {
        if (((*it).first.m_rnti == rnti) && (((*it).second.m_rlcTransmissionQueueSize > 0) ||
                                             ((*it).second.m_rlcRetransmissionQueueSize > 0) ||
                                             ((*it).second.m_rlcStatusPduSize > 0)))
        {
            lcActive++;
        }
        if ((*it).first.m_rnti > rnti)
        {
            break;
        }
    }
    return lcActive;
}

bool
MlwdfFfMacScheduler::HarqProcessAvailability(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    auto it = m_dlHarqCurrentProcessId.find(rnti);
    if (it == m_dlHarqCurrentProcessId.end())
    {
        NS_FATAL_ERROR("No Process Id found for this RNTI " << rnti);
    }
    auto itStat = m_dlHarqProcessesStatus.find(rnti);
    if (itStat == m_dlHarqProcessesStatus.end())
    {
        NS_FATAL_ERROR("No Process Id Status found for this RNTI " << rnti);
    }
    uint8_t i = (*it).second;
    do
    {
        i = (i + 1) % HARQ_PROC_NUM;
    } while (((*itStat).second.at(i) != 0) && (i != (*it).second));
    return (*itStat).second.at(i) == 0;
}

uint8_t
MlwdfFfMacScheduler::UpdateHarqProcessId(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    if (!m_harqOn)
    {
        return 0;
    }
    auto it = m_dlHarqCurrentProcessId.find(rnti);
    if (it == m_dlHarqCurrentProcessId.end())
    {
        NS_FATAL_ERROR("No Process Id found for this RNTI " << rnti);
    }
    auto itStat = m_dlHarqProcessesStatus.find(rnti);
    if (itStat == m_dlHarqProcessesStatus.end())
    {
        NS_FATAL_ERROR("No Process Id Status found for this RNTI " << rnti);
    }
    uint8_t i = (*it).second;
    do
    {
        i = (i + 1) % HARQ_PROC_NUM;
    } while (((*itStat).second.at(i) != 0) && (i != (*it).second));
    if ((*itStat).second.at(i) == 0)
    {
        (*it).second = i;
        (*itStat).second.at(i) = 1;
    }
    else
    {
        NS_FATAL_ERROR("No HARQ process available for RNTI "
                       << rnti << " check before update with HarqProcessAvailability");
    }
    return (*it).second;
}

void
MlwdfFfMacScheduler::RefreshHarqProcesses()
{
    NS_LOG_FUNCTION(this);
    for (auto itTimers = m_dlHarqProcessesTimer.begin();
         itTimers != m_dlHarqProcessesTimer.end(); itTimers++)
    {
        for (uint16_t i = 0; i < HARQ_PROC_NUM; i++)
        {
            if ((*itTimers).second.at(i) == HARQ_DL_TIMEOUT)
            {
                NS_LOG_DEBUG(this << " Reset HARQ proc " << i << " for RNTI "
                                  << (*itTimers).first);
                auto itStat = m_dlHarqProcessesStatus.find((*itTimers).first);
                if (itStat == m_dlHarqProcessesStatus.end())
                {
                    NS_FATAL_ERROR("No Process Id Status found for this RNTI "
                                   << (*itTimers).first);
                }
                (*itStat).second.at(i) = 0;
                (*itTimers).second.at(i) = 0;
            }
            else
            {
                (*itTimers).second.at(i)++;
            }
        }
    }
}

double
MlwdfFfMacScheduler::ComputeMlwdfMetric(uint16_t rnti,
                                        double achievableRate,
                                        double avgThroughput)
{
    // Find the maximum alpha_i * D_HOL_i among all active RT logical channels for this UE.
    // If no RT flows exist, fall back to PF metric.
    double maxWeightedHol = 0.0;
    bool hasRtFlow = false;

    for (auto itBuf = m_rlcBufferReq.begin(); itBuf != m_rlcBufferReq.end(); itBuf++)
    {
        if (itBuf->first.m_rnti != rnti)
        {
            // m_rlcBufferReq is sorted by (rnti, lcid); skip past this UE
            if (itBuf->first.m_rnti > rnti)
            {
                break;
            }
            continue;
        }

        // Only consider LCs with data
        if (itBuf->second.m_rlcTransmissionQueueSize == 0 &&
            itBuf->second.m_rlcRetransmissionQueueSize == 0 &&
            itBuf->second.m_rlcStatusPduSize == 0)
        {
            continue;
        }

        auto itQos = m_lcQosInfo.find(itBuf->first);
        if (itQos == m_lcQosInfo.end() || !itQos->second.isRealTime)
        {
            continue;
        }

        // D_HOL is reported in milliseconds; convert to seconds
        double holSec = static_cast<double>(
            itBuf->second.m_rlcTransmissionQueueHolDelay) * 1e-3;

        double alpha = -std::log(itQos->second.targetPLR) / itQos->second.delayBudget;
        double weightedHol = alpha * holSec;

        if (!hasRtFlow || weightedHol > maxWeightedHol)
        {
            maxWeightedHol = weightedHol;
            hasRtFlow = true;
        }
    }

    if (hasRtFlow)
    {
        return maxWeightedHol * achievableRate / avgThroughput;
    }
    else
    {
        return achievableRate / avgThroughput; // PF fallback
    }
}

void
MlwdfFfMacScheduler::DoSchedDlTriggerReq(
    const FfMacSchedSapProvider::SchedDlTriggerReqParameters& params)
{
    NS_LOG_FUNCTION(this << " Frame no. " << (params.m_sfnSf >> 4) << " subframe no. "
                         << (0xF & params.m_sfnSf));

    RefreshDlCqiMaps();

    int rbgSize = GetRbgSize(m_cschedCellConfig.m_dlBandwidth);
    int rbgNum = m_cschedCellConfig.m_dlBandwidth / rbgSize;
    std::map<uint16_t, std::vector<uint16_t>> allocationMap;
    std::vector<bool> rbgMap;
    uint16_t rbgAllocatedNum = 0;
    std::set<uint16_t> rntiAllocated;
    rbgMap.resize(m_cschedCellConfig.m_dlBandwidth / rbgSize, false);

    rbgMap = m_ffrSapProvider->GetAvailableDlRbg();
    for (auto it = rbgMap.begin(); it != rbgMap.end(); it++)
    {
        if (*it)
        {
            rbgAllocatedNum++;
        }
    }

    FfMacSchedSapUser::SchedDlConfigIndParameters ret;

    // update UL HARQ proc id
    for (auto itProcId = m_ulHarqCurrentProcessId.begin();
         itProcId != m_ulHarqCurrentProcessId.end(); itProcId++)
    {
        (*itProcId).second = ((*itProcId).second + 1) % HARQ_PROC_NUM;
    }

    // RACH Allocation
    std::vector<bool> ulRbMap;
    ulRbMap.resize(m_cschedCellConfig.m_ulBandwidth, false);
    ulRbMap = m_ffrSapProvider->GetAvailableUlRbg();
    uint8_t maxContinuousUlBandwidth = 0;
    uint8_t tmpMinBandwidth = 0;
    uint16_t ffrRbStartOffset = 0;
    uint16_t tmpFfrRbStartOffset = 0;
    uint16_t index = 0;

    for (auto it = ulRbMap.begin(); it != ulRbMap.end(); it++)
    {
        if (*it)
        {
            if (tmpMinBandwidth > maxContinuousUlBandwidth)
            {
                maxContinuousUlBandwidth = tmpMinBandwidth;
                ffrRbStartOffset = tmpFfrRbStartOffset;
            }
            tmpMinBandwidth = 0;
        }
        else
        {
            if (tmpMinBandwidth == 0)
            {
                tmpFfrRbStartOffset = index;
            }
            tmpMinBandwidth++;
        }
        index++;
    }
    if (tmpMinBandwidth > maxContinuousUlBandwidth)
    {
        maxContinuousUlBandwidth = tmpMinBandwidth;
        ffrRbStartOffset = tmpFfrRbStartOffset;
    }

    m_rachAllocationMap.resize(m_cschedCellConfig.m_ulBandwidth, 0);
    uint16_t rbStart = ffrRbStartOffset;
    for (auto itRach = m_rachList.begin(); itRach != m_rachList.end(); itRach++)
    {
        NS_ASSERT_MSG(m_amc->GetUlTbSizeFromMcs(m_ulGrantMcs, m_cschedCellConfig.m_ulBandwidth) >
                          (*itRach).m_estimatedSize,
                      " Default UL Grant MCS does not allow to send RACH messages");
        BuildRarListElement_s newRar;
        newRar.m_rnti = (*itRach).m_rnti;
        newRar.m_grant.m_rnti = newRar.m_rnti;
        newRar.m_grant.m_mcs = m_ulGrantMcs;
        uint16_t rbLen = 1;
        uint16_t tbSizeBits = 0;
        while ((tbSizeBits < (*itRach).m_estimatedSize) &&
               (rbStart + rbLen < (ffrRbStartOffset + maxContinuousUlBandwidth)))
        {
            rbLen++;
            tbSizeBits = m_amc->GetUlTbSizeFromMcs(m_ulGrantMcs, rbLen);
        }
        if (tbSizeBits < (*itRach).m_estimatedSize)
        {
            break;
        }
        newRar.m_grant.m_rbStart = rbStart;
        newRar.m_grant.m_rbLen = rbLen;
        newRar.m_grant.m_tbSize = tbSizeBits / 8;
        newRar.m_grant.m_hopping = false;
        newRar.m_grant.m_tpc = 0;
        newRar.m_grant.m_cqiRequest = false;
        newRar.m_grant.m_ulDelay = false;
        for (uint16_t i = rbStart; i < rbStart + rbLen; i++)
        {
            m_rachAllocationMap.at(i) = (*itRach).m_rnti;
        }
        if (m_harqOn)
        {
            UlDciListElement_s uldci;
            uldci.m_rnti = newRar.m_rnti;
            uldci.m_rbLen = rbLen;
            uldci.m_rbStart = rbStart;
            uldci.m_mcs = m_ulGrantMcs;
            uldci.m_tbSize = tbSizeBits / 8;
            uldci.m_ndi = 1;
            uldci.m_cceIndex = 0;
            uldci.m_aggrLevel = 1;
            uldci.m_ueTxAntennaSelection = 3;
            uldci.m_hopping = false;
            uldci.m_n2Dmrs = 0;
            uldci.m_tpc = 0;
            uldci.m_cqiRequest = false;
            uldci.m_ulIndex = 0;
            uldci.m_dai = 1;
            uldci.m_freqHopping = 0;
            uldci.m_pdcchPowerOffset = 0;
            uint8_t harqId = 0;
            auto itProcId = m_ulHarqCurrentProcessId.find(uldci.m_rnti);
            if (itProcId == m_ulHarqCurrentProcessId.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE " << uldci.m_rnti);
            }
            harqId = (*itProcId).second;
            auto itDci = m_ulHarqProcessesDciBuffer.find(uldci.m_rnti);
            if (itDci == m_ulHarqProcessesDciBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RNTI entry in UL DCI HARQ buffer for RNTI "
                               << uldci.m_rnti);
            }
            (*itDci).second.at(harqId) = uldci;
        }
        rbStart = rbStart + rbLen;
        ret.m_buildRarList.push_back(newRar);
    }
    m_rachList.clear();

    // Process DL HARQ feedback
    RefreshHarqProcesses();
    if (!m_dlInfoListBuffered.empty())
    {
        if (!params.m_dlInfoList.empty())
        {
            NS_LOG_INFO(this << " Received DL-HARQ feedback");
            m_dlInfoListBuffered.insert(m_dlInfoListBuffered.end(),
                                        params.m_dlInfoList.begin(),
                                        params.m_dlInfoList.end());
        }
    }
    else
    {
        if (!params.m_dlInfoList.empty())
        {
            m_dlInfoListBuffered = params.m_dlInfoList;
        }
    }

    if (!m_harqOn)
    {
        m_dlInfoListBuffered.clear();
    }

    std::vector<DlInfoListElement_s> dlInfoListUntxed;
    for (std::size_t i = 0; i < m_dlInfoListBuffered.size(); i++)
    {
        auto itRnti = rntiAllocated.find(m_dlInfoListBuffered.at(i).m_rnti);
        if (itRnti != rntiAllocated.end())
        {
            continue;
        }
        auto nLayers = m_dlInfoListBuffered.at(i).m_harqStatus.size();
        std::vector<bool> retx;
        NS_LOG_INFO(this << " Processing DLHARQ feedback");
        if (nLayers == 1)
        {
            retx.push_back(m_dlInfoListBuffered.at(i).m_harqStatus.at(0) ==
                           DlInfoListElement_s::NACK);
            retx.push_back(false);
        }
        else
        {
            retx.push_back(m_dlInfoListBuffered.at(i).m_harqStatus.at(0) ==
                           DlInfoListElement_s::NACK);
            retx.push_back(m_dlInfoListBuffered.at(i).m_harqStatus.at(1) ==
                           DlInfoListElement_s::NACK);
        }
        if (retx.at(0) || retx.at(1))
        {
            uint16_t rnti = m_dlInfoListBuffered.at(i).m_rnti;
            uint8_t harqId = m_dlInfoListBuffered.at(i).m_harqProcessId;
            NS_LOG_INFO(this << " HARQ retx RNTI " << rnti << " harqId " << (uint16_t)harqId);
            auto itHarq = m_dlHarqProcessesDciBuffer.find(rnti);
            if (itHarq == m_dlHarqProcessesDciBuffer.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE " << rnti);
            }
            DlDciListElement_s dci = (*itHarq).second.at(harqId);
            int rv = 0;
            if (dci.m_rv.size() == 1)
            {
                rv = dci.m_rv.at(0);
            }
            else
            {
                rv = (dci.m_rv.at(0) > dci.m_rv.at(1) ? dci.m_rv.at(0) : dci.m_rv.at(1));
            }
            if (rv == 3)
            {
                NS_LOG_INFO("Maximum number of retransmissions reached -> drop process");
                auto it = m_dlHarqProcessesStatus.find(rnti);
                if (it == m_dlHarqProcessesStatus.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) "
                                 << m_dlInfoListBuffered.at(i).m_rnti);
                }
                (*it).second.at(harqId) = 0;
                auto itRlcPdu = m_dlHarqProcessesRlcPduListBuffer.find(rnti);
                if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
                {
                    NS_FATAL_ERROR("Unable to find RlcPdcList in HARQ buffer for RNTI "
                                   << m_dlInfoListBuffered.at(i).m_rnti);
                }
                for (std::size_t k = 0; k < (*itRlcPdu).second.size(); k++)
                {
                    (*itRlcPdu).second.at(k).at(harqId).clear();
                }
                continue;
            }
            std::vector<int> dciRbg;
            uint32_t mask = 0x1;
            NS_LOG_INFO("Original RBGs " << dci.m_rbBitmap << " rnti " << dci.m_rnti);
            for (int j = 0; j < 32; j++)
            {
                if (((dci.m_rbBitmap & mask) >> j) == 1)
                {
                    dciRbg.push_back(j);
                }
                mask = (mask << 1);
            }
            bool free = true;
            for (std::size_t j = 0; j < dciRbg.size(); j++)
            {
                if (rbgMap.at(dciRbg.at(j)))
                {
                    free = false;
                    break;
                }
            }
            if (free)
            {
                for (std::size_t j = 0; j < dciRbg.size(); j++)
                {
                    rbgMap.at(dciRbg.at(j)) = true;
                    rbgAllocatedNum++;
                }
                NS_LOG_INFO(this << " Send retx in the same RBGs");
            }
            else
            {
                uint8_t j = 0;
                uint8_t rbgId = (dciRbg.at(dciRbg.size() - 1) + 1) % rbgNum;
                uint8_t startRbg = dciRbg.at(dciRbg.size() - 1);
                std::vector<bool> rbgMapCopy = rbgMap;
                while ((j < dciRbg.size()) && (startRbg != rbgId))
                {
                    if (!rbgMapCopy.at(rbgId))
                    {
                        rbgMapCopy.at(rbgId) = true;
                        dciRbg.at(j) = rbgId;
                        j++;
                    }
                    rbgId = (rbgId + 1) % rbgNum;
                }
                if (j == dciRbg.size())
                {
                    uint32_t rbgMask = 0;
                    for (std::size_t k = 0; k < dciRbg.size(); k++)
                    {
                        rbgMask = rbgMask + (0x1 << dciRbg.at(k));
                        rbgAllocatedNum++;
                    }
                    dci.m_rbBitmap = rbgMask;
                    rbgMap = rbgMapCopy;
                }
                else
                {
                    dlInfoListUntxed.push_back(m_dlInfoListBuffered.at(i));
                    NS_LOG_INFO(this << " No resource for this retx -> buffer it");
                }
            }
            BuildDataListElement_s newEl;
            auto itRlcPdu = m_dlHarqProcessesRlcPduListBuffer.find(rnti);
            if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RlcPdcList in HARQ buffer for RNTI " << rnti);
            }
            for (std::size_t j = 0; j < nLayers; j++)
            {
                if (retx.at(j))
                {
                    if (j >= dci.m_ndi.size())
                    {
                        dci.m_ndi.push_back(0);
                        dci.m_rv.push_back(0);
                        dci.m_mcs.push_back(0);
                        dci.m_tbsSize.push_back(0);
                    }
                    else
                    {
                        dci.m_ndi.at(j) = 0;
                        dci.m_rv.at(j)++;
                        (*itHarq).second.at(harqId).m_rv.at(j)++;
                    }
                }
                else
                {
                    dci.m_ndi.at(j) = 0;
                    dci.m_rv.at(j) = 0;
                    dci.m_mcs.at(j) = 0;
                    dci.m_tbsSize.at(j) = 0;
                }
            }
            for (std::size_t k = 0; k < (*itRlcPdu).second.at(0).at(dci.m_harqProcess).size();
                 k++)
            {
                std::vector<RlcPduListElement_s> rlcPduListPerLc;
                for (std::size_t j = 0; j < nLayers; j++)
                {
                    if (retx.at(j))
                    {
                        if (j < dci.m_ndi.size())
                        {
                            rlcPduListPerLc.push_back(
                                (*itRlcPdu).second.at(j).at(dci.m_harqProcess).at(k));
                        }
                    }
                    else
                    {
                        RlcPduListElement_s emptyElement;
                        emptyElement.m_logicalChannelIdentity =
                            (*itRlcPdu).second.at(j).at(dci.m_harqProcess).at(k)
                                .m_logicalChannelIdentity;
                        emptyElement.m_size = 0;
                        rlcPduListPerLc.push_back(emptyElement);
                    }
                }
                if (!rlcPduListPerLc.empty())
                {
                    newEl.m_rlcPduList.push_back(rlcPduListPerLc);
                }
            }
            newEl.m_rnti = rnti;
            newEl.m_dci = dci;
            (*itHarq).second.at(harqId).m_rv = dci.m_rv;
            auto itHarqTimer = m_dlHarqProcessesTimer.find(rnti);
            if (itHarqTimer == m_dlHarqProcessesTimer.end())
            {
                NS_FATAL_ERROR("Unable to find HARQ timer for RNTI " << (uint16_t)rnti);
            }
            (*itHarqTimer).second.at(harqId) = 0;
            ret.m_buildDataList.push_back(newEl);
            rntiAllocated.insert(rnti);
        }
        else
        {
            NS_LOG_INFO(this << " HARQ received ACK for UE " << m_dlInfoListBuffered.at(i).m_rnti);
            auto it = m_dlHarqProcessesStatus.find(m_dlInfoListBuffered.at(i).m_rnti);
            if (it == m_dlHarqProcessesStatus.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE "
                               << m_dlInfoListBuffered.at(i).m_rnti);
            }
            (*it).second.at(m_dlInfoListBuffered.at(i).m_harqProcessId) = 0;
            auto itRlcPdu =
                m_dlHarqProcessesRlcPduListBuffer.find(m_dlInfoListBuffered.at(i).m_rnti);
            if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RlcPdcList in HARQ buffer for RNTI "
                               << m_dlInfoListBuffered.at(i).m_rnti);
            }
            for (std::size_t k = 0; k < (*itRlcPdu).second.size(); k++)
            {
                (*itRlcPdu).second.at(k).at(m_dlInfoListBuffered.at(i).m_harqProcessId).clear();
            }
        }
    }
    m_dlInfoListBuffered.clear();
    m_dlInfoListBuffered = dlInfoListUntxed;

    if (rbgAllocatedNum == rbgNum)
    {
        if (!ret.m_buildDataList.empty() || !ret.m_buildRarList.empty())
        {
            m_schedSapUser->SchedDlConfigInd(ret);
        }
        return;
    }

    // -----------------------------------------------------------------------
    // Main RBG allocation loop — M-LWDF metric replaces PF metric here
    // -----------------------------------------------------------------------
    for (int i = 0; i < rbgNum; i++)
    {
        NS_LOG_INFO(this << " ALLOCATION for RBG " << i << " of " << rbgNum);
        if (!rbgMap.at(i))
        {
            auto itMax = m_flowStatsDl.end();
            double rcqiMax = 0.0;

            for (auto it = m_flowStatsDl.begin(); it != m_flowStatsDl.end(); it++)
            {
                if (!m_ffrSapProvider->IsDlRbgAvailableForUe(i, (*it).first))
                {
                    continue;
                }
                auto itRnti = rntiAllocated.find((*it).first);
                if (itRnti != rntiAllocated.end() || !HarqProcessAvailability((*it).first))
                {
                    continue;
                }

                auto itCqi    = m_a30CqiRxed.find((*it).first);
                auto itTxMode = m_uesTxMode.find((*it).first);
                if (itTxMode == m_uesTxMode.end())
                {
                    NS_FATAL_ERROR("No Transmission Mode info on user " << (*it).first);
                }
                auto nLayer = TransmissionModesLayers::TxMode2LayerNum((*itTxMode).second);

                std::vector<uint8_t> sbCqi;
                if (itCqi == m_a30CqiRxed.end())
                {
                    sbCqi = std::vector<uint8_t>(nLayer, 1);
                }
                else
                {
                    sbCqi = (*itCqi).second.m_higherLayerSelected.at(i).m_sbCqi;
                }

                uint8_t cqi1 = sbCqi.at(0);
                uint8_t cqi2 = 0;
                if (sbCqi.size() > 1)
                {
                    cqi2 = sbCqi.at(1);
                }

                if ((cqi1 > 0) || (cqi2 > 0))
                {
                    if (LcActivePerFlow((*it).first) > 0)
                    {
                        double achievableRate = 0.0;
                        uint8_t mcs = 0;
                        for (uint8_t k = 0; k < nLayer; k++)
                        {
                            if (sbCqi.size() > k)
                            {
                                mcs = m_amc->GetMcsFromCqi(sbCqi.at(k));
                            }
                            else
                            {
                                mcs = 0;
                            }
                            achievableRate +=
                                ((m_amc->GetDlTbSizeFromMcs(mcs, rbgSize) / 8) / 0.001);
                        }

                        // M-LWDF metric (falls back to PF for BE flows)
                        double rcqi = ComputeMlwdfMetric((*it).first,
                                                         achievableRate,
                                                         (*it).second.lastAveragedThroughput);

                        NS_LOG_INFO(this << " RNTI " << (*it).first
                                         << " MCS " << (uint32_t)mcs
                                         << " achievableRate " << achievableRate
                                         << " avgThr " << (*it).second.lastAveragedThroughput
                                         << " MLWDF-metric " << rcqi);

                        if (rcqi > rcqiMax)
                        {
                            rcqiMax = rcqi;
                            itMax = it;
                        }
                    }
                }
            }

            if (itMax == m_flowStatsDl.end())
            {
                NS_LOG_INFO(this << " no UE found for RBG " << i);
            }
            else
            {
                rbgMap.at(i) = true;
                auto itMap = allocationMap.find((*itMax).first);
                if (itMap == allocationMap.end())
                {
                    std::vector<uint16_t> tempMap;
                    tempMap.push_back(i);
                    allocationMap[(*itMax).first] = tempMap;
                }
                else
                {
                    (*itMap).second.push_back(i);
                }
                NS_LOG_INFO(this << " UE assigned " << (*itMax).first);
            }
        }
    }

    // reset TTI stats
    for (auto itStats = m_flowStatsDl.begin(); itStats != m_flowStatsDl.end(); itStats++)
    {
        (*itStats).second.lastTtiBytesTransmitted = 0;
    }

    // Generate DCIs for allocated UEs
    auto itMap = allocationMap.begin();
    while (itMap != allocationMap.end())
    {
        BuildDataListElement_s newEl;
        newEl.m_rnti = (*itMap).first;
        DlDciListElement_s newDci;
        newDci.m_rnti = (*itMap).first;
        newDci.m_harqProcess = UpdateHarqProcessId((*itMap).first);

        uint16_t lcActives = LcActivePerFlow((*itMap).first);
        NS_LOG_INFO(this << " Allocate user " << newEl.m_rnti << " lcActives " << lcActives);
        if (lcActives == 0)
        {
            lcActives = (uint16_t)65535;
        }
        uint16_t RbgPerRnti = (*itMap).second.size();
        auto itCqi    = m_a30CqiRxed.find((*itMap).first);
        auto itTxMode = m_uesTxMode.find((*itMap).first);
        if (itTxMode == m_uesTxMode.end())
        {
            NS_FATAL_ERROR("No Transmission Mode info on user " << (*itMap).first);
        }
        auto nLayer = TransmissionModesLayers::TxMode2LayerNum((*itTxMode).second);
        std::vector<uint8_t> worstCqi(2, 15);
        if (itCqi != m_a30CqiRxed.end())
        {
            for (std::size_t k = 0; k < (*itMap).second.size(); k++)
            {
                if ((*itCqi).second.m_higherLayerSelected.size() > (*itMap).second.at(k))
                {
                    for (uint8_t j = 0; j < nLayer; j++)
                    {
                        if ((*itCqi)
                                .second.m_higherLayerSelected.at((*itMap).second.at(k))
                                .m_sbCqi.size() > j)
                        {
                            if (((*itCqi)
                                     .second.m_higherLayerSelected.at((*itMap).second.at(k))
                                     .m_sbCqi.at(j)) < worstCqi.at(j))
                            {
                                worstCqi.at(j) =
                                    ((*itCqi)
                                         .second.m_higherLayerSelected.at((*itMap).second.at(k))
                                         .m_sbCqi.at(j));
                            }
                        }
                        else
                        {
                            worstCqi.at(j) = 1;
                        }
                    }
                }
                else
                {
                    for (uint8_t j = 0; j < nLayer; j++)
                    {
                        worstCqi.at(j) = 1;
                    }
                }
            }
        }
        else
        {
            for (uint8_t j = 0; j < nLayer; j++)
            {
                worstCqi.at(j) = 1;
            }
        }

        uint32_t bytesTxed = 0;
        for (uint8_t j = 0; j < nLayer; j++)
        {
            newDci.m_mcs.push_back(m_amc->GetMcsFromCqi(worstCqi.at(j)));
            int tbSize =
                (m_amc->GetDlTbSizeFromMcs(newDci.m_mcs.at(j), RbgPerRnti * rbgSize) / 8);
            newDci.m_tbsSize.push_back(tbSize);
            bytesTxed += tbSize;
        }

        newDci.m_resAlloc = 0;
        newDci.m_rbBitmap = 0;
        uint32_t rbgMask = 0;
        for (std::size_t k = 0; k < (*itMap).second.size(); k++)
        {
            rbgMask = rbgMask + (0x1 << (*itMap).second.at(k));
        }
        newDci.m_rbBitmap = rbgMask;

        for (auto itBufReq = m_rlcBufferReq.begin(); itBufReq != m_rlcBufferReq.end();
             itBufReq++)
        {
            if (((*itBufReq).first.m_rnti == (*itMap).first) &&
                (((*itBufReq).second.m_rlcTransmissionQueueSize > 0) ||
                 ((*itBufReq).second.m_rlcRetransmissionQueueSize > 0) ||
                 ((*itBufReq).second.m_rlcStatusPduSize > 0)))
            {
                std::vector<RlcPduListElement_s> newRlcPduLe;
                for (uint8_t j = 0; j < nLayer; j++)
                {
                    RlcPduListElement_s newRlcEl;
                    newRlcEl.m_logicalChannelIdentity = (*itBufReq).first.m_lcId;
                    newRlcEl.m_size = newDci.m_tbsSize.at(j) / lcActives;
                    newRlcPduLe.push_back(newRlcEl);
                    UpdateDlRlcBufferInfo(newDci.m_rnti,
                                          newRlcEl.m_logicalChannelIdentity,
                                          newRlcEl.m_size);
                    if (m_harqOn)
                    {
                        auto itRlcPdu =
                            m_dlHarqProcessesRlcPduListBuffer.find((*itMap).first);
                        if (itRlcPdu == m_dlHarqProcessesRlcPduListBuffer.end())
                        {
                            NS_FATAL_ERROR(
                                "Unable to find RlcPdcList in HARQ buffer for RNTI "
                                << (*itMap).first);
                        }
                        (*itRlcPdu).second.at(j).at(newDci.m_harqProcess).push_back(newRlcEl);
                    }
                }
                newEl.m_rlcPduList.push_back(newRlcPduLe);
            }
            if ((*itBufReq).first.m_rnti > (*itMap).first)
            {
                break;
            }
        }

        for (uint8_t j = 0; j < nLayer; j++)
        {
            newDci.m_ndi.push_back(1);
            newDci.m_rv.push_back(0);
        }
        newDci.m_tpc = m_ffrSapProvider->GetTpc((*itMap).first);
        newEl.m_dci = newDci;

        if (m_harqOn)
        {
            auto itDci = m_dlHarqProcessesDciBuffer.find(newEl.m_rnti);
            if (itDci == m_dlHarqProcessesDciBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RNTI entry in DCI HARQ buffer for RNTI "
                               << newEl.m_rnti);
            }
            (*itDci).second.at(newDci.m_harqProcess) = newDci;
            auto itHarqTimer = m_dlHarqProcessesTimer.find(newEl.m_rnti);
            if (itHarqTimer == m_dlHarqProcessesTimer.end())
            {
                NS_FATAL_ERROR("Unable to find HARQ timer for RNTI " << (uint16_t)newEl.m_rnti);
            }
            (*itHarqTimer).second.at(newDci.m_harqProcess) = 0;
        }

        ret.m_buildDataList.push_back(newEl);
        auto it = m_flowStatsDl.find((*itMap).first);
        if (it != m_flowStatsDl.end())
        {
            (*it).second.lastTtiBytesTransmitted = bytesTxed;
        }
        else
        {
            NS_FATAL_ERROR(this << " No Stats for this allocated UE");
        }
        itMap++;
    }
    ret.m_nrOfPdcchOfdmSymbols = 1;

    // Update per-UE average throughput
    NS_LOG_INFO(this << " Update UEs statistics");
    for (auto itStats = m_flowStatsDl.begin(); itStats != m_flowStatsDl.end(); itStats++)
    {
        (*itStats).second.totalBytesTransmitted += (*itStats).second.lastTtiBytesTransmitted;
        (*itStats).second.lastAveragedThroughput =
            ((1.0 - (1.0 / m_timeWindow)) * (*itStats).second.lastAveragedThroughput) +
            ((1.0 / m_timeWindow) *
             (double)((*itStats).second.lastTtiBytesTransmitted / 0.001));
        (*itStats).second.lastTtiBytesTransmitted = 0;
    }

    m_schedSapUser->SchedDlConfigInd(ret);
}

void
MlwdfFfMacScheduler::DoSchedDlRachInfoReq(
    const FfMacSchedSapProvider::SchedDlRachInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_rachList = params.m_rachList;
}

void
MlwdfFfMacScheduler::DoSchedDlCqiInfoReq(
    const FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_ffrSapProvider->ReportDlCqiInfo(params);

    for (unsigned int i = 0; i < params.m_cqiList.size(); i++)
    {
        if (params.m_cqiList.at(i).m_cqiType == CqiListElement_s::P10)
        {
            uint16_t rnti = params.m_cqiList.at(i).m_rnti;
            auto it = m_p10CqiRxed.find(rnti);
            if (it == m_p10CqiRxed.end())
            {
                m_p10CqiRxed[rnti] = params.m_cqiList.at(i).m_wbCqi.at(0);
                m_p10CqiTimers[rnti] = m_cqiTimersThreshold;
            }
            else
            {
                (*it).second = params.m_cqiList.at(i).m_wbCqi.at(0);
                auto itTimers = m_p10CqiTimers.find(rnti);
                (*itTimers).second = m_cqiTimersThreshold;
            }
        }
        else if (params.m_cqiList.at(i).m_cqiType == CqiListElement_s::A30)
        {
            uint16_t rnti = params.m_cqiList.at(i).m_rnti;
            auto it = m_a30CqiRxed.find(rnti);
            if (it == m_a30CqiRxed.end())
            {
                m_a30CqiRxed[rnti] = params.m_cqiList.at(i).m_sbMeasResult;
                m_a30CqiTimers[rnti] = m_cqiTimersThreshold;
            }
            else
            {
                (*it).second = params.m_cqiList.at(i).m_sbMeasResult;
                auto itTimers = m_a30CqiTimers.find(rnti);
                (*itTimers).second = m_cqiTimersThreshold;
            }
        }
        else
        {
            NS_LOG_ERROR(this << " CQI type unknown");
        }
    }
}

double
MlwdfFfMacScheduler::EstimateUlSinr(uint16_t rnti, uint16_t rb)
{
    auto itCqi = m_ueCqi.find(rnti);
    if (itCqi == m_ueCqi.end())
    {
        return NO_SINR;
    }
    else
    {
        double sinrSum = 0;
        unsigned int sinrNum = 0;
        for (uint32_t i = 0; i < m_cschedCellConfig.m_ulBandwidth; i++)
        {
            double sinr = (*itCqi).second.at(i);
            if (sinr != NO_SINR)
            {
                sinrSum += sinr;
                sinrNum++;
            }
        }
        double estimatedSinr = (sinrNum > 0) ? (sinrSum / sinrNum) : DBL_MAX;
        (*itCqi).second.at(rb) = estimatedSinr;
        return estimatedSinr;
    }
}

void
MlwdfFfMacScheduler::DoSchedUlTriggerReq(
    const FfMacSchedSapProvider::SchedUlTriggerReqParameters& params)
{
    NS_LOG_FUNCTION(this << " UL - Frame no. " << (params.m_sfnSf >> 4) << " subframe no. "
                         << (0xF & params.m_sfnSf) << " size " << params.m_ulInfoList.size());

    RefreshUlCqiMaps();
    m_ffrSapProvider->ReportUlCqiInfo(m_ueCqi);

    FfMacSchedSapUser::SchedUlConfigIndParameters ret;
    std::vector<bool> rbMap;
    uint16_t rbAllocatedNum = 0;
    std::set<uint16_t> rntiAllocated;
    std::vector<uint16_t> rbgAllocationMap;
    rbgAllocationMap = m_rachAllocationMap;
    m_rachAllocationMap.clear();
    m_rachAllocationMap.resize(m_cschedCellConfig.m_ulBandwidth, 0);

    rbMap.resize(m_cschedCellConfig.m_ulBandwidth, false);
    rbMap = m_ffrSapProvider->GetAvailableUlRbg();

    for (auto it = rbMap.begin(); it != rbMap.end(); it++)
    {
        if (*it)
        {
            rbAllocatedNum++;
        }
    }

    uint8_t minContinuousUlBandwidth = m_ffrSapProvider->GetMinContinuousUlBandwidth();
    uint8_t ffrUlBandwidth = m_cschedCellConfig.m_ulBandwidth - rbAllocatedNum;

    for (uint16_t i = 0; i < m_cschedCellConfig.m_ulBandwidth; i++)
    {
        if (rbgAllocationMap.at(i) != 0)
        {
            rbMap.at(i) = true;
        }
    }

    if (m_harqOn)
    {
        for (std::size_t i = 0; i < params.m_ulInfoList.size(); i++)
        {
            if (params.m_ulInfoList.at(i).m_receptionStatus == UlInfoListElement_s::NotOk)
            {
                uint16_t rnti = params.m_ulInfoList.at(i).m_rnti;
                auto itProcId = m_ulHarqCurrentProcessId.find(rnti);
                if (itProcId == m_ulHarqCurrentProcessId.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) " << rnti);
                }
                uint8_t harqId = (uint8_t)((*itProcId).second - HARQ_PERIOD) % HARQ_PROC_NUM;
                auto itHarq = m_ulHarqProcessesDciBuffer.find(rnti);
                if (itHarq == m_ulHarqProcessesDciBuffer.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) " << rnti);
                    continue;
                }
                UlDciListElement_s dci = (*itHarq).second.at(harqId);
                auto itStat = m_ulHarqProcessesStatus.find(rnti);
                if (itStat == m_ulHarqProcessesStatus.end())
                {
                    NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) " << rnti);
                }
                if ((*itStat).second.at(harqId) >= 3)
                {
                    NS_LOG_INFO("Max number of retransmissions reached (UL) -> drop process");
                    continue;
                }
                bool free = true;
                for (int j = dci.m_rbStart; j < dci.m_rbStart + dci.m_rbLen; j++)
                {
                    if (rbMap.at(j))
                    {
                        free = false;
                        break;
                    }
                }
                if (free)
                {
                    for (int j = dci.m_rbStart; j < dci.m_rbStart + dci.m_rbLen; j++)
                    {
                        rbMap.at(j) = true;
                        rbgAllocationMap.at(j) = dci.m_rnti;
                        rbAllocatedNum++;
                    }
                }
                else
                {
                    NS_LOG_INFO("Cannot allocate retx due to RACH allocations for UE " << rnti);
                    continue;
                }
                dci.m_ndi = 0;
                (*itStat).second.at((*itProcId).second) = (*itStat).second.at(harqId) + 1;
                (*itStat).second.at(harqId) = 0;
                (*itHarq).second.at((*itProcId).second) = dci;
                ret.m_dciList.push_back(dci);
                rntiAllocated.insert(dci.m_rnti);
            }
        }
    }

    std::map<uint16_t, uint32_t>::iterator it;
    int nflows = 0;
    for (it = m_ceBsrRxed.begin(); it != m_ceBsrRxed.end(); it++)
    {
        auto itRnti = rntiAllocated.find((*it).first);
        if (((*it).second > 0) && (itRnti == rntiAllocated.end()))
        {
            nflows++;
        }
    }

    if (nflows == 0)
    {
        if (!ret.m_dciList.empty())
        {
            m_allocationMaps[params.m_sfnSf] = rbgAllocationMap;
            m_schedSapUser->SchedUlConfigInd(ret);
        }
        return;
    }

    uint16_t tempRbPerFlow = (ffrUlBandwidth) / (nflows + rntiAllocated.size());
    uint16_t rbPerFlow =
        (minContinuousUlBandwidth < tempRbPerFlow) ? minContinuousUlBandwidth : tempRbPerFlow;

    if (rbPerFlow < 3)
    {
        rbPerFlow = 3;
    }

    int rbAllocated = 0;

    if (m_nextRntiUl != 0)
    {
        for (it = m_ceBsrRxed.begin(); it != m_ceBsrRxed.end(); it++)
        {
            if ((*it).first == m_nextRntiUl)
            {
                break;
            }
        }
        if (it == m_ceBsrRxed.end())
        {
            NS_LOG_ERROR(this << " no user found");
        }
    }
    else
    {
        it = m_ceBsrRxed.begin();
        m_nextRntiUl = (*it).first;
    }

    do
    {
        auto itRnti = rntiAllocated.find((*it).first);
        if ((itRnti != rntiAllocated.end()) || ((*it).second == 0))
        {
            it++;
            if (it == m_ceBsrRxed.end())
            {
                it = m_ceBsrRxed.begin();
            }
            continue;
        }
        if (rbAllocated + rbPerFlow - 1 > m_cschedCellConfig.m_ulBandwidth)
        {
            rbPerFlow = m_cschedCellConfig.m_ulBandwidth - rbAllocated;
            if (rbPerFlow < 3)
            {
                rbPerFlow = 0;
            }
        }

        rbAllocated = 0;
        UlDciListElement_s uldci;
        uldci.m_rnti = (*it).first;
        uldci.m_rbLen = rbPerFlow;
        bool allocated = false;

        while ((!allocated) &&
               ((rbAllocated + rbPerFlow - m_cschedCellConfig.m_ulBandwidth) < 1) &&
               (rbPerFlow != 0))
        {
            bool free = true;
            for (int j = rbAllocated; j < rbAllocated + rbPerFlow; j++)
            {
                if (rbMap.at(j))
                {
                    free = false;
                    break;
                }
                if (!m_ffrSapProvider->IsUlRbgAvailableForUe(j, (*it).first))
                {
                    free = false;
                    break;
                }
            }
            if (free)
            {
                uldci.m_rbStart = rbAllocated;
                for (int j = rbAllocated; j < rbAllocated + rbPerFlow; j++)
                {
                    rbMap.at(j) = true;
                    rbgAllocationMap.at(j) = (*it).first;
                }
                rbAllocated += rbPerFlow;
                allocated = true;
                break;
            }
            rbAllocated++;
            if (rbAllocated + rbPerFlow - 1 > m_cschedCellConfig.m_ulBandwidth)
            {
                rbPerFlow = m_cschedCellConfig.m_ulBandwidth - rbAllocated;
                if (rbPerFlow < 3)
                {
                    rbPerFlow = 0;
                }
            }
        }
        if (!allocated)
        {
            m_nextRntiUl = (*it).first;
            break;
        }

        auto itCqi = m_ueCqi.find((*it).first);
        int cqi = 0;
        if (itCqi == m_ueCqi.end())
        {
            uldci.m_mcs = 0;
        }
        else
        {
            NS_ABORT_MSG_IF((*itCqi).second.empty(),
                            "CQI of RNTI = " << (*it).first << " has expired");
            double minSinr = (*itCqi).second.at(uldci.m_rbStart);
            if (minSinr == NO_SINR)
            {
                minSinr = EstimateUlSinr((*it).first, uldci.m_rbStart);
            }
            for (uint16_t i = uldci.m_rbStart; i < uldci.m_rbStart + uldci.m_rbLen; i++)
            {
                double sinr = (*itCqi).second.at(i);
                if (sinr == NO_SINR)
                {
                    sinr = EstimateUlSinr((*it).first, i);
                }
                if (sinr < minSinr)
                {
                    minSinr = sinr;
                }
            }
            double s = log2(
                1 + (std::pow(10, minSinr / 10) / ((-std::log(5.0 * 0.00005)) / 1.5)));
            cqi = m_amc->GetCqiFromSpectralEfficiency(s);
            if (cqi == 0)
            {
                it++;
                if (it == m_ceBsrRxed.end())
                {
                    it = m_ceBsrRxed.begin();
                }
                for (uint16_t i = uldci.m_rbStart; i < uldci.m_rbStart + uldci.m_rbLen; i++)
                {
                    rbgAllocationMap.at(i) = 0;
                }
                continue;
            }
            uldci.m_mcs = m_amc->GetMcsFromCqi(cqi);
        }

        uldci.m_tbSize = (m_amc->GetUlTbSizeFromMcs(uldci.m_mcs, rbPerFlow) / 8);
        UpdateUlRlcBufferInfo(uldci.m_rnti, uldci.m_tbSize);
        uldci.m_ndi = 1;
        uldci.m_cceIndex = 0;
        uldci.m_aggrLevel = 1;
        uldci.m_ueTxAntennaSelection = 3;
        uldci.m_hopping = false;
        uldci.m_n2Dmrs = 0;
        uldci.m_tpc = 0;
        uldci.m_cqiRequest = false;
        uldci.m_ulIndex = 0;
        uldci.m_dai = 1;
        uldci.m_freqHopping = 0;
        uldci.m_pdcchPowerOffset = 0;
        ret.m_dciList.push_back(uldci);

        uint8_t harqId = 0;
        if (m_harqOn)
        {
            auto itProcId = m_ulHarqCurrentProcessId.find(uldci.m_rnti);
            if (itProcId == m_ulHarqCurrentProcessId.end())
            {
                NS_FATAL_ERROR("No info find in HARQ buffer for UE " << uldci.m_rnti);
            }
            harqId = (*itProcId).second;
            auto itDci = m_ulHarqProcessesDciBuffer.find(uldci.m_rnti);
            if (itDci == m_ulHarqProcessesDciBuffer.end())
            {
                NS_FATAL_ERROR("Unable to find RNTI entry in UL DCI HARQ buffer for RNTI "
                               << uldci.m_rnti);
            }
            (*itDci).second.at(harqId) = uldci;
            auto itStat = m_ulHarqProcessesStatus.find(uldci.m_rnti);
            if (itStat == m_ulHarqProcessesStatus.end())
            {
                NS_LOG_ERROR("No info find in HARQ buffer for UE (might change eNB) "
                             << uldci.m_rnti);
            }
            (*itStat).second.at(harqId) = 0;
        }

        auto itStats = m_flowStatsUl.find((*it).first);
        if (itStats != m_flowStatsUl.end())
        {
            (*itStats).second.lastTtiBytesTransmitted = uldci.m_tbSize;
        }

        it++;
        if (it == m_ceBsrRxed.end())
        {
            it = m_ceBsrRxed.begin();
        }
        if ((rbAllocated == m_cschedCellConfig.m_ulBandwidth) || (rbPerFlow == 0))
        {
            m_nextRntiUl = (*it).first;
            break;
        }
    } while (((*it).first != m_nextRntiUl) && (rbPerFlow != 0));

    for (auto itStats = m_flowStatsUl.begin(); itStats != m_flowStatsUl.end(); itStats++)
    {
        (*itStats).second.totalBytesTransmitted += (*itStats).second.lastTtiBytesTransmitted;
        (*itStats).second.lastAveragedThroughput =
            ((1.0 - (1.0 / m_timeWindow)) * (*itStats).second.lastAveragedThroughput) +
            ((1.0 / m_timeWindow) *
             (double)((*itStats).second.lastTtiBytesTransmitted / 0.001));
        (*itStats).second.lastTtiBytesTransmitted = 0;
    }
    m_allocationMaps[params.m_sfnSf] = rbgAllocationMap;
    m_schedSapUser->SchedUlConfigInd(ret);
}

void
MlwdfFfMacScheduler::DoSchedUlNoiseInterferenceReq(
    const FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters& params)
{
    NS_LOG_FUNCTION(this);
}

void
MlwdfFfMacScheduler::DoSchedUlSrInfoReq(
    const FfMacSchedSapProvider::SchedUlSrInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
}

void
MlwdfFfMacScheduler::DoSchedUlMacCtrlInfoReq(
    const FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);

    for (unsigned int i = 0; i < params.m_macCeList.size(); i++)
    {
        if (params.m_macCeList.at(i).m_macCeType == MacCeListElement_s::BSR)
        {
            uint32_t buffer = 0;
            for (uint8_t lcg = 0; lcg < 4; ++lcg)
            {
                uint8_t bsrId =
                    params.m_macCeList.at(i).m_macCeValue.m_bufferStatus.at(lcg);
                buffer += BufferSizeLevelBsr::BsrId2BufferSize(bsrId);
            }
            uint16_t rnti = params.m_macCeList.at(i).m_rnti;
            auto it = m_ceBsrRxed.find(rnti);
            if (it == m_ceBsrRxed.end())
            {
                m_ceBsrRxed[rnti] = buffer;
            }
            else
            {
                (*it).second = buffer;
            }
        }
    }
}

void
MlwdfFfMacScheduler::DoSchedUlCqiInfoReq(
    const FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    m_ffrSapProvider->ReportUlCqiInfo(params);

    switch (m_ulCqiFilter)
    {
    case FfMacScheduler::SRS_UL_CQI:
        if (params.m_ulCqi.m_type != UlCqi_s::SRS)
        {
            return;
        }
        break;
    case FfMacScheduler::PUSCH_UL_CQI:
        if (params.m_ulCqi.m_type != UlCqi_s::PUSCH)
        {
            return;
        }
        break;
    default:
        NS_FATAL_ERROR("Unknown UL CQI type");
    }

    switch (params.m_ulCqi.m_type)
    {
    case UlCqi_s::PUSCH: {
        auto itMap = m_allocationMaps.find(params.m_sfnSf);
        if (itMap == m_allocationMaps.end())
        {
            return;
        }
        for (uint32_t i = 0; i < (*itMap).second.size(); i++)
        {
            double sinr = LteFfConverter::fpS11dot3toDouble(params.m_ulCqi.m_sinr.at(i));
            auto itCqi = m_ueCqi.find((*itMap).second.at(i));
            if (itCqi == m_ueCqi.end())
            {
                std::vector<double> newCqi;
                for (uint32_t j = 0; j < m_cschedCellConfig.m_ulBandwidth; j++)
                {
                    newCqi.push_back((i == j) ? sinr : NO_SINR);
                }
                m_ueCqi[(*itMap).second.at(i)] = newCqi;
                m_ueCqiTimers[(*itMap).second.at(i)] = m_cqiTimersThreshold;
            }
            else
            {
                (*itCqi).second.at(i) = sinr;
                auto itTimers = m_ueCqiTimers.find((*itMap).second.at(i));
                (*itTimers).second = m_cqiTimersThreshold;
            }
        }
        m_allocationMaps.erase(itMap);
    }
    break;
    case UlCqi_s::SRS: {
        uint16_t rnti = 0;
        NS_ASSERT(!params.m_vendorSpecificList.empty());
        for (std::size_t i = 0; i < params.m_vendorSpecificList.size(); i++)
        {
            if (params.m_vendorSpecificList.at(i).m_type == SRS_CQI_RNTI_VSP)
            {
                Ptr<SrsCqiRntiVsp> vsp =
                    DynamicCast<SrsCqiRntiVsp>(params.m_vendorSpecificList.at(i).m_value);
                rnti = vsp->GetRnti();
            }
        }
        auto itCqi = m_ueCqi.find(rnti);
        if (itCqi == m_ueCqi.end())
        {
            std::vector<double> newCqi;
            for (uint32_t j = 0; j < m_cschedCellConfig.m_ulBandwidth; j++)
            {
                newCqi.push_back(
                    LteFfConverter::fpS11dot3toDouble(params.m_ulCqi.m_sinr.at(j)));
            }
            m_ueCqi[rnti] = newCqi;
            m_ueCqiTimers[rnti] = m_cqiTimersThreshold;
        }
        else
        {
            for (uint32_t j = 0; j < m_cschedCellConfig.m_ulBandwidth; j++)
            {
                (*itCqi).second.at(j) =
                    LteFfConverter::fpS11dot3toDouble(params.m_ulCqi.m_sinr.at(j));
            }
            auto itTimers = m_ueCqiTimers.find(rnti);
            (*itTimers).second = m_cqiTimersThreshold;
        }
    }
    break;
    case UlCqi_s::PUCCH_1:
    case UlCqi_s::PUCCH_2:
    case UlCqi_s::PRACH:
        NS_FATAL_ERROR("MlwdfFfMacScheduler supports only PUSCH and SRS UL-CQIs");
        break;
    default:
        NS_FATAL_ERROR("Unknown type of UL-CQI");
    }
}

void
MlwdfFfMacScheduler::RefreshDlCqiMaps()
{
    auto itP10 = m_p10CqiTimers.begin();
    while (itP10 != m_p10CqiTimers.end())
    {
        if ((*itP10).second == 0)
        {
            auto itMap = m_p10CqiRxed.find((*itP10).first);
            NS_ASSERT_MSG(itMap != m_p10CqiRxed.end(),
                          " Does not find CQI report for user " << (*itP10).first);
            m_p10CqiRxed.erase(itMap);
            auto temp = itP10;
            itP10++;
            m_p10CqiTimers.erase(temp);
        }
        else
        {
            (*itP10).second--;
            itP10++;
        }
    }

    auto itA30 = m_a30CqiTimers.begin();
    while (itA30 != m_a30CqiTimers.end())
    {
        if ((*itA30).second == 0)
        {
            auto itMap = m_a30CqiRxed.find((*itA30).first);
            NS_ASSERT_MSG(itMap != m_a30CqiRxed.end(),
                          " Does not find CQI report for user " << (*itA30).first);
            m_a30CqiRxed.erase(itMap);
            auto temp = itA30;
            itA30++;
            m_a30CqiTimers.erase(temp);
        }
        else
        {
            (*itA30).second--;
            itA30++;
        }
    }
}

void
MlwdfFfMacScheduler::RefreshUlCqiMaps()
{
    auto itUl = m_ueCqiTimers.begin();
    while (itUl != m_ueCqiTimers.end())
    {
        if ((*itUl).second == 0)
        {
            auto itMap = m_ueCqi.find((*itUl).first);
            NS_ASSERT_MSG(itMap != m_ueCqi.end(),
                          " Does not find CQI report for user " << (*itUl).first);
            (*itMap).second.clear();
            m_ueCqi.erase(itMap);
            auto temp = itUl;
            itUl++;
            m_ueCqiTimers.erase(temp);
        }
        else
        {
            (*itUl).second--;
            itUl++;
        }
    }
}

void
MlwdfFfMacScheduler::UpdateDlRlcBufferInfo(uint16_t rnti, uint8_t lcid, uint16_t size)
{
    LteFlowId_t flow(rnti, lcid);
    auto it = m_rlcBufferReq.find(flow);
    if (it != m_rlcBufferReq.end())
    {
        if (((*it).second.m_rlcStatusPduSize > 0) && (size >= (*it).second.m_rlcStatusPduSize))
        {
            (*it).second.m_rlcStatusPduSize = 0;
        }
        else if (((*it).second.m_rlcRetransmissionQueueSize > 0) &&
                 (size >= (*it).second.m_rlcRetransmissionQueueSize))
        {
            (*it).second.m_rlcRetransmissionQueueSize = 0;
        }
        else if ((*it).second.m_rlcTransmissionQueueSize > 0)
        {
            uint32_t rlcOverhead = (lcid == 1) ? 4 : 2;
            if ((*it).second.m_rlcTransmissionQueueSize <= size - rlcOverhead)
            {
                (*it).second.m_rlcTransmissionQueueSize = 0;
            }
            else
            {
                (*it).second.m_rlcTransmissionQueueSize -= size - rlcOverhead;
            }
        }
    }
    else
    {
        NS_LOG_ERROR(this << " Does not find DL RLC Buffer Report of UE " << rnti);
    }
}

void
MlwdfFfMacScheduler::UpdateUlRlcBufferInfo(uint16_t rnti, uint16_t size)
{
    size = size - 2;
    auto it = m_ceBsrRxed.find(rnti);
    if (it != m_ceBsrRxed.end())
    {
        if ((*it).second >= size)
        {
            (*it).second -= size;
        }
        else
        {
            (*it).second = 0;
        }
    }
    else
    {
        NS_LOG_ERROR(this << " Does not find BSR report info of UE " << rnti);
    }
}

void
MlwdfFfMacScheduler::TransmissionModeConfigurationUpdate(uint16_t rnti, uint8_t txMode)
{
    NS_LOG_FUNCTION(this << " RNTI " << rnti << " txMode " << (uint16_t)txMode);
    FfMacCschedSapUser::CschedUeConfigUpdateIndParameters params{};
    params.m_rnti = rnti;
    params.m_transmissionMode = txMode;
    m_cschedSapUser->CschedUeConfigUpdateInd(params);
}

} // namespace ns3
