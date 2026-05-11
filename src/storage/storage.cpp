#include "storage.h"
#include "serializer.h"
#include "pager/page_utils.h"
#include "memory_stream.h"
#include "meta_page.h"

#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
constexpr int METADATA_MAGIC = 0x4D444231; // MDB1
constexpr int METADATA_VERSION = 2;

int checkedSize(size_t value, const char* label) {
    if (value > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(std::string(label) + " is too large");
    }
    return static_cast<int>(value);
}

uint32_t checkedPageNum(size_t value) {
    if (value > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        throw std::runtime_error("Page id is too large");
    }
    return static_cast<uint32_t>(value);
}
}

void Storage::openDatabase(const std::string& path) {
    dbPath = path;
    schemas.clear();
    tablePages.clear();
    indexes.clear();

    pager.open(path);

    if (pager.getPageCount() == 0) {
        initNewDatabase();
    } else {
        loadMetadata();
    }
}

void Storage::initNewDatabase() {
    size_t pageNum = pager.allocatePage();
    if (pageNum != META_PAGE) {
        throw std::runtime_error("Metadata page allocation failed");
    }

    saveMetadata();
}

void Storage::saveMetadata() {
    auto& page = pager.getPage(META_PAGE);

    std::vector<char> buffer(PAGE_SIZE, 0);
    MemoryBuffer buf(buffer.data(), buffer.size());
    std::ostream out(&buf);

    Serializer::writeInt(out, METADATA_MAGIC);
    Serializer::writeInt(out, METADATA_VERSION);

    Serializer::writeInt(out, checkedSize(schemas.size(), "Schema count"));
    for (auto& [name, schema] : schemas) {
        (void)name;

        std::stringstream ss;
        Serializer::writeSchema(ss, schema);
        std::string data = ss.str();

        Serializer::writeInt(out, checkedSize(data.size(), "Schema metadata"));
        out.write(data.data(), data.size());
    }

    Serializer::writeInt(out, checkedSize(tablePages.size(), "Table count"));
    for (auto& [table, pages] : tablePages) {
        Serializer::writeString(out, table);
        Serializer::writeInt(out, checkedSize(pages.size(), "Page count"));

        for (auto p : pages) {
            Serializer::writeInt(out, checkedSize(p, "Page id"));
        }
    }

    Serializer::writeInt(out, checkedSize(indexes.size(), "Index count"));
    for (const auto& index : indexes) {
        Serializer::writeString(out, index.tableName);
        Serializer::writeString(out, index.columnName);
        Serializer::writeInt(out, index.type);
        Serializer::writeInt(out, checkedSize(index.rootPage, "Index root page"));
    }

    if (!out || buf.bytesWritten() > PAGE_SIZE) {
        throw std::runtime_error("Metadata overflow");
    }

    memcpy(page.data(), buffer.data(), PAGE_SIZE);
}

void Storage::loadMetadata() {
    auto& page = pager.getPage(META_PAGE);

    schemas.clear();
    tablePages.clear();
    indexes.clear();

    MemoryBuffer buf(page.data(), page.size());
    std::istream in(&buf);

    int magic = Serializer::readInt(in);
    if (magic != METADATA_MAGIC) {
        // Compatibility with the older fixed MetaPage format. It did not store
        // column definitions, so old databases should be recreated for full use.
        MetaPage meta = getMeta(page);

        for (uint32_t i = 0; i < meta.numTables; i++) {
            const TableMeta& t = meta.tables[i];

            std::string tableName(t.tableName);
            Schema schema;
            schema.tableName = tableName;
            schemas[tableName] = schema;

            std::vector<size_t> pages;
            for (uint32_t j = 0; j < t.numPages; j++) {
                pages.push_back(t.pageIds[j]);
            }

            tablePages[tableName] = pages;
        }

        return;
    }

    int version = Serializer::readInt(in);
    if (version < 1 || version > METADATA_VERSION) {
        throw std::runtime_error("Unsupported metadata version");
    }

    int schemaCount = Serializer::readInt(in);
    if (schemaCount < 0) {
        throw std::runtime_error("Invalid schema count");
    }

    for (int i = 0; i < schemaCount; i++) {
        int schemaSize = Serializer::readInt(in);
        if (schemaSize < 0 || schemaSize > static_cast<int>(PAGE_SIZE)) {
            throw std::runtime_error("Invalid schema metadata size");
        }

        std::string data(schemaSize, '\0');
        in.read(&data[0], schemaSize);

        std::stringstream ss(data);
        Schema schema = Serializer::readSchema(ss);
        schemas[schema.tableName] = schema;
    }

    int tableCount = Serializer::readInt(in);
    if (tableCount < 0) {
        throw std::runtime_error("Invalid table count");
    }

    for (int i = 0; i < tableCount; i++) {
        std::string tableName = Serializer::readString(in);
        int pageCount = Serializer::readInt(in);
        if (pageCount < 0) {
            throw std::runtime_error("Invalid table page count");
        }

        std::vector<size_t> pages;
        for (int j = 0; j < pageCount; j++) {
            int pageId = Serializer::readInt(in);
            if (pageId < 0) {
                throw std::runtime_error("Invalid table page id");
            }
            pages.push_back(static_cast<size_t>(pageId));
        }

        tablePages[tableName] = pages;
    }

    if (version >= 2) {
        int indexCount = Serializer::readInt(in);
        if (indexCount < 0) {
            throw std::runtime_error("Invalid index count");
        }

        for (int i = 0; i < indexCount; i++) {
            StoredIndex index;
            index.tableName = Serializer::readString(in);
            index.columnName = Serializer::readString(in);
            index.type = Serializer::readInt(in);

            int rootPage = Serializer::readInt(in);
            if (rootPage < 0) {
                throw std::runtime_error("Invalid index root page");
            }
            index.rootPage = static_cast<size_t>(rootPage);

            indexes.push_back(index);
        }
    }
}

