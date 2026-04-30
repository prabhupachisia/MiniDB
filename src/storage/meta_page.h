#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

constexpr int MAX_TABLES = 20;
constexpr int MAX_INDEXES = 50;

// ---------- TABLE METADATA ----------
struct TableMeta {
    char tableName[32];
    uint32_t numPages;
    uint32_t pageIds[100]; // simple fixed limit
};

// ---------- INDEX METADATA ----------
struct IndexMeta {
    char tableName[32];
    char columnName[32];
    uint32_t rootPage;
    uint8_t type; // 0 = PRIMARY, 1 = SECONDARY
};

// ---------- ROOT META PAGE ----------
struct MetaPage {
    uint32_t numTables;
    TableMeta tables[MAX_TABLES];

    uint32_t numIndexes;
    IndexMeta indexes[MAX_INDEXES];
};

// ---------- HELPERS ----------
inline MetaPage getMeta(std::vector<char>& page) {
    MetaPage meta{};
    memcpy(&meta, page.data(), sizeof(MetaPage));
    return meta;
}

inline void setMeta(std::vector<char>& page, const MetaPage& meta) {
    memcpy(page.data(), &meta, sizeof(MetaPage));
}