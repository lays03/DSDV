/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hemanth Narra, Yufei Cheng
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hemanth Narra <hemanth@ittc.ku.com>
 * Author: Yufei Cheng   <yfcheng@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

#include "dsdv-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include <ctime>
#include <fstream>
#include <iostream>
using namespace std;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DsdvRoutingProtocol");

namespace dsdv {

NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for DSDV control traffic
const uint32_t RoutingProtocol::DSDV_PORT = 269;

/// Tag used by DSDV implementation
struct DeferredRouteOutputTag : public Tag
{
  /// Positive if output device is fixed in RouteOutput
  int32_t oif;

  /**
   * Constructor
   *
   * \param o outgoing interface (OIF)
   */
  DeferredRouteOutputTag (int32_t o = -1)
    : Tag (),
      oif (o)
  {
  }

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId
  GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::dsdv::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("Dsdv")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId
  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  uint32_t
  GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void
  Serialize (TagBuffer i) const
  {
    i.WriteU32 (oif);
  }

  void
  Deserialize (TagBuffer i)
  {
    oif = i.ReadU32 ();
  }

  void
  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << oif;
  }
};

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::dsdv::RoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName ("Dsdv")
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("PeriodicUpdateInterval","Periodic interval between exchange of full routing tables among nodes. ",
                   TimeValue (Seconds (10)),
                   MakeTimeAccessor (&RoutingProtocol::m_periodicUpdateInterval),
                   MakeTimeChecker ())
    .AddAttribute ("SettlingTime", "Minimum time an update is to be stored in adv table before sending out"
                   "in case of change in metric (in seconds)",
                   TimeValue (Seconds (5)),
                   MakeTimeAccessor (&RoutingProtocol::m_settlingTime),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (500 /*assuming maximum nodes in simulation is 100*/),
                   MakeUintegerAccessor (&RoutingProtocol::m_maxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueuedPacketsPerDst", "Maximum number of packets that we allow per destination to buffer.",
                   UintegerValue (5),
                   MakeUintegerAccessor (&RoutingProtocol::m_maxQueuedPacketsPerDst),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime","Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&RoutingProtocol::m_maxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("EnableBuffering","Enables buffering of data packets if no route to destination is available",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetEnableBufferFlag,
                                        &RoutingProtocol::GetEnableBufferFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableWST","Enables Weighted Settling Time for the updates before advertising",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetWSTFlag,
                                        &RoutingProtocol::GetWSTFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("Holdtimes","Times the forwarding Interval to purge the route.",
                   UintegerValue (3),
                   MakeUintegerAccessor (&RoutingProtocol::Holdtimes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("WeightedFactor","WeightedFactor for the settling time if Weighted Settling Time is enabled",
                   DoubleValue (0.875),
                   MakeDoubleAccessor (&RoutingProtocol::m_weightedFactor),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableRouteAggregation","Enables Weighted Settling Time for the updates before advertising",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::SetEnableRAFlag,
                                        &RoutingProtocol::GetEnableRAFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("RouteAggregationTime","Time to aggregate updates before sending them out (in seconds)",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&RoutingProtocol::m_routeAggregationTime),
                   MakeTimeChecker ());
  return tid;
}

// 设置启用缓冲区标志
void
RoutingProtocol::SetEnableBufferFlag (bool f)
{
  EnableBuffering = f;
}

bool
RoutingProtocol::GetEnableBufferFlag () const
{
  return EnableBuffering;
}

//  设置加权稳定时间 (WST) 标志
void
RoutingProtocol::SetWSTFlag (bool f)
{
  EnableWST = f;
}

bool
RoutingProtocol::GetWSTFlag () const
{
  return EnableWST;
}

// 设置启用路由聚合 (RA) 标志
void
RoutingProtocol::SetEnableRAFlag (bool f)
{
  EnableRouteAggregation = f;
}

bool
RoutingProtocol::GetEnableRAFlag () const
{
  return EnableRouteAggregation;
}

// 为模型使用的随机变量分配一个固定的随机变量流号。返回已分配的流的数目(可能为0)。
int64_t
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

RoutingProtocol::RoutingProtocol ()
  : m_routingTable (),
    m_advRoutingTable (),
    m_queue (),
    m_periodicUpdateTimer (Timer::CANCEL_ON_DESTROY)
{
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv4 = 0;
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter = m_socketAddresses.begin (); iter
       != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  Ipv4RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
                        << ", Time: " << Now ().As (unit)
                        << ", Local time: " << m_ipv4->GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", DSDV Routing table" << std::endl;

  m_routingTable.Print (stream, unit);
  *stream->GetStream () << std::endl;
}

void
RoutingProtocol::Start ()
{
  m_queue.SetMaxPacketsPerDst (m_maxQueuedPacketsPerDst);
  m_queue.SetMaxQueueLen (m_maxQueueLen);
  m_queue.SetQueueTimeout (m_maxQueueTime);
  m_routingTable.Setholddowntime (Time (Holdtimes * m_periodicUpdateInterval));
  m_advRoutingTable.Setholddowntime (Time (Holdtimes * m_periodicUpdateInterval));
  m_scb = MakeCallback (&RoutingProtocol::Send,this);
  m_ecb = MakeCallback (&RoutingProtocol::Drop,this);
  m_periodicUpdateTimer.SetFunction (&RoutingProtocol::SendPeriodicUpdate,this);
  m_periodicUpdateTimer.Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)));
}

// SettlingTime指定节点在传播更新之前等待的时间。它等待这个时间间隔，希望接收到具有更好度量的更新。

// 首先对路由表进行清理，删除掉过期的路由并将他们加入advRoutingTable，使用触发器更新。
// 然后在新的路由表中查找到目的地的路由，如果有则返回，否则返回回环地址。

// 当节点主动的发送数据包的时候会调用，该函数会返回到目的地的路由
Ptr<Ipv4Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p,
                              const Ipv4Header &header,
                              Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));

  if (!p) 
    {
      return LoopbackRoute (header,oif); // 为给定标头创建环回路由
    }
  if (m_socketAddresses.empty ()) //如果socket是空
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_LOGIC ("No dsdv interfaces");
      std::cout<<"NO dsdv interfaces"<<std::endl;
      Ptr<Ipv4Route> route;
      return route;
    }
  std::map<Ipv4Address, RoutingTableEntry> removedAddresses;
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv4Route> route;
  Ipv4Address dst = header.GetDestination ();
  NS_LOG_DEBUG ("Packet Size: " << p->GetSize ()
                                << ", Packet id: " << p->GetUid () << ", Destination address in Packet: " << dst);
  std::cout<<"Packet Size: "<< p->GetSize()<<", Packet id: "<<p->GetUid()<<", Destination address in Packet: "<<dst<<std::endl;
  RoutingTableEntry rt;

  // 清除已经过期的路由,并把它们记录在removedAddresses中
  m_routingTable.Purge (removedAddresses);
  for (std::map<Ipv4Address, RoutingTableEntry>::iterator rmItr = removedAddresses.begin ();
       rmItr != removedAddresses.end (); ++rmItr)
    {
      //ADD:
      rmItr->second.SetX(rmItr->second.GetX());
      rmItr->second.SetY(rmItr->second.GetY());
      rmItr->second.SetZ(rmItr->second.GetX());
      rmItr->second.SetVX(rmItr->second.GetVX());
      rmItr->second.SetVY(rmItr->second.GetVY());
      rmItr->second.SetVZ(rmItr->second.GetVZ());
      rmItr->second.SetTimestamp(rmItr->second.GetTimestamp());

      rmItr->second.SetEntriesChanged (true);
      rmItr->second.SetSeqNo (rmItr->second.GetSeqNo () + 1);
      m_advRoutingTable.AddRoute (rmItr->second);
    }
    // 如果有过期的路由，要添加到advRoutingTable,并且马上使用触发器更新
  if (!removedAddresses.empty ())
    {
      Simulator::Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)),&RoutingProtocol::SendTriggeredUpdate,this);
    }
  // 前提：目前路由表已经干净了
  // 查找路由表中是否有到目的地的路由
  // 有到达目的地的路由
  if (m_routingTable.LookupRoute (dst,rt))
    {
      if (EnableBuffering)
        {
          // 查看是否有可以从队列中发送出去的数据包
          LookForQueuedPackets ();
        }
        // 目的地就是邻居
      if (rt.GetHop () == 1)
        {
          route = rt.GetRoute ();
          NS_ASSERT (route != 0);
          NS_LOG_DEBUG ("A route exists from " << route->GetSource ()
                                               << " to neighboring destination "
                                               << route->GetDestination ());
          std::cout<<"A route exists from "<<route->GetSource ()
                                               << " to neighboring destination "
                                               << route->GetDestination ()<<endl;
          if (oif != 0 && route->GetOutputDevice () != oif)
            {
              NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
              sockerr = Socket::ERROR_NOROUTETOHOST;
              return Ptr<Ipv4Route> ();
            }
            // 返回直接到邻居的路由
          return route;
        }
      // 有路由，但是不是邻居，只有到达目的地应该去的下一跳
      else
        {
          RoutingTableEntry newrt;
          // 查找到下一跳的路由
          if (m_routingTable.LookupRoute (rt.GetNextHop (),newrt))
            {
              route = newrt.GetRoute ();
              NS_ASSERT (route != 0);
              NS_LOG_DEBUG ("A route exists from " << route->GetSource ()
                                                   << " to destination " << dst << " via "
                                                   << rt.GetNextHop ());
              if (oif != 0 && route->GetOutputDevice () != oif)
                {
                  NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
                  cout<<"Output device doesn't match. Dropped."<<endl;
                  sockerr = Socket::ERROR_NOROUTETOHOST;
                  return Ptr<Ipv4Route> ();
                }
                //返回到目的地的下一跳路由
              return route;
            }
        }
    }
  // 路由表中没有到目的地的路由，返回回环地址
  if (EnableBuffering)
    {
      uint32_t iif = (oif ? m_ipv4->GetInterfaceForDevice (oif) : -1);
      DeferredRouteOutputTag tag (iif);
      if (!p->PeekPacketTag (tag))
        {
          p->AddPacketTag (tag);
        }
    }
  return LoopbackRoute (header,oif);
}

