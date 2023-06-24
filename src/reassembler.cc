#include "reassembler.hh"
#include <cstdint>
#include <iostream>
#include <sys/types.h>

using namespace std;

void Reassembler::pop(Writer& output){
  uint64_t len = output.available_capacity();
  if (len > buffered - popped) {
    len = buffered - popped;
  }
  //cerr << output.available_capacity() << " " << buffered << " " << popped << " " << len << endl;
  if (len > 0) {
    uint64_t l = popped % capacity;
    uint64_t r = (l + len) % capacity;
    if (l < r) {
      output.push(buf.substr(l, r-l));
      for (uint64_t i = l; i < r; ++i) {
        is_value[i] = false;
      }
    } else {
      output.push(buf.substr(l, capacity - l));
      output.push(buf.substr(0, r));
      for (uint64_t i = l; i < capacity; ++i) {
        is_value[i] = false;
      }
      for (uint64_t i = 0; i < r; ++i) {
        is_value[i] = false;
      }
    }
  }
  popped += len;
  bytes_pending_ -= len;
  if (endpos_set && popped == endpos) {
    output.close();
  }
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  if (is_last_substring) {
    endpos_set = true;
    endpos = first_index + data.size();
  }
  pop(output);
  if (data.empty() || first_index >= popped + output.available_capacity()) {
    return;
  }
  uint64_t rpos = first_index + data.size();
  if (rpos > popped + output.available_capacity()) {
    rpos = popped + output.available_capacity();
  }
  for (uint64_t i = first_index, pos = first_index % capacity; i < rpos; ++i, pos = pos + 1 == capacity ? 0 : pos + 1) {
    buf[pos] = data[i - first_index];
    if (!is_value[pos] && i >= popped && i < pending_pos) {
      bytes_pending_ += 1;
    }
    is_value[pos] = true;
  }
  while(buffered < popped + capacity && is_value[buffered%capacity]) {
    buffered += 1;
  }
  uint64_t pending_pos_new = popped + output.available_capacity();
  while (pending_pos < pending_pos_new) {
    bytes_pending_ += is_value[(pending_pos++)%capacity];
  }
  pop(output);
  //cerr << first_index << " " << data << " " << len << " " << buffered << endl;
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return bytes_pending_;
}
