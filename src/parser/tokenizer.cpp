#include "tokenizer.h"
#include<cctype>
#include<algorithm>
#include<stdexcept>

Tokenizer::Tokenizer(const std::string &input)
    : input(input), pos(0){}

char Tokenizer::peek(){
    if(isAtEnd()) return '\0';
    return input[pos];
}

char Tokenizer::advance(){
    if(isAtEnd()) return '\0';
    return input[pos++];
}

bool Tokenizer::isAtEnd(){
    return pos>=input.length();
}

void Tokenizer::skipWhitespace(){
    while(!isAtEnd() && std::isspace(peek())){
        advance();
    }
}

Token Tokenizer:: readIdentifier(){
    std::string value;

    while(!isAtEnd() && (std::isalnum(peek()) || peek()=='_')){
        value+=advance();
    }

    //convert to upper case for string matching
    std::string upper = value;
    std::transform(upper.begin(),upper.end(),upper.begin(),::toupper);

    if(upper=="CREATE") return {TokenType::CREATE, value};
    else if(upper=="TABLE") return {TokenType::TABLE, value};
    else if(upper=="INSERT") return {TokenType::INSERT,value};
    else if(upper=="INTO") return {TokenType::INTO,value};
    else if(upper=="VALUES") return {TokenType::VALUES,value};
    else if(upper=="SELECT") return {TokenType::SELECT,value};
    else if(upper=="FROM") return {TokenType::FROM,value};
    else if(upper=="WHERE") return {TokenType::WHERE,value};
    else if(upper=="DELETE") return {TokenType::DELETE,value};
    else if(upper=="PRIMARY") return {TokenType::PRIMARY, value};
    else if(upper=="KEY") return {TokenType::KEY, value};
    else if(upper=="INDEX") return {TokenType::INDEX, value};
    else if (upper == "USE") return {TokenType::USE, value};

    return {TokenType::IDENTIFIER,value};
}

Token Tokenizer::readNumber(){
    std::string value;
    while(!isAtEnd() && std::isdigit(peek())){
        value+=advance();
    }

    return {TokenType::NUMBER,value};
}

Token Tokenizer::readString(){
    advance(); //skip opening '

    std::string value;

    while(!isAtEnd() && peek()!='\''){
        value+=advance();
    }

    if(isAtEnd()){
        throw std::runtime_error("Unterminated string literal");
    }

    advance();

    return {TokenType::STRING,value};
}

//Main function

std:: vector<Token> Tokenizer::tokenize(){
    std::vector<Token> tokens;

    while(!isAtEnd()){
        skipWhitespace();

        if(isAtEnd()) break;

        char c = peek();

        //identifies / keyword
        if(std::isalpha(c)){
            tokens.push_back(readIdentifier());
        }
        //Number
        else if(std::isdigit(c)){
            tokens.push_back(readNumber());
        }
        //Symbols & Special cases
        else{
            switch(c){
                case '(':
                    tokens.push_back({TokenType::LPAREN,"("});
                    advance();
                    break;
                case ')':
                    tokens.push_back({TokenType::RPAREN,")"});
                    advance();
                    break;
                case ',':
                    tokens.push_back({TokenType::COMMA,","});
                    advance();
                    break;
                case ';':
                    tokens.push_back({TokenType::SEMICOLON,";"});
                    advance();
                    break;
                case '*':
                    tokens.push_back({TokenType::STAR,"*"});
                    advance();
                    break;
                case '=':
                    tokens.push_back({TokenType::EQUAL,"="});
                    advance();
                    break;
                case '\'':
                    tokens.push_back(readString());
                    break;
                default:
                    throw std::runtime_error(
                        std::string("Unexpected Character: ") + c
                    );
            }
        }
    }

    //end of file
    tokens.push_back({TokenType::END_OF_FILE,""});
    return tokens;
}