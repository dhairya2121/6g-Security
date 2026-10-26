#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"

#include <fstream>
#include <iomanip>
#include <string>

using namespace ns3;

int
main(int argc, char* argv[])
{
    // ------------------------------------------------------------
    // Simulation parameters
    // ------------------------------------------------------------

    double frequency = 28e9;
    double bandwidth = 100e6;
    uint16_t numerology = 3;

    double simTime = 5.0;
    double appStartTime = 0.1;

    uint32_t packetSize = 1200;

    // Genuine user sends light traffic (approx 1 Mbps)
    double genuineInterval = 0.01;

    // Attacker floods the network (approx 192 Mbps)
    double attackerInterval = 0.00005;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // ------------------------------------------------------------
    // 1. NR helpers
    // ------------------------------------------------------------

    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(beamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);
    beamformingHelper->SetAttribute("BeamformingMethod",
                                    TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // ------------------------------------------------------------
    // 2. Spectrum
    // ------------------------------------------------------------

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(frequency, bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->AssignChannelsToBands({band});

    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    // ------------------------------------------------------------
    // 3. Nodes
    // ------------------------------------------------------------

    NodeContainer gnbNodes;
    NodeContainer ueNodes;

    gnbNodes.Create(1);
    ueNodes.Create(2); // UE 0: Genuine, UE 1: Attacker

    // ------------------------------------------------------------
    // 4. Mobility
    // ------------------------------------------------------------

    MobilityHelper gnbMobility;
    Ptr<ListPositionAllocator> gnbPosition = CreateObject<ListPositionAllocator>();
    gnbPosition->Add(Vector(0.0, 0.0, 10.0));

    gnbMobility.SetPositionAllocator(gnbPosition);
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNodes);

    MobilityHelper ueMobility;
    Ptr<ListPositionAllocator> uePosition = CreateObject<ListPositionAllocator>();

    uePosition->Add(Vector(20.0, 0.0, 1.5)); // Genuine UE
    uePosition->Add(Vector(20.0, 2.0, 1.5)); // Attacker UE

    ueMobility.SetPositionAllocator(uePosition);
    ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    ueMobility.Install(ueNodes);

    // ------------------------------------------------------------
    // 5. Antennas
    // ------------------------------------------------------------

    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(4));

    // ------------------------------------------------------------
    // 6. Install NR devices
    // ------------------------------------------------------------

    NetDeviceContainer gnbDevices = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevices = nrHelper->InstallUeDevice(ueNodes, allBwps);

    NrHelper::GetGnbPhy(gnbDevices.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(numerology));
    NrHelper::GetGnbPhy(gnbDevices.Get(0), 0)->SetAttribute("TxPower", DoubleValue(30.0));

    // ------------------------------------------------------------
    // 7. EPC / Internet with SEVERE BOTTLENECK (The Fix)
    // ------------------------------------------------------------

    Ptr<Node> remoteHost;
    Ipv4Address remoteHostAddress;

    // FIX: We lowered the bottleneck to 2 Mbps.
    // The gNB allows the attacker to push ~10 Mbps.
    // 10 Mbps hitting a 2 Mbps pipe instantly freezes the router queue!
    std::tie(remoteHost, remoteHostAddress) =
        nrEpcHelper->SetupRemoteHost("2Mb/s", 1500, MilliSeconds(2));

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIp = nrEpcHelper->AssignUeIpv4Address(ueDevices);

    // ------------------------------------------------------------
    // 8. Attach
    // ------------------------------------------------------------

    nrHelper->AttachToClosestGnb(ueDevices, gnbDevices);

    // ------------------------------------------------------------
    // 9. Remote Server (The Target)
    // ------------------------------------------------------------

    uint16_t serverPort = 5000;

    UdpServerHelper server(serverPort);
    ApplicationContainer serverApp = server.Install(remoteHost);

    serverApp.Start(Seconds(appStartTime));
    serverApp.Stop(Seconds(simTime));

    // ------------------------------------------------------------
    // 10. Attacker Traffic (The Capacity Freezer)
    // ------------------------------------------------------------

    // Attacker UE 1 floods the uplink towards the Remote Server
    UdpClientHelper attackerClient(remoteHostAddress, serverPort);
    attackerClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    attackerClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    attackerClient.SetAttribute("Interval", TimeValue(Seconds(attackerInterval)));

    ApplicationContainer attackerApp = attackerClient.Install(ueNodes.Get(1));
    attackerApp.Start(Seconds(appStartTime)); // Start flooding immediately at 0.1s
    attackerApp.Stop(Seconds(simTime));

    // ------------------------------------------------------------
    // 11. Genuine Traffic (The Victim arriving late)
    // ------------------------------------------------------------

    // Genuine UE 0 tries to communicate with the Remote Server
    UdpClientHelper genuineClient(remoteHostAddress, serverPort);
    genuineClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    genuineClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    genuineClient.SetAttribute("Interval", TimeValue(Seconds(genuineInterval)));

    ApplicationContainer genuineApp = genuineClient.Install(ueNodes.Get(0));
    // Start at 0.5s -> The network capacity is already completely frozen by the attacker
    genuineApp.Start(Seconds(appStartTime + 0.4));
    genuineApp.Stop(Seconds(simTime));

    // ------------------------------------------------------------
    // 12. FlowMonitor
    // ------------------------------------------------------------

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpoints;

    endpoints.Add(remoteHost);
    endpoints.Add(ueNodes);

    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpoints);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));

    // ------------------------------------------------------------
    // 13. Run
    // ------------------------------------------------------------

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ------------------------------------------------------------
    // 14. Results
    // ------------------------------------------------------------

    std::ofstream csvFile("results/dos.csv");
    csvFile << "scenario,flowType,txPackets,rxPackets,lostPackets,throughputMbps,delayMs,jitterMs\n";

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    auto stats = monitor->GetFlowStats();

    std::cout << std::fixed << std::setprecision(3);

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flow.first);

        if (tuple.destinationPort != serverPort)
        {
            continue;
        }

        std::string flowType = (tuple.sourceAddress == ueIp.GetAddress(0)) ? "GENUINE" : "ATTACKER";

        double throughput = 0;
        double delay = 0;
        double jitter = 0;

        if (flow.second.rxPackets > 0)
        {
            double duration =
                (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
            throughput = flow.second.rxBytes * 8.0 / duration / 1e6;
            delay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            jitter = flow.second.jitterSum.GetSeconds() / flow.second.rxPackets;
        }

        std::cout << "----------------------------------------\n";
        std::cout << "Flow " << flow.first << " (" << flowType << ")\n";
        std::cout << "  Source       : " << tuple.sourceAddress << "\n";
        std::cout << "  Tx Packets   : " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets   : " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets : " << flow.second.lostPackets << "\n";

        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Throughput   : " << throughput << " Mbps\n";
            std::cout << "  Mean Delay   : " << delay * 1000 << " ms\n";
            std::cout << "  Mean Jitter  : " << jitter * 1000 << " ms\n";
        }

        csvFile << "dos," << flowType << "," << flow.second.txPackets << ","
                << flow.second.rxPackets << "," << flow.second.lostPackets << "," << throughput
                << "," << delay * 1000 << "," << jitter * 1000
                << "\n";
    }
    std::cout << "----------------------------------------\n";

    csvFile.close();
    Simulator::Destroy();

    return 0;
}