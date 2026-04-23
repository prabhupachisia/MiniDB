#include "value.h"
#include <stdexcept>

// Constructors
Value::Value(int val) : type(DataType::INT), data(val) {}

Value::Value(const std::string& val) : type(DataType::TEXT), data(val) {}

// Getters
int Value::asInt() const {
    if (type != DataType::INT)
        throw std::runtime_error("Type mismatch: expected INT");
    return std::get<int>(data);
}

std::string Value::asText() const {
    if (type != DataType::TEXT)
        throw std::runtime_error("Type mismatch: expected TEXT");
    return std::get<std::string>(data);
}

// Convert to string (for printing / debugging)
std::string Value::toString() const {
    if (type == DataType::INT)
        return std::to_string(std::get<int>(data));
    return std::get<std::string>(data);
}

// Equality (needed later for WHERE / indexing)
bool Value::operator==(const Value& other) const {
    if (type != other.type) return false;

    if (type == DataType::INT)
        return std::get<int>(data) == std::get<int>(other.data);

    return std::get<std::string>(data) == std::get<std::string>(other.data);
}

// Less-than (needed for B+ tree indexing)
bool Value::operator<(const Value& other) const {
    if (type != other.type)
        throw std::runtime_error("Cannot compare different types");

    if (type == DataType::INT)
        return std::get<int>(data) < std::get<int>(other.data);

    return std::get<std::string>(data) < std::get<std::string>(other.data);
}