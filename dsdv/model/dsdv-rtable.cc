/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hemanth Narra
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
#include "dsdv-rtable.h"
#include "ns3/simulator.h"
#include <iomanip>
#include "ns3/log.h"
using namespace std;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DsdvRoutingTable");

namespace dsdv {
RoutingTableEntry::RoutingTableEntry (uint16_t x,
                                      uint16_t y,
                                      uint16_t z,
                                      int16_t vx,
                                      int16_t vy,
                                      int16_t vz,
                                      uint16_t timestamp,
                                      Ptr<NetDevice> dev,
                                      Ipv4Address dst,
                                      uint32_t seqNo,
                                      Ipv4InterfaceAddress iface,
                                      uint32_t hops,
                                      Ipv4Address nextHop,
                                      Time lifetime,
                                      Time SettlingTime,
                                      bool areChanged)
  : m_x(x),
    m_y(y),
    m_z(z),
    m_vx(vx),
    m_vy(vy),
    m_vz(vz),
    m_timestamp(timestamp),
    m_seqNo (seqNo),
    m_hops (hops),
    m_lifeTime (lifetime),
    m_iface (iface),
    m_flag (VALID),
    m_settlingTime (SettlingTime),
    m_entriesChanged (areChanged)
{
  m_ipv4Route = Create<Ipv4Route> ();
  m_ipv4Route->SetDestination (dst);
  m_ipv4Route->SetGateway (nextHop);
  m_ipv4Route->SetSource (m_iface.GetLocal ());
  m_ipv4Route->SetOutputDevice (dev);
}
RoutingTableEntry::~RoutingTableEntry ()
{
}
RoutingTable::RoutingTable ()
{
}

bool
RoutingTable::LookupRoute (Ipv4Address id,
                           RoutingTableEntry & rt)
{
  if (m_ipv4AddressEntry.empty ())
    {
      cout << "路由表是空的" << endl;
      return false;
    }
  cout << "查找目的地为: " << id << "的路由条目" << endl;
  std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find (id);
  if (i == m_ipv4AddressEntry.end ())
    {
      cout << "Entry.find 没找到啊" << endl;
      return false;
    }
  cout << "Entry.find 找到了" << endl;
  rt = i->second;
  return true;
}

bool
RoutingTable::LookupRoute (Ipv4Address id,
                           RoutingTableEntry & rt,
                           bool forRouteInput)
{
  if (m_ipv4AddressEntry.empty ())
    {
      return false;
    }
  std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find (id);
  if (i == m_ipv4AddressEntry.end ())
    {
      return false;
    }
  if (forRouteInput == true && id == i->second.GetInterface ().GetBroadcast ())
    {
      return false;
    }
  rt = i->second;
  return true;
}

bool
RoutingTable::DeleteRoute (Ipv4Address dst)
{
  if (m_ipv4AddressEntry.erase (dst) != 0)
    {
      // NS_LOG_DEBUG("Route erased");
      return true;
    }
  return false;
}

uint32_t
RoutingTable::RoutingTableSize ()
{
  return m_ipv4AddressEntry.size ();
}

// 1.1  1.1   +1 +1
RoutingTableEntry
RoutingTable::fun1(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart + 1;
  uint32_t dst2_third = dst_thirdPart + 1;
  uint32_t nexthop2_second = nexthop_secondPart + 1;
  uint32_t nexthop2_third = nexthop_thirdPart + 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
    
  // cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 1.1  1.1   +1 不变
RoutingTableEntry
RoutingTable::fun2(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart + 1;
  uint32_t dst2_third = dst_thirdPart + 1;
  uint32_t nexthop2_second = nexthop_secondPart + 1;
  uint32_t nexthop2_third = nexthop_thirdPart + 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);

  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/nexthop, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 1.1  1.1   不变 +1
RoutingTableEntry
RoutingTable::fun3(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart + 1;
  uint32_t dst2_third = dst_thirdPart + 1;
  uint32_t nexthop2_second = nexthop_secondPart + 1;
  uint32_t nexthop2_third = nexthop_thirdPart + 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/dst, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 1.1  2.2   不变 -1
RoutingTableEntry
RoutingTable::fun4(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart + 1;
  uint32_t dst2_third = dst_thirdPart + 1;
  uint32_t nexthop2_second = nexthop_secondPart - 1;
  uint32_t nexthop2_third = nexthop_thirdPart - 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/dst, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 1.1  2.2   +1 -1
RoutingTableEntry
RoutingTable::fun5(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart + 1;
  uint32_t dst2_third = dst_thirdPart + 1;
  uint32_t nexthop2_second = nexthop_secondPart - 1;
  uint32_t nexthop2_third = nexthop_thirdPart - 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 1.1  2.2   +1 不变
RoutingTableEntry
RoutingTable::fun6(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart + 1;
  uint32_t dst2_third = dst_thirdPart + 1;
  uint32_t nexthop2_second = nexthop_secondPart - 1;
  uint32_t nexthop2_third = nexthop_thirdPart - 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);

  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/nexthop, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 2.2  1.1   不变 +1
RoutingTableEntry
RoutingTable::fun7(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart - 1;
  uint32_t dst2_third = dst_thirdPart - 1;
  uint32_t nexthop2_second = nexthop_secondPart + 1;
  uint32_t nexthop2_third = nexthop_thirdPart + 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/dst, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);

  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 2.2  1.1   -1 不变
RoutingTableEntry
RoutingTable::fun8(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart - 1;
  uint32_t dst2_third = dst_thirdPart - 1;
  uint32_t nexthop2_second = nexthop_secondPart + 1;
  uint32_t nexthop2_third = nexthop_thirdPart + 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/nexthop, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 2.2  1.1   -1 +1
RoutingTableEntry
RoutingTable::fun9(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart - 1;
  uint32_t dst2_third = dst_thirdPart - 1;
  uint32_t nexthop2_second = nexthop_secondPart + 1;
  uint32_t nexthop2_third = nexthop_thirdPart + 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 2.2  2.2   -1 -1
RoutingTableEntry
RoutingTable::fun10(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart - 1;
  uint32_t dst2_third = dst_thirdPart - 1;
  uint32_t nexthop2_second = nexthop_secondPart - 1;
  uint32_t nexthop2_third = nexthop_thirdPart - 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);

  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 2.2  2.2   -1 不变
RoutingTableEntry
RoutingTable::fun11(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart - 1;
  uint32_t dst2_third = dst_thirdPart - 1;
  uint32_t nexthop2_second = nexthop_secondPart - 1;
  uint32_t nexthop2_third = nexthop_thirdPart - 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/newDstAddress2, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/nexthop, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}

// 2.2  2.2   不变 -1
RoutingTableEntry
RoutingTable::fun12(RoutingTableEntry & rt)
{ 
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  //把传进来的destination和nexthop的第二、三字段加1
  uint32_t dst2_second = dst_secondPart - 1;
  uint32_t dst2_third = dst_thirdPart - 1;
  uint32_t nexthop2_second = nexthop_secondPart - 1;
  uint32_t nexthop2_third = nexthop_thirdPart - 1;
  
  //拼成新的地址
  uint32_t newDstAsInteger2 = (dstAsInteger & 0xFF0000FF) | ((dst2_second & 0xFF) << 8) | ((dst2_third & 0xFF) << 16);
  Ipv4Address newDstAddress2(newDstAsInteger2);
  uint32_t newNexthopAsInteger2 = (nexthopAsInteger & 0xFF0000FF) | ((nexthop2_second & 0xFF) << 8) | ((nexthop2_third & 0xFF) << 16);
  Ipv4Address newNexthopAddress2(newNexthopAsInteger2);


  RoutingTableEntry newEntry (
    //Add:
    rt.GetX(),
    rt.GetY(),
    rt.GetZ(),
    rt.GetVX(),
    rt.GetVY(),
    rt.GetVZ(),
    rt.GetTimestamp(),
    /*device=*/ rt.GetOutputDevice(), 
    /*dst=*/dst, 
    /*seqno=*/rt.GetSeqNo(),
    /*iface=*/ rt.GetInterface(),  //存储是从哪个接口接收到的包
    /*hops=*/ rt.GetHop (), 
    /*next hop=*/newNexthopAddress2, 
    /*lifetime=*/Simulator::Now (), 
    /*settlingTime*/rt.GetSettlingTime(), 
    /*entries changed*/true);
    newEntry.SetFlag (VALID);
  //   cout << "dst: " << dst << ", next: " << nexthop << 
  // ", newDst: " << newEntry.GetDestination() << ", newNext: " << newEntry.GetNextHop() << endl;
    return newEntry;
}


bool
RoutingTable::AddRoute (RoutingTableEntry & rt)
{
  std::pair<std::map<Ipv4Address, RoutingTableEntry>::iterator, bool> result = m_ipv4AddressEntry.insert (std::make_pair (rt.GetDestination (),rt));

  //ADD:根据destination和nexthop来判断，给每一条路由条目，再增加另外三条
  Ipv4Address dst = rt.GetDestination();
  Ipv4Address nexthop = rt.GetNextHop();
  //ADD:
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = dst.Get();
  uint32_t nexthopAsInteger = nexthop.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t dst_secondPart = (dstAsInteger >> 8) & 0xFF;
  uint32_t nexthop_secondPart = (nexthopAsInteger >> 8) & 0xFF;
  // // 获取IPv4地址的第三部分（子网标识）
  // uint32_t dst_thirdPart = (dstAsInteger >> 16) & 0xFF;
  // uint32_t nexthop_thirdPart = (nexthopAsInteger >> 16) & 0xFF;

  if(dst_secondPart == 1 && nexthop_secondPart ==1){
    RoutingTableEntry rt1 = fun1(rt);
    RoutingTableEntry rt2 = fun2(rt);
    RoutingTableEntry rt3 = fun3(rt);
    m_ipv4AddressEntry.insert (std::make_pair (rt1.GetDestination (),rt1));
    m_ipv4AddressEntry.insert (std::make_pair (rt2.GetDestination (),rt2));
    m_ipv4AddressEntry.insert (std::make_pair (rt3.GetDestination (),rt3));
    // if(result1.second == true) cout << "rt1 添加成功" << endl;
    // else cout << "rt1 添加失败" << endl;
    // if(result2.second == true) cout << "rt2 添加成功" << endl;
    // else cout << "rt2 添加失败" << endl;
    // if(result3.second == true) cout << "rt3 添加成功" << endl;
    // else cout << "rt3 添加失败" << endl;  

  } 
  if(dst_secondPart == 1 && nexthop_secondPart ==2){
    RoutingTableEntry rt1 = fun4(rt);
    RoutingTableEntry rt2 = fun5(rt);
    RoutingTableEntry rt3 = fun6(rt);
    m_ipv4AddressEntry.insert (std::make_pair (rt1.GetDestination (),rt1));
    m_ipv4AddressEntry.insert (std::make_pair (rt2.GetDestination (),rt2));
    m_ipv4AddressEntry.insert (std::make_pair (rt3.GetDestination (),rt3));
    // if(result1.second == true) cout << "rt1 添加成功" << endl;
    // else cout << "rt1 添加失败" << endl;
    // if(result2.second == true) cout << "rt2 添加成功" << endl;
    // else cout << "rt2 添加失败" << endl;
    // if(result3.second == true) cout << "rt3 添加成功" << endl;
    // else cout << "rt3 添加失败" << endl;
  }
  if(dst_secondPart == 2 && nexthop_secondPart ==1){
    RoutingTableEntry rt1 = fun7(rt);
    RoutingTableEntry rt2 = fun8(rt);
    RoutingTableEntry rt3 = fun9(rt);
    m_ipv4AddressEntry.insert (std::make_pair (rt1.GetDestination (),rt1));
    m_ipv4AddressEntry.insert (std::make_pair (rt2.GetDestination (),rt2));
    m_ipv4AddressEntry.insert (std::make_pair (rt3.GetDestination (),rt3));
    // if(result1.second == true) cout << "rt1 添加成功" << endl;
    // else cout << "rt1 添加失败" << endl;
    // if(result2.second == true) cout << "rt2 添加成功" << endl;
    // else cout << "rt2 添加失败" << endl;
    // if(result3.second == true) cout << "rt3 添加成功" << endl;
    // else cout << "rt3 添加失败" << endl;  
  }
  if(dst_secondPart == 2 && nexthop_secondPart ==2){
    RoutingTableEntry rt1 = fun10(rt);
    RoutingTableEntry rt2 = fun11(rt);
    RoutingTableEntry rt3 = fun12(rt);
    m_ipv4AddressEntry.insert (std::make_pair (rt1.GetDestination (),rt1));
    m_ipv4AddressEntry.insert (std::make_pair (rt2.GetDestination (),rt2));
    m_ipv4AddressEntry.insert (std::make_pair (rt3.GetDestination (),rt3));
    // if(result1.second == true) cout << "rt1 添加成功" << endl;
    // else cout << "rt1 添加失败" << endl;
    // if(result2.second == true) cout << "rt2 添加成功" << endl;
    // else cout << "rt2 添加失败" << endl;
    // if(result3.second == true) cout << "rt3 添加成功" << endl;
    // else cout << "rt3 添加失败" << endl;  
  }
  
  
  return result.second;
}

bool
RoutingTable::Update (RoutingTableEntry & rt)
{
  std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.find (rt.GetDestination ());
  if (i == m_ipv4AddressEntry.end ())
    {
      return false;
    }
  i->second = rt;
  return true;
}

void
RoutingTable::DeleteAllRoutesFromInterface (Ipv4InterfaceAddress iface)
{
  if (m_ipv4AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); )
    {
      if (i->second.GetInterface () == iface)
        {
          std::map<Ipv4Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          m_ipv4AddressEntry.erase (tmp);
        }
      else
        {
          ++i;
        }
    }
}

void
RoutingTable::GetListOfAllRoutes (std::map<Ipv4Address, RoutingTableEntry> & allRoutes)
{
  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
    {
      if (i->second.GetDestination () != Ipv4Address ("127.0.0.1") && i->second.GetFlag () == VALID)
        {
          allRoutes.insert (
            std::make_pair (i->first,i->second));
        }
    }
}

void
RoutingTable::GetListOfDestinationWithNextHop (Ipv4Address nextHop,
                                               std::map<Ipv4Address, RoutingTableEntry> & unreachable)
{
  unreachable.clear ();
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.begin (); i
       != m_ipv4AddressEntry.end (); ++i)
    {
      if (i->second.GetNextHop () == nextHop)
        {
          unreachable.insert (std::make_pair (i->first,i->second));
        }
    }
}

void
RoutingTableEntry::Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit /*= Time::S*/) const
{
  std::ostream* os = stream->GetStream ();
  // Copy the current ostream state
  std::ios oldState (nullptr);
  oldState.copyfmt (*os);

  *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);


  std::ostringstream x, y, z, vx, vy, vz, timestamp, dest, gw, iface, ltime, stime;

  //ADD:

  dest << m_ipv4Route->GetDestination ();
  gw << m_ipv4Route->GetGateway ();
  iface << m_iface.GetLocal ();
  ltime << std::setprecision (3) << (Simulator::Now () - m_lifeTime).As (unit);
  stime << m_settlingTime.As (unit);

  *os << std::setw (16) << m_x;
  *os << std::setw (16) << m_y;
  *os << std::setw (16) << m_z;
  *os << std::setw (16) << m_vx;
  *os << std::setw (16) << m_vy;
  *os << std::setw (16) << m_vz;
  *os << std::setw (16) << m_timestamp;

  *os << std::setw (16) << dest.str ();
  *os << std::setw (16) << gw.str ();
  *os << std::setw (16) << iface.str ();
  *os << std::setw (16) << m_hops;
  *os << std::setw (16) << m_seqNo;
  *os << std::setw(16) << ltime.str ();
  *os << stime.str () << std::endl;
  // Restore the previous ostream state
  (*os).copyfmt (oldState);
}

void
RoutingTable::Purge (std::map<Ipv4Address, RoutingTableEntry> & removedAddresses)
{
  if (m_ipv4AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); )
    {
      std::map<Ipv4Address, RoutingTableEntry>::iterator itmp = i;
      if (i->second.GetLifeTime () > m_holddownTime && (i->second.GetHop () > 0))
        {
          for (std::map<Ipv4Address, RoutingTableEntry>::iterator j = m_ipv4AddressEntry.begin (); j != m_ipv4AddressEntry.end (); )
            {
              if ((j->second.GetNextHop () == i->second.GetDestination ()) && (i->second.GetHop () != j->second.GetHop ()))
                {
                  std::map<Ipv4Address, RoutingTableEntry>::iterator jtmp = j;
                  removedAddresses.insert (std::make_pair (j->first,j->second));
                  ++j;
                  m_ipv4AddressEntry.erase (jtmp);
                }
              else
                {
                  ++j;
                }
            }
          removedAddresses.insert (std::make_pair (i->first,i->second));
          ++i;
          m_ipv4AddressEntry.erase (itmp);
        }
      /** \todo Need to decide when to invalidate a route */
      /*          else if (i->second.GetLifeTime() > m_holddownTime)
       {
       ++i;
       itmp->second.SetFlag(INVALID);
       }*/
      else
        {
          ++i;
        }
    }
  return;
}

void
RoutingTable::Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit /*= Time::S*/) const
{
  std::ostream* os = stream->GetStream ();
  // Copy the current ostream state
  std::ios oldState (nullptr);
  oldState.copyfmt (*os);

  *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);

