#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! initial retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    // Additional private member
    //！the latest acknowledged (absolute) sequence number
    uint64_t _latest_ack_seqno;

    //! initial retransmission timer for the connection
    unsigned int _current_retransmission_timeout;

    //! if the timer has already been set
    bool _is_timer_on;

    //! if SYN has already been sent
    bool _syn_sent;

    //! if FIN has already been sent
    bool _fin_sent;

    //! last tick's timestamp
    size_t _last_tick_time;

    //! the outstanding segment sent but not acknowledged
    std::queue<TCPSegment> _outstanding_segment;

    //! the number of consecutive retransmission before any acknowledge
    unsigned int _consecutive_retransmission;

    //！ bytes currently sent but not acknowledged
    uint16_t _bytes_in_flight;

    //! the latest window size of the receiver
    uint16_t _receiver_window_size;

    //! the current remaining receiver free space the sender perceive
    uint64_t _receiver_freespace;

    void _send_segment(TCPSegment &seg);

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    //! helper function : make a TCP Segment to be sent,
    //                    also increase the counting of bytes sent recording

    //! \brief to test if a newly received ackno is valid one
    bool valid_ackno(uint64_t abs_ack_seqno) {
        if (_outstanding_segment.empty()) {
            return abs_ack_seqno <= _next_seqno;
        }
        return abs_ack_seqno <= _next_seqno &&
               abs_ack_seqno >= unwrap(_outstanding_segment.front().header().seqno, _isn, _next_seqno);
    }
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
