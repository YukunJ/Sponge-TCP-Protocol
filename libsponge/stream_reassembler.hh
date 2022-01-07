#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    std::unordered_map<uint64_t, char> map;
    ByteStream output;  //!< The reassembled in-order byte stream
    size_t capacity;    //!< The maximum number of bytes
    size_t nextToPush;  //!< The index of next char to be pushed into the stream
    size_t streamEOF;   //!< The last index to be pushed before finish
    bool EOFSeen;

    // functions:
    size_t remain_capacity() const {
        return capacity - output_not_read() - unassembled_bytes();
    }  //!< The remaining capacity we have for the reassembler
    size_t output_not_read() const { return output.buffer_size(); }  //!< The remaining unread char length in stream
    size_t first_unread() const { return nextToPush - output_not_read(); }  //!< The first unread index
    void try_output();  //!< Try to push all the in-order char in map into the output stream

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return output; }
    ByteStream &stream_out() { return output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    size_t window_size() const {
        return first_unread() + capacity - nextToPush;
    }  //!<  The differ between first unacceptable index and first unassembled, aka window span right boundary

    size_t first_unassembled() const { return nextToPush; }  //!< The first unassembled index

    size_t checkpoint() const { return nextToPush - 1; }  //!< The last reassembled byte index
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
