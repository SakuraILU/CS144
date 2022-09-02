#include "wrapping_integers.hh"

#include <iostream>
#include <limits>
using namespace std;

#define BASE32(num) ((num)&0xffffffff00000000)
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // DUMMY_CODE(n, isn);
    uint32_t num = static_cast<uint32_t>(n) + isn.raw_value();
    return WrappingInt32{num};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // DUMMY_CODE(n, isn, checkpoint);
    uint32_t offset = n.raw_value() - isn.raw_value();
    uint64_t base = BASE32(checkpoint);
    uint64_t abs_val = base + offset;

    //   abs_val      checkpoint
    //-----|---------------|---------
    if (static_cast<int64_t>(abs_val) - static_cast<int64_t>(checkpoint) < 0) {
        if ((abs_val + (1ll << 32) - checkpoint) < (checkpoint - abs_val))
            abs_val += (1ll << 32);
    } else {
        //   checkpoint      abs_val
        //-------|---------------|---------
        if ((abs_val > (1ll << 32)) && ((checkpoint - (abs_val - (1ll << 32))) < (abs_val - checkpoint))) {
            abs_val -= (1ll << 32);
        }
    }
    return abs_val;
}
