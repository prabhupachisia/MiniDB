#include "config.h"

std::string Config::db_path = "";

void Config::set_db_path(const std::string &path){
    db_path=path;
}

std::string Config::get_db_path(){
    return db_path;
}

bool Config::has_db(){
    return !db_path.empty();
}