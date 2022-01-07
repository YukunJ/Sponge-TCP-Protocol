#include "stream_reassembler.hh"

using namespace std;

//!< Try to push all the in-order char in map into the output stream
void StreamReassembler::try_output() {
    string toPush = "";
    while (map.find(nextToPush) != map.end()) {
        toPush += map[nextToPush];
        map.erase(nextToPush);
        if (EOFSeen && nextToPush == streamEOF) {
            output.end_input();
        }
        ++nextToPush;
    }
    if (!toPush.empty()) {
        output.write(toPush);
    }
}

StreamReassembler::StreamReassembler(const size_t _capacity)
    : map(), output(_capacity), capacity(_capacity), nextToPush(0), streamEOF(0), EOFSeen(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if (eof) {
        EOFSeen = true;
        streamEOF = index + data.size() - 1;
        if (data.size() == 0 && unassembled_bytes() == 0 && nextToPush == index) {
            // special treatment for empty string
            output.end_input();
        }
        try_output();
    }
    if ((nextToPush + capacity - 1 < index) || (index + data.size() - 1 < nextToPush)) {
        // for sure either too far in future or too old in history
        return;
    }
    size_t first_unread_index = first_unread();
    for (uint64_t i = index; i < index + data.size(); ++i) {
        // the accept range is [nextToPush, nextToPush + remaining_capacity() - 1]
        if (nextToPush <= i && i < first_unread_index + capacity) {
            // add this char to storage(not unassembled)
            map[i] = data[i - index];
            try_output();
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { return map.size(); }

bool StreamReassembler::empty() const { return map.empty(); }
