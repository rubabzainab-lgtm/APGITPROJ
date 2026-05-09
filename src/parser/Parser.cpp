#include "Parser.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

bool Parser::parse(const char* sql, ParsedQuery& out) {
    out.type         = QueryType::UNKNOWN;
    out.joinCount    = 0;
    out.hasWhere     = false;
    out.postfixLen   = 0;
    out.insertColCount = 0;
    out.updateValIsNum = false;
    out.updateNumVal   = 0.0;
    out.tableName[0]   = '\0';
    out.whereRaw[0]    = '\0';
    out.postfixStr[0]  = '\0';
    out.updateCol[0]   = '\0';
    out.updateVal[0]   = '\0';

    Lexer lex(sql);
    Token first = lex.nextToken();

    if (first.type == TokenType::SELECT) {
        out.type = QueryType::SELECT;
        return parseSelect(lex, out);
    }
    if (first.type == TokenType::INSERT) {
        out.type = QueryType::INSERT;
        return parseInsert(lex, out);
    }
    if (first.type == TokenType::UPDATE) {
        out.type = QueryType::UPDATE;
        return parseUpdate(lex, out);
    }
    if (first.type == TokenType::DELETE) {
        out.type = QueryType::DELETE;
        return parseDelete(lex, out);
    }
    return false;
}

bool Parser::parseSelect(Lexer& lex, ParsedQuery& q) {
    // SELECT [*|cols] FROM table [JOIN table ...] [WHERE ...]
    Token t = lex.nextToken();
    // Skip columns (*, or identifiers until FROM)
    while (t.type != TokenType::FROM && t.type != TokenType::END_OF_INPUT)
        t = lex.nextToken();
    if (t.type == TokenType::END_OF_INPUT) return false;

    // Table name
    t = lex.nextToken();
    if (t.type != TokenType::IDENTIFIER) return false;
    snprintf(q.tableName, 64, "%s", t.lexeme);

    // Check for JOINs
    t = lex.nextToken();
    while (t.type == TokenType::JOIN) {
        t = lex.nextToken();
        if (t.type == TokenType::IDENTIFIER && q.joinCount < MAX_JOIN_TABLES) {
            snprintf(q.joinTables[q.joinCount++], 64, "%s", t.lexeme);
        }
        t = lex.nextToken();
        // Skip ON condition tokens until JOIN or WHERE or EOF
        while (t.type != TokenType::JOIN && t.type != TokenType::WHERE
               && t.type != TokenType::END_OF_INPUT && t.type != TokenType::SEMICOLON)
            t = lex.nextToken();
    }

    if (t.type == TokenType::WHERE) {
        return parseWhere(lex, q);
    }
    return true;
}

bool Parser::parseInsert(Lexer& lex, ParsedQuery& q) {
    // INSERT INTO table (col,...) VALUES (val,...)
    Token t = lex.nextToken();
    if (t.type != TokenType::INTO) return false;
    t = lex.nextToken();
    if (t.type != TokenType::IDENTIFIER) return false;
    snprintf(q.tableName, 64, "%s", t.lexeme);

    t = lex.nextToken();
    if (t.type == TokenType::LPAREN) {
        t = lex.nextToken();
        while (t.type != TokenType::RPAREN && t.type != TokenType::END_OF_INPUT) {
            if (t.type == TokenType::IDENTIFIER && q.insertColCount < MAX_COLUMNS) {
                snprintf(q.insertCols[q.insertColCount++], 64, "%s", t.lexeme);
            }
            t = lex.nextToken();
        }
        t = lex.nextToken(); // VALUES
    }
    // Skip VALUES keyword
    if (t.type == TokenType::VALUES) t = lex.nextToken();
    // Read values
    if (t.type == TokenType::LPAREN) {
        t = lex.nextToken();
        int vi = 0;
        while (t.type != TokenType::RPAREN && t.type != TokenType::END_OF_INPUT) {
            if (t.type != TokenType::COMMA && vi < MAX_COLUMNS) {
                snprintf(q.insertVals[vi++], 256, "%s", t.lexeme);
            }
            t = lex.nextToken();
        }
    }
    return true;
}

