#pragma once

#include <string>
#include "executor/executor.h"

class CLI {
private:
    Executor executor;

    std::string current_db_path = "";
    bool db_selected = false;

public:
    void run();
    void handleMetaCommand(const std::string& input);
    void executeQuery(const std::string& query);
};
