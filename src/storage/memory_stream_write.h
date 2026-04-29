#pragma once
#include <streambuf>

class WriteBuffer : public std::streambuf {
public:
    WriteBuffer(char* data, size_t size) {
        setp(data, data + size);
    }
};