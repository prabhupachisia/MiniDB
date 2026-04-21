#pragma once
#include<string>


class Config{
public:
    static void set_db_path(const std::string &path); // to set the db location
        
    static std::string get_db_path(); //to get the db location

    static bool has_db(); //to check if database is connected

private:
    static std::string db_path;
};