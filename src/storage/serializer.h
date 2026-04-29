#pragma once

#include <iostream>
#include "../common/value.h"
#include "../common/schema.h"

class Serializer {
public:
    // Primitive
    static void writeInt(std::ostream& out, int value);
    static int readInt(std::istream& in);

    static void writeString(std::ostream& out, const std::string& str);
    static std::string readString(std::istream& in);

    // Core
    static void writeValue(std::ostream& out, const Value& val);
    static Value readValue(std::istream& in, DataType type);

    static void writeRow(std::ostream& out, const Row& row);
    static Row readRow(std::istream& in, const Schema& schema);

    static void writeSchema(std::ostream& out, const Schema& schema);
    static Schema readSchema(std::istream& in);
};