// 排队等待，直到找到路由
void
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p,
                                      const Ipv4Header & header,
                                      UnicastForwardCallback ucb,
                                      ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());
  QueueEntry newEntry (p,header,ucb,ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      NS_LOG_DEBUG ("Added packet " << p->GetUid () << " to queue.");
    }
}

// 用来接收别人发过来的数据包，然后决定如何继续转发出去。
// 返回的值是一个布尔值，如果Ipv4RoutingProtocol负责转发或者交付数据包，则为true，否则为false。
bool
RoutingProtocol::RouteInput (Ptr<const Packet> p,
                             const Ipv4Header &header,
                             Ptr<const NetDevice> idev,
                             UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb,
                             LocalDeliverCallback lcb,
                             ErrorCallback ecb)
{
  NS_LOG_FUNCTION (m_mainAddress << " received packet " << p->GetUid ()
                                 << " from " << header.GetSource ()
                                 << " on interface " << idev->GetAddress ()
                                 << " to destination " << header.GetDestination ());
  cout<<m_mainAddress << " received packet " << p->GetUid ()
                                 << " from " << header.GetSource ()
                                 << " on interface " << idev->GetAddress ()
                                 << " to destination " << header.GetDestination ()<<endl;
  if (m_socketAddresses.empty ())
    {
      NS_LOG_DEBUG ("No dsdv interfaces");
      cout<<"No dsdv interfaces"<<endl;
      return false;
    }
  NS_ASSERT (m_ipv4 != 0);
  // Check if input device supports IP 检查输入设备是否支持IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv4->GetInterfaceForDevice (idev);
  cout<<"RouteInput iif: "<<iif<<endl;


  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();

  // DSDV is not a multicast routing protocol
  if (dst.IsMulticast ())
    {
      return false;
    }

  // Deferred route request 延迟路由请求
  if (EnableBuffering == true && idev == m_lo)
    {
      DeferredRouteOutputTag tag;
      if (p->PeekPacketTag (tag))
        {
          DeferredRouteOutput (p,header,ucb,ecb);
          return true;
        }
    }
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (origin == iface.GetLocal ())
        {
          return true;
        }
    }
  // LOCAL DELIVARY TO DSDV INTERFACES 本地转发到dsdv接口
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()) == iif)
        {
          if (dst == iface.GetBroadcast () || dst.IsBroadcast ())
            {
              Ptr<Packet> packet = p->Copy ();
              if (lcb.IsNull () == false)
                {
                  NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetLocal ());
                  lcb (p, header, iif);
                  // Fall through to additional processing
                }
              else
                {
                  NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                  ecb (p, header, Socket::ERROR_NOROUTETOHOST);
                }
              if (header.GetTtl () > 1)
                {
                  NS_LOG_LOGIC ("Forward broadcast. TTL " << (uint16_t) header.GetTtl ());
                  RoutingTableEntry toBroadcast;
                  if (m_routingTable.LookupRoute (dst,toBroadcast,true))
                    {
                      Ptr<Ipv4Route> route = toBroadcast.GetRoute ();
                      ucb (route,packet,header);
                    }
                  else
                    {
                      NS_LOG_DEBUG ("No route to forward. Drop packet " << p->GetUid ());
                    }
                }
              return true;
            }
        }
    }

  if (m_ipv4->IsDestinationAddress (dst, iif))
    {
      if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
          lcb (p, header, iif);
        }
      else
        {
          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return true;
    }

  // Check if input device supports IP forwarding 检查输入设备是否支持IP转发
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }

  RoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst,toDst))
    {
      RoutingTableEntry ne;
      if (m_routingTable.LookupRoute (toDst.GetNextHop (),ne))
        {
          Ptr<Ipv4Route> route = ne.GetRoute ();
          NS_LOG_LOGIC (m_mainAddress << " is forwarding packet " << p->GetUid ()
                                      << " to " << dst
                                      << " from " << header.GetSource ()
                                      << " via nexthop neighbor " << toDst.GetNextHop ());
          ucb (route,p,header);
          return true;
        }
    }
  NS_LOG_LOGIC ("Drop packet " << p->GetUid ()
                               << " as there is no route to forward it.");
  return false;
}

