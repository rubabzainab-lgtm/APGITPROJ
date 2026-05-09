#pragma once
#include <cstring>
#include <cstdio>

enum class TokenType {
    // Keywords
    SELECT, FROM, WHERE, INSERT, INTO, VALUES,
    UPDATE, SET, DELETE, JOIN, ON, ORDER, BY,
    AND, OR, NOT,
    // Comparison operators
    EQ,   // ==
    NEQ,  // !=
    LT,   // <
    GT,   // >
    LTE,  // <=
    GTE,  // >=
    // Arithmetic operators
    PLUS, MINUS, MUL, DIV, MOD,
    // Structural
    LPAREN, RPAREN, COMMA, SEMICOLON, DOT,
    ASSIGN,  // = (used in UPDATE SET col = val)
    // Literals
    INTEGER, FLOAT_LIT, STRING_LIT,
    // Identifier (column/table names)
    IDENTIFIER,
    // End of input
    END_OF_INPUT,
    UNKNOWN
};

struct Token {
    TokenType type;
    char      lexeme[256];
    int       intVal;
    double    floatVal;

    Token() : type(TokenType::UNKNOWN), intVal(0), floatVal(0.0) { lexeme[0] = '\0'; }

    Token(TokenType t, const char* lex) : type(t), intVal(0), floatVal(0.0) {
        snprintf(lexeme, 256, "%s", lex);
    }

    bool isOperator() const {
        return type == TokenType::EQ   || type == TokenType::NEQ ||
               type == TokenType::LT   || type == TokenType::GT  ||
               type == TokenType::LTE  || type == TokenType::GTE ||
               type == TokenType::AND  || type == TokenType::OR  ||
               type == TokenType::PLUS || type == TokenType::MINUS ||
               type == TokenType::MUL  || type == TokenType::DIV  ||
               type == TokenType::MOD;
    }

    int precedence() const {
        switch (type) {
            case TokenType::OR:    return 1;
            case TokenType::AND:   return 2;
            case TokenType::NOT:   return 3;
            case TokenType::EQ:
            case TokenType::NEQ:
            case TokenType::LT:
            case TokenType::GT:
            case TokenType::LTE:
            case TokenType::GTE:   return 4;
            case TokenType::PLUS:
            case TokenType::MINUS: return 5;
            case TokenType::MUL:
            case TokenType::DIV:
            case TokenType::MOD:   return 6;
            default:               return 0;
        }
    }

    bool isLeftAssoc() const { return type != TokenType::NOT; }

    const char* typeName() const {
        switch (type) {
            case TokenType::SELECT:    return "SELECT";
            case TokenType::WHERE:     return "WHERE";
            case TokenType::INSERT:    return "INSERT";
            case TokenType::UPDATE:    return "UPDATE";
            case TokenType::DELETE:    return "DELETE";
            case TokenType::JOIN:      return "JOIN";
            case TokenType::AND:       return "AND";
            case TokenType::OR:        return "OR";
            case TokenType::NOT:       return "NOT";
            case TokenType::EQ:        return "==";
            case TokenType::NEQ:       return "!=";
            case TokenType::LT:        return "<";
            case TokenType::GT:        return ">";
            case TokenType::LTE:       return "<=";
            case TokenType::GTE:       return ">=";
            case TokenType::PLUS:      return "+";
            case TokenType::MINUS:     return "-";
            case TokenType::MUL:       return "*";
            case TokenType::DIV:       return "/";
            case TokenType::MOD:       return "%";
            case TokenType::LPAREN:    return "(";
            case TokenType::RPAREN:    return ")";
            case TokenType::INTEGER:   return "INT";
            case TokenType::FLOAT_LIT: return "FLOAT";
            case TokenType::STRING_LIT:return "STRING";
            case TokenType::IDENTIFIER:return "IDENT";
            case TokenType::END_OF_INPUT: return "EOF";
            default:                   return "?";
        }
    }
};
