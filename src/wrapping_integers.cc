#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <type_traits>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { static_cast<uint32_t>(n) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint32_t tail = wrap(checkpoint, zero_point).raw_value_;
  uint32_t raw = raw_value_;
  uint64_t ret = checkpoint - tail + raw;
  uint64_t ret2 = ret + (1ULL<<32);
  if (tail > checkpoint + raw) {
    return ret2;
  }
  if (tail < raw) {
    if (ret < (1ULL<<32)) {
      return ret;
    }
    ret2 -= 1ULL<<33;
    std::swap(ret, ret2);
  }
  return checkpoint - ret < ret2 - checkpoint ?
     ret : ret2;
}
