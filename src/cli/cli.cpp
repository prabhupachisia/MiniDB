#include "cli.h"
#include "parser/parser.h"
#include<iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;


void CLI::run() {
    std::string input;
    std::string buffer;

    while (true) {
        std::cout << "MiniDB > ";
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
        std::cout
            << "MiniDB commands\n"
            << "  .help                                Show this help\n"
            << "  .commit                              Persist pending changes to disk\n"
            << "  .exit                                Exit without committing pending changes\n"
            << "\n"
            << "Persistence\n"
            << "  COMMIT; or .commit                    Write pending changes to the .db file\n"
            << "  .exit                                 Leaves uncommitted changes in memory only\n"
            << "\n"
            << "SQL syntax\n"
            << "  USE <database>;                       Open/create <database>.db\n"
            << "  CREATE TABLE <table> (<columns>);     Create a table\n"
            << "  INSERT INTO <table> VALUES (...);     Insert one row\n"
            << "  SELECT * FROM <table> [WHERE ...];    Read rows\n"
            << "  SELECT col1, col2 FROM <table>;       Read selected columns\n"
            << "  UPDATE <table> SET col = value [WHERE col = value];\n"
            << "  DELETE FROM <table> [WHERE col = value];\n"
            << "  DESCRIBE <table>; or DESC <table>;    Show columns, keys, and indexes\n"
            << "\n"
            << "Types and quoting\n"
            << "  INT values are numbers without quotes: 1, 42, 900\n"
            << "  TEXT values must use single quotes: 'Ada', 'hello world'\n"
            << "  Table and column names are not quoted: users, id, name\n"
            << "  WHERE follows the column type: id = 1, name = 'Ada'\n"
            << "\n"
            << "Indexes\n"
            << "  PRIMARY KEY creates a primary index on an INT column\n"
            << "  INDEX <index_name> (<column>) creates a secondary index\n"
            << "  Secondary indexes currently support INT columns only\n"
            << "  Indexed equality filters can be used by SELECT WHERE\n"
            << "\n"
            << "Examples\n"
            << "  USE lab;\n"
            << "  CREATE TABLE users (id INT PRIMARY KEY, age INT, name TEXT, INDEX age_idx (age));\n"
            << "  INSERT INTO users VALUES (1, 21, 'Ada');\n"
            << "  SELECT id, name FROM users WHERE age = 21;\n"
            << "  UPDATE users SET name = 'Lovelace' WHERE id = 1;\n"
            << "  DELETE FROM users WHERE id = 1;\n"
            << "  DESCRIBE users;\n"
            << "  COMMIT;\n";
    }else if(input==".commit"){
        if (!db_selected) {
            std::cout << "Error: no database selected. Use USE <name>;\n";
            return;
        }
        executor.commit();
        std::cout << "Changes committed.\n";
    }else{
        std::cout<<"Unknown command\n";
    }
}

void CLI::executeQuery(const std::string& query) {
    try {
        auto parsedQuery = parse_query(query);

        if (parsedQuery->type == QueryType::USE) {
            auto* uq = static_cast<UseQuery*>(parsedQuery.get());

            std::string dbFile = uq->db_name;

            if (dbFile.find(".db") == std::string::npos) {
                dbFile += ".db";
            }

            if (!fs::exists(dbFile)) {
                std::ofstream file(dbFile, std::ios::binary);
                if (!file) {
                    std::cout << "Error: failed to create database file.\n";
                    return;
                }
                file.close();
            }

            if (!fs::is_regular_file(dbFile)) {
                std::cout << "Error: invalid database file.\n";
                return;
            }

            current_db_path = dbFile;
            db_selected = true;
            executor.setDatabase(dbFile);

            std::cout << "Using database: " << dbFile << "\n";
            return;
        }

        if (!db_selected) {
            std::cout << "Error: no database selected. Use USE <name>;\n";
            return;
        }
        executor.execute(*parsedQuery);
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}
