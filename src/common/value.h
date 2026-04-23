#pragma once

#include<string>
#include<variant>
#include<vector>
#include "types.h"

class Value{
    public:
        DataType type;
        std::variant<int,std::string> data;
        

        //Constructor
        Value(int val);
        Value(const std::string &val);

        //Getters
        int asInt() const;
        std::string asText() const;

        //Utility
        std::string toString() const;

        //(For Indexing)
        bool operator==(const Value& other) const;
        bool operator<(const Value& other) const;
};

using Row = std::vector<Value>;