// 为给定报头创建环回路由
Ptr<Ipv4Route>
RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const
{
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetDestination (hdr.GetDestination ());
  // rt->SetSource (hdr.GetSource ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when DSDV does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // DSDV needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available DSDV interface.
  // If RouteOutput() caller specified an outgoing interface, that
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4Address addr = j->second.GetLocal ();
          int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
          if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetLocal ());
    }
  NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid DSDV source address not found");
  rt->SetGateway (Ipv4Address ("127.0.0.1"));
  rt->SetOutputDevice (m_lo);
  return rt;
}

// Receive and process dsdv control packet
// 参数是：用于接收DSDV控制报文的socket
// 这是socket收到数据包时调用的回调函数->RouteInput->RouteOutput
// 修改路由表表项的地方
void
RoutingProtocol::RecvDsdv (Ptr<Socket> socket)
{
  Address sourceAddress;
  Ptr<Packet> advpacket = Create<Packet> ();
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiver = m_socketAddresses[socket].GetLocal ();
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));

// ipv4->GetAddress (1,0); 将获取第二个接口的第一个地址（第一个是环回，其地址为 127.0.0.1）
//   cout<<"dev: "<<dev<<", 接口数量："<<m_ipv4->GetNInterfaces()<<endl;x
//   cout<<"dev: "<<dev<<", 第一个接口的ip地址是"<<m_ipv4->GetAddress (0, 0)<<endl;
//   cout<<"dev: "<<dev<<", 第二个接口的ip地址是"<<m_ipv4->GetAddress (1, 0)<<endl;
//   cout<<"dev: "<<dev<<", 第三个接口的ip地址是"<<m_ipv4->GetAddress (2, 0)<<endl<<endl;

