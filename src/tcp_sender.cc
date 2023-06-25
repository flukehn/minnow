#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string_view>
#include <utility>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ), rto(initial_RTO_ms)
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return seqno - ackno;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return failed_cnt;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  while (!mq.empty()) {
    auto msgp = mq.front();
    mq.pop();
    if (msgp->seqno.unwrap(isn_, ackno)+msgp->sequence_length() > ackno) {
      return *msgp;
    } 
  }
  return std::nullopt;
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  do{
    string m;
    uint64_t len = window_size;
    if (len < 1) {
      len = 1;
    }
    if (len < seqno - ackno) {
      len = 0;
    } else {
      len -= seqno - ackno;
    }
    bool cantake = false;
    if (len > TCPConfig::MAX_PAYLOAD_SIZE) {
      len = TCPConfig::MAX_PAYLOAD_SIZE;
      cantake = true;
    }
    while (len) {
      string_view sv = outbound_stream.peek();
      if (sv.empty()) {
        break;
      }
      if (sv.length() >= len) {
        m += sv.substr(0, len);
        outbound_stream.pop(len);
        len = 0;
      } else {
        m += sv;
        len -= sv.length();
        outbound_stream.pop(sv.length());
      }
    }
    bool syn = seqno == 0;
    bool fin = !fin_send && outbound_stream.is_finished() && (len > 0 || cantake);
    //cerr << syn << " " << fin << " " << window_size << " " << m.length() << " " << ackno << " " << len << endl;
    //cerr << outbound_stream.bytes_popped() << " "<< outbound_stream.bytes_buffered() << endl;
    if (!syn && !fin && m.empty()) {
      return;
    }
    //window_size -= m.size()+fin;
    auto sp = make_shared<TCPSenderMessage>(Wrap32::wrap(seqno, isn_), syn, m, fin);
    mq.push(sp);
    if (wq.empty()) {
      start_time = time_point;
    }
    wq.push(sp);
    seqno += syn + m.size()+ (fin && !fin_send);
    fin_send |= fin; //outbound_stream.is_finished();
  }while(window_size > seqno - ackno && !outbound_stream.peek().empty());
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return {Wrap32::wrap(seqno, isn_), false, {}, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  uint64_t ack = msg.ackno.value_or(isn_).unwrap(isn_, ackno);
  //cerr << "ackno = " << ackno << endl;
  if (ack <= seqno) {
    if (ack > ackno) {
      ackno = ack;
      rto = initial_RTO_ms_;
      failed_cnt = 0;
      start_time = time_point;
    }
    
  }
  window_size = msg.window_size;
  /*while (!mq.empty() && mq.front().seqno.unwrap(isn_, ackno) < ackno) {
    mq.pop();
  }*/
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  time_point += ms_since_last_tick;
  while (!wq.empty() && wq.front()->seqno.unwrap(isn_, ackno)+wq.front()->sequence_length() <= ackno) {
    wq.pop();
  }
  if (!wq.empty() && time_point >= start_time + rto) {
    auto sp = wq.front();
    mq.push(sp);
    start_time = time_point;
    if (window_size) {
      failed_cnt += 1;
      rto += rto;
    }
  }
}
