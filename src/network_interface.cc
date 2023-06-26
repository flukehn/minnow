#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include <iostream>
#include <memory>
#include <utility>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto next_hop_i = next_hop.ipv4_numeric();
  /*auto p = ip_to_eth.find(next_hop_i);
  bool flag = p == ip_to_eth.end() || time_point>=p->second.get_time_point+30000;
  bool send_arp = p == ip_to_eth.end() || 
    (time_point>=p->second.get_time_point+30000 && time_point>=p->second.query_time_point+5000);
  if (p!=ip_to_eth.end()) {
    cerr << send_arp << " "
    << (time_point>=p->second.get_time_point+30000) << " "
    <<  (time_point>=p->second.query_time_point+30000) << " "
    << time_point << " " << p->second.query_time_point
    << endl;
  }*/
  ip_to_eth_item &q = ip_to_eth[next_hop_i];
  bool flag = !q.address_initialized || time_point>=q.get_time_point+30000;
  bool send_arp = (!q.address_initialized || time_point>=q.get_time_point+30000) && 
  (!q.query_initialized || time_point>=q.query_time_point+5000);
  if (send_arp) {
    // arp
    ARPMessage req;
    req.opcode = ARPMessage::OPCODE_REQUEST;
    req.sender_ip_address = ip_address_.ipv4_numeric();
    req.sender_ethernet_address = ethernet_address_;
    req.target_ip_address = next_hop_i;
    EthernetHeader header = {ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP};
    auto msg=make_shared<EthernetFrame>(header, serialize(req));
    mq.push(msg);
    q.query_time_point=time_point;
    q.query_initialized = true;
  }
  if (flag) {
    auto sp=std::make_shared<wait_eth>(dgram, next_hop_i);
    wq.push(std::make_pair(sp, time_point));
    q.wd.emplace_back(sp);
    return;
  }
  cerr << "send a frame" << endl;
  EthernetHeader header = {q.ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4};
  auto msg=make_shared<EthernetFrame>(header, serialize(dgram));
  mq.push(msg);
}

void NetworkInterface::send_queued_datagram(ip_to_eth_item& item) {
  for (const auto &p: item.wd) {
    auto q = p.lock();
    if (!q) {
      continue;
    }
    EthernetHeader header = {item.ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4};
    auto msg=make_shared<EthernetFrame>(header, serialize(q->data));
    mq.push(msg);
  }
  item.wd.clear();
}
// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  const auto& dst_addr = frame.header.dst;
  if(dst_addr != ethernet_address_ && dst_addr != ETHERNET_BROADCAST) {
    return {};
  }
  if(frame.header.type == EthernetHeader::TYPE_IPv4){
    InternetDatagram ret;
    if(parse(ret, frame.payload)) {
      return ret;
    }
    cerr << "Parse IPv4 failed from: " << to_string(dst_addr) << endl;
  }
  // ARP
  ARPMessage msg;
  if(!parse(msg, frame.payload)) {
    cerr << "Parse ARP failed from: " << to_string(dst_addr) << endl;
    return {};
  }
  ip_to_eth_item &p = ip_to_eth[msg.sender_ip_address];
  p.ethernet_address = msg.sender_ethernet_address;
  p.get_time_point = time_point;
  p.address_initialized = true;
  if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric()){
    auto reply = msg;
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ip_address = ip_address_.ipv4_numeric();
    reply.sender_ethernet_address = ethernet_address_;
    reply.target_ip_address = msg.sender_ip_address;
    reply.target_ethernet_address = msg.sender_ethernet_address;
    EthernetHeader header{msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP};
    auto rep = std::make_shared<EthernetFrame>(header, serialize(reply));
    mq.push(rep);
  }
  send_queued_datagram(p);
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time_point += ms_since_last_tick;
  while (!wq.empty() && time_point >= wq.front().second + 60'000) {
    wq.pop();
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if(mq.empty()){
    return std::nullopt;
  }
  auto w=mq.front();
  mq.pop();
  return *w;
}
