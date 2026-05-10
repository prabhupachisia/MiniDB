#include "cli.h"
#include "parser/parser.h"
#include<iostream>
#include <filesystem>
#include <fstream>
#include "parser/tokenizer.h"

namespace fs = std::filesystem;


void CLI::run() {
    std::string input;
    std::string buffer;

    while (true) {
        std::cout << "minidb > ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input[0] == '.') {
            handleMetaCommand(input);
            if (input == ".exit") break;
            continue;
        }

        

        buffer += input + " ";

        if (!input.empty() && input.back() == ';') {
            executeQuery(buffer);
            buffer.clear();
        }
    }
}

void CLI::handleMetaCommand(const std::string& input){
    if(input==".exit"){
        std::cout<<"Bye!\n";
    }else if(input==".help"){
        std::cout<<"Command:\n.exit\n.help\n";
    }else{
        std::cout<<"Unknown command\n";
    }
}

void CLI::printQuery(Query* q) {
    switch (q->type) {

        case QueryType::CREATE: {
            auto* cq = static_cast<CreateQuery*>(q);

            std::cout << "CREATE TABLE: " << cq->table_name << "\n";

            std::cout << "Columns:\n";
            for (auto& col : cq->columns) {
                std::cout << "  - " << col.name << " " << col.type;
                if (col.is_primary) std::cout << " [PRIMARY KEY]";
                std::cout << "\n";
            }

            if (!cq->indexes.empty()) {
                std::cout << "Indexes:\n";
                for (auto& idx : cq->indexes) {
                    std::cout << "  - " << idx.name
                              << " (" << idx.column << ")\n";
                }
            }

            break;
        }

        case QueryType::INSERT: {
            auto* iq = static_cast<InsertQuery*>(q);

            std::cout << "INSERT INTO: " << iq->table_name << "\n";
            std::cout << "Values: ";
            for (auto& v : iq->values) {
                std::cout << v << " ";
            }
            std::cout << "\n";

            break;
        }

        case QueryType::SELECT: {
            auto* sq = static_cast<SelectQuery*>(q);

            std::cout << "SELECT FROM: " << sq->table_name << "\n";

            std::cout << "Columns: ";
            for (auto& c : sq->columns) {
                std::cout << c << " ";
            }
            std::cout << "\n";

            if (sq->has_where) {
                std::cout << "WHERE: "
                          << sq->where.column << " "
                          << sq->where.op << " "
                          << sq->where.value << "\n";
            }

            break;
        }

        case QueryType::DELETE: {
            auto* dq = static_cast<DeleteQuery*>(q);

            std::cout << "DELETE FROM: " << dq->table_name << "\n";

            if (dq->has_where) {
                std::cout << "WHERE: "
                          << dq->where.column << " "
                          << dq->where.op << " "
                          << dq->where.value << "\n";
            }

            break;
        }

        case QueryType::UPDATE: {
            auto* uq = static_cast<UpdateQuery*>(q);

            std::cout << "UPDATE TABLE: " << uq->table_name << "\n";
            std::cout << "SET: " << uq->column << " = " << uq->value << "\n";

            if (uq->has_where) {
                std::cout << "WHERE: "
                          << uq->where.column << " "
                          << uq->where.op << " "
                          << uq->where.value << "\n";
            }

            break;
        }

        case QueryType::USE: {
            auto* uq = static_cast<UseQuery*>(q);
            std::cout << "USE DATABASE: " << uq->db_name << "\n";
            break;
        }
    }
}

void CLI::executeQuery(const std::string& query) {
    std::cout << "[CLI] Query Received: " << query << std::endl;

    try {
        auto parsedQuery = parse_query(query);

        std::cout << "[CLI] Query parsed successfully.\n";

        printQuery(parsedQuery.get());

        

        if (parsedQuery->type == QueryType::USE) {
            auto* uq = static_cast<UseQuery*>(parsedQuery.get());

            std::string dbFile = uq->db_name;

            if (dbFile.find(".db") == std::string::npos) {
                dbFile += ".db";
            }

            if (!fs::exists(dbFile)) {
                std::cout << "[CLI] Database file not found. Creating...\n";

                std::ofstream file(dbFile, std::ios::binary);
                if (!file) {
                    std::cout << "[ERROR] Failed to create database file\n";
                    return;
                }
                file.close();
            }

            if (!fs::is_regular_file(dbFile)) {
                std::cout << "[ERROR] Invalid database file\n";
                return;
            }

            current_db_path = dbFile;
            db_selected = true;
            executor.setDatabase(dbFile);

            std::cout << "[CLI] Using database: " << dbFile << "\n";
            return;
        }

        if (!db_selected) {
            std::cout << "[ERROR] No database selected. Use USE <name>;\n";
            return;
        }
        executor.execute(*parsedQuery);
    } catch (const std::exception& e) {
        auto tokens = Tokenizer(query).tokenize();

        for (auto& t : tokens) {
            std::cout << (int)t.type << " : " << t.value << "\n";
        }
        std::cout << "[ERROR] " << e.what() << std::endl;
    }
}
