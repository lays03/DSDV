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

#ifndef DSDV_ROUTING_PROTOCOL_H
#define DSDV_ROUTING_PROTOCOL_H

#include "dsdv-rtable.h"
#include "dsdv-packet-queue.h"
#include "dsdv-packet.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/output-stream-wrapper.h"

#include "ns3/mobility-model.h"

namespace ns3 {
namespace dsdv {

/**
 * \ingroup dsdv
 * \brief DSDV routing protocol.
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const uint32_t DSDV_PORT;

  /// c-tor
  RoutingProtocol ();
  virtual
  ~RoutingProtocol ();
  virtual void
  DoDispose ();

// //ADD: 新增一个函数，函数功能是：当查找的下一跳为10.1.1.x时，如果找不到，可以再去尝试查找下一跳为10.2.2.x的路由条目
// //反之，当查找的下一跳为10.2.2.x时，如果找不到，可以再去尝试查找下一跳为10.1.1.x的路由条目
//ADD: 新增一个函数，函数功能是：将Ip地址在10.1.1.x和10.2.2.x之间转换
Ipv4Address Trans(Ipv4Address ip){
  // 将IPv4地址转换为整数
  uint32_t dstAsInteger = ip.Get();
  // 获取IPv4地址的第二部分（子网标识）
  uint32_t ip_secondPart = (dstAsInteger >> 8) & 0xFF;
  // 获取IPv4地址的第三部分（子网标识）
  uint32_t ip_thirdPart = (dstAsInteger >> 16) & 0xFF;

  //把传进来的ip的第二、三字段加1或者减一
  if(ip_secondPart == 1){
    ip_secondPart += 1;
    ip_thirdPart += 1;
  }
  else{
    ip_secondPart -= 1;
    ip_thirdPart -= 1;
  }
  
  //拼成新的地址
  uint32_t newipAsInteger = (dstAsInteger & 0xFF0000FF) | ((ip_secondPart & 0xFF) << 8) | ((ip_thirdPart & 0xFF) << 16);
  Ipv4Address newip(newipAsInteger);
  return newip;
}

  //ADD: 添加函数，从数据包中获得正确的带方向的速度，用于更新路由表
  //ADD: 转换速度符号的两个函数，sign用来记录速度是否为负数，
  //0：都不是负数   1：x轴为负    2：y轴为负    3：z轴为负
  //4：xy为负数   5：xz为负数   6：yz为负   7：全都是负数

  Vector GetRightVelocity(uint16_t vx, uint16_t vy, uint16_t vz, uint16_t sign){
    if(sign == 0){ return Vector(vx, vy, vz);}
    else if(sign == 1){ return Vector(-vx, vy, vz);}
    else if(sign == 2){ return Vector(vx, -vy, vz);}
    else if(sign == 3){ return Vector(vx, vy, -vz);}
    else if(sign == 4){ return Vector(-vx, -vy, vz);}
    else if(sign == 5){ return Vector(-vx, vy, -vz);}
    else if(sign == 6){ return Vector(vx, -vy, -vz);}
    else if(sign == 7){ return Vector(-vx, -vy, -vz);}
    else{
      std::cout<<"sign is wrong!!!\n";
      return Vector(999, 999, 999);
    }

  }

  //ADD: 添加函数，获得速度对应的符号，这样就可以把路由表中的速度绝对值放入数据包中
  uint16_t SetRightVelocity(int16_t vx, int16_t vy, int16_t vz){
    uint16_t sign = 9;
    if(vx >= 0 && vy >= 0 && vz >= 0) sign = 0;
    if(vx < 0 && vy >= 0 && vz >= 0) sign = 1;
    if(vx >= 0 && vy < 0 && vz >= 0) sign = 2;
    if(vx >= 0 && vy >= 0 && vz < 0) sign = 3;
    if(vx < 0 && vy < 0 && vz >= 0) sign = 4;
    if(vx < 0 && vy >= 0 && vz < 0) sign = 5;
    if(vx >= 0 && vy < 0 && vz < 0) sign = 6;
    if(vx < 0 && vy < 0 && vz < 0 ) sign =7;
    return sign; 
  }

  // From Ipv4RoutingProtocol
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  /**
   * Route input packet
   * \param p The packet
   * \param header The IPv4 header
   * \param idev The device
   * \param ucb The unicast forward callback
   * \param mcb The multicast forward callback
   * \param lcb The local deliver callback
   * \param ecb The error callback
   * \returns true if successful
   */
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                   MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);

  // Methods to handle protocol parameters
  /**
   * Set enable buffer flag 设置启用缓冲区标志
   * \param f The enable buffer flag
   */
  void SetEnableBufferFlag (bool f);
  /**
   * Get enable buffer flag
   * \returns the enable buffer flag
   */
  bool GetEnableBufferFlag () const;
  /**
   * Set weighted settling time (WST) flag 设置加权稳定时间 (WST) 标志
   * \param f the weighted settling time (WST) flag
   */
  void SetWSTFlag (bool f);
  /**
   * Get weighted settling time (WST) flag
   * \returns the weighted settling time (WST) flag
   */
  bool GetWSTFlag () const;
  /**
   * Set enable route aggregation (RA) flag 设置启用路由聚合 (RA) 标志
   * \param f the enable route aggregation (RA) flag
   */
  void SetEnableRAFlag (bool f);
  /**
   * Get enable route aggregation (RA) flag
   * \returns the enable route aggregation (RA) flag
   */
  bool GetEnableRAFlag () const;

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   * 为模型使用的随机变量分配一个固定的随机变量流号。返回已分配的流的数目(可能为0)。
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

private:
  // Protocol parameters.
  /// Holdtimes is the multiplicative factor of PeriodicUpdateInterval for which the node waits since the last update
  /// before flushing a route from the routing table. If PeriodicUpdateInterval is 8s and Holdtimes is 3, the node
  /// waits for 24s since the last update to flush this route from its routing table.
  // 协议参数。
  // Holdtimes是PeriodicUpdateInterval的乘积因子，该节点从刷新路由表之前的最后一次更新开始等待。
  // 如果PeriodicUpdateInterval的值为8s, Holdtimes的值为3，则从上次更新路由开始，
  // 节点需要等待24秒才能将该路由从路由表中清除。
  uint32_t Holdtimes;