// for (uint32_t ifIndex = 0; ifIndex <= m_ipv4->GetNInterfaces(); ifIndex++)
// {
//   for (uint32_t addrIndex = 0; addrIndex < m_ipv4->GetNAddresses(ifIndex); addrIndex++)
//   {
//     Ipv4InterfaceAddress iaddr = m_ipv4->GetAddress (ifIndex, addrIndex);
//     Ipv4Address source = iaddr.GetAddress();
//   }
// }
  

  uint32_t packetSize = packet->GetSize ();
  NS_LOG_FUNCTION (m_mainAddress << " received dsdv packet of size: " << packetSize
                                 << " and packet id: " << packet->GetUid ());
  cout<<m_mainAddress << " received dsdv packet of size: " << packetSize
                                 << " and packet id: " << packet->GetUid ()<<endl;
  uint32_t count = 0;
  //ADD 头部大小改了，改成28了，所以这里之前的12要改成28
  for (; packetSize > 0; packetSize = packetSize - 28)
    {
      count = 0;
      DsdvHeader dsdvHeader, tempDsdvHeader;
      packet->RemoveHeader (dsdvHeader);
      NS_LOG_DEBUG ("Processing new update for " << dsdvHeader.GetDst ());
      /*Verifying if the packets sent by me were returned back to me. If yes, discarding them!*/
      /*验证我发送的数据包是否被退回给我。如果是，则丢弃它们!*/
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
           != m_socketAddresses.end (); ++j)
        {
          Ipv4InterfaceAddress interface = j->second;
          cout << "j->second: "<<j->second<<endl;
          if (dsdvHeader.GetDst () == interface.GetLocal ()) //如果目的地跟自己的地址一样
            {
              if (dsdvHeader.GetDstSeqno () % 2 == 1) //判断序列号是不是正确的
                {
                  NS_LOG_DEBUG ("Sent Dsdv update back to the same Destination, "
                                "with infinite metric. Time left to send fwd update: "
                                << m_periodicUpdateTimer.GetDelayLeft ());
                  cout<<"Sent Dsdv update back to the same Destination, "
                                "with infinite metric. Time left to send fwd update: "
                                << m_periodicUpdateTimer.GetDelayLeft ()<<endl;
                  count++;
                }
              else
                {
                  NS_LOG_DEBUG ("Received update for my address. Discarding this.");
                  cout<<"Received update for my address. Discarding this."<<endl;
                  count++;
                }
            }
        }
      if (count > 0)
        {
          continue;
        }
      NS_LOG_DEBUG ("Received a DSDV packet from "
                    << sender << " to " << receiver << ". Details are: Destination: " << dsdvHeader.GetDst () << ", Seq No: "
                    << dsdvHeader.GetDstSeqno () << ", HopCount: " << dsdvHeader.GetHopCount ());
      // cout<<"Received a DSDV packet from "<< sender << " to " << receiver << ". Details are: Destination: " << dsdvHeader.GetDst () << ", Seq No: "
      //               << dsdvHeader.GetDstSeqno () << ", HopCount: " << dsdvHeader.GetHopCount ()<<endl;
      RoutingTableEntry fwdTableEntry, advTableEntry;
      EventId event;
      bool permanentTableVerifier = m_routingTable.LookupRoute (dsdvHeader.GetDst (),fwdTableEntry);
      // 收到的更新在主路由表中不存在
      if (permanentTableVerifier == false)
        {
          if (dsdvHeader.GetDstSeqno () % 2 != 1)
            {
              NS_LOG_DEBUG ("Received New Route!");
              cout<<"Received New Route!"<<endl;
              RoutingTableEntry newEntry (
                //Add:
                dsdvHeader.GetX(),
                dsdvHeader.GetY(),
                dsdvHeader.GetZ(),
                dsdvHeader.GetVX(),
                dsdvHeader.GetVY(),
                dsdvHeader.GetVZ(),
                dsdvHeader.GetTimestamp(),
                /*device=*/ dev, 
                /*dst=*/dsdvHeader.GetDst (), 
                /*seqno=*/dsdvHeader.GetDstSeqno (),
                /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),  //存储是从哪个接口接收到的包
                /*hops=*/ dsdvHeader.GetHopCount (), 
                /*next hop=*/sender, 
                /*lifetime=*/Simulator::Now (), 
                /*settlingTime*/m_settlingTime, 
                /*entries changed*/true);
              newEntry.SetFlag (VALID);
              m_routingTable.AddRoute (newEntry);
              NS_LOG_DEBUG ("New Route added to both tables");
              cout<<"New Route added to both tables"<<endl;
              m_advRoutingTable.AddRoute (newEntry);
            }
          else
            {
              // received update not present in main routing table and also with infinite metric
              // 收到的更新在主路由表中不存在，并且具有无限度量
              NS_LOG_DEBUG ("Discarding this update as this route is not present in "
                            "main routing table and received with infinite metric");
              cout<<"Discarding this update as this route is not present in "
                            "main routing table and received with infinite metric"<<endl;
            }
        }
        // 主路由表中有到达目的地的路由
      else
        {
          if (!m_advRoutingTable.LookupRoute (dsdvHeader.GetDst (),advTableEntry))
            {
              RoutingTableEntry tr;
              std::map<Ipv4Address, RoutingTableEntry> allRoutes;
              m_advRoutingTable.GetListOfAllRoutes (allRoutes);
              for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
                {
                  NS_LOG_DEBUG ("ADV table routes are:" << i->second.GetDestination ());
                }
              // present in fwd table and not in advtable
              // 在FWD表中存在，在advtable中不存在
              m_advRoutingTable.AddRoute (fwdTableEntry);
              m_advRoutingTable.LookupRoute (dsdvHeader.GetDst (),advTableEntry);
            }
            // 现在两个表中都有到目的地的表项了，并且路由表项是有效的
          if (dsdvHeader.GetDstSeqno () % 2 != 1)
            {
              if (dsdvHeader.GetDstSeqno () > advTableEntry.GetSeqNo ())
                {
                  // Received update with better seq number. Clear any old events that are running
                  // 收到更新与更好的序列号。清除正在运行的旧事件
                  if (m_advRoutingTable.ForceDeleteIpv4Event (dsdvHeader.GetDst ()))
                    {
                      NS_LOG_DEBUG ("Canceling the timer to update route with better seq number");
                      //取消定时器，以更新路由的更好的seq编号
                    }
                  // if its a changed metric *nomatter* where the update came from, wait  for WST
                  // 如果指标发生了变化，* *无论* *更新来自哪里，都要等待WST
                  if (dsdvHeader.GetHopCount () != advTableEntry.GetHop ())
                    {
                      //ADD:
                      advTableEntry.SetX(dsdvHeader.GetX());
                      advTableEntry.SetY(dsdvHeader.GetY());
                      advTableEntry.SetZ(dsdvHeader.GetZ());
                      advTableEntry.SetVX(dsdvHeader.GetVX());
                      advTableEntry.SetVY(dsdvHeader.GetVY());
                      advTableEntry.SetVZ(dsdvHeader.GetVZ());
                      advTableEntry.SetTimestamp(dsdvHeader.GetTimestamp());
                      
                      advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (sender);
                      advTableEntry.SetHop (dsdvHeader.GetHopCount ());
                      NS_LOG_DEBUG ("Received update with better sequence number and changed metric.Waiting for WST");
                      Time tempSettlingtime = GetSettlingTime (dsdvHeader.GetDst ());
                      advTableEntry.SetSettlingTime (tempSettlingtime);
                      NS_LOG_DEBUG ("Added Settling Time:" << tempSettlingtime.As (Time::S)
                                                           << " as there is no event running for this route");
                      event = Simulator::Schedule (tempSettlingtime,&RoutingProtocol::SendTriggeredUpdate,this);
                      m_advRoutingTable.AddIpv4Event (dsdvHeader.GetDst (),event);
                      NS_LOG_DEBUG ("EventCreated EventUID: " << event.GetUid ());
                      cout<<"EventCreated EventUID: " << event.GetUid ()<<endl;
                      // if received changed metric, use it but adv it only after wst
                      // 如果收到更改的度量，则使用它，但仅在WST之后进行adv
                      m_routingTable.Update (advTableEntry);
                      m_advRoutingTable.Update (advTableEntry);
                    }
                  else
                    {
                      // Received update with better seq number and same metric.
                      // 收到了更好的seq数和相同的度量更新。

                      //ADD:
                      advTableEntry.SetX(dsdvHeader.GetX());
                      advTableEntry.SetY(dsdvHeader.GetY());
                      advTableEntry.SetZ(dsdvHeader.GetZ());
                      advTableEntry.SetVX(dsdvHeader.GetVX());
                      advTableEntry.SetVY(dsdvHeader.GetVY());
                      advTableEntry.SetVZ(dsdvHeader.GetVZ());
                      advTableEntry.SetTimestamp(dsdvHeader.GetTimestamp());

                      advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (sender);
                      advTableEntry.SetHop (dsdvHeader.GetHopCount ());
                      m_advRoutingTable.Update (advTableEntry);
                      NS_LOG_DEBUG ("Route with better sequence number and same metric received. Advertised without WST");
                      cout<<"Route with better sequence number and same metric received. Advertised without WST"<<endl;
                    }
                }
              else if (dsdvHeader.GetDstSeqno () == advTableEntry.GetSeqNo ())
                {
                  if (dsdvHeader.GetHopCount () < advTableEntry.GetHop ())
                    {
                      /*Received update with same seq number and better hop count.
                      // 相同的seq数和更好的跳数
                       * As the metric is changed, we will have to wait for WST before sending out this update.
                       */
                      // 由于指标发生了变化，我们必须等待WST之后才能发送此更新。
                      NS_LOG_DEBUG ("Canceling any existing timer to update route with same sequence number "
                                    "and better hop count");
                      cout<<"Canceling any existing timer to update route with same sequence number "
                                    "and better hop count"<<endl;
                      m_advRoutingTable.ForceDeleteIpv4Event (dsdvHeader.GetDst ());

                      //ADD:
                      advTableEntry.SetX(dsdvHeader.GetX());
                      advTableEntry.SetY(dsdvHeader.GetY());
                      advTableEntry.SetZ(dsdvHeader.GetZ());
                      advTableEntry.SetVX(dsdvHeader.GetVX());
                      advTableEntry.SetVY(dsdvHeader.GetVY());
                      advTableEntry.SetVZ(dsdvHeader.GetVZ());
                      advTableEntry.SetTimestamp(dsdvHeader.GetTimestamp());

                      advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (sender);
                      advTableEntry.SetHop (dsdvHeader.GetHopCount ());
                      Time tempSettlingtime = GetSettlingTime (dsdvHeader.GetDst ());
                      advTableEntry.SetSettlingTime (tempSettlingtime);
                      NS_LOG_DEBUG ("Added Settling Time," << tempSettlingtime.As (Time::S)
                                                           << " as there is no current event running for this route");
                      event = Simulator::Schedule (tempSettlingtime,&RoutingProtocol::SendTriggeredUpdate,this);
                      m_advRoutingTable.AddIpv4Event (dsdvHeader.GetDst (),event);
                      NS_LOG_DEBUG ("EventCreated EventUID: " << event.GetUid ());
                      // if received changed metric, use it but adv it only after wst
                      m_routingTable.Update (advTableEntry);
                      m_advRoutingTable.Update (advTableEntry);
                    }
                  else
                    {
                      /*Received update with same seq number but with same or greater hop count.
                       * Discard that update.
                       */
                      // 收到更新，seq编号相同，但跳数相同或更大。丢弃该更新。
                      if (!m_advRoutingTable.AnyRunningEvent (dsdvHeader.GetDst ()))
                        {
                          /*update the timer only if nexthop address matches thus discarding
                           * updates to that destination from other nodes.
                           */
                          // 仅当下一跳地址匹配时更新定时器，从而丢弃其他节点对该目的地址的更新。
                          if (advTableEntry.GetNextHop () == sender)
                            {
                              advTableEntry.SetLifeTime (Simulator::Now ());
                              m_routingTable.Update (advTableEntry);
                            }
                          m_advRoutingTable.DeleteRoute (
                            dsdvHeader.GetDst ());
                        }
                      NS_LOG_DEBUG ("Received update with same seq number and "
                                    "same/worst metric for, " << dsdvHeader.GetDst () << ". Discarding the update.");
                    }
                }
              else
                {
                  // Received update with an old sequence number. Discard the update
                  // 收到更新的旧序列号。丢弃更新
                  if (!m_advRoutingTable.AnyRunningEvent (dsdvHeader.GetDst ()))
                    {
                      m_advRoutingTable.DeleteRoute (dsdvHeader.GetDst ());
                    }
                  NS_LOG_DEBUG (dsdvHeader.GetDst () << " : Received update with old seq number. Discarding the update.");
                }
            }
          else
            {
              NS_LOG_DEBUG ("Route with infinite metric received for "
                            << dsdvHeader.GetDst () << " from " << sender);
              // Delete route only if update was received from my nexthop neighbor
              // 仅当收到下一跳邻居的更新时才删除路由
              if (sender == advTableEntry.GetNextHop ())
                {
                  // 触发此不可达路由的更新
                  NS_LOG_DEBUG ("Triggering an update for this unreachable route:");
                  std::map<Ipv4Address, RoutingTableEntry> dstsWithNextHopSrc;
                  m_routingTable.GetListOfDestinationWithNextHop (dsdvHeader.GetDst (),dstsWithNextHopSrc);
                  m_routingTable.DeleteRoute (dsdvHeader.GetDst ());
                  advTableEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
                  advTableEntry.SetEntriesChanged (true);
                  m_advRoutingTable.Update (advTableEntry);
                  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = dstsWithNextHopSrc.begin (); i
                       != dstsWithNextHopSrc.end (); ++i)
                    {
                      i->second.SetSeqNo (i->second.GetSeqNo () + 1);
                      i->second.SetEntriesChanged (true);
                      m_advRoutingTable.AddRoute (i->second);
                      m_routingTable.DeleteRoute (i->second.GetDestination ());
                    }
                }
              else
                {
                  if (!m_advRoutingTable.AnyRunningEvent (dsdvHeader.GetDst ()))
                    {
                      m_advRoutingTable.DeleteRoute (dsdvHeader.GetDst ());
                    }
                  NS_LOG_DEBUG (dsdvHeader.GetDst () <<
                                " : Discard this link break update as it was received from a different neighbor "
                                "and I can reach the destination");
                }
            }
        }
    }
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllRoutes (allRoutes);
  if (EnableRouteAggregation && allRoutes.size () > 0)
    {
      Simulator::Schedule (m_routeAggregationTime,&RoutingProtocol::SendTriggeredUpdate,this);
    }
  else
    {
      Simulator::Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)),&RoutingProtocol::SendTriggeredUpdate,this);
    }
}

