#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

using namespace std;
//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _latest_ack_seqno(0)
    , _current_retransmission_timeout{retx_timeout}
    , _is_timer_on(false)
    , _syn_sent(false)
    , _fin_sent(false)
    , _last_tick_time(0)
    , _outstanding_segment{}
    , _consecutive_retransmission(0)
    , _bytes_in_flight(0)
    , _receiver_window_size(0)
    , _receiver_freespace(0) {}

size_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::_send_segment(TCPSegment &seg) {
    seg.header().seqno = next_seqno();
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    if (!_is_timer_on) {
        // start the timer and default set the time
        _is_timer_on = true;
        _last_tick_time = 0;
    }
    if (_syn_sent) {
        _receiver_freespace -= seg.length_in_sequence_space();
    }
    _segments_out.push(seg);
    _outstanding_segment.push(seg);
}

void TCPSender::fill_window() {
    if (!_syn_sent) {
        // initially assume the window size is 1, only sent a 'SYN' flag
        _syn_sent = true;
        TCPSegment seg;
        seg.header().syn = true;
        _send_segment(seg);
        return;
    }
    if (!_outstanding_segment.empty() && _outstanding_segment.front().header().syn) {
        // syn already sent but not acknowledged yet
        return;
    }
    if (_fin_sent) {
        // fin already sent, no more new data to be sent
        return;
    }
    if (!_stream.input_ended() && _stream.buffer_size() == 0) {
        // currently no more data to be send out, but input stream has not ended
        return;
    }
    // branches: last announced window_size is zero or non-zero
    // if non-zero, try to fill the window. When eof reached, if possible, feed FIN(occupy 1 placeholder seqno)
    // if zero, if freespace is 0, send tester-segment (fin or 1-byte depending on if stream eof reached)
    if (_receiver_window_size) {
        while (_receiver_freespace) {
            TCPSegment seg;
            size_t next_read =
                min({_stream.buffer_size(), static_cast<size_t>(_receiver_freespace), TCPConfig::MAX_PAYLOAD_SIZE});
            seg.payload() = Buffer{_stream.read(next_read)};
            if (_stream.eof() && _receiver_freespace > next_read) {
                // have space for the FIN flag
                seg.header().fin = true;
                _fin_sent = true;
            }
            _send_segment(seg);
            if (_stream.buffer_empty()) {
                break;
            }
        }
    } else {
        // window size is 0, need to send tester segment
        if (_receiver_freespace == 0) {
            TCPSegment seg;
            if (_stream.eof()) {
                // send FIN flag
                seg.header().fin = true;
                _fin_sent = true;
                _send_segment(seg);
            } else if (!_stream.buffer_empty()) {
                // send 1 byte tester
                seg.payload() = Buffer{_stream.read(1)};
                _send_segment(seg);
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (!valid_ackno(abs_ackno)) {
        // invalid acknowledge number
        return;
    }

    _receiver_window_size = window_size;
    _receiver_freespace = window_size;
    // try to pop out fully-acknowledged segment
    while (!_outstanding_segment.empty()) {
        uint64_t outstanding_abs_seq_begin = unwrap(_outstanding_segment.front().header().seqno, _isn, _next_seqno);
        uint64_t outstanding_seq_len = _outstanding_segment.front().length_in_sequence_space();
        if (outstanding_abs_seq_begin + outstanding_seq_len <= abs_ackno) {
            TCPSegment seg = _outstanding_segment.front();
            // a success pop out
            _bytes_in_flight -= seg.length_in_sequence_space();
            _outstanding_segment.pop();
            // reset RTO
            _current_retransmission_timeout = _initial_retransmission_timeout;
            // if any remaining outstanding, reset the timer
            _last_tick_time = 0;
            // refresh the consecutive retrans back to 0
            _consecutive_retransmission = 0;
        } else {
            break;
        }
    }

    if (!_outstanding_segment.empty()) {
        _receiver_freespace = abs_ackno + window_size -
                              unwrap(_outstanding_segment.front().header().seqno, _isn, _next_seqno) - _bytes_in_flight;
    }
    if (!_bytes_in_flight) {
        // all sent acknowledged so far, close the timer
        _is_timer_on = false;
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_is_timer_on) {
        // timer currently not available
        return;
    }
    _last_tick_time += ms_since_last_tick;  // increment the time elapse
    if (_last_tick_time < _current_retransmission_timeout) {
        // not yet expired
        return;
    } else {
        _segments_out.push(_outstanding_segment.front());
        if (_receiver_window_size || _outstanding_segment.front().header().syn) {
            ++_consecutive_retransmission;
            _current_retransmission_timeout *= 2;
        }
        _last_tick_time = 0;  // reset the clock
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission; }

void TCPSender::send_empty_segment() {
    // no retransmission for empty segment
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
