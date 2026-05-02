#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <stdbool.h>

typedef enum {
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_THEN,
    TOKEN_KEYWORD_LOCAL,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_OPERATOR, 
    TOKEN_EOF 
} TokenType;


typedef struct {
    TokenType type;
    char* value;
    int line;
} Token;


typedef enum {
    TOKEN_KW_IF, TOKEN_KW_THEN, TOKEN_KW_ELSEIF, TOKEN_KW_ELSE, TOKEN_KW_END,
    TOKEN_KW_WHILE, TOKEN_KW_DO, TOKEN_KW_FOR, TOKEN_KW_REPEAT, TOKEN_KW_UNTIL,
    TOKEN_KW_FUNCTION, TOKEN_KW_RETURN, TOKEN_KW_LOCAL,
    TOKEN_KW_TRUE, TOKEN_KW_FALSE, TOKEN_KW_NIL,
    TOKEN_KW_AND, TOKEN_KW_OR, TOKEN_KW_NOT,
    TOKEN_OP_PLUS, TOKEN_OP_MINUS, TOKEN_OP_MUL, TOKEN_OP_DIV, TOKEN_OP_MOD, TOKEN_OP_POW,
    TOKEN_OP_EQ,
    TOKEN_OP_NEQ,
    TOKEN_OP_LT,
    TOKEN_OP_GT,
    TOKEN_OP_LTE,
    TOKEN_OP_GTE,
    TOKEN_OP_ASSIGN,
    TOKEN_OP_CONCAT,
    TOKEN_OP_LEN,
    TOKEN_PUNC_LPAREN, TOKEN_PUNC_RPAREN,
    TOKEN_PUNC_LBRACE, TOKEN_PUNC_RBRACE,
    TOKEN_PUNC_LBRACKET, TOKEN_PUNC_RBRACKET,
    TOKEN_PUNC_COMMA, TOKEN_PUNC_SEMI, TOKEN_PUNC_COLON,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef enum {
    AST_PROGRAM,
    AST_BLOCK,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_FUNCTION_DEF,
    AST_RETURN,
    AST_ASSIGN,
    AST_LOCAL_ASSIGN,
    AST_FUNCTION_CALL,
    AST_BINARY_OP,
    AST_NUMBER,
    AST_STRING,
    AST_IDENTIFIER,
    AST_NIL
} ASTNodeType;

typedef struct ASTNode ASTNode;

typedef struct {
    ASTNode** nodes;
    int count;
    int capacity;
} ASTNodeArray;

struct ASTNode {
    ASTNodeType type;
    char* value;
    ASTNodeArray* children;
};


typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_FUNCTION
} SymbolType;


typedef enum {
    SCOPE_FILE_LOCAL,
    SCOPE_GLOBAL,
    SCOPE_GLOBAL_IMPLICIT,
    SCOPE_BLOCK_LOCAL
} ScopeType;


typedef struct {
    SymbolType* param_types;
    int param_count;
} FunctionParams;

typedef struct {
    char* name;
    SymbolType type; 
    ScopeType scope;
    union {
        struct {
            bool is_initialized;
        } var_data;
        struct {
            SymbolType return_type;
            FunctionParams params;
        } func_data;
    } data;
} SymbolRecord;

typedef struct HashEntry {
    SymbolRecord* record;
    struct HashEntry* next;
} HashEntry;

#define HASH_TABLE_SIZE 256

typedef struct SymbolTable {
    HashEntry* buckets[HASH_TABLE_SIZE];
    struct SymbolTable* parent_table;
} SymbolTable;