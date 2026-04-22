#pragma once
#include<string>
#include "parser/ast.h"

class CLI{
    private:
        void handleMetaCommand(const std::string &input);
        void executeQuery(const std::string &query);
        void printQuery(Query* q);
    
    public:
        void run();
};