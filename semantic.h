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

// טבלת הסמלים עצמה - היררכית! מצביעה לאבא שלה
typedef struct SymbolTable {
    HashEntry* buckets[HASH_TABLE_SIZE];
    struct SymbolTable* parent_table;
} SymbolTable;

// --- חתימות לפונקציות הניהול ---
SymbolTable* createSymbolTable(SymbolTable* parent);
void insertSymbol(SymbolTable* table, SymbolRecord* record);
SymbolRecord* lookupSymbol(SymbolTable* table, const char* name);
SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init);

// --- חתימות לפונקציות האנליזה המרכזיות (נכתוב בהמשך) ---
SymbolType inferType(ASTNode* node, SymbolTable* table);
SymbolTable* analyzeSemantic(ASTNode* root);
void printSymbolTable(SymbolTable* table, const char* scopeName);
void printFinalSymbolTables(SymbolTable* globalScope);

#endif // SEMANTIC_H