  /// PeriodicUpdateInterval specifies the periodic time interval between which the a node broadcasts
  /// its entire routing table.
  // “定时更新时间间隔”指定节点广播整个路由表的周期时间间隔。
  Time m_periodicUpdateInterval;

  /// SettlingTime specifies the time for which a node waits before propagating an update.
  /// It waits for this time interval in hope of receiving an update with a better metric.
  Time m_settlingTime;

  /// Nodes IP address
  Ipv4Address m_mainAddress;

  /// IP protocol
  Ptr<Ipv4> m_ipv4;

  /// Raw socket per each IP interface, map socket -> iface address (IP + mask)
  // 每个IP接口的原始套接字，映射套接字->接口地址(IP +掩码)
  std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketAddresses;

  /// Loopback device used to defer route requests until a route is found
  // 环回设备，用于延迟路由请求，直到找到路由为止
  Ptr<NetDevice> m_lo;

  /// Main Routing table for the node
  // 节点的主路由表
  RoutingTable m_routingTable;

  /// Advertised Routing table for the node
  // 节点的发布（公告）路由表
  RoutingTable m_advRoutingTable;

  /// The maximum number of packets that we allow a routing protocol to buffer.
  uint32_t m_maxQueueLen;

  /// The maximum number of packets that we allow per destination to buffer.
  uint32_t m_maxQueuedPacketsPerDst;

  /// The maximum period of time that a routing protocol is allowed to buffer a packet for.
  Time m_maxQueueTime;

  /// A "drop front on full" queue used by the routing layer to buffer packets to which it does not have a route.
  // 路由层使用的一种“全放前端”队列，用于缓冲没有路由的分组。
  PacketQueue m_queue;

  /// Flag that is used to enable or disable buffering
  bool EnableBuffering;

  /// Flag that is used to enable or disable Weighted Settling Time
  // 用于启用或禁用加权稳定时间的标志
  bool EnableWST;

  /// This is the wighted factor to determine the weighted settling time
  // 这是决定加权稳定时间的加权因子
  double m_weightedFactor;

  /// This is a flag to enable route aggregation. Route aggregation will aggregate all routes for
  /// 'RouteAggregationTime' from the time an update is received by a node and sends them as a single update .
  // 保存路由聚合时间间隔的参数。这是启用路由聚合的标志。
  // 路由聚合将在“路由聚合时间”内聚合节点接收到更新后的所有路由，并将它们作为单个更新发送。
  bool EnableRouteAggregation;

  /// Parameter that holds the route aggregation time interval
  // 保存路由聚合时间间隔的参数
  Time m_routeAggregationTime;

  /// Unicast callback for own packets
  // 为自己的数据包进行单播回调
  UnicastForwardCallback m_scb;

  /// Error callback for own packets
  // 自己数据包的错误回调
  ErrorCallback m_ecb;

private:
  /// Start protocol operation
  void
  Start ();
  /**
   * Queue packet until we find a route
   * \param p the packet to route
   * \param header the Ipv4Header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   */
  void
  DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  
  /// Look for any queued packets to send them out
  // 查找任何排队的数据包来发送它们
  void
  LookForQueuedPackets (void);

  /**
   * Send packet from queue
   * \param dst - destination address to which we are sending the packet to
   * \param route - route identified for this packet
   */
  void
  SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route);

  /**
   * Find socket with local interface address iface
   * \param iface the interface
   * \returns the socket
   */
  Ptr<Socket>
  FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;

  // Receive dsdv control packets
  /**
   * Receive and process dsdv control packet
   * \param socket the socket for receiving dsdv control packets
   */
  void
  RecvDsdv (Ptr<Socket> socket);

  /// Send packet
  void
  Send (Ptr<Ipv4Route>, Ptr<const Packet>, const Ipv4Header &);

  /**
   * Create loopback route for given header
   * 为给定标头创建环回路由
   * \param header the IP header
   * \param oif the device
   * \returns the route
   */
  Ptr<Ipv4Route>
  LoopbackRoute (const Ipv4Header & header, Ptr<NetDevice> oif) const;

  /**
   * Get settlingTime for a destination
   * \param dst - destination address
   * \return settlingTime for the destination if found
   */
  Time
  GetSettlingTime (Ipv4Address dst);

  /// Sends trigger update from a node
  void
  SendTriggeredUpdate ();

  /// Broadcasts the entire routing table for every PeriodicUpdateInterval
  // 每隔一个周期更新间隔，广播整个路由表
  void
  SendPeriodicUpdate ();

  /// Merge periodic updates
  // 合并定期更新
  void
  MergeTriggerPeriodicUpdates ();

  /// Notify that packet is dropped for some reason
  // 通知数据包由于某种原因被丢弃
  void
  Drop (Ptr<const Packet>, const Ipv4Header &, Socket::SocketErrno);

  /// Timer to trigger periodic updates from a node
  // 触发节点周期性更新的定时器
  Timer m_periodicUpdateTimer;

  /// Timer used by the trigger updates in case of Weighted Settling Time is used
  // 触发器使用的定时器在加权恢复时间情况下进行更新
  Timer m_triggeredExpireTimer;

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;
};

}
}

#endif /* DSDV_ROUTING_PROTOCOL_H */
