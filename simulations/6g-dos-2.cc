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
    double genuineStartTime = 1.0; // Genuine starts at 1.0s, when link is already saturated

    uint32_t packetSize = 1200;

    // Genuine user traffic (1200 B every 20ms = ~480 Kbps demand)
    double genuineInterval = 0.02;

    // Attacker volumetric flood (1200 B every 50us = saturated uplink blast)
    double attackerInterval = 0.00005;

    // ------------------------------------------------------------
    // 0. Anti-Bufferbloat: Restrict bottleneck queue to 5 packets.
    //    This forces immediate tail-drops when the link is saturated.
    // ------------------------------------------------------------
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("5p"));

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
    ueNodes.Create(2); // UE 0: Genuine Victim, UE 1: Attacker

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
    // 7. EPC / Internet Bottleneck Link
    //    Capacity is set to 250 Kbps. The attacker delivers ~1.2 Mbps
    //    from the 5G air interface, completely saturating this 250 Kbps
    //    pipe by ~480%.
    // ------------------------------------------------------------

    Ptr<Node> remoteHost;
    Ipv4Address remoteHostAddress;

    std::tie(remoteHost, remoteHostAddress) =
        nrEpcHelper->SetupRemoteHost("250Kb/s", 1500, MilliSeconds(2));

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIp = nrEpcHelper->AssignUeIpv4Address(ueDevices);

    // ------------------------------------------------------------
    // 8. Attach UEs to gNB
    // ------------------------------------------------------------

    nrHelper->AttachToClosestGnb(ueDevices, gnbDevices);

    std::cout << "Legitimate UE IP : " << ueIp.GetAddress(0) << std::endl;
    std::cout << "Attacker UE IP   : " << ueIp.GetAddress(1) << std::endl;
    std::cout << "Remote Server IP : " << remoteHostAddress << std::endl;

    // ------------------------------------------------------------
    // 9. Remote Server (Target)
    // ------------------------------------------------------------

    uint16_t serverPort = 5000;

    UdpServerHelper server(serverPort);
    ApplicationContainer serverApp = server.Install(remoteHost);

    serverApp.Start(Seconds(appStartTime));
    serverApp.Stop(Seconds(simTime));

    // ------------------------------------------------------------
    // 10. Attacker Traffic (Saturating Flood)
    // ------------------------------------------------------------

    UdpClientHelper attackerClient(remoteHostAddress, serverPort);
    attackerClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    attackerClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    attackerClient.SetAttribute("Interval", TimeValue(Seconds(attackerInterval)));

    ApplicationContainer attackerApp = attackerClient.Install(ueNodes.Get(1));
    attackerApp.Start(Seconds(appStartTime)); // Start flooding early at t=0.1s
    attackerApp.Stop(Seconds(simTime));

    // ------------------------------------------------------------
    // 11. Genuine Traffic (The Victim)
    // ------------------------------------------------------------

    UdpClientHelper genuineClient(remoteHostAddress, serverPort);
    genuineClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    genuineClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    genuineClient.SetAttribute("Interval", TimeValue(Seconds(genuineInterval)));

    ApplicationContainer genuineApp = genuineClient.Install(ueNodes.Get(0));
    // Start at 1.0s -> the bottleneck and buffer are already completely saturated
    genuineApp.Start(Seconds(genuineStartTime));
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
    // 13. Run Simulation
    // ------------------------------------------------------------

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ------------------------------------------------------------
    // 14. Results
    // ------------------------------------------------------------

    std::ofstream csvFile("results/dos-2.csv");
    csvFile << "scenario,flowType,sourceIp,txPackets,rxPackets,lostPackets,throughputMbps,delayMs,jitterMs\n";

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    auto stats = monitor->GetFlowStats();

    std::cout << "\n================ SIMULATION RESULTS ================\n";

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

        std::cout << "\nFlow " << flow.first << " (" << flowType << "):\n"
                  << "  Source IP     : " << tuple.sourceAddress << "\n"
                  << "  Tx Packets    : " << flow.second.txPackets << "\n"
                  << "  Rx Packets    : " << flow.second.rxPackets << "\n"
                  << "  Lost Packets  : " << (flow.second.txPackets - flow.second.rxPackets) << "\n"
                  << "  Delivery Ratio: " << std::fixed << std::setprecision(2)
                  << (flow.second.txPackets > 0 ? (100.0 * flow.second.rxPackets / flow.second.txPackets) : 0.0) << " %\n"
                  << "  Throughput    : " << throughput << " Mbps\n"
                  << "  Mean Delay    : " << delay * 1000 << " ms\n"
                  << "  Mean Jitter   : " << jitter * 1000 << " ms\n";

        csvFile << "dos-2," << flowType << "," << tuple.sourceAddress << ","
                << flow.second.txPackets << "," << flow.second.rxPackets << ","
                << (flow.second.txPackets - flow.second.rxPackets) << ","
                << throughput << "," << delay * 1000 << "," << jitter * 1000 << "\n";
    }

    std::cout << "====================================================\n\n";

    csvFile.close();
    Simulator::Destroy();

    return 0;
}

