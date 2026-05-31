/*
 * ofdma-scheduler-eval.cc
 *
 * Script principal para evaluar paradigmas de scheduling OFDMA en LTE (ns-3.47).
 * Implementa los 8 grupos de escenario del diseño experimental.
 *
 * Uso:
 *   ./ns3 run "ofdma-scheduler-eval \
 *       --scheduler=pf --nUEs=20 --spatial=uniform \
 *       --traffic=homogeneous --runId=1 \
 *       --simTime=30 --outputDir=results/raw/pf_u_h_n20_r1"
 *
 * Schedulers: rr | bet | mt | tta | pf | mlwdf | pss
 * Spatial:    uniform | clustered
 * Traffic:    homogeneous | heterogeneous
 *
 * Cambios aplicados:
 *   Fix1 - Tráfico heterogéneo interleaved (no correlacionado con clusters)
 *   Fix2 - Fading Rayleigh via TraceFadingLossModel (EPA 3 km/h)
 *   Fix3 - Tráfico homogéneo full-buffer (OnOff always-on, 10 Mbps/UE)
 *   Fix5 - FlowMonitor E2E → FlowStats.csv
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OfdmaSchedulerEval");

static void
MkdirP(const std::string& path)
{
    std::string cmd = "mkdir -p \"" + path + "\"";
    if (std::system(cmd.c_str()) != 0)
    {
        NS_LOG_WARN("Could not create output directory: " << path);
    }
}

int
main(int argc, char* argv[])
{
    std::string scheduler  = "pf";
    uint32_t    nUEs       = 20;
    std::string spatial    = "uniform";
    std::string traffic    = "homogeneous";
    uint32_t    runId      = 1;
    double      simTime    = 30.0;
    std::string outputDir  = "results/raw/default";

    CommandLine cmd(__FILE__);
    cmd.AddValue("scheduler",  "Scheduler: rr|bet|mt|tta|pf|mlwdf|pss",    scheduler);
    cmd.AddValue("nUEs",       "Number of UEs (10, 20 or 40)",             nUEs);
    cmd.AddValue("spatial",    "Spatial distribution: uniform|clustered",  spatial);
    cmd.AddValue("traffic",    "Traffic model: homogeneous|heterogeneous", traffic);
    cmd.AddValue("runId",      "Run ID for RNG (1-20)",                    runId);
    cmd.AddValue("simTime",    "Simulation duration [s]",                  simTime);
    cmd.AddValue("outputDir",  "Directory for trace output files",         outputDir);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(12345);
    RngSeedManager::SetRun(runId);

    // Taxonomía Capozzi 2-3-2
    static const std::map<std::string, std::string> schedulerTypes = {
        {"rr",    "ns3::RrFfMacScheduler"},
        {"bet",   "ns3::FdBetFfMacScheduler"},
        {"mt",    "ns3::TdMtFfMacScheduler"},
        {"tta",   "ns3::TtaFfMacScheduler"},
        {"pf",    "ns3::PfFfMacScheduler"},
        {"mlwdf", "ns3::MlwdfFfMacScheduler"},
        {"pss",   "ns3::PssFfMacScheduler"},
    };
    NS_ABORT_MSG_IF(schedulerTypes.find(scheduler) == schedulerTypes.end(),
                    "Unknown scheduler '" << scheduler << "'. Use: rr bet mt tta pf mlwdf pss");
    NS_ABORT_MSG_IF(spatial != "uniform" && spatial != "clustered",
                    "Unknown spatial '" << spatial << "'. Use: uniform clustered");
    NS_ABORT_MSG_IF(traffic != "homogeneous" && traffic != "heterogeneous",
                    "Unknown traffic '" << traffic << "'. Use: homogeneous heterogeneous");

    MkdirP(outputDir);

    // ---- Redirect LTE trace files to outputDir ----
    Config::SetDefault("ns3::RadioBearerStatsCalculator::DlRlcOutputFilename",
                       StringValue(outputDir + "/DlRlcStats.txt"));
    Config::SetDefault("ns3::RadioBearerStatsCalculator::UlRlcOutputFilename",
                       StringValue(outputDir + "/UlRlcStats.txt"));
    Config::SetDefault("ns3::RadioBearerStatsCalculator::DlPdcpOutputFilename",
                       StringValue(outputDir + "/DlPdcpStats.txt"));
    Config::SetDefault("ns3::RadioBearerStatsCalculator::UlPdcpOutputFilename",
                       StringValue(outputDir + "/UlPdcpStats.txt"));
    Config::SetDefault("ns3::MacStatsCalculator::DlOutputFilename",
                       StringValue(outputDir + "/DlMacStats.txt"));
    Config::SetDefault("ns3::MacStatsCalculator::UlOutputFilename",
                       StringValue(outputDir + "/UlMacStats.txt"));

    // ---- LTE Helper ----
    // 46 dBm: macro eNB estándar; cubre 490 m con SINR > 0 dB bajo log-distance exp=3.5
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(46.0));

    Ptr<LteHelper>             lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Scheduler
    lteHelper->SetSchedulerType(schedulerTypes.at(scheduler));
    if (scheduler == "pss")
    {
        lteHelper->SetSchedulerAttribute("nMux",               UintegerValue(10));
        lteHelper->SetSchedulerAttribute("PssFdSchedulerType", StringValue("PFsch"));
    }

    // Canal: 10 MHz DL (50 RBs), 2.1 GHz, log-distance path loss
    lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(50));
    lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(50));
    lteHelper->SetEnbDeviceAttribute("DlEarfcn",    UintegerValue(100));

    lteHelper->SetAttribute("PathlossModel",
                            StringValue("ns3::LogDistancePropagationLossModel"));
    lteHelper->SetPathlossModelAttribute("Exponent",      DoubleValue(3.5));
    lteHelper->SetPathlossModelAttribute("ReferenceLoss", DoubleValue(46.7));

    // FIX2: Fading Rayleigh — TraceFadingLossModel con traza EPA 3 km/h (peatonal)
    // UEs estáticos con variación temporal del canal por fading (cada TTI diferente).
    // Sin este modelo, UEs a igual distancia tienen CQI idéntico — schedulers channel-aware
    // no pueden explotar diversidad multiusuario.
    lteHelper->SetAttribute("FadingModel", StringValue("ns3::TraceFadingLossModel"));
    lteHelper->SetFadingModelAttribute("TraceFilename",
        StringValue("src/lte/model/fading-traces/fading_trace_EPA_3kmph.fad"));
    lteHelper->SetFadingModelAttribute("TraceLength",  TimeValue(Seconds(10.0)));
    lteHelper->SetFadingModelAttribute("SamplesNum",   UintegerValue(10000));
    lteHelper->SetFadingModelAttribute("WindowSize",   TimeValue(Seconds(0.5)));
    lteHelper->SetFadingModelAttribute("RbNum",        UintegerValue(100));

    // ---- Internet / EPC backbone ----
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node>       remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu",      UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay",   TimeValue(MilliSeconds(1)));
    NetDeviceContainer      internetDevices  = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer  internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address             remoteHostAddr   = internetIpIfaces.GetAddress(1);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting>  remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                               Ipv4Mask("255.0.0.0"), 1);

    // ---- eNB: celda única en el origen ----
    NodeContainer enbNodes;
    enbNodes.Create(1);
    {
        MobilityHelper enbMob;
        enbMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        Ptr<ListPositionAllocator> pa = CreateObject<ListPositionAllocator>();
        pa->Add(Vector(0.0, 0.0, 30.0));  // antena a 30 m de altura
        enbMob.SetPositionAllocator(pa);
        enbMob.Install(enbNodes);
    }

    // ---- UEs: asignación de posiciones ----
    NodeContainer ueNodes;
    ueNodes.Create(nUEs);
    {
        MobilityHelper ueMob;
        ueMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");

        if (spatial == "uniform")
        {
            // D1: disco uniforme de 490 m centrado en el eNB
            Ptr<UniformDiscPositionAllocator> pa = CreateObject<UniformDiscPositionAllocator>();
            pa->SetX(0.0);
            pa->SetY(0.0);
            pa->SetRho(490.0);
            ueMob.SetPositionAllocator(pa);
            ueMob.Install(ueNodes);
        }
        else // clustered
        {
            // D2: 3 clusters a distancias DISTINTAS — cerca / medio / lejos
            // Requerimiento del proyecto: "condiciones de canal heterogéneas (distancia al eNodeB)"
            //   C1 cerca:  centro 100 m (0°)   → UEs a  30-170 m → SINR alto
            //   C2 medio:  centro 250 m (120°) → UEs a 180-320 m → SINR moderado
            //   C3 lejos:  centro 400 m (240°) → UEs a 330-470 m → SINR bajo
            const std::vector<std::pair<double, double>> centres = {
                { 100.0,    0.0},   // C1: (100·cos0°,   100·sin0°)
                {-125.0,  216.0},   // C2: (250·cos120°, 250·sin120°)
                {-200.0, -346.0}};  // C3: (400·cos240°, 400·sin240°)
            const double clusterR = 70.0;
            uint32_t     base     = nUEs / 3;
            uint32_t     ueIdx   = 0;

            for (std::size_t c = 0; c < centres.size(); ++c)
            {
                uint32_t cnt = (c < 2) ? base : (nUEs - 2 * base);
                Ptr<UniformDiscPositionAllocator> pa =
                    CreateObject<UniformDiscPositionAllocator>();
                pa->SetX(centres[c].first);
                pa->SetY(centres[c].second);
                pa->SetRho(clusterR);

                for (uint32_t i = 0; i < cnt; ++i)
                {
                    ueMob.SetPositionAllocator(pa);
                    ueMob.Install(ueNodes.Get(ueIdx++));
                }
            }
        }
    }

    // ---- Instalar dispositivos LTE ----
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs  = lteHelper->InstallUeDevice(ueNodes);

    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface =
        epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Ipv4StaticRouting> ueRoute =
            ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(u)->GetObject<Ipv4>());
        ueRoute->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // ---- Aplicaciones de tráfico ----
    const double   appStart = 1.0;
    const double   appStop  = simTime;
    const uint16_t basePort = 5000;

    ApplicationContainer serverApps, clientApps;

    if (traffic == "homogeneous")
    {
        // FIX3: Full-buffer — OnOff always-on a 10 Mbps/UE
        // Con CBR (1 Mbps fijo) el buffer se vacía rápido y schedulers channel-aware
        // no pueden explotar diversidad. Full-buffer mantiene siempre demanda en cola.
        // 10 Mbps × N UEs >> capacidad celda (~20 Mbps) → régimen de saturación real.
        for (uint32_t u = 0; u < nUEs; ++u)
        {
            uint16_t port = basePort + u;

            PacketSinkHelper sink("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            serverApps.Add(sink.Install(ueNodes.Get(u)));

            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(ueIpIface.GetAddress(u), port));
            onoff.SetConstantRate(DataRate("10Mbps"), 1250);
            onoff.SetAttribute("OnTime",
                StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime",
                StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            clientApps.Add(onoff.Install(remoteHost));
        }
    }
    else // heterogeneous
    {
        // FIX1: Asignación interleaved de tipos de tráfico (u % 3)
        // Con asignación secuencial: C1=video, C2=gaming, C3=BE → confunde H3/H4
        // Con interleaved: cada cluster recibe los 3 tipos de tráfico mezclados.
        //   u % 3 == 0 → VIDEO    (GBR QCI-4, 1.5 Mbps)
        //   u % 3 == 1 → GAMING   (GBR QCI-3, 64 kbps)
        //   u % 3 == 2 → BEST-EFFORT (non-GBR, full-buffer)
        for (uint32_t u = 0; u < nUEs; ++u)
        {
            uint16_t port = basePort + u;

            PacketSinkHelper sink("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            serverApps.Add(sink.Install(ueNodes.Get(u)));

            if (u % 3 == 0)
            {
                // VIDEO — GBR_NON_CONV_VIDEO (QCI 4): tau=300ms, delta=1e-6
                GbrQosInformation gbrInfo;
                gbrInfo.gbrDl = 1500000;
                gbrInfo.mbrDl = 2000000;
                EpsBearer videoBearer(EpsBearer::GBR_NON_CONV_VIDEO, gbrInfo);

                Ptr<EpcTft> tft = Create<EpcTft>();
                EpcTft::PacketFilter pf;
                pf.direction      = EpcTft::DOWNLINK;
                pf.localPortStart = port;
                pf.localPortEnd   = port;
                tft->Add(pf);
                lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(u), videoBearer, tft);

                // CBR 1.5 Mbps: paquetes de 1000 B cada 5.3 ms
                UdpClientHelper client(ueIpIface.GetAddress(u), port);
                client.SetAttribute("Interval",   TimeValue(MicroSeconds(5333)));
                client.SetAttribute("PacketSize", UintegerValue(1000));
                client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                clientApps.Add(client.Install(remoteHost));
            }
            else if (u % 3 == 1)
            {
                // LOW-LATENCY GAMING — GBR_GAMING (QCI 3): tau=50ms, delta=1e-3
                GbrQosInformation gbrInfo;
                gbrInfo.gbrDl = 64000;
                gbrInfo.mbrDl = 100000;
                EpsBearer latBearer(EpsBearer::GBR_GAMING, gbrInfo);

                Ptr<EpcTft> tft = Create<EpcTft>();
                EpcTft::PacketFilter pf;
                pf.direction      = EpcTft::DOWNLINK;
                pf.localPortStart = port;
                pf.localPortEnd   = port;
                tft->Add(pf);
                lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(u), latBearer, tft);

                // VoIP-like: 160 B cada 20 ms = 64 kbps
                UdpClientHelper client(ueIpIface.GetAddress(u), port);
                client.SetAttribute("Interval",   TimeValue(MilliSeconds(20)));
                client.SetAttribute("PacketSize", UintegerValue(160));
                client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                clientApps.Add(client.Install(remoteHost));
            }
            else
            {
                // BEST-EFFORT — non-GBR, full-buffer (mismo modelo que homogéneo)
                OnOffHelper onoff("ns3::UdpSocketFactory",
                                  InetSocketAddress(ueIpIface.GetAddress(u), port));
                onoff.SetConstantRate(DataRate("10Mbps"), 1250);
                onoff.SetAttribute("OnTime",
                    StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime",
                    StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                clientApps.Add(onoff.Install(remoteHost));
            }
        }
    }

    serverApps.Start(Seconds(appStart));
    serverApps.Stop(Seconds(appStop));
    clientApps.Start(Seconds(appStart));
    clientApps.Stop(Seconds(appStop));

    // ---- Habilitar trazas LTE ----
    lteHelper->EnableRlcTraces();
    lteHelper->EnablePdcpTraces();
    lteHelper->EnableMacTraces();

    // FIX5: FlowMonitor — captura métricas E2E (delay, PLR, throughput por flujo IP)
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor>  monitor = flowmon.InstallAll();

    // ---- Ejecutar simulación ----
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // FIX5: Escribir FlowStats.csv con métricas E2E compactas
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    auto flowStats = monitor->GetFlowStats();

    std::ofstream e2eFile(outputDir + "/FlowStats.csv");
    e2eFile << "flow_id,src,dst,tx_pkts,rx_pkts,lost_pkts,mean_e2e_delay_ms,rx_bytes\n";
    for (auto& kv : flowStats)
    {
        auto   ftuple    = classifier->FindFlow(kv.first);
        double meanDelay = (kv.second.rxPackets > 0)
                           ? kv.second.delaySum.GetMilliSeconds() / kv.second.rxPackets
                           : 0.0;
        e2eFile << kv.first                       << ","
                << ftuple.sourceAddress           << ","
                << ftuple.destinationAddress      << ","
                << kv.second.txPackets            << ","
                << kv.second.rxPackets            << ","
                << kv.second.lostPackets          << ","
                << meanDelay                      << ","
                << kv.second.rxBytes              << "\n";
    }
    e2eFile.close();

    Simulator::Destroy();
    return 0;
}
