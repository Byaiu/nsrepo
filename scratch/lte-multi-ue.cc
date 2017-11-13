#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/exp-module.h"

#include <sstream>
#include <string>
#include <time.h>
#include <stdio.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteMultiUe");

void
PrintMobility (Ptr<Node> node)
{
  Ptr<MobilityModel> model = node->GetObject<MobilityModel> ();
  if (model == 0)
    {
      NS_LOG_INFO("There are no mobility on Node. " << node->GetId());
      return;
    }
  NS_LOG_INFO("Node. " << node->GetId() << ", Position " << model->GetPosition());
}

void
PrintIpAddress (Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4 == 0)
    {
      NS_LOG_INFO("There are no Ipv4 on Node. " << node->GetId());
      return;
    }
  NS_LOG_INFO("Node. " << node->GetId() << ", Ipv4 Address " << ipv4->GetAddress(1, 0).GetLocal());
}

void
CountGlue (Ptr<CounterCalculator<> > datac, std::string path)
{
  datac->Update ();
}

void
TimeGlue (Ptr<TimeMinMaxAvgTotalCalculator> datac, std::string path, Time t)
{
  datac->Update (t);
}

template<typename T>
  std::string
  NumberToString (T Number)
  {
    std::ostringstream ss;
    ss << Number;
    return ss.str ();
  }

const std::string currentDateTime()
{
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y%m%d%H%I%M%S", &tstruct);
  return buf;
}

int
main (int argc, char *argv[])
{
  NS_LOG_UNCOND("==== eNB -- UEs");
//-----------------------------
// Command Line

  CommandLine cmd;

  bool verbose = false;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  bool savedb = false;
  cmd.AddValue ("savedb", "Whether to save statics to db", savedb);

  bool enabletrace = false;
  cmd.AddValue ("enabletrace", "whether to enable lte trace", enabletrace);

  double simTime = 10;
  cmd.AddValue ("simTime", "Simulation Time", simTime);

  uint32_t nUE = 5;
  cmd.AddValue ("nUE", "UE number", nUE);

  uint16_t sizemin = 1200;
  cmd.AddValue ("sizemin", "Packet size min", sizemin);
  uint16_t sizemax = 1300;
  cmd.AddValue ("sizemax", "Packet size min", sizemax);

  std::string experiment = "lte-multi-ue";
  cmd.AddValue ("experiment", "Current experiment", experiment);

  std::string strategy = "relay";
  cmd.AddValue ("strategy", "The strategy used for this experment", strategy);

  std::string input = "5";
  cmd.AddValue ("input", "The input of this experiment", input);

  std::string prefix = "lte-multi-ue";
  cmd.AddValue ("prefix", "Prefix of output data file name", prefix);

  cmd.Parse (argc, argv);

  std::string runID = currentDateTime();

  std::stringstream packetsize_s;
  packetsize_s << "ns3::UniformRandomVariable[Min=" << sizemin << "|Max=" << sizemax << "]";
  std::string packetsize = packetsize_s.str ();
  NS_LOG_DEBUG(packetsize);

  //-----------------------------
  // Global Value Setting
  uint32_t cfi = 1;
  if (nUE > 12)
    {
      cfi = 3;
    }
  else if (nUE > 4)
    {
      cfi = 2;
    }
  IntegerValue cfiValue;
  cfiValue.Set(cfi);
  GlobalValue::Bind("LteEnbCfi", cfiValue);

//-----------------------------
// Configure Default

//  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (320));
//  Config::SetDefault ("ns3::PhyStatsCalculator::DlRsrpSinrFilename", StringValue ("lte-multi-ue-dl-rsrp"));
//  Config::SetDefault ("ns3::PhyStatsCalculator::UlSinrFilename", StringValue ("lte-multi-ue-sinr"));
//  Config::SetDefault ("ns3::PhyStatsCalculator::UlInterferenceFilename", StringValue ("lte-multi-ue-interference"));
//
//  Config::SetDefault ("ns3::MacStatsCalculator::DlOutputFilename", StringValue ("lte-multi-ue-mac-dl"));
//  Config::SetDefault ("ns3::MacStatsCalculator::UlOutputFilename", StringValue ("lte-multi-ue-mac-ul"));
//
//  Config::SetDefault ("ns3::RadioBearerStatsCalculator::DlRlcOutputFilename", StringValue ("lte-multi-ue-rlc-dl"));
//  Config::SetDefault ("ns3::RadioBearerStatsCalculator::UlRlcOutputFilename", StringValue ("lte-multi-ue-rlc-ul"));
//  Config::SetDefault ("ns3::RadioBearerStatsCalculator::DlPdcpOutputFilename", StringValue ("lte-multi-ue-pdcp-dl"));
//  Config::SetDefault ("ns3::RadioBearerStatsCalculator::UlPdcpOutputFilename", StringValue ("lte-multi-ue-pdcp-ul"));
//
//  Config::SetDefault ("ns3::PhyTxStatsCalculator::DlTxOutputFilename", StringValue ("lte-multi-ue-tx-dl"));
//  Config::SetDefault ("ns3::PhyRxStatsCalculator::DlRxOutputFilename", StringValue ("lte-multi-ue-rx-ul"));
//-----------------------------
// Node

  NodeContainer lte_ue_node;
  NodeContainer lte_enb_node;
  lte_enb_node.Create (1);
  lte_ue_node.Create (nUE);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);

