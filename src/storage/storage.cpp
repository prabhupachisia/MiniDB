#include "storage.h"
#include "serializer.h"
#include <iostream>


void Storage::openDatabase(const std::string& path) {
    dbPath = path;

    dbFile.open(path, std::ios::in | std::ios::out | std::ios::binary);

    if (!dbFile.is_open()) {
        dbFile.open(path, std::ios::out | std::ios::binary);
        dbFile.close();

        dbFile.open(path, std::ios::in | std::ios::out | std::ios::binary);
    }

    dbFile.clear();
    dbFile.seekg(0, std::ios::end);
    std::streampos size = dbFile.tellg();

    if (size == 0) {
        initNewDatabase();
    } else {
        readHeader();
        loadSchemas();
    }
}

void Storage::initNewDatabase() {
    dbFile.seekp(0);

    dbFile.write("MINIDB", 6);
    Serializer::writeInt(dbFile, 1);

    Serializer::writeInt(dbFile, 0);

    int currentSize = 6 + 4 + 4;
    int padding = 64 - currentSize;

    std::string pad(padding, '\0');
    dbFile.write(pad.c_str(), pad.size());

    dbFile.flush();
}

// ------------------ HEADER ------------------

void Storage::readHeader() {
    dbFile.seekg(0);

    char magic[6];
    dbFile.read(magic, 6);

    if (std::string(magic, 6) != "MINIDB") {
        throw std::runtime_error("Invalid database file");
    }

    int version = Serializer::readInt(dbFile);

    if (version != 1) {
        throw std::runtime_error("Unsupported DB version");
    }
}

// ------------------ SCHEMA ------------------

void Storage::saveSchema(const Schema& schema) {
    dbFile.clear();

    dbFile.seekg(6 + 4);
    int schemaCount = Serializer::readInt(dbFile);

    dbFile.seekp(6 + 4);
    Serializer::writeInt(dbFile, schemaCount + 1);

    dbFile.seekp(0, std::ios::end);
    Serializer::writeSchema(dbFile, schema);

    schemas[schema.tableName] = schema;

    dbFile.flush();
}

void Storage::loadSchemas() {
    dbFile.clear();

    dbFile.seekg(6 + 4);
    int schemaCount = Serializer::readInt(dbFile);

    dbFile.seekg(64);

    for (int i = 0; i < schemaCount; i++) {
        Schema schema = Serializer::readSchema(dbFile);
        schemas[schema.tableName] = schema;
    }
}

// ------------------ INSERT ------------------

void Storage::insertRow(const std::string& tableName, const Row& row) {
    if (schemas.find(tableName) == schemas.end()) {
        throw std::runtime_error("Table not found: " + tableName);
    }

    if (row.size() != schemas[tableName].columns.size()) {
        throw std::runtime_error("Invalid row size");
    }

    dbFile.clear();
    dbFile.seekp(0, std::ios::end);

    std::cout << "[DEBUG] Writing row for table: " << tableName << "\n";

    Serializer::writeString(dbFile, tableName);

std::streampos sizePos = dbFile.tellp();
Serializer::writeInt(dbFile, 0);

std::streampos start = dbFile.tellp();

Serializer::writeRow(dbFile, row);

std::streampos end = dbFile.tellp();

int rowSize = static_cast<int>(end - start);

dbFile.seekp(sizePos);
Serializer::writeInt(dbFile, rowSize);

dbFile.seekp(end);
}

// ------------------ SELECT ------------------

std::vector<Row> Storage::readAllRows(const std::string& tableName) {
    std::vector<Row> result;

    if (schemas.find(tableName) == schemas.end()) {
        throw std::runtime_error("Table not found: " + tableName);
    }

    dbFile.clear();
    dbFile.seekg(6 + 4);
    int schemaCount = Serializer::readInt(dbFile);

    dbFile.seekg(64);

    // Skip schemas properly
    for (int i = 0; i < schemaCount; i++) {
        Serializer::readSchema(dbFile);
    }

    std::cout << "[DEBUG] Reading rows for table: " << tableName << "\n";

    // Read rows
    while (dbFile.peek() != EOF) {
    std::string table;

    try {
        table = Serializer::readString(dbFile);
    } catch (...) {
        break;
    }

    if (schemas.find(table) == schemas.end()) {
        break;
    }

    int rowSize = Serializer::readInt(dbFile);

    std::streampos rowStart = dbFile.tellg();

    Row row = Serializer::readRow(dbFile, schemas[table]);

    dbFile.seekg(rowStart + static_cast<std::streamoff>(rowSize));

    if (table == tableName) {
        result.push_back(row);
    }
}

    return result;
}
// ------------------ GET ------------------

const std::unordered_map<std::string, Schema>& Storage::getSchemas() const {
    return schemas;
}