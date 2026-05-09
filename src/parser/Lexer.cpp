#include "Lexer.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

Lexer::Lexer(const char* input) : pos_(0) {
    len_ = 0;
    while (input[len_]) ++len_;
    src_ = new char[len_ + 1];
    for (int i = 0; i <= len_; ++i) src_[i] = input[i];
}

Lexer::~Lexer() { delete[] src_; }

bool Lexer::atEnd() const { return pos_ >= len_; }

void Lexer::skipWhitespace() {
    while (pos_ < len_ && (src_[pos_] == ' ' || src_[pos_] == '\t' ||
           src_[pos_] == '\n' || src_[pos_] == '\r'))
        ++pos_;
}

Token Lexer::makeOp(TokenType t, const char* lex, int advance) {
    pos_ += advance;
    return Token(t, lex);
}

Token Lexer::readString() {
    ++pos_; // skip opening quote
    int start = pos_;
    while (pos_ < len_ && src_[pos_] != '"' && src_[pos_] != '\'') ++pos_;
    char buf[256];
    int slen = pos_ - start;
    if (slen > 255) slen = 255;
    for (int i = 0; i < slen; ++i) buf[i] = src_[start + i];
    buf[slen] = '\0';
    if (pos_ < len_) ++pos_; // skip closing quote
    Token t(TokenType::STRING_LIT, buf);
    return t;
}

Token Lexer::readNumber() {
    int start = pos_;
    bool isFloat = false;
    while (pos_ < len_ && (isdigit((unsigned char)src_[pos_]) || src_[pos_] == '.')) {
        if (src_[pos_] == '.') isFloat = true;
        ++pos_;
    }
    char buf[64];
    int nlen = pos_ - start;
    if (nlen > 63) nlen = 63;
    for (int i = 0; i < nlen; ++i) buf[i] = src_[start + i];
    buf[nlen] = '\0';
    Token t(isFloat ? TokenType::FLOAT_LIT : TokenType::INTEGER, buf);
    if (isFloat) t.floatVal = atof(buf);
    else         t.intVal   = atoi(buf);
    return t;
}

Token Lexer::readIdentOrKeyword() {
    int start = pos_;
    while (pos_ < len_ && (isalnum((unsigned char)src_[pos_]) || src_[pos_] == '_')) ++pos_;
    char buf[256];
    int ilen = pos_ - start;
    if (ilen > 255) ilen = 255;
    for (int i = 0; i < ilen; ++i) buf[i] = src_[start + i];
    buf[ilen] = '\0';

    // Case-insensitive keyword matching
    char upper[256];
    for (int i = 0; buf[i]; ++i)
        upper[i] = (char)toupper((unsigned char)buf[i]);
    upper[ilen] = '\0';

    if (strcmp(upper, "SELECT") == 0) return Token(TokenType::SELECT,  buf);
    if (strcmp(upper, "FROM")   == 0) return Token(TokenType::FROM,    buf);
    if (strcmp(upper, "WHERE")  == 0) return Token(TokenType::WHERE,   buf);
    if (strcmp(upper, "INSERT") == 0) return Token(TokenType::INSERT,  buf);
    if (strcmp(upper, "INTO")   == 0) return Token(TokenType::INTO,    buf);
    if (strcmp(upper, "VALUES") == 0) return Token(TokenType::VALUES,  buf);
    if (strcmp(upper, "UPDATE") == 0) return Token(TokenType::UPDATE,  buf);
    if (strcmp(upper, "SET")    == 0) return Token(TokenType::SET,     buf);
    if (strcmp(upper, "DELETE") == 0) return Token(TokenType::DELETE,  buf);
    if (strcmp(upper, "JOIN")   == 0) return Token(TokenType::JOIN,    buf);
    if (strcmp(upper, "ON")     == 0) return Token(TokenType::ON,      buf);
    if (strcmp(upper, "ORDER")  == 0) return Token(TokenType::ORDER,   buf);
    if (strcmp(upper, "BY")     == 0) return Token(TokenType::BY,      buf);
    if (strcmp(upper, "AND")    == 0) return Token(TokenType::AND,     buf);
    if (strcmp(upper, "OR")     == 0) return Token(TokenType::OR,      buf);
    if (strcmp(upper, "NOT")    == 0) return Token(TokenType::NOT,     buf);
    return Token(TokenType::IDENTIFIER, buf);
}

Token Lexer::nextToken() {
    skipWhitespace();
    if (pos_ >= len_) return Token(TokenType::END_OF_INPUT, "");

    char c  = src_[pos_];
    char c2 = (pos_ + 1 < len_) ? src_[pos_ + 1] : '\0';

    if (c == '"' || c == '\'') return readString();
    if (isdigit((unsigned char)c)) return readNumber();
    if (isalpha((unsigned char)c) || c == '_') return readIdentOrKeyword();

    switch (c) {
        case '(': return makeOp(TokenType::LPAREN,  "(", 1);
        case ')': return makeOp(TokenType::RPAREN,  ")", 1);
        case ',': return makeOp(TokenType::COMMA,   ",", 1);
        case ';': return makeOp(TokenType::SEMICOLON,";",1);
        case '.': return makeOp(TokenType::DOT,     ".", 1);
        case '+': return makeOp(TokenType::PLUS,    "+", 1);
        case '-': return makeOp(TokenType::MINUS,   "-", 1);
        case '*': return makeOp(TokenType::MUL,     "*", 1);
        case '/': return makeOp(TokenType::DIV,     "/", 1);
        case '%': return makeOp(TokenType::MOD,     "%", 1);
        case '=':
            if (c2 == '=') return makeOp(TokenType::EQ, "==", 2);
            return makeOp(TokenType::ASSIGN, "=", 1);
        case '!':
            if (c2 == '=') return makeOp(TokenType::NEQ, "!=", 2);
            break;
        case '<':
            if (c2 == '=') return makeOp(TokenType::LTE, "<=", 2);
            return makeOp(TokenType::LT, "<", 1);
        case '>':
            if (c2 == '=') return makeOp(TokenType::GTE, ">=", 2);
            return makeOp(TokenType::GT, ">", 1);
    }
    char unk[2] = {c, '\0'};
    ++pos_;
    return Token(TokenType::UNKNOWN, unk);
}

Token Lexer::peekToken() {
    int savedPos = pos_;
    Token t = nextToken();
    pos_ = savedPos;
    return t;
}

int Lexer::tokenize(Token* out, int maxOut) {
    pos_ = 0;
    int count = 0;
    while (count < maxOut) {
        out[count] = nextToken();
        if (out[count].type == TokenType::END_OF_INPUT) { ++count; break; }
        ++count;
    }
    return count;
}
