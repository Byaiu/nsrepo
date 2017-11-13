/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

/**
 * ./waf --run="wifionly --simTime=50 --sizemin=2268 --sizemax=2268 --numberOfStas=10" --vis
 *
 */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/exp-module.h"
#include "ns3/stats-module.h"
#include "ns3/exp-module.h"
#include "ns3/flow-monitor-module.h"

#include <sstream>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultiUe");

void
CountGlue (Ptr<CounterCalculator<> > datac, std::string path)
{
  datac->Update ();
  NS_LOG_INFO(path << "CountGlue");
}

void
TimeGlue (Ptr<TimeMinMaxAvgTotalCalculator> datac, std::string path, Time t)
{
  datac->Update (t);
  NS_LOG_INFO(path << "TimeGlue");
}

void
TxDropTrace (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("Packet " << p->GetUid () << "is been droped.");
}

void
IpDropTrace (const Ipv4Header &h, Ptr< const Packet > p, Ipv4L3Protocol::DropReason r, Ptr< Ipv4 > ipv4, uint32_t interface)
{
  NS_LOG_UNCOND ("IP Drop: " << p->GetUid ());
}

void
IpTxTrace (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t i)
{
  NS_LOG_UNCOND ("IP Trans: " << p->GetUid () << " size->" << p->GetSize ());
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

  NS_LOG_UNCOND("==== AP -- UEs");

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

  std::string experiment = "wifi-multi-ue";
  cmd.AddValue ("experiment", "Current experiment", experiment);

  std::string strategy = "wifi-multi-ue";
  cmd.AddValue ("strategy", "The strategy used for this experment", strategy);

  std::string input = "5";
  cmd.AddValue ("input", "The input of this experiment", input);

  std::string prefix = "wifi-multi-ue";
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
  if (nUE > 10)
    {
      cfi = 2;
    }
  else if (nUE > 20)
    {
      cfi = 3;
    }
  IntegerValue cfiValue;
  cfiValue.Set(cfi);
  GlobalValue::Bind("LteEnbCfi", cfiValue);

  //-----------------------------
  // Configure Default

    Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (320));
  //-----------------------------
  // Node

  NodeContainer wifiStas;
  wifiStas.Create (nUE);
  NodeContainer wifiAP;
  wifiAP.Create(1);

  //-----------------------------
  // Mobility

  NS_LOG_INFO ("Mobility");

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
				 "rho", DoubleValue (10),
				 "X",   DoubleValue (0),
				 "Y",   DoubleValue (20));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStas);

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 2.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifiAP);

  //-----------------------------
  // NetDevice

  NS_LOG_INFO ("NetDevice");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStas);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiAP);

  //-----------------------------
  // Internet

  NS_LOG_INFO ("Internet");
  InternetStackHelper stack;
  stack.Install (wifiAP);
  stack.Install (wifiStas);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiAPInterface;
  wifiAPInterface = address.Assign (apDevices);
  Ipv4InterfaceContainer wifiStasInterfaces;
  wifiStasInterfaces = address.Assign (staDevices);

  //-----------------------------
  // Application

  NS_LOG_INFO ("Application");

  // packet size should < 2268 if don't want fragment
  ExpAppSenderHelper sender;
  sender.SetAttribute ("Port", UintegerValue (9));
  sender.SetAttribute ("PacketSize", StringValue (packetsize));
  sender.SetAttribute ("Interval", StringValue ("ns3::ExponentialRandomVariable[Mean=0.005]"));

  ExpAppReceiverHelper receiver (9);

  ApplicationContainer receiverApps;
  ApplicationContainer senderApps;

  for (uint32_t i = 0; i < wifiStas.GetN (); i++)
    {
      receiverApps.Add (receiver.Install (wifiStas.Get (i)));
      sender.SetAttribute ("Destination", Ipv4AddressValue (wifiStasInterfaces.GetAddress (i)));
      senderApps.Add (sender.Install (wifiAP.Get (0)));
    }

  receiverApps.Start(Seconds(0.1));
  receiverApps.Stop(Seconds(simTime));
  senderApps.Start(Seconds(0.1));
  senderApps.Stop(Seconds(simTime));

  //-----------------------------
  // Statistics

  DataCollector data;
  data.DescribeRun (experiment, strategy, input, runID);

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

  //-----------------------------
  // Simulation Configuration
  NS_LOG_INFO ("======= Wifi Multi UE END ========");

//  Ptr<FlowMonitor> flowMonitor;
//  FlowMonitorHelper flowHelper;
//  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  Ptr<DataOutputInterface> output = 0;
  output = CreateObject<SqliteDataOutput> ();
  output->SetFilePrefix(prefix);
  if (output != 0)
    output->Output (data);

//  flowMonitor->SerializeToXmlFile("wifionly.xml", false, true);

  Simulator::Destroy ();
}
