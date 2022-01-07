#include "tcp_connection.hh"

#include <iostream>

using namespace std;

void TCPConnection::send_sender_segments() {
    // clear sender's outstream
    TCPSegment temp;
    while (!_sender.segments_out().empty()) {
        temp = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            // if the connection is already established, set the ACK flag and ackno
            temp.header().ack = true;
            temp.header().ackno = _receiver.ackno().value();
            temp.header().win = _receiver.window_size();
        }
        _segments_out.push(temp);
    }
    // try to see if a clean shutdown could be reached
    clean_shutdown();
}

void TCPConnection::unclean_shutdown() {
    // set the error state for both inbound and outbound and send a RST segment, deactivate
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    // the outbound should not be empty at this point
    TCPSegment temp = _sender.segments_out().front();
    _sender.segments_out().pop();
    temp.header().ack = true;
    if (_receiver.ackno().has_value()) {
        // if the connection is already established, set the ackno
        temp.header().ackno = _receiver.ackno().value();
    }
    temp.header().rst = true;
    temp.header().win = _receiver.window_size();
    _segments_out.push(temp);
}

void TCPConnection::clean_shutdown() {
    // if inbound stream ends before reach EOF, no need to linger
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof()) {
            // there will be no more incoming, only outcoming
            // the other side is only "receiving"
            _linger_after_streams_finish = false;
        } else if (bytes_in_flight() == 0) {
            // already send and get acknowledged all segments
            if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                // no need to linger or already pass the timeout period, deactivate
                _active = false;
            }
        }
    }
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active()) {
        return;
    }
    _time_since_last_segment_received = 0;  // reset the time elapse
    // passive peer
    if (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() == 0) {
        if (!seg.header().syn) {
            // must establish connection first
            return;
        }
        _receiver.segment_received(seg);
        connect();
        return;
    }
    // active peer
    if (_sender.next_seqno_absolute() > 0 && !_receiver.ackno().has_value() &&
        bytes_in_flight() == _sender.next_seqno_absolute()) {
        if (seg.payload().size()) {
            // no data should be sent before connection established
            return;
        }
        if (!seg.header().ack) {
            if (seg.header().syn) {
                // simultaneous send SYN to each other
                _receiver.segment_received(seg);
                _sender.send_empty_segment();
            }
            return;
        }
        if (seg.header().rst) {
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            _active = false;
            return;
        }
    }
    // ordinary case
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);
    if (_sender.stream_in().buffer_empty() && seg.length_in_sequence_space()) {
        // no more data, but have to send a reply
        _sender.send_empty_segment();
    }
    if (seg.header().rst) {
        _sender.send_empty_segment();
        unclean_shutdown();
        return;
    }
    send_sender_segments();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (data.empty()) {
        return 0;
    }
    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();  // try generate new segments
    send_sender_segments();
    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active()) {
        return;
    }
    // let the sender tick and access the consecutive retransmission count, if too much, kill connection
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown();
    }
    // tick may trigger retransmission
    send_sender_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();  // clear storage and send
    send_sender_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();  // fill window will be able to send SYN if not sent already
    send_sender_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();  // make sure there is at least one segment to carry RST flag
            unclean_shutdown();
        }
    } catch (const exception &e) {
        cerr << "Exception destructing TCP FSM: " << e.what() << endl;
    }
}
