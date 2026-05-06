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
    int                  childCount;
    int                  childCapacity;
    int                  nextChild;
} SymbolTable;


SymbolTable* createSymbolTable(SymbolTable* parent);
void insertSymbol(SymbolTable* table, SymbolRecord* record);
SymbolRecord* lookupSymbol(SymbolTable* table, const char* name);
SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init);

// Returns (and advances past) the next child scope of `table` in the
// order they were created during semantic analysis.  Codegen calls this
// once every time it enters a block that introduces a new scope.
SymbolTable* getNextChildScope(SymbolTable* table);

// --- חתימות לפונקציות האנליזה המרכזיות (נכתוב בהמשך) ---
SymbolType inferType(ASTNode* node, SymbolTable* table);
SymbolTable* analyzeSemantic(ASTNode* root);
void printSymbolTable(SymbolTable* table, const char* scopeName);
void printFinalSymbolTables(SymbolTable* globalScope);
SymbolTable* getFuncScope(const char* funcName);

#endif