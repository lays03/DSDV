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
#include "dsdv-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"

namespace ns3 {
namespace dsdv {

NS_OBJECT_ENSURE_REGISTERED (DsdvHeader);

DsdvHeader::DsdvHeader (Ipv4Address dst, uint32_t hopCount, uint32_t dstSeqNo)
  : m_dst (dst),
    m_hopCount (hopCount),
    m_dstSeqNo (dstSeqNo)
{
}

DsdvHeader::~DsdvHeader ()
{
}

TypeId
DsdvHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::dsdv::DsdvHeader")
    .SetParent<Header> ()
    .SetGroupName ("Dsdv")
    .AddConstructor<DsdvHeader> ();
  return tid;
}

TypeId
DsdvHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
DsdvHeader::GetSerializedSize () const
{
  //ADD: 新加了8个uint16_t的字段，一共是 16bit / 8 = 2byte 2*8=16byte
  // return 12;
  return 28;
}

void
DsdvHeader::Serialize (Buffer::Iterator i) const
{
  //ADD: 序列化位置信息和速度信息
  i.WriteHtonU16 (m_x);
  i.WriteHtonU16 (m_y);
  i.WriteHtonU16 (m_z);
  i.WriteHtonU16 (m_vx);
  i.WriteHtonU16 (m_vy);
  i.WriteHtonU16 (m_vz);
  i.WriteHtonU16 (m_sign);
  i.WriteHtonU16 (m_timestamp);

  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_hopCount);
  i.WriteHtonU32 (m_dstSeqNo);
}

uint32_t
DsdvHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  //ADD: 反序列化位置信息和速度信息
  m_x = i.ReadNtohU16 ();
  m_y = i.ReadNtohU16 ();
  m_z = i.ReadNtohU16 ();
  m_vx = i.ReadNtohU16 ();
  m_vy = i.ReadNtohU16 ();
  m_vz = i.ReadNtohU16 ();
  m_sign = i.ReadNtohU16 ();
  m_timestamp = i.ReadNtohU16 ();

  ReadFrom (i, m_dst);
  m_hopCount = i.ReadNtohU32 ();
  m_dstSeqNo = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
DsdvHeader::Print (std::ostream &os) const
{
  os << "DestinationIpv4: " << m_dst
     << " Hopcount: " << m_hopCount
     << " SequenceNumber: " << m_dstSeqNo;
}
}
}
