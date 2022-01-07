#include "wrapping_integers.hh"

#include <limits>

using namespace std;

template <typename T>
T unsigned_abs(const T &a, const T &b) {
    return (a > b) ? (a - b) : (b - a);
}

void update(uint64_t &currDist, uint64_t &recordDist, int &currSign, int &recordSign) {
    if (currDist < recordDist) {
        recordDist = currDist;
        recordSign = currSign;
    }
}

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t lower32bit = static_cast<uint32_t>(n);
    return isn + lower32bit;
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
    // transform checkpoint into WrappingInt32
    WrappingInt32 wrapCheckpoint = wrap(checkpoint, isn);
    // take the difference between wrapCheckpoint and n
    // we fix wrapCheckpoint, there are 2 ways for it to reach n
    // 1. Go forward 2. Go backward
    // we will take the shortest absolute distance walked in these two options
    uint32_t diff = n.raw_value() - wrapCheckpoint.raw_value();
    if (static_cast<int64_t>(static_cast<int32_t>(diff) + checkpoint) < 0) {
        // this will render the unwrap value be negative, cannot do
        return checkpoint + diff;
    }
    return checkpoint + static_cast<int32_t>(diff);
}
