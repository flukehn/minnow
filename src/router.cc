#include "router.hh"
#include "address.hh"
#include "ipv4_datagram.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  table.add_route(route_prefix, prefix_length, next_hop, interface_num);
}

void route_table::add_route( const uint32_t route_prefix,
                             const uint8_t prefix_length, 
                             const optional<Address> next_hop,
                             const size_t interface_num )
{
  auto p=root;
  for (uint8_t i = 0; i < prefix_length; ++i) {
    uint8_t v = (route_prefix >> (31-i)) & 1;
    if (!p->ch[v]) {
      p->ch[v]=std::make_shared<trie>();
    }
    p=p->ch[v];
  }
  
  p->next_hop = {next_hop, interface_num};
}

std::optional<next_hop_info>  route_table::find_next(uint32_t dst) const{
  auto p=root;
  std::optional<next_hop_info>  ret=p->next_hop;
  for (int i=0;i<32;++i){
    uint8_t v=(dst>>(31-i))&1;
    if(!p->ch[v]){
      break;
    }
    p=p->ch[v];
    //cerr<<i<<" "<<p->next_hop.has_value()<<endl;
    if(p->next_hop.has_value()){
      ret=p->next_hop;
    }
  }
  return ret;
}

void Router::route() {
  for(auto &x:interfaces_){
    while(true){
      auto maybe_datagram=x.maybe_receive();
      if(!maybe_datagram.has_value()) {
        break;
      }
      cerr<<"1111"<<endl;
      InternetDatagram datagram = maybe_datagram.value();
      if (datagram.header.ttl < 2) {
        continue;
      }
      datagram.header.ttl -= 1;
      datagram.header.compute_checksum();
      std::optional<next_hop_info> nxt = table.find_next(datagram.header.dst);
      /*cerr 
        << "src = " << Address::from_ipv4_numeric(datagram.header.src).ip() << " "
        << "dst = " << Address::from_ipv4_numeric(datagram.header.dst).ip() << " "
        << "next_hop = " << nxt.has_value() << ", "
        << endl;*/
      if(!nxt.has_value()){
        continue;
      }
      Address dst_addr = Address::from_ipv4_numeric(datagram.header.dst);
      if(nxt->addr.has_value()){
        dst_addr = nxt->addr.value();
      }
      cerr<<nxt->addr.has_value()<<" "<<dst_addr.ip()<<endl;
      
      interface(nxt->id).send_datagram(datagram, dst_addr);
    }
  }
}