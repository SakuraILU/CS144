#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{_initial_retransmission_timeout}
    , _retransmission_cnt(0)
    , _stream(capacity) {}

// bytes_in_flight: bytes send but not fully acknowledged
// Other words: bytes stored in segments.buffer
uint64_t TCPSender::bytes_in_flight() const { return _byte_in_flight; }

void TCPSender::fill_window() {
    // syn signal is used to sync a TCP connection, doesn't contain any payload ( according to book << Network:Top Down
    // Method >>). although lab2 has strang test cases like: one segment contains syn, payload and fin at the same
    // time...
    // Lab guide also note: initially, assume window space is 1, means just send 1 btye syn signal...
    // cout << " fill window: " << _is_syn << endl;
    if (!_is_syn) {
        TCPSegment segment;
        TCPHeader &head = segment.header();
        head.seqno = _isn;
        head.syn = true;
        head.fin = false;

        // init ackno and it's absolute value checkpoint
        _ackno = head.seqno;
        _ackno_checkpoint = 0;
        _next_seqno += segment.length_in_sequence_space();  // move next seqno ahead

        // send segment
        send_segment(segment);
        _is_syn = true;
        return;
    }

    // if is finished or stream is empty but not end, do nothing
    // Note: when stream is empty, there's two case:
    //      1.stream is not eof...don't send anything
    //      2.stream is eof...need add fin, which takes one byte, so need to be sent.
    if (_is_fin || (_stream.buffer_empty() && !_stream.eof()))
        return;

    // SIZE MODEL:
    //                                  by tick()               by fill_window()
    //                             buffered and clolcked      sent them as segments
    // stream:   |  fully acked|       to be resend          |   to fill window    |     unread yet       |
    //                         ^    ^
    //                         |    |<-----           window size            ------>
    //                         |  ackno                       ^
    //             last segment right side <= ackno        next_seqno

    // init remain size between 1. next_seqno and 2. window right edge,
    // send this field by segments to fill the window of receiver...
    size_t remain_size =
        _window_size + (_window_size == 0) - (next_seqno_absolute() - unwrap(_ackno, _isn, _ackno_checkpoint));
    // cout << "window size " << _window_size << " next seqno " << next_seqno_absolute() << " ackno "
    //      << unwrap(_ackno, _isn, _ackno_checkpoint) << endl;
    // cout << "stream is end? " << _stream.eof() << endl;
    while (remain_size > 0) {
        // init a tcp segment and get it's head and payload BY REFERENCE!!
        TCPSegment segment;
        TCPHeader &head = segment.header();
        Buffer &payload = segment.payload();

        head.seqno = next_seqno();

        Buffer buf(_stream.read(min<size_t>(remain_size, TCPConfig::MAX_PAYLOAD_SIZE)));
        payload = std::move(buf);  // buf is no longer needed anymore, use std::move to make it a tmp variable,
                                   // operator= will call move copy func to do shallow copy in data structures
        _next_seqno = _next_seqno + segment.length_in_sequence_space();  // move next seqno ahead

        remain_size -= segment.length_in_sequence_space();

        // deal with fin signal
        // Note: if no window space, do not add fin! some test cases tell me about this...
        if (_stream.eof() && remain_size > 0) {
            head.fin = true;
            _is_fin = true;
            _next_seqno += 1;
        }

        // cout << "send data: " << segment.payload().str() << endl;
        send_segment(segment);

        if (_stream.buffer_empty())
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _ackno = ackno;
    uint64_t ackno_abs = unwrap(ackno, _isn, _ackno_checkpoint);
    _window_size = window_size;

    // lab guide note: may due to some error, ackno may be incorrectly.
    // specifically, when ackno larger than next_seqno, ignore it.
    if (ackno_abs > next_seqno_absolute())
        return;

    // unbuffered segments whose right side is <= ackno (Fully acknoledged)
    // about palf acknoledged segment, they are still needed to be resent untill fully acked.
    //       |      segment       |
    //     received    ^    unreassemled by the recevier
    //               ackno        need to be resent
    //
    // traverse _segment_buffer and pop this segments whose right side is <= ackno
    while (!_segments_buffer.empty()) {
        TCPSegment &segment = _segments_buffer.front();
        if (unwrap(segment.header().seqno, _isn, _ackno_checkpoint) + segment.length_in_sequence_space() <= ackno_abs) {
            _byte_in_flight -=
                segment.length_in_sequence_space();  // bytes in this segment is fully acked, update byte_in_flight
            _segments_buffer.pop();

            _timer = 0;  // buffer has changed, reset timer for the new frontest unacked segment
        } else
            break;
    }

    // call fill_window to fill the spare window space...
    fill_window();

    // oh, ackno is received. it seems like the network is not congested, reset retransmission config
    _retransmission_cnt = 0;
    _retransmission_timeout = _initial_retransmission_timeout;

    _ackno_checkpoint = ackno_abs;  // update checkpoint
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
// deal with situation where no ackno is received,
// tick() will update timer and resend lowest-seqno(front in queue) segment if timer is expired
//
// note: only send one segment. bcz if lost is due to network congestion,
//       resend a butch of segments will make things wrose.
//       also, it's a bit waste, many segments may has already be received.
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_segments_buffer.empty())
        return;
    // cout << "time is " << _timer << " set clock to " << _retransmission_timeout << " win size " << _window_size <<
    // endl;
    _timer += ms_since_last_tick;
    if (_timer >= _retransmission_timeout) {
        // cout << "tick " << endl;
        // if timeout, network seems very congested, double retransmission time to reduce network flow...
        // but if window size is 0, really want to know when win_size is nonzeron, so don't double retans_timeout...
        _retransmission_cnt++;
        _retransmission_timeout = (_window_size > 0) ? 2 * _retransmission_timeout : _retransmission_timeout;
        // cout << "timeout " << _retransmission_timeout << endl;
        _segments_out.push(_segments_buffer.front());  // try to send the frontest buffered segment
        _timer = 0;                                    // restart timer again...
    }
}
//
// how many timeout occurs between two ackno received..
// use it to adjust newtwork congestion or something...
unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_cnt; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}

// TCPSender is responsible to convert byte_stream to segments...
// Actually, send_segment means to put the segment to segments_out queue.
// The segments in this queue will be sent by TCPConnection later.
void TCPSender::send_segment(const TCPSegment segment) {
    _segments_buffer.push(segment);  // send segment
    _segments_out.push(segment);     // buffer this segment meanwhile

    _byte_in_flight += segment.length_in_sequence_space();  // send new segment, so update bytes_in_flight
}
