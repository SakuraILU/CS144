#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

// how many bytes can be writen to the sender's input stream
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

// how many bytes are sent but not acknowledged yet
size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active)
        return;

    // receive segment, clear timer for linger....
    _time_since_last_segment_received = 0;

    // set ackno and win_size of received segment for the sender
    if (seg.header().ack) {
        _ackno = seg.header().ackno;
        _window_size = seg.header().win;
    }

    // most priority case...when receive rst signal, (unclean) shutdown immediately
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // two possible error case for three-way handshake:
    //      1. LISTEN state must receive syn, otherwise is invalid, just return
    //      2. SYN_RECV state must not be a syn, otherwise is invalid, just return
    if ((!seg.header().syn && _state == LISTEN) || (seg.header().syn && _state == SYN_RECV))
        return;

    // receive the segment
    _receiver.segment_received(seg);

    // send segments
    if (_state == LISTEN) {
        // listen client's syn segment and connect to client (also send syn)
        //  do some safety check, make sure the seg is [syn] segment without [ack]
        //
        //  i don't know why the project disabled assert by macro....
        //  it's really inconvenient for defensive programming before deployment
        if (!seg.header().ack && seg.header().syn && seg.payload().size() == 0) {
            connect();
            _state = SYN_RECV;
        }
    } else if (_state == SYN_SENT) {
        // after sending the [syn] segment to connect,
        // the client waits for the [syn,ack] or [syn] segment from the sever
        //
        // Note:
        // usually, sever will send a [syn, ack] back...
        // but fsm_connect_relaxed is a very strange test case, the sever sends [syn] back to the client
        // and sever's [ack] is send afterwards...  thus, [syn,ack] and [syn] is both ok, don't check seg.header().ack!!
        // PROCESS:  client                      sever
        //           [syn] -->
        //                                   <-- [syn]
        //           [ack, payload] -->
        //                                   <-- [ack]
        if (seg.header().syn && seg.payload().size() == 0) {
            send_ack_segment();
            _state = SYN_ACKED;
        }
    } else if (seg.header().ack) {
        // normal case:
        //      after receiving segment, get ackno and win_size, send all new segments out
        //      if no new segment can send, send ack segment if the segment received is not empty (syn, payload and
        //      fin).
        if (!send_all_segments()) {
            if (seg.header().syn || seg.payload().size() != 0 || seg.header().fin)
                send_ack_segment();
        }
    }

    return;
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (!_active)
        return 0;
    // write string to sender's input stream
    size_t n = _sender.stream_in().write(data);
    // after write data, actively send segments of course, otherwise nothing will happen after connection is synced.
    send_all_segments();

    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active)
        return;

    _time_since_last_segment_received += ms_since_last_tick;

    // if retransmission too many times for the same segment, shutdown the connection uncleanly
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown();
        return;
    }

    // drive the timer of the sender for possible retransmission
    _sender.tick(ms_since_last_tick);
    // sender may send some segments, so send them out
    // Note: this send is caused by tick, no segment is received...
    //       so when sending them out, ackno and win is still old, no new segments to send...
    //       thus, pass arg: receive_msg = false;
    send_all_segments(false);
}

// end the sender's input stream, no more data will be accepted by the stream
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // fin (takes one byte) will be added to the stream,
    // so... bcz write something, send segments actively
    send_all_segments();
}