  *os << "\nDSDV Routing table\n";
  //ADD
  *os << std::setw (16) << "x";
  *os << std::setw (16) << "y";
  *os << std::setw (16) << "z";
  *os << std::setw (16) << "vx";
  *os << std::setw (16) << "vy";
  *os << std::setw (16) << "vz";
  *os << std::setw (16) << "timestamp";
  *os << std::setw (16) << "Destination";
  *os << std::setw (16) << "Gateway";
  *os << std::setw (16) << "Interface";
  *os << std::setw (16) << "HopCount";
  *os << std::setw (16) << "SeqNum";
  *os << std::setw (16) << "LifeTime";
  *os << "SettlingTime" << std::endl;
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.begin (); i
       != m_ipv4AddressEntry.end (); ++i)
    {
      i->second.Print (stream, unit);
    }
  *os << std::endl;
  // Restore the previous ostream state
  (*os).copyfmt (oldState);
}

bool
RoutingTable::AddIpv4Event (Ipv4Address address,
                            EventId id)
{
  std::pair<std::map<Ipv4Address, EventId>::iterator, bool> result = m_ipv4Events.insert (std::make_pair (address,id));
  return result.second;
}

bool
RoutingTable::AnyRunningEvent (Ipv4Address address)
{
  EventId event;
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find (address);
  if (m_ipv4Events.empty ())
    {
      return false;
    }
  if (i == m_ipv4Events.end ())
    {
      return false;
    }
  event = i->second;
  if (event.IsRunning ())
    {
      return true;
    }
  else
    {
      return false;
    }
}

bool
RoutingTable::ForceDeleteIpv4Event (Ipv4Address address)
{
  EventId event;
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find (address);
  if (m_ipv4Events.empty () || i == m_ipv4Events.end ())
    {
      return false;
    }
  event = i->second;
  Simulator::Cancel (event);
  m_ipv4Events.erase (address);
  return true;
}

bool
RoutingTable::DeleteIpv4Event (Ipv4Address address)
{
  EventId event;
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find (address);
  if (m_ipv4Events.empty () || i == m_ipv4Events.end ())
    {
      return false;
    }
  event = i->second;
  if (event.IsRunning ())
    {
      return false;
    }
  if (event.IsExpired ())
    {
      event.Cancel ();
      m_ipv4Events.erase (address);
      return true;
    }
  else
    {
      m_ipv4Events.erase (address);
      return true;
    }
}

EventId
RoutingTable::GetEventId (Ipv4Address address)
{
  std::map <Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find (address);
  if (m_ipv4Events.empty () || i == m_ipv4Events.end ())
    {
      return EventId ();
    }
  else
    {
      return i->second;
    }
}
}
}
