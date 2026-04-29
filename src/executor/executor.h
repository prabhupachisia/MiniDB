#pragma once

#include <string>
#include "../parser/ast.h"
#include "../storage/storage.h"

class Executor {
private:
    Storage storage;
    std::string currentDB;

public:
    void execute(const Query& query);

private:
    void executeUse(const UseQuery& query);
    void executeCreate(const CreateQuery& query);
    void executeInsert(const InsertQuery& query);
    void executeSelect(const SelectQuery& query);
    void executeDelete(const DeleteQuery& query);
};