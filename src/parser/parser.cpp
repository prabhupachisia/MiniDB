#include "parser.h"
#include "tokenizer.h"
#include <stdexcept>

class Parser {
public:
    Parser(const std::vector<Token>& tokens)
        : tokens(tokens), pos(0) {}

    std::unique_ptr<Query> parse() {
        if (match(TokenType::CREATE)) return parseCreate();
        if (match(TokenType::INSERT)) return parseInsert();
        if (match(TokenType::SELECT)) return parseSelect();
        if (match(TokenType::DELETE)) return parseDelete();
        if (match(TokenType::UPDATE)) return parseUpdate();
        if (match(TokenType::USE)) return parseUse();
        if (match(TokenType::COMMIT)) return parseCommit();
        if (match(TokenType::DESCRIBE) || match(TokenType::DESC)) return parseDescribe();

        throw std::runtime_error("Unknown query type");
    }

private:
    std::vector<Token> tokens;
    size_t pos;

    // HELPER FUNCTIONS
    
    Token peek(){
        return tokens[pos];   
    }

    Token advance(){
        return tokens[pos++];
    }

    bool match(TokenType type){
        if(peek().type==type){
            advance();
            return true;
        }
        return false;
    }

    Token consume(TokenType type){
        if(peek().type==type){
            return advance();
        }

        throw std::runtime_error("Unexpected token");
    }

    Token consumeValue(const std::string& context) {
        Token val = advance();

        if (val.type != TokenType::NUMBER &&
            val.type != TokenType::STRING &&
            val.type != TokenType::IDENTIFIER) {
            throw std::runtime_error("Invalid value in " + context);
        }

        return val;
    }

    // QUERY PARSERS

    // Create Parser
    std::unique_ptr<Query> parseCreate() {
        consume(TokenType::TABLE);

        auto query = std::make_unique<CreateQuery>();

        query->table_name = consume(TokenType::IDENTIFIER).value;

        consume(TokenType::LPAREN);

        // We loop until RPAREN
        while (true) {
            // ---- INDEX definition ----
            if (match(TokenType::INDEX)) {
                Index idx;

                // optional index name
                if (peek().type == TokenType::IDENTIFIER) {
                    idx.name = advance().value;
                }

                consume(TokenType::LPAREN);
                idx.column = consume(TokenType::IDENTIFIER).value;
                consume(TokenType::RPAREN);

                query->indexes.push_back(idx);
            }
            // ---- COLUMN definition ----
            else {
                ColumnDef col;

                col.name = consume(TokenType::IDENTIFIER).value;
                col.type = consume(TokenType::IDENTIFIER).value;

                // PRIMARY KEY
                if (match(TokenType::PRIMARY)) {
                    consume(TokenType::KEY);
                    col.is_primary = true;
                }

                query->columns.push_back(col);
            }

            // Stop or continue
            if (match(TokenType::COMMA)) {
                continue;
            } else {
                break;
            }
        }

        consume(TokenType::RPAREN);

        return query;
    }

    std::unique_ptr<Query> parseInsert() {
        consume(TokenType::INTO);   // 🔥 IMPORTANT FIX

        auto query = std::make_unique<InsertQuery>();

        query->table_name = consume(TokenType::IDENTIFIER).value;

        consume(TokenType::VALUES);
        consume(TokenType::LPAREN);

        // First value
        Token val = consumeValue("INSERT");
        query->values.push_back(val.value);

    // Remaining values
        while (match(TokenType::COMMA)) {
            Token val = consumeValue("INSERT");
            query->values.push_back(val.value);
        }

        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);

        return query;
    }

    //Select Query
    std::unique_ptr<Query> parseSelect(){
        auto query = std::make_unique<SelectQuery>();

        if(match(TokenType::STAR)){
            query->columns.push_back("*");
        }else {
            do{
                query->columns.push_back(consume(TokenType::IDENTIFIER).value);
            }while(match(TokenType::COMMA));
        }

        consume(TokenType::FROM);

        query->table_name=consume(TokenType::IDENTIFIER).value;

        if(match(TokenType::WHERE)){
            query->has_where = true;
            query->where =parseCondition();
        }

        return query;
    }

    //Delete Query
    std::unique_ptr<Query> parseDelete(){
        consume(TokenType::FROM);

        auto query= std::make_unique<DeleteQuery>();

        query->table_name = consume(TokenType::IDENTIFIER).value;

        if(match(TokenType::WHERE)){
            query->has_where=true;
            query->where = parseCondition();
        }

        return query;
    }

    std::unique_ptr<Query> parseUpdate(){
        auto query = std::make_unique<UpdateQuery>();

        query->table_name = consume(TokenType::IDENTIFIER).value;

        consume(TokenType::SET);
        query->column = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::EQUAL);
        query->value = consumeValue("UPDATE").value;

        if(match(TokenType::WHERE)){
            query->has_where = true;
            query->where = parseCondition();
        }

        if (match(TokenType::SEMICOLON)) {
            return query;
        }

        return query;
    }

    // WHERE
    Condition parseCondition(){
        std::string column = consume(TokenType::IDENTIFIER).value;

        consume(TokenType::EQUAL);

        Token val = consumeValue("WHERE");

        return Condition(column, "=", val.value);
    }

    //Use
    std::unique_ptr<Query> parseUse() {
        auto query = std::make_unique<UseQuery>();

        if (peek().type == TokenType::STRING) {
            query->db_name = advance().value;
        } else if (peek().type == TokenType::IDENTIFIER) {
            query->db_name = advance().value;
        } else {
            throw std::runtime_error("Expected database path");
        }

        return query;
    }

    std::unique_ptr<Query> parseCommit() {
        auto query = std::make_unique<CommitQuery>();
        match(TokenType::SEMICOLON);
        return query;
    }

    std::unique_ptr<Query> parseDescribe() {
        auto query = std::make_unique<DescribeQuery>();
        query->table_name = consume(TokenType::IDENTIFIER).value;
        match(TokenType::SEMICOLON);
        return query;
    }
};


//ENTRY POINT
std::unique_ptr<Query> parse_query(const std::string& input) {
    Tokenizer tokenizer(input);
    auto tokens = tokenizer.tokenize();

    Parser parser(tokens);
    return parser.parse();
}
