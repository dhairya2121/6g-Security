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
    // Simulation parameters
    double frequency = 28e9;
    double bandwidth = 100e6;
    uint16_t numerology = 3;

    double simTime = 5.0;
    double appStartTime = 0.1;

    uint32_t packetSize = 1200;
    double packetInterval = 0.001; // 1 ms

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // ------------------------------------------------------------
    // 1. Create NR helpers
    // ------------------------------------------------------------

    Ptr<NrPointToPointEpcHelper> nrEpcHelper =
        CreateObject<NrPointToPointEpcHelper>();

    Ptr<IdealBeamformingHelper> beamformingHelper =
        CreateObject<IdealBeamformingHelper>();

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(beamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    beamformingHelper->SetAttribute(
        "BeamformingMethod",
        TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // ------------------------------------------------------------
    // 2. Configure spectrum
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
    // 3. Create nodes
    // ------------------------------------------------------------

    NodeContainer gnbNodes;
    NodeContainer ueNodes;

    gnbNodes.Create(1);
    ueNodes.Create(1);

    // ------------------------------------------------------------
    // 4. Mobility
    // ------------------------------------------------------------

    MobilityHelper gnbMobility;
    Ptr<ListPositionAllocator> gnbPosition =
        CreateObject<ListPositionAllocator>();

    gnbPosition->Add(Vector(0.0, 0.0, 10.0));

    gnbMobility.SetPositionAllocator(gnbPosition);
    gnbMobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNodes);

    MobilityHelper ueMobility;
    Ptr<ListPositionAllocator> uePosition =
        CreateObject<ListPositionAllocator>();

    uePosition->Add(Vector(20.0, 0.0, 1.5));

    ueMobility.SetPositionAllocator(uePosition);
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

    // Configure PHY
    NrHelper::GetGnbPhy(gnbDevices.Get(0), 0)
        ->SetAttribute(
            "Numerology",
            UintegerValue(numerology));

    NrHelper::GetGnbPhy(gnbDevices.Get(0), 0)
        ->SetAttribute(
            "TxPower",
            DoubleValue(30.0));

    // ------------------------------------------------------------
    // 7. Internet / EPC
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
    // 8. Attach UE to gNB
    // ------------------------------------------------------------

    nrHelper->AttachToClosestGnb(
        ueDevices,
        gnbDevices);

    // ------------------------------------------------------------
    // 9. UDP server on UE
    // ------------------------------------------------------------

    uint16_t port = 5000;

    UdpServerHelper server(port);

    ApplicationContainer serverApp =
        server.Install(ueNodes.Get(0));

    serverApp.Start(
        Seconds(appStartTime));

    serverApp.Stop(
        Seconds(simTime));

    // ------------------------------------------------------------
    // 10. UDP client on remote host
    // ------------------------------------------------------------

    UdpClientHelper client(
        ueIp.GetAddress(0),
        port);

    client.SetAttribute(
        "MaxPackets",
        UintegerValue(0xFFFFFFFF));

    client.SetAttribute(
        "PacketSize",
        UintegerValue(packetSize));

    client.SetAttribute(
        "Interval",
        TimeValue(Seconds(packetInterval)));

    ApplicationContainer clientApp =
        client.Install(remoteHost);

    clientApp.Start(
        Seconds(appStartTime));

    clientApp.Stop(
        Seconds(simTime));

    // ------------------------------------------------------------
    // 11. FlowMonitor
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
    // 12. Run simulation
    // ------------------------------------------------------------

    Simulator::Stop(Seconds(simTime));

    Simulator::Run();

        // ------------------------------------------------------------
    // 13. Print results
    // ------------------------------------------------------------

    std::ofstream csvFile("results/baseline.csv");

    csvFile << "scenario,txPackets,rxPackets,lostPackets,"
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

        std::cout << "\nFlow " << flow.first << "\n";

        std::cout
            << "  Source: "
            << tuple.sourceAddress
            << ":" << tuple.sourcePort
            << "\n";

        std::cout
            << "  Destination: "
            << tuple.destinationAddress
            << ":" << tuple.destinationPort
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
                duration / 1e6;

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
                << "baseline,"
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
