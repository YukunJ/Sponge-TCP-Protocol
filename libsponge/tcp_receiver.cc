#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const Buffer &payload = seg.payload();
    const TCPHeader &header = seg.header();
    const WrappingInt32 segmentBegin = header.seqno;

    if (header.syn && _isn.has_value() && segmentBegin != _isn.value()) {
        // error0 : different SYN beginning
        return;
    }
    if (header.syn && !_isn.has_value()) {
        setSYN(segmentBegin);
    }
    if (!seenSYN()) {
        // error1 : if not SYN, not accepting data
        return;
    }
    const uint64_t expect_ack = abs_ackno();
    const uint64_t checkpoint = _reassembler.first_unassembled();
    const uint64_t abs_seqno = unwrap(segmentBegin, _isn.value(), checkpoint);
    const uint64_t stream_idx = abs_seqno > 0 ? abs_seqno - 1 : 0;
    const uint64_t w_size = window_size();
    if (!(abs_seqno < expect_ack + w_size && abs_seqno + seg.length_in_sequence_space() > expect_ack)) {
        // error2 : not overlapping with current acceptable window
        return;
    }
    // if seen a FIN, is the window big enough to incorporate it?
    if (header.fin && stream_idx + payload.size() <= checkpoint + w_size) {
        setFIN(abs_seqno + seg.length_in_sequence_space());
    }

    string payload_string = payload.copy();
    _reassembler.push_substring(payload_string, stream_idx, header.fin);
}

uint64_t TCPReceiver::abs_ackno() const {
    uint64_t abs_ack = 0;
    if (seenSYN()) {
        abs_ack = 1 + _reassembler.first_unassembled();
        if ((abs_ack + 1) == _abs_fin)
            ++abs_ack;
    }
    return abs_ack;
}
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (seenSYN()) {
        // only after already seen the SYN flag setup
        uint64_t abs_ack = abs_ackno();
        return make_optional<WrappingInt32>(wrap(abs_ack, _isn.value()));
    }
    return {};
}

size_t TCPReceiver::window_size() const { return _reassembler.window_size(); }