// 更新m_advRoutingTable中的数据，只有在settlingtime之后才会广播
// 通过引入**稳定时间(settling time)**和**延迟广播**可以避免DSDV协议中的波动。
// 稳定时间是在某个给定的序列号下，某节点收到第一条路由和最佳路由的时间差，即表示该节点找到最佳路由的稳定时间。
// 为避免路由表发生频繁波动，当一个节点收到一条路由信息，它会根据情况更新自己的路由表，
// 但不会立即广播本次更新，而是会等一段时间。这个等待时间被定义为平均稳定时间的两倍值。

map<string,bool> mp; //判断同一时间的操作是否重复

void
RoutingProtocol::SendTriggeredUpdate ()
{
  NS_LOG_FUNCTION (m_mainAddress << " is sending a triggered update");
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllRoutes (allRoutes);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      DsdvHeader dsdvHeader;
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      // cout<<"SendTriggeredUpdate iface:"<<iface<<endl;
      Ptr<Packet> packet = Create<Packet> ();
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          NS_LOG_LOGIC ("Destination: " << i->second.GetDestination ()
                                        << " SeqNo:" << i->second.GetSeqNo () << " HopCount:"
                                        << i->second.GetHop () + 1);
          RoutingTableEntry temp = i->second;
          if ((i->second.GetEntriesChanged () == true) && (!m_advRoutingTable.AnyRunningEvent (temp.GetDestination ())))
            {
              //ADD:
              dsdvHeader.SetX(i->second.GetX());
              dsdvHeader.SetY(i->second.GetY());
              dsdvHeader.SetZ(i->second.GetZ());
              dsdvHeader.SetVX(i->second.GetVX());
              dsdvHeader.SetVY(i->second.GetVY());
              dsdvHeader.SetVZ(i->second.GetVZ());
              dsdvHeader.SetTimestamp(i->second.GetTimestamp());
              dsdvHeader.SetSign(dsdvHeader.GetSign());

              dsdvHeader.SetDst (i->second.GetDestination ());
              dsdvHeader.SetDstSeqno (i->second.GetSeqNo ());
              dsdvHeader.SetHopCount (i->second.GetHop () + 1);
              temp.SetFlag (VALID);
              temp.SetEntriesChanged (false);
              m_advRoutingTable.DeleteIpv4Event (temp.GetDestination ());
              if (!(temp.GetSeqNo () % 2))
                {
                  m_routingTable.Update (temp);
                }
              packet->AddHeader (dsdvHeader);
              m_advRoutingTable.DeleteRoute (temp.GetDestination ());
              NS_LOG_DEBUG ("Deleted this route from the advertised table");
            }
          else
            {
              EventId event = m_advRoutingTable.GetEventId (temp.GetDestination ());
              NS_ASSERT (event.GetUid () != 0);
              NS_LOG_DEBUG ("EventID " << event.GetUid () << " associated with "
                                       << temp.GetDestination () << " has not expired, waiting in adv table");
            }
        }
      // 只有数据包大小大于12才会转发，因为12是header的大小，小于12说明没有header
       //ADD 头部大小改了，改成28了，所以这里之前的12要改成28
      if (packet->GetSize () >= 28)
        {
          RoutingTableEntry temp2;
          m_routingTable.LookupRoute (m_ipv4->GetAddress (1, 0).GetBroadcast (), temp2);

          //ADD:
          dsdvHeader.SetX(temp2.GetX());
          dsdvHeader.SetY(temp2.GetY());
          dsdvHeader.SetZ(temp2.GetZ());
          dsdvHeader.SetVX(temp2.GetVX());
          dsdvHeader.SetVY(temp2.GetVY());
          dsdvHeader.SetVZ(temp2.GetVZ());
          dsdvHeader.SetTimestamp(temp2.GetTimestamp());
          dsdvHeader.SetSign(dsdvHeader.GetSign());

          dsdvHeader.SetDst (m_ipv4->GetAddress (1, 0).GetLocal ());
          dsdvHeader.SetDstSeqno (temp2.GetSeqNo ());
          dsdvHeader.SetHopCount (temp2.GetHop () + 1);
          NS_LOG_DEBUG ("Adding my update as well to the packet");
          packet->AddHeader (dsdvHeader);
          // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
          Ipv4Address destination;
          if (iface.GetMask () == Ipv4Mask::GetOnes ())
            {
              destination = Ipv4Address ("255.255.255.255");
            }
          else
            {
              destination = iface.GetBroadcast ();
            }
          socket->SendTo (packet, 0, InetSocketAddress (destination, DSDV_PORT));
          NS_LOG_FUNCTION ("Sent Triggered Update from "
                           << dsdvHeader.GetDst ()
                           << " with packet id : " << packet->GetUid () << " and packet Size: " << packet->GetSize ());
        }
      else
        {
          NS_LOG_FUNCTION ("Update not sent as there are no updates to be triggered");
        }
    }
}

