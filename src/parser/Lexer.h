#pragma once
#include "Token.h"

// Tokenizes a SQL-like input string into a stream of Tokens.
class Lexer {
public:
    static const int MAX_TOKENS = 1024;

    explicit Lexer(const char* input);
    ~Lexer();

    // Returns next token (advances cursor)
    Token nextToken();
    // Peek at next token without advancing
    Token peekToken();
    bool  atEnd() const;

    // Tokenize entire input into array. Returns count.
    int tokenize(Token* out, int maxOut);

private:
    char* src_;
    int   pos_;
    int   len_;

    void skipWhitespace();
    Token readString();
    Token readNumber();
    Token readIdentOrKeyword();
    Token makeOp(TokenType t, const char* lex, int advance);
};