//-----------------------------
// Mobility

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                 "rho", DoubleValue (10),
                                 "X", DoubleValue (0),
                                 "Y", DoubleValue (20));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (lte_ue_node);

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));  // enb
  positionAlloc->Add (Vector (10.0, 0.0, 0.0)); // pgw
  positionAlloc->Add (Vector (20.0, 0.0, 0.0)); // remote
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (lte_enb_node);
  mobility.Install(pgw);
  mobility.Install(remoteHost);

//-----------------------------
// NetDevice

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));  // Make sure not to be bottle neck
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.0)));
  NetDeviceContainer p2p_device = p2ph.Install (pgw, remoteHost);

  NetDeviceContainer lte_enb_device = lteHelper->InstallEnbDevice (lte_enb_node);
  NetDeviceContainer lte_ue_device = lteHelper->InstallUeDevice (lte_ue_node);

//-----------------------------
// Internet

  InternetStackHelper internet;

  // remote configure
  internet.Install (remoteHostContainer);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  internet.Install (lte_ue_node);
  Ipv4InterfaceContainer ueInterfaces = epcHelper->AssignUeIpv4Address (lte_ue_device);

  // Set the default gateway for each UE
  for (uint32_t u = 0; u < lte_ue_node.GetN (); ++u)
    {
      Ptr<Node> ueNode = lte_ue_node.Get (u);
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach each UE per eNodeB
  // side effect: the default EPS bearer will be activated
  for (uint16_t i = 0; i < lte_ue_device.GetN (); i++)
    {
      lteHelper->Attach (lte_ue_device.Get (i), lte_enb_device.Get (0));
    }

  Ipv4AddressHelper address;
  address.SetBase ("1.0.0.0", "255.0.0.0");
  address.Assign (p2p_device);

#if 0 // could be removed
  Ptr<LteEnbNetDevice> enbDev = lte_enb_device.Get(0)->GetObject<LteEnbNetDevice>();
  if (enbDev)
    {
      NS_LOG_DEBUG ("Enb is get");
      UintegerValue dlBW;
      enbDev->GetAttribute ("DlBandwidth", dlBW);
      enbDev->SetAttribute ("DlBandwidth", UintegerValue (100));
      enbDev->GetAttribute ("DlBandwidth", dlBW);
      NS_LOG_DEBUG ("Download Bandwidth is " << dlBW.Get());
    }
#endif

//-----------------------------
// Application

  ExpAppSenderHelper sender;
  sender.SetAttribute ("Port", UintegerValue (9));
  sender.SetAttribute ("PacketSize", StringValue (packetsize));
  sender.SetAttribute ("Interval", StringValue ("ns3::ExponentialRandomVariable[Mean=0.005]"));

  ExpAppReceiverHelper receiver (9);

  ApplicationContainer receiver_app;
  ApplicationContainer sender_app;

  for (uint32_t i = 0; i < lte_ue_node.GetN (); i++)
    {
      receiver_app.Add (receiver.Install (lte_ue_node.Get (i)));
      sender.SetAttribute ("Destination", Ipv4AddressValue (ueInterfaces.GetAddress (i)));
      sender_app.Add (sender.Install (remoteHost));
    }

  receiver_app.Start (Seconds (0.1));
  receiver_app.Stop (Seconds (simTime));
  sender_app.Start (Seconds (0.1));
  sender_app.Stop (Seconds (simTime));

  NS_LOG_DEBUG("remoteHost Applications number: " << remoteHost->GetNApplications ());


//-----------------------------
// Statistics

  DataCollector data;
  data.DescribeRun (experiment, strategy, input, runID);

  if (savedb)
    {

      // Sender packets
      Ptr<PacketSizeMinMaxAvgTotalCalculator> totalTx = CreateObject<PacketSizeMinMaxAvgTotalCalculator> ();
      totalTx->SetKey ("SendPackets");
      totalTx->SetContext ("remote");
      Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::ExpAppSender/Tx",
                       MakeCallback (&PacketSizeMinMaxAvgTotalCalculator::PacketUpdate, totalTx));
      data.AddDataCalculator (totalTx);

      // Receiver packets
      Ptr<PacketSizeMinMaxAvgTotalCalculator> totalRx = CreateObject<PacketSizeMinMaxAvgTotalCalculator> ();
      totalRx->SetKey ("ReceivePackets");
      totalRx->SetContext ("ue");
      Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::ExpAppReceiver/RxPacket",
                       MakeCallback (&PacketSizeMinMaxAvgTotalCalculator::PacketUpdate, totalRx));
      data.AddDataCalculator (totalRx);

      // Receiver time
      Ptr<TimeMinMaxAvgTotalCalculator> delayStat = CreateObject<TimeMinMaxAvgTotalCalculator> ();
      delayStat->SetKey ("Delay");
      delayStat->SetContext ("system");
      Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::ExpAppReceiver/RxTime",
                       MakeBoundCallback (&TimeGlue, delayStat));
      data.AddDataCalculator (delayStat);
    }

  if (enabletrace)
    {
      lteHelper->EnablePdcpTraces();
      lteHelper->EnableRlcTraces();
      lteHelper->EnableMacTraces();
      lteHelper->EnablePhyTraces();
    }
//-----------------------------
// Simulation Configuration

  NS_LOG_DEBUG("======= LTE ONLY END ========");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  if (savedb)
    {
      Ptr<DataOutputInterface> output = 0;
      output = CreateObject<SqliteDataOutput> ();
      output->SetFilePrefix (prefix);
      if (output != 0)
        output->Output (data);
    }
  Simulator::Destroy ();
  return 0;

}
