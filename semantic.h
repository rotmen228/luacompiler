#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include "ast.h"
#include "lexerH.h"

// סוגי ההיקפים (Scopes) - מגדיר איפה המשתנה חי
typedef enum {
    SCOPE_FILE_LOCAL,
    SCOPE_GLOBAL,
    SCOPE_GLOBAL_IMPLICIT,
    SCOPE_BLOCK_LOCAL
} ScopeType;

// סוגי הטיפוסים שהמהדר מסיק
typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_FUNCTION
} SymbolType;

// מבנה לשמירת פרמטרים של פונקציות
typedef struct {
    SymbolType* param_types;
    int param_count;
} FunctionParams;

// רשומה בודדת בטבלת הסמלים (מייצגת משתנה או פונקציה)
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

// צומת ברשימה המקושרת של טבלת הגיבוב (לטיפול בהתנגשויות)
typedef struct HashEntry {
    SymbolRecord* record;
    struct HashEntry* next;
} HashEntry;

#define HASH_TABLE_SIZE 256

// טבלת הסמלים עצמה - היררכית! מצביעה לאבא שלה.
// Children are stored in creation order (= block appearance order in the
// source), so codegen can consume them in the same order to always get
// the correct scope when entering a nested block.
#define MAX_CHILD_SCOPES 64
typedef struct SymbolTable {
    HashEntry* buckets[HASH_TABLE_SIZE];
    struct SymbolTable*  parent_table;
    struct SymbolTable** children;      // dynamic array of child scopes
    int                  childCount;    // how many children created so far
    int                  childCapacity; // allocated capacity
    int                  nextChild;     // codegen cursor: next child to consume
} SymbolTable;

// --- חתימות לפונקציות הניהול ---
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

#endif // SEMANTIC_H