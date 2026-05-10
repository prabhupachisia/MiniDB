#pragma once
#include <streambuf>
#include <vector>

class MemoryBuffer : public std::streambuf {
public:
    MemoryBuffer(char* data, size_t size) {
        setg(data, data, data + size);
        setp(data, data + size);
    }

    size_t bytesWritten() const {
        return static_cast<size_t>(pptr() - pbase());
    }
};