// 发送定期更新
// 更新m_routingTable中的数据
void
RoutingProtocol::SendPeriodicUpdate ()
{ 
  std::map<Ipv4Address, RoutingTableEntry> removedAddresses, allRoutes;
  m_routingTable.Purge (removedAddresses);
  MergeTriggerPeriodicUpdates ();
  m_routingTable.GetListOfAllRoutes (allRoutes);
  if (allRoutes.empty ())
    {
      return;
    }
  NS_LOG_FUNCTION (m_mainAddress << " is sending out its periodic update");

  //首先定义一个header，放入对应的信息。再通过AddHeader()将包头添加到packet上
  //遍历节点的每一个socket，使用sendTo将packet广播
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      // m_ipv4->GetAddress (1, 0)
      Ipv4InterfaceAddress iface = j->second;
      // cout<<"SendPeriodicUpdate iface:"<<iface<<endl;
      Ptr<Packet> packet = Create<Packet> ();

      //Add:
      Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();
      Vector myPos = MM->GetPosition();
      Vector myVel = MM->GetVelocity();
      int16_t vx = (int16_t)myVel.x;
      int16_t vy = (int16_t)myVel.y;
      int16_t vz = (int16_t)myVel.z;
      cout<<"x: "<<(uint16_t)myPos.x<<", y: "<<(uint16_t)myPos.y<<", z: "<<(uint16_t)myPos.z<<endl;
      cout<<"vx: "<<vx<<", vy: "<<vy<<", vz: "<<vz<<endl;
      uint16_t sign = SetRightVelocity(vx, vy, vz);

      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          DsdvHeader dsdvHeader;
          // 发送自己最新的消息
          if (i->second.GetHop () == 0)
            {
              RoutingTableEntry ownEntry;
               // 自己的IP地址

              //Add
              dsdvHeader.SetX((uint16_t)myPos.x);
              cout<<"dsdvHeader.GetX(): "<<dsdvHeader.GetX()<<endl;
              dsdvHeader.SetY((uint16_t)myPos.y);
              cout<<"dsdvHeader.GetY(): "<<dsdvHeader.GetY()<<endl;
              dsdvHeader.SetZ((uint16_t)myPos.z);
              dsdvHeader.SetVX(abs(vx));
              cout<<"dsdvHeader.GetVY(): "<<dsdvHeader.GetVX()<<endl;
              dsdvHeader.SetVY(abs(vy));
              cout<<"dsdvHeader.GetVY(): "<<dsdvHeader.GetVY()<<endl;
              dsdvHeader.SetVZ(abs(vz));
              dsdvHeader.SetSign(sign);
              dsdvHeader.SetTimestamp(Simulator::Now ().ToInteger(Time::S));
              
              dsdvHeader.SetDst (m_ipv4->GetAddress (1,0).GetLocal ());
              dsdvHeader.SetDstSeqno (i->second.GetSeqNo () + 2);
              dsdvHeader.SetHopCount (i->second.GetHop () + 1);
              m_routingTable.LookupRoute (m_ipv4->GetAddress (1,0).GetBroadcast (),ownEntry);
              ownEntry.SetSeqNo (dsdvHeader.GetDstSeqno ());
              //ADD:
              ownEntry.SetX(dsdvHeader.GetX());
              m_routingTable.Update (ownEntry);
              packet->AddHeader (dsdvHeader);
            }
          else
            {
              //Add
              dsdvHeader.SetX((uint16_t)myPos.x);
              dsdvHeader.SetY((uint16_t)myPos.y);
              dsdvHeader.SetZ((uint16_t)myPos.z);
              dsdvHeader.SetVX(abs(vx));
              dsdvHeader.SetVY(abs(vy));
              dsdvHeader.SetVZ(abs(vz));
              dsdvHeader.SetSign(sign);
              dsdvHeader.SetTimestamp(Simulator::Now ().ToInteger(Time::S));

              dsdvHeader.SetDst (i->second.GetDestination ());
              dsdvHeader.SetDstSeqno ((i->second.GetSeqNo ()));
              dsdvHeader.SetHopCount (i->second.GetHop () + 1);
              packet->AddHeader (dsdvHeader);
            }
          NS_LOG_DEBUG ("Forwarding the update for " << i->first);
          NS_LOG_DEBUG ("Forwarding details are, Destination: " << dsdvHeader.GetDst ()
                                                                << ", SeqNo:" << dsdvHeader.GetDstSeqno ()
                                                                << ", HopCount:" << dsdvHeader.GetHopCount ()
                                                                << ", LifeTime: " << i->second.GetLifeTime ().As (Time::S));
        }
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator rmItr = removedAddresses.begin (); rmItr
           != removedAddresses.end (); ++rmItr)
        {
          DsdvHeader removedHeader;
          //ADD:
          removedHeader.SetX(rmItr->second.GetX());
          removedHeader.SetY(rmItr->second.GetY());
          removedHeader.SetZ(rmItr->second.GetZ());
          removedHeader.SetVX(rmItr->second.GetVX());
          removedHeader.SetVY(rmItr->second.GetVY());
          removedHeader.SetVZ(rmItr->second.GetVZ());
          removedHeader.SetTimestamp(rmItr->second.GetTimestamp());

          removedHeader.SetDst (rmItr->second.GetDestination ());
          removedHeader.SetDstSeqno (rmItr->second.GetSeqNo () + 1);
          removedHeader.SetHopCount (rmItr->second.GetHop () + 1);
          packet->AddHeader (removedHeader);
          NS_LOG_DEBUG ("Update for removed record is: Destination: " << removedHeader.GetDst ()
                                                                      << " SeqNo:" << removedHeader.GetDstSeqno ()
                                                                      << " HopCount:" << removedHeader.GetHopCount ());
        }
      socket->Send (packet);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        {
          destination = iface.GetBroadcast ();
        }
        //广播这个数据包
      
      socket->SendTo (packet, 0, InetSocketAddress (destination, DSDV_PORT));
      NS_LOG_FUNCTION ("PeriodicUpdate Packet UID is : " << packet->GetUid ());
    }
  m_periodicUpdateTimer.Schedule (m_periodicUpdateInterval + MicroSeconds (25 * m_uniformRandomVariable->GetInteger (0,1000)));
}

