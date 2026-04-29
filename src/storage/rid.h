#pragma once
#include <cstdint>

struct RID {
    uint32_t pageNum;
    uint16_t slotIndex;

    RID(uint32_t p = 0, uint16_t s = 0) : pageNum(p), slotIndex(s) {}
};