bool Parser::parseUpdate(Lexer& lex, ParsedQuery& q) {
    // UPDATE table SET col = val [WHERE ...]
    Token t = lex.nextToken();
    if (t.type != TokenType::IDENTIFIER) return false;
    snprintf(q.tableName, 64, "%s", t.lexeme);
    t = lex.nextToken(); // SET
    if (t.type != TokenType::SET) return false;
    t = lex.nextToken(); // col
    if (t.type != TokenType::IDENTIFIER) return false;
    snprintf(q.updateCol, 64, "%s", t.lexeme);
    t = lex.nextToken(); // =
    t = lex.nextToken(); // val
    snprintf(q.updateVal, 256, "%s", t.lexeme);
    if (t.type == TokenType::INTEGER || t.type == TokenType::FLOAT_LIT) {
        q.updateValIsNum = true;
        q.updateNumVal   = (t.type == TokenType::FLOAT_LIT) ? t.floatVal : (double)t.intVal;
    }
    t = lex.nextToken();
    if (t.type == TokenType::WHERE) return parseWhere(lex, q);
    return true;
}

bool Parser::parseDelete(Lexer& lex, ParsedQuery& q) {
    // DELETE FROM table [WHERE ...]
    Token t = lex.nextToken();
    if (t.type == TokenType::FROM) t = lex.nextToken();
    if (t.type != TokenType::IDENTIFIER) return false;
    snprintf(q.tableName, 64, "%s", t.lexeme);
    t = lex.nextToken();
    if (t.type == TokenType::WHERE) return parseWhere(lex, q);
    return true;
}

bool Parser::parseWhere(Lexer& lex, ParsedQuery& q) {
    q.hasWhere = true;

    // Collect WHERE tokens until end
    Token whereTokens[MAX_POSTFIX];
    int   wcount = 0;
    char  rawBuf[512] = {};
    int   rawLen = 0;

    Token t = lex.nextToken();
    while (t.type != TokenType::END_OF_INPUT &&
           t.type != TokenType::SEMICOLON &&
           t.type != TokenType::ORDER)
    {
        if (wcount < MAX_POSTFIX) whereTokens[wcount++] = t;
        // Build raw string
        if (rawLen < 510) {
            int ll = 0; while (t.lexeme[ll]) ++ll;
            if (rawLen + ll + 1 < 510) {
                for (int i = 0; i < ll; ++i) rawBuf[rawLen++] = t.lexeme[i];
                rawBuf[rawLen++] = ' ';
            }
        }
        t = lex.nextToken();
    }
    rawBuf[rawLen] = '\0';
    snprintf(q.whereRaw, 512, "%s", rawBuf);

    // Convert to postfix
    q.postfixLen = shuntingYard(whereTokens, wcount,
                                q.postfix, MAX_POSTFIX,
                                q.postfixStr, 1024);

    LOG_INFO("[LOG] Infix \"%s\" converted to Postfix \"%s\"",
             q.whereRaw, q.postfixStr);
    return true;
}

int Parser::shuntingYard(const Token* infix, int infixLen,
                          Token* postfix, int maxPostfix,
                          char* postfixStr, int strMaxLen) {
    Stack<Token> opStack(256);
    int outCount = 0;
    char strBuf[1024] = {};
    int  strLen = 0;

    auto appendToStr = [&](const char* word) {
        int wlen = 0; while (word[wlen]) ++wlen;
        if (strLen + wlen + 2 < strMaxLen - 1) {
            for (int i = 0; i < wlen; ++i) strBuf[strLen++] = word[i];
            strBuf[strLen++] = ' ';
        }
    };

    auto pushOut = [&](const Token& tk) {
        if (outCount < maxPostfix) {
            postfix[outCount++] = tk;
            appendToStr(tk.lexeme);
        }
    };

    for (int i = 0; i < infixLen; ++i) {
        const Token& tok = infix[i];

        if (tok.type == TokenType::INTEGER   ||
            tok.type == TokenType::FLOAT_LIT ||
            tok.type == TokenType::STRING_LIT||
            tok.type == TokenType::IDENTIFIER) {
            pushOut(tok);

        } else if (tok.type == TokenType::LPAREN) {
            opStack.push(tok);

        } else if (tok.type == TokenType::RPAREN) {
            while (!opStack.isEmpty() && opStack.peek().type != TokenType::LPAREN)
                pushOut(opStack.pop());
            if (!opStack.isEmpty()) opStack.pop(); // discard '('

        } else if (tok.isOperator()) {
            while (!opStack.isEmpty() &&
                   opStack.peek().type != TokenType::LPAREN &&
                   opStack.peek().isOperator() &&
                   ((tok.isLeftAssoc() && tok.precedence() <= opStack.peek().precedence()) ||
                    (!tok.isLeftAssoc() && tok.precedence() < opStack.peek().precedence()))) {
                pushOut(opStack.pop());
            }
            opStack.push(tok);
        }
    }
    while (!opStack.isEmpty()) pushOut(opStack.pop());

    strBuf[strLen] = '\0';
    snprintf(postfixStr, strMaxLen, "%s", strBuf);
    return outCount;
}
