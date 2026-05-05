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

typedef enum {
    STATE_START,
    STATE_IN_ID,
    STATE_NUMBER,
    STATE_OPERATOR,
    STATE_STRING,
    STATE_ERROR
} LexerState;

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

typedef struct {
    const char* text;
    TokenType type;
} TokenDict;

// מילון מילות המפתח (Keywords)
static const TokenDict keyword_dict[] = {
    {"and",      TOKEN_KW_AND},
    {"do",       TOKEN_KW_DO},
    {"else",     TOKEN_KW_ELSE},
    {"elseif",   TOKEN_KW_ELSEIF},
    {"end",      TOKEN_KW_END},
    {"false",    TOKEN_KW_FALSE},
    {"for",      TOKEN_KW_FOR},
    {"function", TOKEN_KW_FUNCTION},
    {"if",       TOKEN_KW_IF},
    {"local",    TOKEN_KW_LOCAL},
    {"nil",      TOKEN_KW_NIL},
    {"not",      TOKEN_KW_NOT},
    {"or",       TOKEN_KW_OR},
    {"repeat",   TOKEN_KW_REPEAT},
    {"return",   TOKEN_KW_RETURN},
    {"then",     TOKEN_KW_THEN},
    {"true",     TOKEN_KW_TRUE},
    {"until",    TOKEN_KW_UNTIL},
    {"while",    TOKEN_KW_WHILE}
};
// מילון האופרטורים והסימנים (Operators & Punctuation)
static const TokenDict operator_dict[] = {
    {"==", TOKEN_OP_EQ},
    {"~=", TOKEN_OP_NEQ},
    {"<=", TOKEN_OP_LTE},
    {">=", TOKEN_OP_GTE},
    {"..", TOKEN_OP_CONCAT},
    {"+",  TOKEN_OP_PLUS},
    {"-",  TOKEN_OP_MINUS},
    {"*",  TOKEN_OP_MUL},
    {"/",  TOKEN_OP_DIV},
    {"%",  TOKEN_OP_MOD},
    {"<",  TOKEN_OP_LT},
    {">",  TOKEN_OP_GT},
    {"=",  TOKEN_OP_ASSIGN},
    {"(",  TOKEN_PUNC_LPAREN},
    {")",  TOKEN_PUNC_RPAREN},
    {"{",  TOKEN_PUNC_LBRACE},   // <-- הוספנו
    {"}",  TOKEN_PUNC_RBRACE},   // <-- הוספנו
    {"[",  TOKEN_PUNC_LBRACKET}, // <-- הוספנו
    {"]",  TOKEN_PUNC_RBRACKET}, // <-- הוספנו
    {";",  TOKEN_PUNC_SEMI},     // <-- הוספנו ליתר ביטחון
    {":",  TOKEN_PUNC_COLON},     // <-- הוספנו ליתר ביטחון
    {",",  TOKEN_PUNC_COMMA}
};

TokenList runLexer(const char* sourceCode);
void freeTokenList(TokenList* list);

#endif