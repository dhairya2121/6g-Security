#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"
#include "ns3/flow-monitor-module.h"

#include <fstream>

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

    // Legitimate traffic
    uint32_t packetSize = 1200;
    double packetInterval = 0.001;       // 9.6 Mbps

    // Jammer traffic
    uint32_t jammerPacketSize = 1200;
    double jammerPacketInterval = 0.0001; // 96 Mbps

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // ------------------------------------------------------------
    // 1. NR helpers
    // ------------------------------------------------------------

    Ptr<NrPointToPointEpcHelper> nrEpcHelper =
        CreateObject<NrPointToPointEpcHelper>();

    Ptr<IdealBeamformingHelper> beamformingHelper =
        CreateObject<IdealBeamformingHelper>();

    Ptr<NrHelper> nrHelper =
        CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(beamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    beamformingHelper->SetAttribute(
        "BeamformingMethod",
        TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // ------------------------------------------------------------
    // 2. Spectrum
    // ------------------------------------------------------------

    CcBwpCreator ccBwpCreator;

    CcBwpCreator::SimpleOperationBandConf bandConf(
        frequency,
        bandwidth,
        1);

    OperationBandInfo band =
        ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper =
        CreateObject<NrChannelHelper>();

    channelHelper->ConfigureFactories(
        "UMi",
        "Default",
        "ThreeGpp");

    channelHelper->SetChannelConditionModelAttribute(
        "UpdatePeriod",
        TimeValue(MilliSeconds(0)));

    channelHelper->SetPathlossAttribute(
        "ShadowingEnabled",
        BooleanValue(false));

    channelHelper->AssignChannelsToBands({band});

    BandwidthPartInfoPtrVector allBwps =
        CcBwpCreator::GetAllBwps({band});

    // ------------------------------------------------------------
    // 3. Create two legitimate/interfering network pairs
    // ------------------------------------------------------------

    NodeContainer gnbNodes;
    NodeContainer ueNodes;

    // gNB 0 + UE 0 = legitimate communication
    // gNB 1 + UE 1 = jammer/interferer
    gnbNodes.Create(2);
    ueNodes.Create(2);

    // ------------------------------------------------------------
    // 4. Mobility
    // ------------------------------------------------------------

    MobilityHelper gnbMobility;

    Ptr<ListPositionAllocator> gnbPosition =
        CreateObject<ListPositionAllocator>();

    // Legitimate gNB
gnbPosition->Add(
    Vector(0.0, 0.0, 10.0));

// Jammer gNB - very close to legitimate gNB
gnbPosition->Add(
    Vector(0.0, 1.0, 10.0));

    gnbMobility.SetPositionAllocator(
        gnbPosition);

    gnbMobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");

    gnbMobility.Install(gnbNodes);

    MobilityHelper ueMobility;

    Ptr<ListPositionAllocator> uePosition =
        CreateObject<ListPositionAllocator>();

    // Legitimate UE
uePosition->Add(
    Vector(20.0, 0.0, 1.5));

// Jammer UE
uePosition->Add(
    Vector(20.0, 1.0, 1.5));

    ueMobility.SetPositionAllocator(
        uePosition);

    ueMobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");

    ueMobility.Install(ueNodes);

    // ------------------------------------------------------------
    // 5. Antennas
    // ------------------------------------------------------------

    nrHelper->SetUeAntennaAttribute(
        "NumRows",
        UintegerValue(2));

    nrHelper->SetUeAntennaAttribute(
        "NumColumns",
        UintegerValue(2));

    nrHelper->SetGnbAntennaAttribute(
        "NumRows",
        UintegerValue(2));

    nrHelper->SetGnbAntennaAttribute(
        "NumColumns",
        UintegerValue(4));

    // ------------------------------------------------------------
    // 6. Install NR devices
    // ------------------------------------------------------------

    NetDeviceContainer gnbDevices =
        nrHelper->InstallGnbDevice(
            gnbNodes,
            allBwps);

    NetDeviceContainer ueDevices =
        nrHelper->InstallUeDevice(
            ueNodes,
            allBwps);

    // Configure both gNBs
    // Legitimate gNB
NrHelper::GetGnbPhy(
    gnbDevices.Get(0), 0)
    ->SetAttribute(
        "Numerology",
        UintegerValue(numerology));

NrHelper::GetGnbPhy(
    gnbDevices.Get(0), 0)
    ->SetAttribute(
        "TxPower",
        DoubleValue(30.0));

// Jammer gNB
NrHelper::GetGnbPhy(
    gnbDevices.Get(1), 0)
    ->SetAttribute(
        "Numerology",
        UintegerValue(numerology));

NrHelper::GetGnbPhy(
    gnbDevices.Get(1), 0)
    ->SetAttribute(
        "TxPower",
        DoubleValue(46.0));

    // ------------------------------------------------------------
    // 7. EPC / Internet
    // ------------------------------------------------------------

    Ptr<Node> remoteHost;
    Ipv4Address remoteHostAddress;

    std::tie(remoteHost, remoteHostAddress) =
        nrEpcHelper->SetupRemoteHost(
            "100Gb/s",
            2500,
            Seconds(0));

    InternetStackHelper internet;

    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIp =
        nrEpcHelper->AssignUeIpv4Address(
            ueDevices);

    // ------------------------------------------------------------
    // 8. Attach both UEs to their closest gNB
    // ------------------------------------------------------------

    nrHelper->AttachToClosestGnb(
        ueDevices,
        gnbDevices);

    // ------------------------------------------------------------
    // 9. Legitimate UDP server
    // ------------------------------------------------------------

    uint16_t legitimatePort = 5000;

    UdpServerHelper legitimateServer(
        legitimatePort);

    ApplicationContainer legitimateServerApp =
        legitimateServer.Install(
            ueNodes.Get(0));

    legitimateServerApp.Start(
        Seconds(appStartTime));

    legitimateServerApp.Stop(
        Seconds(simTime));

    // ------------------------------------------------------------
    // 10. Legitimate UDP client
    // ------------------------------------------------------------

    UdpClientHelper legitimateClient(
        ueIp.GetAddress(0),
        legitimatePort);

    legitimateClient.SetAttribute(
        "MaxPackets",
        UintegerValue(0xFFFFFFFF));

    legitimateClient.SetAttribute(
        "PacketSize",
        UintegerValue(packetSize));

    legitimateClient.SetAttribute(
        "Interval",
        TimeValue(
            Seconds(packetInterval)));

    ApplicationContainer legitimateClientApp =
        legitimateClient.Install(
            remoteHost);

    legitimateClientApp.Start(
        Seconds(appStartTime));

    legitimateClientApp.Stop(
        Seconds(simTime));

    // ------------------------------------------------------------
    // 11. Jammer UDP server
    // ------------------------------------------------------------

    uint16_t jammerPort = 6000;

    UdpServerHelper jammerServer(
        jammerPort);

    ApplicationContainer jammerServerApp =
        jammerServer.Install(
            ueNodes.Get(1));

    jammerServerApp.Start(
        Seconds(appStartTime));

    jammerServerApp.Stop(
        Seconds(simTime));

    // ------------------------------------------------------------
    // 12. Heavy jammer traffic
    // ------------------------------------------------------------

    UdpClientHelper jammerClient(
        ueIp.GetAddress(1),
        jammerPort);

    jammerClient.SetAttribute(
        "MaxPackets",
        UintegerValue(0xFFFFFFFF));

    jammerClient.SetAttribute(
        "PacketSize",
        UintegerValue(jammerPacketSize));

    jammerClient.SetAttribute(
        "Interval",
        TimeValue(
            Seconds(jammerPacketInterval)));

    ApplicationContainer jammerClientApp =
        jammerClient.Install(
            remoteHost);

    jammerClientApp.Start(
        Seconds(appStartTime));

    jammerClientApp.Stop(
        Seconds(simTime));

    // ------------------------------------------------------------
    // 13. FlowMonitor
    // ------------------------------------------------------------

    FlowMonitorHelper flowmonHelper;

    NodeContainer endpoints;

    endpoints.Add(remoteHost);
    endpoints.Add(ueNodes);

    Ptr<FlowMonitor> monitor =
        flowmonHelper.Install(endpoints);

    monitor->SetAttribute(
        "DelayBinWidth",
        DoubleValue(0.001));

    monitor->SetAttribute(
        "JitterBinWidth",
        DoubleValue(0.001));

    // ------------------------------------------------------------
    // 14. Run
    // ------------------------------------------------------------

    Simulator::Stop(
        Seconds(simTime));

    Simulator::Run();

    // ------------------------------------------------------------
    // 15. Results
    // ------------------------------------------------------------

    std::ofstream csvFile(
        "results/jamming.csv");

    csvFile
        << "scenario,txPackets,rxPackets,lostPackets,"
           "throughputMbps,delayMs,jitterMs\n";

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(
            flowmonHelper.GetClassifier());

    auto stats = monitor->GetFlowStats();

    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple =
            classifier->FindFlow(flow.first);

        // Only record the legitimate flow.
        if (tuple.destinationPort != legitimatePort)
        {
            continue;
        }

        std::cout
            << "\nFlow "
            << flow.first
            << "\n";

        std::cout
            << "  Source: "
            << tuple.sourceAddress
            << ":"
            << tuple.sourcePort
            << "\n";

        std::cout
            << "  Destination: "
            << tuple.destinationAddress
            << ":"
            << tuple.destinationPort
            << "\n";

        std::cout
            << "  Tx Packets: "
            << flow.second.txPackets
            << "\n";

        std::cout
            << "  Rx Packets: "
            << flow.second.rxPackets
            << "\n";

        std::cout
            << "  Lost Packets: "
            << flow.second.lostPackets
            << "\n";

        if (flow.second.rxPackets > 0)
        {
            double duration =
                (flow.second.timeLastRxPacket -
                 flow.second.timeFirstTxPacket)
                    .GetSeconds();

            double throughput =
                flow.second.rxBytes * 8.0 /
                duration /
                1e6;

            double delay =
                flow.second.delaySum.GetSeconds() /
                flow.second.rxPackets;

            double jitter =
                flow.second.jitterSum.GetSeconds() /
                flow.second.rxPackets;

            std::cout
                << "  Throughput: "
                << throughput
                << " Mbps\n";

            std::cout
                << "  Mean Delay: "
                << delay * 1000
                << " ms\n";

            std::cout
                << "  Mean Jitter: "
                << jitter * 1000
                << " ms\n";

            csvFile
                << "jamming,"
                << flow.second.txPackets << ","
                << flow.second.rxPackets << ","
                << flow.second.lostPackets << ","
                << throughput << ","
                << delay * 1000 << ","
                << jitter * 1000 << "\n";
        }
    }

    csvFile.close();

    Simulator::Destroy();

    return 0;
}
