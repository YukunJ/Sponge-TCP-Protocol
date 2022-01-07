#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t _capacity) : buffer(vector<char>(_capacity)), capacity(_capacity) {}

size_t ByteStream::write(const string &data) {
    size_t count = 0;
    for (size_t i = 0; i < data.size() && remaining_capacity(); ++i, ++count, ++totalWritten) {
        buffer[currWrite] = data[i];
        currWrite = (currWrite + 1) % capacity;
    }
    return count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string peek;
    size_t walker = currRead;
    size_t toPeek = min(len, buffer_size());
    while (toPeek--) {
        peek += buffer[walker];
        walker = (walker + 1) % capacity;
    }
    return peek;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t toPop = min(len, buffer_size());
    while (toPop--) {
        currRead = (currRead + 1) % capacity;
        ++totalRead;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string read;
    size_t toRead = min(len, buffer_size());
    while (toRead--) {
        read += buffer[currRead];
        currRead = (currRead + 1) % capacity;
        ++totalRead;
    }
    return read;
}

void ByteStream::end_input() { isInputEnded = true; }

bool ByteStream::input_ended() const { return isInputEnded; }

size_t ByteStream::buffer_size() const { return totalWritten - totalRead; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return (buffer_empty() && input_ended()) ? true : false; }

size_t ByteStream::bytes_written() const { return totalWritten; }

size_t ByteStream::bytes_read() const { return totalRead; }

size_t ByteStream::remaining_capacity() const { return capacity - totalWritten + totalRead; }
