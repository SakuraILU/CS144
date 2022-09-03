#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // extract head of the segment
    TCPHeader head = seg.header();

    // if has syn signal
    if (head.syn) {
        if (_is_syn)  // return to refuse resyn...may be some error occur in the sender side?
            return;
        _is_syn = true;
        _isn = head.seqno;  // isn: the initial of seqno
        _checkpoint = 0;    // checkpoint: last absolute index
        _ackno = _isn + 1;  // ackno: next seqno we want to receive
        // cout << _isn.raw_value() << "--" << _ackno.raw_value() << endl;
    }
    // if not syn yet, just return to refuse the segment
    if (!_is_syn)
        return;

    // extract payload, convert(unwrap) seqno to absolute index and set eof according to head.fin signal
    std::string payload = seg.payload().copy();  // copy() method turn Buffer to string
    uint64_t abs_index = unwrap(head.seqno + (head.syn == true), _isn, _checkpoint);
    bool eof = (head.fin) ? true : false;
    // push payload to reassembler
    _reassembler.push_substring(payload, abs_index - 1, eof);

    // get next index we want (first byte in unreassembler field) and convert(wrap) it to seqno
    //
    // important size model in reassembler:
    //                                                |<-----                capacity                       ------>|
    // map:          |reassembler field(index = 0) is empty, str is writed to byte_stream|   unreassembler field   |
    // byte stream:  |       byte_stream: readed      |     byte_stream: unreaded        |<----   window size  --->|
    //               |<-- stream_out().bytes_read()-->|<---stream_out().buffer_size()--->|
    //                                                                                    ^
    //                                                                                  ackno
    _ackno = wrap(_reassembler.stream_out().bytes_read() + _reassembler.stream_out().buffer_size() + 1, _isn);

    // fin signal count one byte in the end segment logically for ackno
    // the fin segment may arrive early than segment before bcz the internet route delay,
    // thus remains in unreassembler filed, thus _ackno don't need to plus one.
    //
    // when fin segment is reassembled(write to byte stream), ackno should move one more byte for fin signal
    // after reassembler fin segment, ackno should be:
    // | syn |  normal data    | fin |
    //                                ^
    //                              ackno
    // Note: when reassembler reassembled all segment, reassembler will call stream_out().end_input() to end byte
    // stream. so, stream_out().input_ended() marks the transmission ending.
    if (_reassembler.stream_out().input_ended())
        _ackno = _ackno + 1;

    _checkpoint = abs_index;
}

// ackno      : unreassemble filed start seqno
// window size: unreassemble field size
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_is_syn)
        return nullopt;
    return _ackno;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
