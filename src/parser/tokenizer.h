#pragma once
#include<vector>
#include<string>

#include "token.h"

class Tokenizer{
    public:
        Tokenizer(const std:: string& input);
        std::vector<Token> tokenize();
    private:
        std::string input;
        size_t pos;

        char peek();
        char advance();
        bool isAtEnd();

        void skipWhitespace();

        Token readIdentifier();
        Token readNumber();
        Token readString();
};
