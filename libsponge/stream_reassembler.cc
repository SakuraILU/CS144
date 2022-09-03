#include "stream_reassembler.hh"

#include <iostream>
using namespace std;

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unreassm_bytes(0), _segments(std::map<size_t, Segment>()) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
#include <iostream>
using namespace std;

void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    int truc_len = _output.bytes_read() + _capacity - index;
    size_t seg_index = index;
    string data_truc = data.substr(0, max<int>(0, truc_len));

    Segment segment(eof, data_truc);
    // cout << segment._eof << endl;
    // itr_left: refer to the left connected neighbor
    // itr_right: refer to the right connected neighbor
    map<size_t, Segment>::iterator itr_left = _segments.end(), itr_right = _segments.end();
    size_t seg_l = seg_index, seg_r = seg_index + segment._len;

    auto itr = _segments.begin();
    while (itr != _segments.end()) {
        size_t l = itr->first, r = itr->first + itr->second._len;
        // printf("segment: %ld, %ld\n", seg_l, seg_r);
        // printf("itr: %ld,%ld\n", l, r);
        if (l <= seg_l) {
            if (r < seg_l) {
                //          [ segment )
                // [ itr)
                //: do nothing , continue next loop
            } else if (r >= seg_l && r <= seg_r) {
                //          seg_l
                //            v
                //            [ segment )
                //    [ itr       )
                //                ^
                //                r
                // trunc segment's overlap part
                // and itr is refered by itr_left, it needed be to merged with segment later
                //
                // Note: don't truc itr's overlap part!
                // because if itr' index is 0, it has already been write to _output stream...
                int n = r - seg_l;
                segment.erase_front(n);
                seg_index += n;  // segment index move right bcz trunc operation
                // _unreassm_bytes -= n;
                itr_left = itr;
            } else {
                //           [ segment )
                //   [ itr                  )
                //: don't need to insert segment, just return
                // _unreassm_bytes -= segment._len;
                return;
            }
        } else if (l > seg_l && l <= seg_r) {
            if (r <= seg_r) {
                //    [ segment )
                //        [itr)
                // itr can be completely overlapped by segment,
                // and it's index is definitely not 0, just throw it away and remain segment...

                // An Important operation:
                // itr moves to the next and delete the origin segment...
                // if erase itr first, itr will point to nothing and be a wild pointer,
                // the information to the next object is stored in the object that itr pointed to,
                // thus itr++ will failed and return rubbish...
                _unreassm_bytes -= itr->second._data.size();
                _segments.erase(itr++);
                // because itr is already be ++, continue to avoid itr++ in the end of the while loop
                continue;
            }
            if (r > seg_r) {
                //            seg_r
                //              v
                //    [ segment )
                //        [  itr     )
                //        ^
                //        l
                // trunc segment's overlap part
                // and itr is refered by itr_right, it needed to be merged with segment later
                //
                // Note: this time, still trunc segment,
                // on the one hand: make it consistent with the left side...
                // one the other hand: itr index is the key of map...so is fixed, no easy way to change.
                int n = seg_r - l;
                // _unreassm_bytes -= n;
                segment.erase_back(n);
                itr_right = itr;
            }
        } else if (l > seg_r)
            //      [ segment )
            //                  [ itr   )
            // on and after this situation, itr is always on the right side of segment,
            // no overlap anymore, break traverse...
            break;

        ++itr;
    }

    _unreassm_bytes += segment._len;

    map<size_t, Segment>::iterator itr_merged;
    // if has a left connected neighbor, merge segment with it
    // cout << "merge left eof is " << itr->second._eof << endl;
    if (itr_left != _segments.end()) {
        itr_left->second += segment;
        // if also has a right connected neighbor, merge segment with it
        // cout << "merge left eof is " << segment._eof << endl;
        // cout << "merge left eof is " << itr->second._eof << endl;
        if (itr_right != _segments.end()) {
            itr_left->second += itr_right->second;
            _segments.erase(itr_right);
        }
        itr_merged = itr_left;
        // if has a right connected neighbor, merge segment with it
    } else if (itr_right != _segments.end()) {
        segment += itr_right->second;
        _segments.insert(make_pair(seg_index, segment));
        _segments.erase(itr_right);
        itr_merged = _segments.find(seg_index);
        // if no connected neighbor
    } else {
        _segments.insert(make_pair(seg_index, segment));
        itr_merged = _segments.find(seg_index);
    }

    // the left most segment with index 0 is reassemblered.
    // so if merged segment is at the beginning and index is 0,
    // indicates that the reassemblered stream is updated in this merge operation,
    // write the string in itr_merged to _output stream and clear it.
    if (itr_merged == _segments.begin() && itr_merged->first == 0) {
        if (itr_merged->second._eof) {
            // cout << "end" << endl;
            _output.end_input();
        }
        _output.write(itr_merged->second._data);
        _unreassm_bytes -= itr_merged->second._data.size();
        itr_merged->second._data.clear();
    }

    return;
}

size_t StreamReassembler::unassembled_bytes() const { return _unreassm_bytes; }

bool StreamReassembler::empty() const { return _unreassm_bytes == 0; }