void
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  DsdvHeader dsdvHeader;
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
  m_ipv4 = ipv4;
  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
  m_lo = m_ipv4->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt (
    dsdvHeader.GetX(), dsdvHeader.GetY(), dsdvHeader.GetZ(),
    dsdvHeader.GetVX(), dsdvHeader.GetVY(), dsdvHeader.GetVZ(), dsdvHeader.GetTimestamp(),
    /*device=*/ m_lo,  /*dst=*/
    Ipv4Address::GetLoopback (), /*seqno=*/
    0,
    /*iface=*/ Ipv4InterfaceAddress (Ipv4Address::GetLoopback (),Ipv4Mask ("255.0.0.0")),
    /*hops=*/ 0,  /*next hop=*/
    Ipv4Address::GetLoopback (),
    /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  rt.SetFlag (INVALID);
  rt.SetEntriesChanged (false);
  m_routingTable.AddRoute (rt);
  Simulator::ScheduleNow (&RoutingProtocol::Start,this);
}

// 通知接口up
void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  DsdvHeader dsdvHeader;
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ()
                        << " interface is up");
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
  if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
      return;
    }
  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvDsdv,this));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DSDV_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetAttribute ("IpTtl",UintegerValue (1));
  m_socketAddresses.insert (std::make_pair (socket,iface));
  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
  RoutingTableEntry rt (dsdvHeader.GetX(), dsdvHeader.GetY(), dsdvHeader.GetZ(),
    dsdvHeader.GetVX(), dsdvHeader.GetVY(), dsdvHeader.GetVZ(), dsdvHeader.GetTimestamp(),
    /*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*seqno=*/ 0,/*iface=*/ iface,/*hops=*/ 0,
                                    /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);
  if (m_mainAddress == Ipv4Address ())
    {
      m_mainAddress = iface.GetLocal ();
    }
  NS_ASSERT (m_mainAddress != Ipv4Address ());
}

// 通知接口down
void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i,0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No dsdv interfaces");
      m_routingTable.Clear ();
      return;
    }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i,0));
  m_advRoutingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i,0));
}

//通知添加address
void
RoutingProtocol::NotifyAddAddress (uint32_t i,
                                   Ipv4InterfaceAddress address)
{
  DsdvHeader dsdvHeader;
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (!l3->IsUp (i))
    {
      return;
    }
  Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
  if (!socket)
    {
      if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
        {
          return;
        }
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);
      socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvDsdv,this));
      // Bind to any IP address so that broadcasts can be received
      socket->BindToNetDevice (l3->GetNetDevice (i));
      socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DSDV_PORT));
      socket->SetAllowBroadcast (true);
      m_socketAddresses.insert (std::make_pair (socket,iface));
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
      RoutingTableEntry rt (dsdvHeader.GetX(), dsdvHeader.GetY(), dsdvHeader.GetZ(),
        dsdvHeader.GetVX(), dsdvHeader.GetVY(), dsdvHeader.GetVZ(), dsdvHeader.GetTimestamp(),
        /*device=*/ dev, /*dst=*/ iface.GetBroadcast (),/*seqno=*/ 0, /*iface=*/ iface,/*hops=*/ 0,
                                        /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      m_routingTable.AddRoute (rt);
    }
}

