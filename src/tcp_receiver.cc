#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  if (message.SYN) {
    zero_point_set = true;
    zero_point = message.seqno;
  }
  if (!zero_point_set) {
    return;
  }
  uint64_t absseq = message.seqno.unwrap(zero_point, ackno);
  //checkpoint = absseq + message.sequence_length();
  uint64_t steam_index = absseq;
  if (!message.SYN) {
    steam_index -= 1;
  } 
  if (message.FIN) {
    endno = absseq + message.sequence_length();
  }
  reassembler.insert(steam_index, message.payload, message.FIN, inbound_stream);
  ackno = inbound_stream.bytes_pushed() + 1;
  if (ackno + 1 == endno) {
    ackno += 1;
  }
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  uint16_t window_size = UINT16_MAX;
  if (window_size > inbound_stream.available_capacity()) {
    window_size = inbound_stream.available_capacity();
  }
  if (!zero_point_set) {
    return {std::nullopt, window_size};
  } 
  return {Wrap32::wrap(ackno, zero_point), window_size};
}
