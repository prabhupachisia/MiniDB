#include "storage.h"
#include "serializer.h"
#include "pager/page_utils.h"
#include "memory_stream_write.h"
#include "meta_page.h"
#include "memory_stream.h"
#include <iostream>
#include <cstring>
#include <sstream>

void Storage::openDatabase(const std::string& path) {
    dbPath = path;
    pager.open(path);

    if (pager.getPageCount() == 0) {
        initNewDatabase();
    } else {
        loadMetadata();
    }
}

// ---------- INIT ----------

void Storage::initNewDatabase() {
    size_t pageNum = pager.allocatePage(); // should be page 0
    auto& page = pager.getPage(pageNum);

    // initialize structured meta
    MetaPage meta{};
    meta.numTables = 0;
    meta.numIndexes = 0;

    setMeta(page, meta);

    pager.flush(pageNum);
}

void Storage::saveMetadata() {
    auto& page = pager.getPage(META_PAGE);

    std::vector<char> buffer(PAGE_SIZE, 0);

    size_t offset = 0;

    auto writeInt = [&](int x) {
        memcpy(buffer.data() + offset, &x, sizeof(int));
        offset += sizeof(int);
    };

    auto writeString = [&](const std::string& s) {
        int len = s.size();
        writeInt(len);
        memcpy(buffer.data() + offset, s.data(), len);
        offset += len;
    };

    // ---- header ----
    writeInt(0xDBDB);
    writeInt(1);

    // ---- schemas ----
    writeInt(schemas.size());

    for (auto& [name, schema] : schemas) {
        std::stringstream ss;
        Serializer::writeSchema(ss, schema);
        std::string data = ss.str();

        writeInt(data.size());
        memcpy(buffer.data() + offset, data.data(), data.size());
        offset += data.size();
    }

    // ---- tablePages ----
    writeInt(tablePages.size());

    for (auto& [table, pages] : tablePages) {
        writeString(table);
        writeInt(pages.size());

        for (auto p : pages)
            writeInt(p);
    }

    if (offset > PAGE_SIZE)
        throw std::runtime_error("Metadata overflow");

    memcpy(page.data(), buffer.data(), PAGE_SIZE);
    pager.flush(META_PAGE);
}

void Storage::loadMetadata() {
    auto& page = pager.getPage(META_PAGE);

    MetaPage meta = getMeta(page);

    schemas.clear();
    tablePages.clear();

    // ---------- LOAD TABLES ----------
    for (uint32_t i = 0; i < meta.numTables; i++) {
        const TableMeta& t = meta.tables[i];

        std::string tableName(t.tableName);

        // ⚠️ Schema reconstruction (simplified for now)
        // You still need your Serializer if you want full schema info
        Schema schema;
        schema.tableName = tableName;

        schemas[tableName] = schema;

        std::vector<size_t> pages;
        for (uint32_t j = 0; j < t.numPages; j++) {
            pages.push_back(t.pageIds[j]);
        }

        tablePages[tableName] = pages;
    }

    // ---------- NOTE ----------
    // Index metadata will be loaded inside IndexManager
}

// ---------- SCHEMA (TEMP STUB) ----------

void Storage::saveSchema(const Schema& schema) {
    // -------- in-memory --------
    schemas[schema.tableName] = schema;
    tablePages[schema.tableName] = {};

    // -------- meta page --------
    auto& page = pager.getPage(META_PAGE);
    MetaPage meta = getMeta(page);

    // check duplicate table
    for (uint32_t i = 0; i < meta.numTables; i++) {
        if (std::string(meta.tables[i].tableName) == schema.tableName) {
            throw std::runtime_error("Table already exists in meta");
        }
    }

    TableMeta entry{};
    
    // safe copy
    strncpy(entry.tableName, schema.tableName.c_str(), 31);
    entry.tableName[31] = '\0';

    entry.numPages = 0;

    // add to meta
    meta.tables[meta.numTables++] = entry;

    // persist
    setMeta(page, meta);
    pager.flush(META_PAGE);
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
    validateTable(tableName);
    const Schema& schema = schemas[tableName];

    if (row.size() != schemas[tableName].columns.size()) {
        throw std::runtime_error("Invalid row size");
    }

    size_t pageNum = getLastDataPage(tableName);
    auto& page = pager.getPage(pageNum);

    PageHeader header = getHeader(page);

    // ---- serialize row ----
    std::vector<char> temp(PAGE_SIZE);

    MemoryBuffer buf(temp.data(), temp.size());
    std::ostream out(&buf);

    Serializer::writeRow(out, row);

    uint16_t rowSize = static_cast<uint16_t>(buf.bytesWritten());

    for (uint16_t i = 0; i < header.numSlots; i++) {
        Slot slot;
        memcpy(&slot,
               page.data() + sizeof(PageHeader) + i * sizeof(Slot),
               sizeof(Slot));

        if (slot.isDeleted && slot.size >= rowSize) {
            // reuse slot
            memcpy(page.data() + slot.offset, temp.data(), rowSize);

            slot.size = rowSize;
            slot.isDeleted = 0;

            memcpy(page.data() + sizeof(PageHeader) + i * sizeof(Slot),
                   &slot, sizeof(Slot));

            pager.flush(pageNum);

            return RID(pageNum, i);
        }
    }

    uint16_t slotSize = sizeof(Slot);
    uint16_t slotStart = sizeof(PageHeader) + header.numSlots * sizeof(Slot);

    if (header.freeSpaceOffset - slotStart < rowSize + slotSize) {
        pageNum = allocateNewDataPage(tableName);
        return insertRow(tableName, row);
    }

    // write row at bottom
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

Row Storage::getRow(const RID& rid, const std::string& tableName) {
    validateTable(tableName);

    const Schema& schema = schemas[tableName];

    auto& page = pager.getPage(rid.pageNum);

    Slot slot;
    memcpy(&slot,
           page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           sizeof(Slot));

    if (slot.isDeleted) return {};

    char* rowPtr = page.data() + slot.offset;

    MemoryBuffer buf(rowPtr, slot.size);
    std::istream in(&buf);

    return Serializer::readRow(in, schema);
}


std::vector<std::pair<RID, Row>> Storage::readAllRows(const std::string& tableName) {
    std::vector<std::pair<RID, Row>> result;

    if (!schemas.count(tableName)) {
        throw std::runtime_error("Table not found");
    }

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
void Storage::updateRow(const RID& rid, const Row& newRow, const std::string& tableName) {

    validateTable(tableName);

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

    uint16_t newSize = static_cast<uint16_t>(buf.bytesWritten());

    if (newSize <= slot.size) {
        memcpy(page.data() + slot.offset, temp.data(), newSize);
        slot.size = newSize;

        memcpy(page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
               &slot, sizeof(Slot));

        pager.flush(rid.pageNum);
    } else {
        deleteRow(rid);
        insertRow(tableName, newRow);
    }
}


// ---------- ACCESS ----------

const std::unordered_map<std::string, Schema>& Storage::getSchemas() const {
    return schemas;
}

Pager* Storage::getPager() {
    return &pager;
}

//---------HELPER-------
void Storage::validateTable(const std::string& tableName) {
    if (!schemas.count(tableName)) {
        throw std::runtime_error("Table not found: " + tableName);
    }
}