void TCPConnection::connect() {
    if (!_active)
        return;

    // only the first time to call _sender.fill_window() will make sender to send a syn segment,
    // so this function should just send once.
    // considered that when called connect(), the state will beconme SYN_SENT for client or SYN_RECV for sever from
    // LISTEN, this condition can guarantee connection() to be called only once...
    if (_state != LISTEN)
        return;

    _sender.fill_window();
    TCPSegment &seg = _sender.segments_out().front();
    // for client side: (receiver has no ackno value)
    //      receiver don't receive anything yet, no need to ack anyone, thus don't carry ackno and windows
    // for sever side: (receiver has ackno value)
    //      receiver already get a syn segment, need to ack client, so send ackno and win_size
    // Simply, add receiver's ack information to segment when receiver has ackno value, otherwise, set ack = false;
    // above description is already supported by add_ack_information() method
    add_ack_information(seg);

    send_all_segments();

    // a little tricky...
    // client's TCPConnection will call connect() directively, _state will be SYN_SENT
    // but sever's TCPConnection calls connect() by segment_received(), _state will be set to SYN_RECV just after
    // return back to segment_received()
    _state = SYN_SENT;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            // //cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer

            // when TCPConnection is destructed for some error or signal, the onlything to rescue is to unclean
            // shutdown
            unclean_shutdown();
        }
    } catch (const exception &e) {
        // cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::send_all_segments(bool receive_msg) {
    if (receive_msg) {
        _sender.ack_received(_ackno, _window_size);
    }

    if (_sender.segments_out().empty()) {
        // still, this is called after _receiver.segment_received(), received something, thus check clean shutdown
        // condition... see the last comment in this function for more information...
        clean_shutdown_if_possible();
        return false;
    }

    // push all segments to be sent in sender
    //      _sender.segments_out() stores all the segments should be send out in _sender
    //      _segments_out() stores all segments will be really send out to network by TCPConnection's owner...
    while (!_sender.segments_out().empty()) {
        TCPSegment &segment = _sender.segments_out().front();
        // add receiver's ack, ackno and winsize information to the segments
        add_ack_information(segment);

        _segments_out.push(std::move(segment));
        _sender.segments_out().pop();
    }

    // check whethe can clean shutdown the connection, if can, shutdown...
    //
    // everythime receive or send something, clean shutdown condition may changed
    // because  _receiver.segment_received() is always followed by send_all_segments(), just do checking in
    // send_all_segments()
    clean_shutdown_if_possible();
    return true;
}

void TCPConnection::clean_shutdown_if_possible() {
    // the first end peer should linger sometime after reassembled fin segment,
    // while the last one doesn't need...
    // when local peer reassembled fin segment but still not end (has something to send), it is the last end one, so not
    // need to linger...
    //
    // another thing: when the fin segment is reassembled by receiver?
    // note that after reassembled the last fin segment, receiver will end it's byte stream and make
    // _receiver.stream_out().input_ended() to be true.
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = 0;
    }

    // local peer's ending:
    //      1. outside stream is ended and all readed: _receiver.stream_out().eof()
    //      2. inside stream is ended and all readed:  _sender.stream_in().eof()
    //      3. all segment's are sent and full acknowledged
    // after this, local peer is finished it's work: received all segments and readed; sent all segments
    // confidently(full acknoledged) however, the first end peer may not sure whether remote peer is received fin's ack,
    // if not received, remote peer will retrans needed segments and local peer needs to ack..
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.bytes_in_flight() == 0) {
        // timer for linger is too long((10 times retransmission timeout)
        if (_linger_after_streams_finish && time_since_last_segment_received() >= 10 * _cfg.rt_timeout)
            _active = false;

        else if (!_linger_after_streams_finish)  // no need to linger sometime, when local peer is finished it's
                                                 // work, end local connection.
            _active = false;
    }
}

// send a pure ack segment without any bytes (including syn, payloud and fin)
void TCPConnection::send_ack_segment() {
    if (!_active)
        return;

    _sender.send_empty_segment();  // this is an empty segment..(send won't retrans empty segment)
    TCPSegment &seg = _sender.segments_out().front();
    add_ack_information(seg);

    send_all_segments();
}

void TCPConnection::unclean_shutdown() {
    // unclean shutdown for some errors causing TCPConnection to be destructed (e.g. kill -9 process)
    // Unclean shutdown: set error for in and out stream and send rst segment out... and close connection
    // immediately
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    _sender.send_empty_segment();
    TCPSegment &segment = _sender.segments_out().front();
    segment.header().rst = true;
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();

    _active = false;  // close connection.... mark _active = false
                      // when active is false, disabled all send and receive functions and also the timer ticking..
}

void TCPConnection::add_ack_information(TCPSegment &segment) {
    // add receiver's ack information (ackno and window_size) to segment when receiver has ackno value, and set ack
    // is true. otherwise, don't add and set ack to false;

    if (_receiver.ackno().has_value()) {
        segment.header().ack = true;
        segment.header().ackno = _receiver.ackno().value();
    } else {
        segment.header().ack = false;
    }
    segment.header().win = _receiver.window_size();
}