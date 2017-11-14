#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/exp-module.h"


#include <sstream>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("'WifiLte'");

void
PrintAddress (Ipv4InterfaceContainer& iifc)
{
  NS_LOG_UNCOND("PrintAddress");
  for (uint32_t i = 0; i < iifc.GetN (); ++i)
    {
      NS_LOG_UNCOND(iifc.GetAddress(i) << " with index " << iifc.Get(i).second);
    }
}

uint32_t
GetInterfaceFromNodeAndDevice (Ptr<Node> node, Ptr<NetDevice> nd)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  return ipv4->GetInterfaceForDevice (nd);
}


/**
 * Name Style:
 * 1. use container to access component
 * 2. {wifi,lte}_{ap,sta,ue,enb}_{node,device,interface}
 */
int 
main (int argc, char *argv[])
{

  NS_LOG_UNCOND("==== eNB -- AP -- UEs");

//-----------------------------
// Command Line

  CommandLine cmd;
  
  bool verbose = false;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  double simTime = 10;
  cmd.AddValue ("simTime", "Simulation Time", simTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("ExpAppSender", LOG_LEVEL_INFO);
      LogComponentEnable ("ExpAppReceiver", LOG_LEVEL_INFO);
    }


//-----------------------------
// Configure Default

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (320));

//-----------------------------
// Node

  // Wifi
  NodeContainer wifi_sta_node;
  wifi_sta_node.Create (3);
  NodeContainer wifi_ap_node;
  wifi_ap_node.Create(1);

  // LTE
  NodeContainer lte_ue_node;
  lte_ue_node.Add (wifi_ap_node);
  NodeContainer lte_enb_node;
  lte_enb_node.Create (1);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
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
																 "X",   DoubleValue (10),
																 "Y",   DoubleValue (20));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifi_sta_node);

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 30.0, 0.0)); 				// for wifi_ap_node
  positionAlloc->Add (Vector (-30.0, -30.0, 0.0)); 	// for eNB
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifi_ap_node.Get (0));
  mobility.Install (lte_enb_node.Get (0));


//-----------------------------
// NetDevice

  // Wifi Part
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

  NetDeviceContainer wifi_sta_device;
  wifi_sta_device = wifi.Install (phy, mac, wifi_sta_node);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer wifi_ap_device;
  wifi_ap_device = wifi.Install (phy, mac, wifi_ap_node);

  // LTE part
  NetDeviceContainer lte_enb_device = lteHelper->InstallEnbDevice (lte_enb_node);
  NetDeviceContainer lte_ue_device = lteHelper->InstallUeDevice (lte_ue_node);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));  // Make sure not to be bottle neck
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.0)));
  NetDeviceContainer p2pDevs = p2ph.Install (pgw, remoteHost);


//-----------------------------
// Internet

  // Wifi
  InternetStackHelper internet;
  internet.Install (wifi_ap_node);
  internet.Install (wifi_sta_node);

  internet.Install (remoteHostContainer);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), 1);

  Ipv4AddressHelper address;

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wifi_ap_interface;
  wifi_ap_interface = address.Assign (wifi_ap_device);
  Ipv4InterfaceContainer wifi_sta_interface;
  wifi_sta_interface = address.Assign (wifi_sta_device);
  address.SetBase ("10.2.2.0", "255.255.255.0");
  address.Assign (p2pDevs);

  // LTE

  Ipv4InterfaceContainer lte_ue_interface = epcHelper->AssignUeIpv4Address (lte_ue_device);

  NS_LOG_UNCOND ("UE lte interface");
  NS_LOG_UNCOND ( lte_ue_node.Get(0)->GetId() << " : " << GetInterfaceFromNodeAndDevice (lte_ue_node.Get(0), lte_ue_device.Get(0)));
  NS_LOG_UNCOND ("UE Ap interface");
  NS_LOG_UNCOND (wifi_ap_node.Get(0)->GetId() << " : "<< GetInterfaceFromNodeAndDevice (wifi_ap_node.Get(0), wifi_ap_device.Get(0)));

  // Set the default gateway for each UE
	for (uint32_t u = 0; u < lte_ue_node.GetN (); ++u)
		{
			Ptr<Node> ueNode = lte_ue_node.Get (u);
			Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
			ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 2);
		}

  // Attach each UE per eNodeB
  // side effect: the default EPS bearer will be activated
	for (uint16_t i = 0; i < lte_ue_device.GetN (); i++)
		{
			lteHelper->Attach (lte_ue_device.Get (i), lte_enb_device.Get (0));
		}


//-----------------------------
// NAT Utils
	Ipv4NatHelper natHelper;
	Ptr<Ipv4Nat> nat = natHelper.Install (wifi_ap_node.Get(0));
  nat->SetInside (GetInterfaceFromNodeAndDevice (wifi_ap_node.Get (0), wifi_ap_device.Get(0)));
  NS_LOG_UNCOND ("Inside -> " << GetInterfaceFromNodeAndDevice (wifi_ap_node.Get (0), wifi_ap_device.Get(0)));
  nat->SetOutside (GetInterfaceFromNodeAndDevice (wifi_ap_node.Get (0), lte_ue_device.Get(0)));
  NS_LOG_UNCOND ("Outside -> " << GetInterfaceFromNodeAndDevice (wifi_ap_node.Get (0), lte_ue_device.Get(0)));
  Ipv4StaticNatRule rule (wifi_sta_interface.GetAddress (0), 8080, lte_ue_interface.GetAddress (0), 9, 0);
  nat->AddStaticRule (rule);
  Ptr<OutputStreamWrapper> natStream = Create<OutputStreamWrapper> ("nat.rules", std::ios::out);
  nat->PrintTable (natStream);
  //system ("cat nat.rules && echo && rm nat.rules");

//-----------------------------
// Application
  ExpAppSenderHelper sender;
  sender.SetAttribute ("Port", UintegerValue (9));
  sender.SetAttribute ("PacketSize", StringValue ("ns3::UniformRandomVariable[Min=1200|Max=1300]"));
  sender.SetAttribute ("Interval", StringValue ("ns3::ExponentialRandomVariable[Mean=0.01]"));

  ExpAppReceiverHelper receiver (8080);

  ApplicationContainer receiverApps;
  ApplicationContainer senderApps;

  receiverApps.Add (receiver.Install (wifi_sta_node.Get (0)));
  sender.SetAttribute ("Destination", Ipv4AddressValue (lte_ue_interface.GetAddress (0)));
  NS_LOG_UNCOND ("--- UE Interface ");
  PrintAddress(lte_ue_interface);
  senderApps.Add (sender.Install (remoteHost));
  NS_LOG_UNCOND ("--- Sta Interface ");
  PrintAddress (wifi_sta_interface);

  receiverApps.Start(Seconds(0.1));
  receiverApps.Stop(Seconds(simTime));
  senderApps.Start(Seconds(0.1));
  senderApps.Stop(Seconds(simTime));

  Ptr<Ipv4StaticRouting> ap_ue_static_route = ipv4RoutingHelper.GetStaticRouting (
      lte_ue_node.Get (0)->GetObject<Ipv4> ());
  ap_ue_static_route->AddNetworkRouteTo (Ipv4Address ("192.168.1.0"), Ipv4Mask ("255.255.255.0"),
                                         GetInterfaceFromNodeAndDevice (wifi_ap_node.Get (0), wifi_ap_device.Get (0)));
//-----------------------------
// Statisitcs


//-----------------------------
// Simulation Configuration

  NS_LOG_UNCOND ("==================Wifi Lte END================");
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}