//通知移除address
void
RoutingProtocol::NotifyRemoveAddress (uint32_t i,
                                      Ipv4InterfaceAddress address)
{
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      m_socketAddresses.erase (socket);
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvDsdv,this));
          // Bind to any IP address so that broadcasts can be received
          socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), DSDV_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket,iface));
        }
    }
}

  /**
   * Find socket with local interface address iface
   * \param iface the interface
   * \returns the socket
   */
Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr) const
{
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

void
RoutingProtocol::Send (Ptr<Ipv4Route> route,
                       Ptr<const Packet> packet,
                       const Ipv4Header & header)
{
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  NS_ASSERT (l3 != 0);
  Ptr<Packet> p = packet->Copy ();
  l3->Send (p,route->GetSource (),header.GetDestination (),header.GetProtocol (),route);
}

void
RoutingProtocol::Drop (Ptr<const Packet> packet,
                       const Ipv4Header & header,
                       Socket::SocketErrno err)
{
  NS_LOG_DEBUG (m_mainAddress << " drop packet " << packet->GetUid () << " to "
                              << header.GetDestination () << " from queue. Error " << err);
}

// 寻找所有排队的数据包并将它们发送出去
void
RoutingProtocol::LookForQueuedPackets ()  
{
  cout << "Now is look for queue packets......" << endl;
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4Route> route;
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_routingTable.GetListOfAllRoutes (allRoutes);

  //ADD:
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i){
    RoutingTableEntry rt;
    rt = i->second;
    cout << "Route: " << rt.GetRoute() << endl;
    cout << "X: " << rt.GetX() << endl;
      cout << "Y: " << rt.GetY() << endl;
      cout << "Z: " << rt.GetX() << endl;
      cout << "VX: " << rt.GetVX() << endl;
      cout << "VY: " << rt.GetVY() << endl;
      cout << "VZ: " << rt.GetVZ() << endl;
      cout << "timestamp: " << rt.GetTimestamp() << endl;
      cout << "Destination: " << rt.GetDestination() << endl;
      cout << "Nexthop: " << rt.GetNextHop() << endl;
      cout << "Interface: " << rt.GetInterface() << endl;
      cout << "HopCount: " << rt.GetHop() << endl;
      cout << "OutputDevice: " << rt.GetOutputDevice() << endl;
      cout << "SeqNum: " << rt.GetSeqNo() << endl;
  }



  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
    {
      RoutingTableEntry rt;
      rt = i->second;
      if (m_queue.Find (rt.GetDestination ()))
        {
          //ADD
      cout << endl;
      cout << "X: " << rt.GetX() << endl;
      cout << "Y: " << rt.GetY() << endl;
      cout << "Z: " << rt.GetX() << endl;
      cout << "VX: " << rt.GetVX() << endl;
      cout << "VY: " << rt.GetVY() << endl;
      cout << "VZ: " << rt.GetVZ() << endl;
      cout << "timestamp: " << rt.GetTimestamp() << endl;
      cout << "Route: " << rt.GetRoute() << endl;
      cout << "Destination: " << rt.GetDestination() << endl;
      cout << "Nexthop: " << rt.GetNextHop() << endl;
      cout << "Interface: " << rt.GetInterface() << endl;
      cout << "HopCount: " << rt.GetHop() << endl;
      cout << "OutputDevice: " << rt.GetOutputDevice() << endl;
      cout << "SeqNum: " << rt.GetSeqNo() << endl;
      cout<<"the old route->GetOutputDevice: "<<rt.GetRoute()->GetOutputDevice ()<<endl<<endl;
          if (rt.GetHop () == 1)
            {
              route = rt.GetRoute ();
              NS_LOG_LOGIC ("A route exists from " << route->GetSource ()
                                                   << " to neighboring destination "
                                                   << route->GetDestination ());
              cout << "A route exists from " << route->GetSource ()
                                                   << " to neighboring destination "
                                                   << route->GetDestination () 
                                                   << endl;
              NS_ASSERT (route != 0);
            }
          else
            {
              RoutingTableEntry newrt;
              m_routingTable.LookupRoute (rt.GetNextHop (),newrt);
              route = newrt.GetRoute ();
              cout << "the newrt: " << route << endl;
              cout<<"the new route->GetOutputDevice: "<<route->GetOutputDevice ()<<endl;

              //ADD
      cout << endl;
      cout << "X: " << newrt.GetX() << endl;
      cout << "Y: " << newrt.GetY() << endl;
      cout << "Z: " << newrt.GetX() << endl;
      cout << "VX: " << newrt.GetVX() << endl;
      cout << "VY: " << newrt.GetVY() << endl;
      cout << "VZ: " << newrt.GetVZ() << endl;
      cout << "timestamp: " << newrt.GetTimestamp() << endl;
      cout << "Route: " << newrt.GetRoute() << endl;
      cout << "Destination: " << newrt.GetDestination() << endl;
      cout << "Nexthop: " << newrt.GetNextHop() << endl;
      cout << "Interface: " << newrt.GetInterface() << endl;
      cout << "HopCount: " << newrt.GetHop() << endl;
      cout << "OutputDevice: " << newrt.GetOutputDevice() << endl;
      cout << "SeqNum: " << newrt.GetSeqNo() << endl;


              NS_LOG_LOGIC ("A route exists from " << route->GetSource ()
                                                   << " to destination " << route->GetDestination () << " via "
                                                   << rt.GetNextHop ());
              cout << "A route exists from " << route->GetSource ()
                                                   << " to destination " << route->GetDestination () << " via "
                                                   << rt.GetNextHop () 
                                                   << endl;
              NS_ASSERT (route != 0);
            }
          SendPacketFromQueue (rt.GetDestination (),route);
        }
    }
}

 /**
   * Send packet from queue
   * \param dst - destination address to which we are sending the packet to
   * \param route - route identified for this packet
   */
void
RoutingProtocol::SendPacketFromQueue (Ipv4Address dst,
                                      Ptr<Ipv4Route> route)
{
  cout << "Now is Send Packet from Queue......" << endl;


  NS_LOG_DEBUG (m_mainAddress << " is sending a queued packet to destination " << dst);
  QueueEntry queueEntry;
  if (m_queue.Dequeue (dst,queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag))
        {
          if (tag.oif != -1 && tag.oif != m_ipv4->GetInterfaceForDevice (route->GetOutputDevice ()))
            {
              NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
              return;
            }
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv4Header header = queueEntry.GetIpv4Header ();
      header.SetSource (route->GetSource ());
      header.SetTtl (header.GetTtl () + 1); // compensate extra TTL decrement by fake loopback routing
      ucb (route,p,header);
      
      if (m_queue.GetSize () != 0 && m_queue.Find (dst))
        {
          Simulator::Schedule (MilliSeconds (m_uniformRandomVariable->GetInteger (0,100)),
                               &RoutingProtocol::SendPacketFromQueue,this,dst,route);
        }
    }
}

  /**
   * Get settlingTime for a destination
   * \param dst - destination address
   * \return settlingTime for the destination if found
   */
Time
RoutingProtocol::GetSettlingTime (Ipv4Address address)
{
  NS_LOG_FUNCTION ("Calculating the settling time for " << address);
  RoutingTableEntry mainrt;
  Time weightedTime;
  m_routingTable.LookupRoute (address,mainrt);
  if (EnableWST)
    {
      if (mainrt.GetSettlingTime () == Seconds (0))
        {
          return Seconds (0);
        }
      else
        {
          NS_LOG_DEBUG ("Route SettlingTime: " << mainrt.GetSettlingTime ().As (Time::S)
                                               << " and LifeTime:" << mainrt.GetLifeTime ().As (Time::S));
          weightedTime = m_weightedFactor * mainrt.GetSettlingTime ()  +
            (1.0 - m_weightedFactor) * mainrt.GetLifeTime ();
          NS_LOG_DEBUG ("Calculated weightedTime:" << weightedTime.As (Time::S));
          return weightedTime;
        }
    }
  return mainrt.GetSettlingTime ();
}

//合并触发定期更新
void
RoutingProtocol::MergeTriggerPeriodicUpdates ()
{
  NS_LOG_FUNCTION ("Merging advertised table changes with main table before sending out periodic update");
  std::map<Ipv4Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllRoutes (allRoutes);
  if (allRoutes.size () > 0)
    {
      for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          RoutingTableEntry advEntry = i->second;
          if ((advEntry.GetEntriesChanged () == true) && (!m_advRoutingTable.AnyRunningEvent (advEntry.GetDestination ())))
            {
              if (!(advEntry.GetSeqNo () % 2))
                {
                  advEntry.SetFlag (VALID);
                  advEntry.SetEntriesChanged (false);
                  m_routingTable.Update (advEntry);
                  NS_LOG_DEBUG ("Merged update for " << advEntry.GetDestination () << " with main routing Table");
                }
              m_advRoutingTable.DeleteRoute (advEntry.GetDestination ());
            }
          else
            {
              NS_LOG_DEBUG ("Event currently running. Cannot Merge Routing Tables");
            }
        }
    }
}
}
}
