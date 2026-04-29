#pragma once

#include <vector>
#include <cstring>
#include <cstdint>

struct PageHeader {
    uint16_t numSlots;
    uint16_t freeSpaceOffset;
};


struct Slot {
    uint16_t offset; 
    uint16_t size; 
    uint8_t isDeleted;
};


inline PageHeader getHeader(std::vector<char>& page) {
    PageHeader header;
    memcpy(&header, page.data(), sizeof(PageHeader));
    return header;
}

inline void setHeader(std::vector<char>& page, const PageHeader& header) {
    memcpy(page.data(), &header, sizeof(PageHeader));
}