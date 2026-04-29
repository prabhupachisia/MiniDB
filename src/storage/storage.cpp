#include "storage.h"
#include "serializer.h"
#include "pager/page_utils.h"
#include "memory_stream.h"
#include <iostream>
#include <cstring>

void Storage::openDatabase(const std::string& path) {
    dbPath = path;

    pager.open(path);

    if (pager.getPageCount() == 0) {
        initNewDatabase();
    } else {
        readHeader();
        loadSchemas();
    }
}

// ---------- INIT ----------

void Storage::initNewDatabase() {
    size_t pageNum = pager.allocatePage();
    auto& page = pager.getPage(pageNum);

    PageHeader header;
    header.numSlots = 0;
    header.freeSpaceOffset = PAGE_SIZE;

    setHeader(page, header);

    pager.flush(pageNum);
}

void Storage::readHeader() {
    if (pager.getPageCount() == 0) return;

    auto& page = pager.getPage(META_PAGE);

    PageHeader header = getHeader(page);

    // future use (schema, metadata)
}

// ---------- SCHEMA (TEMP STUB) ----------

void Storage::saveSchema(const Schema& schema) {
    schemas[schema.tableName] = schema;

    tablePages[schema.tableName] = {};
}

void Storage::loadSchemas() {
    // TODO: load from META_PAGE later
}

// ---------- PAGE HELPERS ----------

size_t Storage::getLastDataPage(const std::string& tableName) {
    auto& pages = tablePages[tableName];

    if (pages.empty()) {
        return allocateNewDataPage(tableName);
    }

    return pages.back();
}


size_t Storage::allocateNewDataPage(const std::string& tableName) {
    size_t pageNum = pager.allocatePage();
    auto& page = pager.getPage(pageNum);

    PageHeader header;
    header.numSlots = 0;
    header.freeSpaceOffset = PAGE_SIZE;

    setHeader(page, header);

    pager.flush(pageNum);

    tablePages[tableName].push_back(pageNum);

    return pageNum;
}

// ---------- INSERT ----------

RID Storage::insertRow(const std::string& tableName, const Row& row) {
    const Schema& schema = schemas[tableName];

    size_t pageNum = getLastDataPage(tableName);
    auto& page = pager.getPage(pageNum);

    PageHeader header = getHeader(page);

    std::vector<char> temp(PAGE_SIZE);
    MemoryBuffer buf(temp.data(), temp.size());
    std::ostream out(&buf);

    Serializer::writeRow(out, row);

    uint16_t rowSize = static_cast<uint16_t>(out.tellp());
    uint16_t slotSize = sizeof(Slot);

    uint16_t slotStart = sizeof(PageHeader) + header.numSlots * sizeof(Slot);

    if (header.freeSpaceOffset - slotStart < rowSize + slotSize) {
        pageNum = allocateNewDataPage(tableName);
        return insertRow(tableName, row);
    }

    // write row
    header.freeSpaceOffset -= rowSize;
    uint16_t rowOffset = header.freeSpaceOffset;

    memcpy(page.data() + rowOffset, temp.data(), rowSize);

    // create slot
    Slot slot;
    slot.offset = rowOffset;
    slot.size = rowSize;
    slot.isDeleted = 0;

    uint16_t slotIndex = header.numSlots;

    memcpy(page.data() + sizeof(PageHeader) + slotIndex * sizeof(Slot),
           &slot, sizeof(Slot));

    header.numSlots++;

    setHeader(page, header);
    pager.flush(pageNum);

    return RID(pageNum, slotIndex);
}

//----------READ-----------

Row Storage::getRow(const RID& rid) {
    auto& page = pager.getPage(rid.pageNum);

    Slot slot;
    memcpy(&slot,
           page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           sizeof(Slot));

    if (slot.isDeleted) return {};

    const Schema& schema = schemas.begin()->second; // temp (fix later per table)

    char* rowPtr = page.data() + slot.offset;

    MemoryBuffer buf(rowPtr, slot.size);
    std::istream in(&buf);

    return Serializer::readRow(in, schema);
}


std::vector<std::pair<RID, Row>> Storage::readAllRows(const std::string& tableName) {
    std::vector<std::pair<RID, Row>> result;

    const Schema& schema = schemas[tableName];

    for (size_t p : tablePages[tableName]) {
        auto& page = pager.getPage(p);

        PageHeader header = getHeader(page);

        for (uint16_t i = 0; i < header.numSlots; i++) {
            Slot slot;
            memcpy(&slot,
                   page.data() + sizeof(PageHeader) + i * sizeof(Slot),
                   sizeof(Slot));

            if (slot.isDeleted) continue;

            char* rowPtr = page.data() + slot.offset;

            MemoryBuffer buf(rowPtr, slot.size);
            std::istream in(&buf);

            Row row = Serializer::readRow(in, schema);

            result.push_back({RID(p, i), row});
        }
    }

    return result;
}


//-----DeleteRow---------

void Storage::deleteRow(const RID& rid) {
    auto& page = pager.getPage(rid.pageNum);

    Slot slot;
    memcpy(&slot,
           page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           sizeof(Slot));

    slot.isDeleted = 1;

    memcpy(page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           &slot, sizeof(Slot));

    pager.flush(rid.pageNum);
}

//-------------Update----------
void Storage::updateRow(const RID& rid, const Row& newRow) {
    auto& page = pager.getPage(rid.pageNum);

    Slot slot;
    memcpy(&slot,
           page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           sizeof(Slot));

    if (slot.isDeleted) return;

    std::vector<char> temp(PAGE_SIZE);
    MemoryBuffer buf(temp.data(), temp.size());
    std::ostream out(&buf);

    Serializer::writeRow(out, newRow);

    uint16_t newSize = static_cast<uint16_t>(out.tellp());

    if (newSize <= slot.size) {
        memcpy(page.data() + slot.offset, temp.data(), newSize);
        slot.size = newSize;

        memcpy(page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
               &slot, sizeof(Slot));

        pager.flush(rid.pageNum);
    } else {
        deleteRow(rid);
        insertRow("TODO_TABLE", newRow);
    }
}


// ---------- DEBUG ----------

void Storage::debugPager() {
    size_t pageNum = pager.allocatePage();
    auto& page = pager.getPage(pageNum);

    std::string msg = "pager working";
    memcpy(page.data(), msg.c_str(), msg.size());

    pager.flush(pageNum);
}



// ---------- ACCESS ----------

const std::unordered_map<std::string, Schema>& Storage::getSchemas() const {
    return schemas;
}