void Storage::commit() {
    saveMetadata();
    pager.flushAll();
}

void Storage::saveSchema(const Schema& schema) {
    schemas[schema.tableName] = schema;
    tablePages[schema.tableName] = {};
    saveMetadata();
}

void Storage::saveIndex(const StoredIndex& index) {
    for (auto& existing : indexes) {
        if (existing.tableName == index.tableName &&
            existing.columnName == index.columnName) {
            existing = index;
            saveMetadata();
            return;
        }
    }

    indexes.push_back(index);
    saveMetadata();
}

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

    tablePages[tableName].push_back(pageNum);
    saveMetadata();

    return pageNum;
}

RID Storage::insertRow(const std::string& tableName, const Row& row) {
    validateTable(tableName);

    if (row.size() != schemas[tableName].columns.size()) {
        throw std::runtime_error("Invalid row size");
    }

    size_t pageNum = getLastDataPage(tableName);
    auto& page = pager.getPage(pageNum);

    PageHeader header = getHeader(page);

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
            memcpy(page.data() + slot.offset, temp.data(), rowSize);

            slot.size = rowSize;
            slot.isDeleted = 0;

            memcpy(page.data() + sizeof(PageHeader) + i * sizeof(Slot),
                   &slot, sizeof(Slot));

            return RID(checkedPageNum(pageNum), i);
        }
    }

    uint16_t slotSize = sizeof(Slot);
    uint16_t slotStart = sizeof(PageHeader) + header.numSlots * sizeof(Slot);

    if (header.freeSpaceOffset - slotStart < rowSize + slotSize) {
        pageNum = allocateNewDataPage(tableName);
        return insertRow(tableName, row);
    }

    header.freeSpaceOffset -= rowSize;
    uint16_t rowOffset = header.freeSpaceOffset;

    memcpy(page.data() + rowOffset, temp.data(), rowSize);

    Slot slot;
    slot.offset = rowOffset;
    slot.size = rowSize;
    slot.isDeleted = 0;

    uint16_t slotIndex = header.numSlots;

    memcpy(page.data() + sizeof(PageHeader) + slotIndex * sizeof(Slot),
           &slot, sizeof(Slot));

    header.numSlots++;

    setHeader(page, header);

    return RID(checkedPageNum(pageNum), slotIndex);
}

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

            result.push_back({RID(checkedPageNum(p), i), row});
        }
    }

    return result;
}

void Storage::deleteRow(const RID& rid) {
    auto& page = pager.getPage(rid.pageNum);

    Slot slot;
    memcpy(&slot,
           page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           sizeof(Slot));

    slot.isDeleted = 1;

    memcpy(page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           &slot, sizeof(Slot));

}

RID Storage::updateRow(const RID& rid, const Row& newRow, const std::string& tableName) {
    validateTable(tableName);

    auto& page = pager.getPage(rid.pageNum);

    Slot slot;
    memcpy(&slot,
           page.data() + sizeof(PageHeader) + rid.slotIndex * sizeof(Slot),
           sizeof(Slot));

    if (slot.isDeleted) return rid;

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

        return rid;
    } else {
        deleteRow(rid);
        return insertRow(tableName, newRow);
    }
}

const std::unordered_map<std::string, Schema>& Storage::getSchemas() const {
    return schemas;
}

const std::vector<StoredIndex>& Storage::getIndexes() const {
    return indexes;
}

Pager* Storage::getPager() {
    return &pager;
}

void Storage::validateTable(const std::string& tableName) {
    if (!schemas.count(tableName)) {
        throw std::runtime_error("Table not found: " + tableName);
    }
}
