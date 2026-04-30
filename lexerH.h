#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
typedef enum {
    TOKEN_KW_IF, TOKEN_KW_THEN, TOKEN_KW_ELSEIF, TOKEN_KW_ELSE, TOKEN_KW_END,
    TOKEN_KW_WHILE, TOKEN_KW_DO, TOKEN_KW_FOR, TOKEN_KW_REPEAT, TOKEN_KW_UNTIL,
    TOKEN_KW_FUNCTION, TOKEN_KW_RETURN, TOKEN_KW_LOCAL,
    TOKEN_KW_TRUE, TOKEN_KW_FALSE, TOKEN_KW_NIL,
    TOKEN_KW_AND, TOKEN_KW_OR, TOKEN_KW_NOT,
    
    TOKEN_OP_PLUS, TOKEN_OP_MINUS, TOKEN_OP_MUL, TOKEN_OP_DIV, TOKEN_OP_MOD, TOKEN_OP_POW,
    TOKEN_OP_EQ, TOKEN_OP_NEQ, TOKEN_OP_LT, TOKEN_OP_GT, TOKEN_OP_LTE, TOKEN_OP_GTE,
    TOKEN_OP_ASSIGN, TOKEN_OP_CONCAT, TOKEN_OP_LEN,
    
    TOKEN_PUNC_LPAREN, TOKEN_PUNC_RPAREN,
    TOKEN_PUNC_LBRACE, TOKEN_PUNC_RBRACE,
    TOKEN_PUNC_LBRACKET, TOKEN_PUNC_RBRACKET,
    TOKEN_PUNC_COMMA, TOKEN_PUNC_SEMI, TOKEN_PUNC_COLON,
    
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
} Token;

typedef struct {
    Token* tokens;
    int count;
    int capacity;
} TokenList;

TokenList runLexer(const char* sourceCode);
void freeTokenList(TokenList* list);
void printTokens(const TokenList* list);

#endif