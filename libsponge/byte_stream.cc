#include "byte_stream.hh"

#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _buffer(std::string()), _total_capacity(capacity), _read_bytes(0), _write_bytes(0), _end_input(false) {}

size_t ByteStream::write(const string &data) {
    size_t write_len = min<size_t>(_total_capacity - _buffer.size(), data.size());
    if (write_len == 0)
        return 0;

    _buffer += data.substr(0, write_len);

    _write_bytes += write_len;
    return write_len;
}

//! \param[in] len bytes will be copied from the output side of the _buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = min<size_t>(len, _buffer.size());

    return _buffer.substr(0, peek_len);
}

//! \param[in] len bytes will be removed from the output side of the _buffer
void ByteStream::pop_output(const size_t len) {
    size_t remove_len = min<size_t>(len, _buffer.size());
    _buffer.erase(0, remove_len);
    _read_bytes += remove_len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string str = peek_output(len);
    pop_output(len);

    return str;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return _buffer.size() == 0 && _end_input; }

size_t ByteStream::bytes_written() const { return _write_bytes; }

size_t ByteStream::bytes_read() const { return _read_bytes; }

size_t ByteStream::remaining_capacity() const { return _total_capacity - _buffer.size(); }
