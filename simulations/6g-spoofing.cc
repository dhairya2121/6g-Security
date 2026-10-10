#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"
#include "ns3/flow-monitor-module.h"

#include <fstream>
#include <functional>

using namespace ns3;

static uint32_t g_spoofRxCount = 0;
static uint64_t g_spoofRxBytes = 0;
static double g_spoofFirstRxTime = -1.0;
static double g_spoofLastRxTime = -1.0;
static uint16_t g_spoofSourcePort = 4500; // must match spoofSourcePort below

static void
RxWithAddressesCallback(Ptr<const Packet> packet, const Address& from, const Address& localAddr)
{
    if (InetSocketAddress::IsMatchingType(from))
    {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
        if (addr.GetPort() == g_spoofSourcePort)
        {
            g_spoofRxCount++;
            g_spoofRxBytes += packet->GetSize();
            double now = Simulator::Now().GetSeconds();
            if (g_spoofFirstRxTime < 0)
            {
                g_spoofFirstRxTime = now;
            }
            g_spoofLastRxTime = now;
        }
    }
}

int
main(int argc, char* argv[])
{
    double frequency = 28e9;
    double bandwidth = 100e6;
    uint16_t numerology = 3;

    double simTime = 5.0;
    double appStartTime = 0.1;

    uint32_t packetSize = 1200;
    double packetInterval = 0.001;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // ------------------------------------------------------------
    // 1. NR helpers
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
    // 3. Nodes
    // ------------------------------------------------------------

    NodeContainer gnbNodes;
    NodeContainer ueNodes;

    gnbNodes.Create(1);
    ueNodes.Create(2);

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
    uePosition->Add(Vector(20.0, 2.0, 1.5));

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
    // 6. NR devices
    // ------------------------------------------------------------

    NetDeviceContainer gnbDevices =
        nrHelper->InstallGnbDevice(
            gnbNodes,
            allBwps);

    NetDeviceContainer ueDevices =
        nrHelper->InstallUeDevice(
            ueNodes,
            allBwps);

    NrHelper::GetGnbPhy(gnbDevices.Get(0), 0)
        ->SetAttribute(
            "Numerology",
            UintegerValue(numerology));

    NrHelper::GetGnbPhy(gnbDevices.Get(0), 0)
        ->SetAttribute(
            "TxPower",
            DoubleValue(30.0));

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
    // 8. Attach
    // ------------------------------------------------------------

    nrHelper->AttachToClosestGnb(
        ueDevices,
        gnbDevices);

    Ipv4Address legitimateIp =
        ueIp.GetAddress(0);

    std::cout << "\nLegitimate UE IP: "
              << legitimateIp << std::endl;

    std::cout << "Attacker UE IP: "
              << ueIp.GetAddress(1) << std::endl;

    std::cout << "Remote Host IP: "
              << remoteHostAddress << std::endl;

    // ------------------------------------------------------------
    // 9. Normal server
    // ------------------------------------------------------------

    uint16_t serverPort = 5000;

    UdpServerHelper server(serverPort);

    ApplicationContainer serverApp =
        server.Install(remoteHost);

    serverApp.Start(Seconds(appStartTime));
    serverApp.Stop(Seconds(simTime));

    // Connect the address-aware Rx trace BEFORE Simulator::Run(),
    // so it is actually live while the simulation executes.
    Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));
    udpServer->TraceConnectWithoutContext(
        "RxWithAddresses",
        MakeCallback(&RxWithAddressesCallback));

    // ------------------------------------------------------------
    // 10. Legitimate UE -> Remote Host
    // ------------------------------------------------------------

    UdpClientHelper normalClient(
        remoteHostAddress,
        serverPort);

    normalClient.SetAttribute(
        "MaxPackets",
        UintegerValue(0xFFFFFFFF));

    normalClient.SetAttribute(
        "PacketSize",
        UintegerValue(packetSize));

    normalClient.SetAttribute(
        "Interval",
        TimeValue(Seconds(packetInterval)));

    ApplicationContainer normalApp =
        normalClient.Install(ueNodes.Get(0));

    normalApp.Start(Seconds(appStartTime));
    normalApp.Stop(Seconds(simTime));

    // ------------------------------------------------------------
    // 11. Raw socket attacker
    // ------------------------------------------------------------

    Ptr<Socket> spoofSocket =
        Socket::CreateSocket(
            ueNodes.Get(1),
            Ipv4RawSocketFactory::GetTypeId());

    spoofSocket->SetAttribute(
        "IpHeaderInclude",
        BooleanValue(true));

    spoofSocket->BindToNetDevice(ueDevices.Get(1));

    uint16_t spoofSourcePort = 4500; // must match g_spoofSourcePort above

    std::function<void()> sendSpoofedPacket;

    sendSpoofedPacket =
        [&]()
        {
            Ptr<Packet> packet =
                Create<Packet>(packetSize);

            UdpHeader udp;

            udp.SetSourcePort(spoofSourcePort);
            udp.SetDestinationPort(serverPort);

            udp.InitializeChecksum(
                legitimateIp,
                remoteHostAddress,
                17);

            packet->AddHeader(udp);

            Ipv4Header ip;

            ip.SetSource(legitimateIp);
            ip.SetDestination(remoteHostAddress);
            ip.SetProtocol(17);
            ip.SetPayloadSize(packet->GetSize());
            ip.SetTtl(64);

            packet->AddHeader(ip);

            int ret = spoofSocket->SendTo(
                packet,
                0,
                InetSocketAddress(remoteHostAddress, 0));

            static uint32_t failCount = 0;
            if (ret < 0 && failCount < 5)
            {
                std::cout << "Spoofed send FAILED, errno="
                          << spoofSocket->GetErrno() << std::endl;
                failCount++;
            }

            Simulator::Schedule(
                Seconds(packetInterval),
                sendSpoofedPacket);
        };

    Simulator::Schedule(
        Seconds(appStartTime),
        sendSpoofedPacket);

    // ------------------------------------------------------------
    // 12. FlowMonitor
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
    // 13. Run
    // ------------------------------------------------------------

    Simulator::Stop(Seconds(simTime));

    Simulator::Run();

    std::cout << "\nTotal packets received at server app layer: "
              << udpServer->GetReceived() << std::endl;

    // ------------------------------------------------------------
    // 14. Results
    // ------------------------------------------------------------

    std::ofstream csvFile(
        "results/spoofing.csv");

    csvFile
        << "scenario,source,txPackets,rxPackets,"
           "lostPackets,throughputMbps,delayMs,jitterMs\n";

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(
            flowmonHelper.GetClassifier());

    auto stats = monitor->GetFlowStats();

    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple =
            classifier->FindFlow(flow.first);

        if (tuple.destinationPort != serverPort)
        {
            continue;
        }

        std::cout << "\nFlow "
                  << flow.first << "\n";

        std::cout
            << "Source: "
            << tuple.sourceAddress
            << ":" << tuple.sourcePort
            << "\n";

        std::cout
            << "Destination: "
            << tuple.destinationAddress
            << ":" << tuple.destinationPort
            << "\n";

        std::cout
            << "Tx Packets: "
            << flow.second.txPackets
            << "\n";

        std::cout
            << "Rx Packets: "
            << flow.second.rxPackets
            << "\n";

        std::cout
            << "Lost Packets: "
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

            std::string sourceType;

            if (tuple.sourceAddress == legitimateIp &&
                tuple.sourcePort == spoofSourcePort)
            {
                sourceType = "SPOOFED";
            }
            else
            {
                sourceType = "LEGITIMATE";
            }

            std::cout
                << "Type: "
                << sourceType
                << "\n";

            std::cout
                << "Throughput: "
                << throughput
                << " Mbps\n";

            std::cout
                << "Mean Delay: "
                << delay * 1000
                << " ms\n";

            std::cout
                << "Mean Jitter: "
                << jitter * 1000
                << " ms\n";

            csvFile
                << "spoofing,"
                << sourceType << ","
                << flow.second.txPackets << ","
                << flow.second.rxPackets << ","
                << flow.second.lostPackets << ","
                << throughput << ","
                << delay * 1000 << ","
                << jitter * 1000 << "\n";
        }
    }

    // ------------------------------------------------------------
    // 15. Spoofed-flow metrics measured at the application layer
    //     (FlowMonitor cannot see this flow: Ipv4RawSocketImpl's
    //     IP-header-included path calls Ipv4::SendWithHeader(),
    //     which does not fire the Tx trace FlowMonitor's probes
    //     listen to. These numbers come from UdpServer's
    //     "RxWithAddresses" trace instead, captured during Run()
    //     via the callback connected in section 9.)
    // ------------------------------------------------------------

    double spoofThroughput = 0.0;

    if (g_spoofRxCount > 1 && g_spoofLastRxTime > g_spoofFirstRxTime)
    {
        spoofThroughput =
            (g_spoofRxBytes * 8.0) /
            (g_spoofLastRxTime - g_spoofFirstRxTime) / 1e6;
    }

    std::cout << "\n--- Spoofed flow (measured via RxWithAddresses) ---\n";
    std::cout << "Rx Packets: " << g_spoofRxCount << "\n";
    std::cout << "Throughput: " << spoofThroughput << " Mbps\n";

    csvFile
        << "spoofing,SPOOFED-APPLAYER,,"
        << g_spoofRxCount << ",,"
        << spoofThroughput << ",,\n";

    csvFile.close();

    Simulator::Destroy();

    return 0;
}