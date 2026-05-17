#ifndef SEMANTIC_H
#define SEMANTIC_H
#include <stdbool.h>
#include "ast.h"
#include "lexerH.h"

//defines the visibility/lifespan of a variable or function
typedef enum {
    SCOPE_FILE_LOCAL, //local variables at the root
    SCOPE_GLOBAL, //global variables
    SCOPE_GLOBAL_IMPLICIT, //global variables in blocks
    SCOPE_BLOCK_LOCAL //local vars in blocks
} ScopeType;

//defines the data types supported by the compiler
typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_FUNCTION
} SymbolType;

//dynamic arrya that holds the param types of a function
typedef struct {
    SymbolType* param_types;
    int param_count;
} FunctionParams;

//represents a single identifier (variable or function) in the symbol table
typedef struct {
    char* name;
    SymbolType type;
    ScopeType scope;
    //a symbol is EITHER a variable OR a function, never both
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

//a node for the linked list used to handle hash collisions (open hashing)
typedef struct HashEntry {
    SymbolRecord* record;
    struct HashEntry* next;
} HashEntry;


#define HASH_TABLE_SIZE 256

//the symbol table for a specific scope
typedef struct SymbolTable {
    HashEntry* buckets[HASH_TABLE_SIZE];//the actual hash map storing the symbols
    struct SymbolTable* parent_table;//pointer to the outer scope
    struct SymbolTable** children;//pointers to inner scopes/scope
    int childCount; //number of inner scopes
    int childCapacity; //max inner scopes
    int nextChild; //counter used during code generation to traverse scopes in order
} SymbolTable;


//helper functions
SymbolTable* createSymbolTable(SymbolTable* parent);
void insertSymbol(SymbolTable* table, SymbolRecord* record);
SymbolRecord* lookupSymbol(SymbolTable* table, const char* name);
SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init);
SymbolTable* getNextChildScope(SymbolTable* table);
void freeSymbolTable(SymbolTable* table);
void printSymbolTable(SymbolTable* table, const char* scopeName);
void printFinalSymbolTables(SymbolTable* globalScope);
SymbolTable* getFuncScope(const char* funcName);

//the main sementic part
SymbolTable* analyzeSemantic(ASTNode* root);

SymbolType inferType(ASTNode* node, SymbolTable* table);
static SymbolType checkTypeCompatibility(SymbolType left, TokenType op, SymbolType right, int line);

//sementic analyze
static void analyzeSemanticBlock(ASTNode** nodes, int count, SymbolTable* table);
static void analyzeSemanticAssign(ASTNode* node, SymbolTable* table);
static void analyzeSemanticLocal(ASTNode* node, SymbolTable* table);
static void analyzeSemanticIf(ASTNode* node, SymbolTable* table);
static void analyzeSemanticLoop(ASTNode* node, SymbolTable* table);
static void analyzeSemanticFunction(ASTNode* node, SymbolTable* table);
static void analyzeSemanticFor(ASTNode* node, SymbolTable* table);
static void analyzeSemanticCall(ASTNode* node, SymbolTable* table);
static void analyzeSemanticReturn(ASTNode* node, SymbolTable* table);

#endif