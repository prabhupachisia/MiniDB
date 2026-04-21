#include <iostream>
#include "config/config.h"

int main(int argc, char* argv[]) {
    
    if (argc > 1) {
        Config::set_db_path(argv[1]);
    }

    if (Config::has_db()) {
        std::cout << "Database selected: " << Config::get_db_path() << "\n";
    } else {
        std::cout << "No database selected\n";
    }

    return 0;
}