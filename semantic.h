#ifndef SEMANTIC_H
#define SEMANTIC_H
#include <stdbool.h>
#include "ast.h"
#include "lexerH.h"

typedef enum {
    SCOPE_FILE_LOCAL,
    SCOPE_GLOBAL,
    SCOPE_GLOBAL_IMPLICIT,
    SCOPE_BLOCK_LOCAL
} ScopeType;

typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_FUNCTION
} SymbolType;

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

//open hashing
typedef struct HashEntry {
    SymbolRecord* record;
    struct HashEntry* next;
} HashEntry;

#define HASH_TABLE_SIZE 256

//symbol table, children are saved in order
#define MAX_CHILD_SCOPES 64
typedef struct SymbolTable {
    HashEntry* buckets[HASH_TABLE_SIZE];
    struct SymbolTable*  parent_table;
    struct SymbolTable** children;
    int childCount;
    int childCapacity;
    int nextChild;
} SymbolTable;


SymbolTable* createSymbolTable(SymbolTable* parent);
void insertSymbol(SymbolTable* table, SymbolRecord* record);
SymbolRecord* lookupSymbol(SymbolTable* table, const char* name);
SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init);
SymbolTable* getNextChildScope(SymbolTable* table);

SymbolType inferType(ASTNode* node, SymbolTable* table);
SymbolTable* analyzeSemantic(ASTNode* root);
void printSymbolTable(SymbolTable* table, const char* scopeName);
void printFinalSymbolTables(SymbolTable* globalScope);
SymbolTable* getFuncScope(const char* funcName);
static void analyzeSemanticBlock(ASTNode** nodes, int count, SymbolTable* table);
static void analyzeSemanticAssign(ASTNode* node, SymbolTable* table);
static void analyzeSemanticLocal(ASTNode* node, SymbolTable* table);
static void analyzeSemanticIf(ASTNode* node, SymbolTable* table);
static void analyzeSemanticLoop(ASTNode* node, SymbolTable* table);
static void analyzeSemanticFunction(ASTNode* node, SymbolTable* table);
static void analyzeSemanticFor(ASTNode* node, SymbolTable* table);
static void analyzeSemanticCall(ASTNode* node, SymbolTable* table);
static void analyzeSemanticReturn(ASTNode* node, SymbolTable* table);
static SymbolType checkTypeCompatibility(SymbolType left, TokenType op, SymbolType right, int line);

#endif