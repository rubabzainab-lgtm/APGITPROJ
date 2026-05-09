#pragma once
#include "Token.h"
#include "Lexer.h"
#include "../common/Stack.h"
#include "../common/Logger.h"
#include "../catalog/Schema.h"
#include <cstring>
#include <cstdio>

// ParsedQuery holds the result of parsing a SQL-like command.
enum class QueryType { SELECT, INSERT, UPDATE, DELETE, UNKNOWN };

struct WhereCondition {
    char   column[64];
    TokenType op;
    char   strVal[256];
    double numVal;
    bool   isNumeric;
};

// Maximum join tables and conditions in one query
static const int MAX_JOIN_TABLES = 8;
static const int MAX_CONDITIONS  = 32;
static const int MAX_POSTFIX     = 256;

struct ParsedQuery {
    QueryType  type;
    char       tableName[64];
    char       joinTables[MAX_JOIN_TABLES][64];
    int        joinCount;
    bool       hasWhere;
    char       whereRaw[512];         // raw WHERE clause string
    Token      postfix[MAX_POSTFIX];  // postfix tokens of WHERE
    int        postfixLen;
    char       postfixStr[1024];      // human-readable postfix

    // For INSERT
    char       insertCols[MAX_COLUMNS][64];
    char       insertVals[MAX_COLUMNS][256];
    int        insertColCount;

    // For UPDATE
    char       updateCol[64];
    char       updateVal[256];
    bool       updateValIsNum;
    double     updateNumVal;
};

// ── Parser ────────────────────────────────────────────────────────────────────
class Parser {
public:
    // Parse a SQL-like command string. Returns true on success.
    bool parse(const char* sql, ParsedQuery& out);

    // Convert infix token array to postfix (Shunting-Yard).
    // Returns length of postfix array.
    int shuntingYard(const Token* infix, int infixLen,
                     Token* postfix, int maxPostfix,
                     char* postfixStr, int strMaxLen);

private:
    bool parseSelect(Lexer& lex, ParsedQuery& q);
    bool parseInsert(Lexer& lex, ParsedQuery& q);
    bool parseUpdate(Lexer& lex, ParsedQuery& q);
    bool parseDelete(Lexer& lex, ParsedQuery& q);
    bool parseWhere (Lexer& lex, ParsedQuery& q);
};
