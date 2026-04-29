#include "serializer.h"


void Serializer::writeInt(std::ostream& out, int value) {
    out.write(reinterpret_cast<char*>(&value), sizeof(int));
}

int Serializer::readInt(std::istream& in) {
    int value;
    in.read(reinterpret_cast<char*>(&value), sizeof(int));
    return value;
}

void Serializer::writeString(std::ostream& out, const std::string& str) {
    writeInt(out, str.size());
    out.write(str.c_str(), str.size());
}

std::string Serializer::readString(std::istream& in) {
    int len = readInt(in);
    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}

// ------------------ VALUE ------------------

void Serializer::writeValue(std::ostream& out, const Value& val) {
    if (val.type == DataType::INT) {
        writeInt(out, std::get<int>(val.data));
    } else {
        writeString(out, std::get<std::string>(val.data));
    }
}

Value Serializer::readValue(std::istream& in, DataType type) {
    if (type == DataType::INT) {
        return Value(readInt(in));
    } else {
        return Value(readString(in));
    }
}

// ------------------ ROW ------------------

void Serializer::writeRow(std::ostream& out, const Row& row) {
    for (const auto& val : row) {
        writeValue(out, val);
    }
}

Row Serializer::readRow(std::istream& in, const Schema& schema) {
    Row row;
    for (const auto& col : schema.columns) {
        row.push_back(readValue(in, col.type));
    }
    return row;
}

// ------------------ SCHEMA ------------------

void Serializer::writeSchema(std::ostream& out, const Schema& schema) {
    writeString(out, schema.tableName);

    writeInt(out, schema.columns.size());

    for (const auto& col : schema.columns) {
        writeString(out, col.name);
        writeInt(out, static_cast<int>(col.type));
        writeInt(out, col.isPrimaryKey ? 1 : 0);
    }
}

Schema Serializer::readSchema(std::istream& in) {
    Schema schema;

    schema.tableName = readString(in);

    int colCount = readInt(in);

    for (int i = 0; i < colCount; i++) {
        Column col;
        col.name = readString(in);
        col.type = static_cast<DataType>(readInt(in));
        col.isPrimaryKey = readInt(in) == 1;

        schema.columns.push_back(col);
    }

    return schema;
}