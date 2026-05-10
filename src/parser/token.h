#pragma once
#include <string>

enum class TokenType{

    //keywords
    CREATE, TABLE, INSERT, INTO, VALUES, SELECT,
    FROM, WHERE, DELETE, PRIMARY, KEY, INDEX, USE,
    UPDATE, SET,

    //Symbols
    LPAREN,     // (
    RPAREN,     // )
    COMMA,      // , 
    SEMICOLON,  // ;
    STAR,       // *
    EQUAL,      // =

    //Literals
    IDENTIFIER, //table name, column names
    NUMBER,     //123
    STRING,     //'abc'

    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
};
