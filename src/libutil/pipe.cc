#include "pipe.hh"
#include <cstring>
#include "serialise.hh"

namespace nix::pipe {

size_t BufferedPipe::write(const unsigned char* data, size_t len) {
    buffer.insert(buffer.end(), len, *data);
    return len;
}

size_t BufferedPipe::read(size_t max_size, unsigned char* data) {
    auto actual_size = std::max(buffer.size(), max_size);
    std::memcpy(data, buffer.data(), actual_size);
    buffer.erase(buffer.begin(), buffer.begin() + actual_size - 1);
    return actual_size;
}

bool BufferedPipe::isEmpty() {
    return buffer.empty();
}

void BufferedPipe::pull(Pipe & source) {
    size_t last_read = 0;
    unsigned char tmp[256];
    do {
        last_read = source.read(256, tmp);
        write(tmp, last_read);
    } while (last_read > 0);
}

void BufferedPipe::push(Pipe & sink) {
    auto pushed_bytes = sink.write(buffer.data(), buffer.size());
    buffer.erase(buffer.begin(), buffer.begin() + pushed_bytes);
}

size_t CombinedPipe::read(size_t max_size, unsigned char* dest) {
    if (max_size == 0) return 0;
    auto done = second.read(max_size, dest);
    buffer.pull(first);
    if (buffer.isEmpty()) return done;
    buffer.push(second);
    return done + read(max_size-done, dest+done);
}

size_t CombinedPipe::write(const unsigned char* src, size_t len) {
    return first.write(src, len);
}

size_t PipedSource::read(unsigned char* data, size_t len) {

}

}
