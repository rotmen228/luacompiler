#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"

static SymbolRecord* currentFunctionScope = NULL;

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




//THIS IS FOR DEBUG DELETE LATER
// ---> הוסף את 3 השורות האלו: מערך לאיסוף הטבלאות המקומיות <---
// מערך לאיסוף הטבלאות המקומיות כדי שנוכל להדפיס אותן בסוף הריצה
static SymbolTable* allScopes[100];
static const char* allScopeNames[100];
static const char* allFuncParamNames[100][20]; // <--- הוסף את השורה הזו!
static int scopeCount = 0;
//END



// ==========================================
// פונקציות דיבאג והדפסה לטבלת הסמלים
// ==========================================

static const char* getSymbolTypeName(SymbolType type) {
    switch(type) {
        case TYPE_INT: return "int";
        case TYPE_DOUBLE: return "double";
        case TYPE_STRING: return "string";
        case TYPE_BOOL: return "bool";
        case TYPE_FUNCTION: return "function";
        case TYPE_VOID: return "void";
        case TYPE_UNKNOWN: return "UNKNOWN";
        default: return "???";
    }
}

static const char* getScopeTypeName(ScopeType scope) {
    switch(scope) {
        case SCOPE_GLOBAL: return "GLOBAL";
        case SCOPE_GLOBAL_IMPLICIT: return "GLOBAL_IMPLICIT";
        case SCOPE_FILE_LOCAL: return "FILE_LOCAL";
        case SCOPE_BLOCK_LOCAL: return "BLOCK_LOCAL";
        default: return "???";
    }
}

void printSymbolTable(SymbolTable* table, const char* scopeName) {
    if (!table) return;
    
    printf("\n=== Symbol Table: %s ===\n", scopeName);
    printf("%-15s | %-10s | %-15s | %s\n", "Name", "Type", "Scope", "Initialized?");
    printf("----------------------------------------------------------\n");
    
    bool isEmpty = true;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = table->buckets[i];
        while (entry != NULL) {
            isEmpty = false;
            SymbolRecord* rec = entry->record;
            
            // בדיקה אם מאותחל (לפונקציות נציג N/A כי הן מוגדרות מעצם קיומן)
            const char* initStr = (rec->type == TYPE_FUNCTION) ? "N/A" : 
                                  (rec->data.var_data.is_initialized ? "Yes" : "No");
                                  
            printf("%-15s | %-10s | %-15s | %s\n", 
                   rec->name, 
                   getSymbolTypeName(rec->type), 
                   getScopeTypeName(rec->scope),
                   initStr);
                   
            entry = entry->next;
        }
    }
    
    if (isEmpty) {
        printf("(Empty Table)\n");
    }
    printf("==========================================================\n\n");
}
// פונקציה להדפסה מרוכזת של כל הטבלאות הסופיות בסיום האנליזה
void printFinalSymbolTables(SymbolTable* globalScope) {
    printf("\n=== FINAL SYMBOL TABLES ===\n");
    
    // 1. הדפסת כל הטבלאות המקומיות (הפונקציות) בגרסתן הסופית והמעודכנת
    for (int i = 0; i < scopeCount; i++) {
        printSymbolTable(allScopes[i], allScopeNames[i]);
    }
    
    // 2. הדפסת הטבלה הגלובלית
    if (globalScope != NULL) {
        printSymbolTable(globalScope, "Global Scope");
    }
}

static unsigned int hash(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % HASH_TABLE_SIZE;
}

// יצירת טבלת סמלים חדשה (בלוק חדש)
SymbolTable* createSymbolTable(SymbolTable* parent) {
    SymbolTable* table = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (!table) {
        printf("Memory allocation failed for SymbolTable\n");
        exit(1);
    }
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        table->buckets[i] = NULL;
    }
    table->parent_table = parent; // חיבור לעץ ההיררכי
    return table;
}

// פונקציית עזר ליצירת רשומת משתנה חדשה בזיכרון
SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init) {
    SymbolRecord* record = (SymbolRecord*)malloc(sizeof(SymbolRecord));
    record->name = strdup(name); // העתקת המחרוזת כדי שלא תידרס
    record->type = type;
    record->scope = scope;
    record->data.var_data.is_initialized = is_init;
    return record;
}
// פונקציית עזר ליצירת רשומת פונקציה בזיכרון
SymbolRecord* createFuncRecord(const char* name, SymbolType returnType, SymbolType* paramTypes, int paramCount) {
    SymbolRecord* record = (SymbolRecord*)malloc(sizeof(SymbolRecord));
    record->name = strdup(name);
    record->type = TYPE_FUNCTION;
    record->scope = SCOPE_GLOBAL; // פונקציות בלואה הן לרוב גלובליות
    record->data.func_data.return_type = returnType;
    record->data.func_data.params.param_types = paramTypes;
    record->data.func_data.params.param_count = paramCount;
    return record;
}

// הכנסת רשומה חדשה לטבלה הנוכחית
void insertSymbol(SymbolTable* table, SymbolRecord* record) {
    unsigned int index = hash(record->name);
    
    HashEntry* entry = (HashEntry*)malloc(sizeof(HashEntry));
    entry->record = record;
    
    // הכנסה לתחילת הרשימה המקושרת (O(1) insertion)
    entry->next = table->buckets[index];
    table->buckets[index] = entry;
}

// שליפת רשומה לפי שם - מחפש למעלה בהיררכיה אם לא נמצא בבלוק הנוכחי!
SymbolRecord* lookupSymbol(SymbolTable* table, const char* name) {
    SymbolTable* current_table = table;
    
    // מטפסים למעלה עד שמגיעים ל-NULL (מעל הטבלה הגלובלית)
    while (current_table != NULL) {
        unsigned int index = hash(name);
        HashEntry* entry = current_table->buckets[index];
        
        // סורקים את הרשימה המקושרת בתא הזה (טיפול בהתנגשויות גיבוב)
        while (entry != NULL) {
            if (strcmp(entry->record->name, name) == 0) {
                return entry->record; // מצאנו!
            }
            entry = entry->next;
        }
        
        // לא מצאנו בבלוק הזה, עולים לבלוק שעוטף אותנו
        current_table = current_table->parent_table;
    }
    
    return NULL; // המשתנה לא הוגדר באף טווח הכרה שזמין לנו
}

static SymbolTable* getGlobalTable(SymbolTable* table) {
    SymbolTable* current = table;
    while (current->parent_table != NULL) {
        current = current->parent_table;
    }
    return current;
}
 
// עזר: בדיקה האם הטבלה הנוכחית היא הגלובלית
static bool isGlobalTable(SymbolTable* table) {
    return table->parent_table == NULL;
}
SymbolType inferType(ASTNode* node, SymbolTable* table) {
    if (!node) return TYPE_UNKNOWN;
 
    switch (node->type) {
 
        // מספר שלם
        case AST_NUMBER:
            if (strchr(node->token.value, '.')) return TYPE_DOUBLE;
            return TYPE_INT;
 
        // מחרוזת
        case AST_STRING:
            return TYPE_STRING;
 
        // nil → UNKNOWN (טיפוס עדיין לא ידוע)
        case AST_NIL:
            return TYPE_UNKNOWN;
 
        // מזהה: true/false או שם משתנה
        case AST_IDENTIFIER: {
            if (strcmp(node->token.value, "true") == 0 ||
                strcmp(node->token.value, "false") == 0) {
                return TYPE_BOOL;
            }
            // חיפוש בטבלת הסמלים וטבלאות האב
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record) {
                return record->type;
            }
            // משתנה לא הוגדר
            printf("Semantic Error at line %d: Use of undeclared variable '%s'\n",
                   node->token.line, node->token.value);
            return TYPE_UNKNOWN;
        }
 
        // ביטוי בינארי — הסקה bottom-up: שמאל ← ימין ← שורש
        case AST_BINOP: {
            SymbolType leftType  = inferType(node->children[0], table);
            SymbolType rightType = inferType(node->children[1], table);
            return checkTypeCompatibility(leftType, node->token.type, rightType, node->token.line);
        }
 
        // קריאת פונקציה — מחזיר את טיפוס ההחזרה
        case AST_FUNCTION_CALL: {
            // ---> התיקון: לפני שמסיקים את טיפוס ההחזרה, בצע אנליזה מלאה לקריאה! <---
            // זה יבדוק שהארגומנטים תקינים ויעדכן את טיפוסי הפרמטרים (כמו n)
            analyzeSemanticCall(node, table);
            
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record && record->type == TYPE_FUNCTION) {
                return record->data.func_data.return_type;
            }
            return TYPE_UNKNOWN;
        }
 
        default:
            return TYPE_UNKNOWN;
    }
}
// ==========================================
// חלק 3: בדיקת תאימות טיפוסים
// ==========================================
 
static SymbolType checkTypeCompatibility(SymbolType left, TokenType op, SymbolType right, int line) {
 
    // --- אופרטורי יחס והשוואה → תוצאה תמיד bool ---
    if (op == TOKEN_OP_EQ  || op == TOKEN_OP_NEQ ||
        op == TOKEN_OP_LT  || op == TOKEN_OP_GT  ||
        op == TOKEN_OP_LTE || op == TOKEN_OP_GTE) {
 
        // לא ניתן להשוות מחרוזת עם לא-מחרוזת
        if (left != TYPE_UNKNOWN && right != TYPE_UNKNOWN) {
            if ((left == TYPE_STRING) != (right == TYPE_STRING)) {
                printf("Semantic Error at line %d: Cannot compare string with non-string\n", line);
            }
        }
        return TYPE_BOOL;
    }
 
    // --- אופרטורים לוגיים (and, or) → תוצאה תמיד bool ---
    if (op == TOKEN_KW_AND || op == TOKEN_KW_OR) {
        return TYPE_BOOL;
    }
 
    // --- שרשור מחרוזות (..) → שני הצדדים חייבים להיות char* ---
    if (op == TOKEN_OP_CONCAT) {
        if (left != TYPE_STRING || right != TYPE_STRING) {
            printf("Semantic Error at line %d: Concatenation (..) requires both operands to be strings\n", line);
            return TYPE_UNKNOWN;
        }
        return TYPE_STRING;
    }
 
    // --- פעולות מתמטיות (+, -, *, /) ---
    // חוקיות: מתמטיקה רק על מספרים (int, double, או UNKNOWN שטרם נקבע)
    bool leftValid  = (left  == TYPE_INT || left  == TYPE_DOUBLE || left  == TYPE_UNKNOWN);
    bool rightValid = (right == TYPE_INT || right == TYPE_DOUBLE || right == TYPE_UNKNOWN);
 
    if (!leftValid || !rightValid) {
        printf("Semantic Error at line %d: Cannot perform math operation on non-numeric types\n", line);
        return TYPE_UNKNOWN;
    }
 
    // קידום טיפוס: אם אחד מהם double, התוצאה double
    if (left == TYPE_DOUBLE || right == TYPE_DOUBLE) {
        return TYPE_DOUBLE;
    }
    return TYPE_INT;
}
static void analyzeSemanticAssign(ASTNode* node, SymbolTable* table) {
    // הילד הימני הוא הערך, הילד השמאלי הוא שם המשתנה
    ASTNode* varNode = node->children[0];
    ASTNode* valNode = node->children[1];
    const char* varName = varNode->token.value;
 
    // שלב 1: הסק את הטיפוס מצד ימין
    SymbolType inferred = inferType(valNode, table);
    if (inferred == TYPE_UNKNOWN && valNode->type != AST_NIL) {
        printf("Semantic Error at line %d: Cannot infer type for variable '%s'\n",
               node->token.line, varName);
        return;
    }
 
    // שלב 2: חפש את המשתנה בכל ההיררכיה
    SymbolRecord* existing = lookupSymbol(table, varName);
 
    if (existing != NULL) {
        // המשתנה כבר קיים בטבלה כלשהי
        if (existing->type == TYPE_UNKNOWN) {
            // הטיפוס לא היה ידוע — עדכן אותו עכשיו
            existing->type = inferred;
            existing->data.var_data.is_initialized = true;
        } else if (existing->type != inferred && inferred != TYPE_UNKNOWN) {
            // הטיפוס לא תואם — שגיאה סמנטית
            printf("Semantic Error at line %d: Type mismatch for variable '%s'\n",
                   node->token.line, varName);
        }
        // אחרת — הטיפוס תואם, אין שגיאה
    } else {
        // המשתנה לא קיים — הצהרה אימפליציטית
        // קבע היקף לפי מיקום ההצהרה
        ScopeType scope;
        SymbolTable* targetTable;
 
        if (isGlobalTable(table)) {
            // אנחנו ברמה הגלובלית
            scope = SCOPE_GLOBAL;
            targetTable = table;
        } else {
            // אנחנו בתוך בלוק — המשתנה נחשב גלובלי אימפליציטי ב-Lua
            scope = SCOPE_GLOBAL_IMPLICIT;
            targetTable = getGlobalTable(table);
        }
 
        SymbolRecord* newRecord = createVarRecord(varName, inferred, scope, true);
        insertSymbol(targetTable, newRecord);
    }
}
// -------------------------------------------
// analyzeSemanticLocal — הצהרת משתנה מקומי (local x = ...)
// -------------------------------------------
static void analyzeSemanticLocal(ASTNode* node, SymbolTable* table) {
    ASTNode* varNode = node->children[0];
    ASTNode* valNode = node->children[1]; // יכול להיות NULL אם אין ערך ראשוני
    const char* varName = varNode->token.value;
 
    SymbolType inferred = TYPE_UNKNOWN;
    bool initialized = false;
 
    if (valNode != NULL && valNode->type != AST_NIL) {
        // יש ערך ראשוני שאינו nil
        inferred = inferType(valNode, table);
        initialized = true;
    }
    // אם הערך הוא nil או אין ערך — UNKNOWN ומאותחל=לא
 
    // קבע היקף לפי עומק ההיררכיה
    ScopeType scope;
    if (isGlobalTable(table)) {
        // local בדרגה העליונה של הקובץ
        scope = SCOPE_FILE_LOCAL;
    } else {
        // local בתוך בלוק (פונקציה, if, לולאה)
        scope = SCOPE_BLOCK_LOCAL;
    }
 
    // הכנס לטבלת הסמלים הנוכחית (לא לגלובלית!)
    SymbolRecord* newRecord = createVarRecord(varName, inferred, scope, initialized);
    insertSymbol(table, newRecord);
}
static void analyzeSemanticIf(ASTNode* node, SymbolTable* table) {
    // children[0] = תנאי, children[1] = גוף if, children[2] = גוף else (יכול להיות NULL)
    ASTNode* condNode = node->children[0];
    ASTNode* bodyNode = node->children[1];
    ASTNode* elseNode = node->children[2];
 
    // בדוק שהתנאי הוא טיפוס בוליאני או מספרי
    SymbolType condType = inferType(condNode, table);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN) {
        printf("Semantic Error at line %d: if condition must be boolean or numeric\n",
               node->token.line);
    }
 
    // צור היקף חדש לגוף ה-if
    SymbolTable* ifScope = createSymbolTable(table);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, ifScope);
 
    // אם יש else — צור היקף חדש נפרד
    if (elseNode != NULL) {
        SymbolTable* elseScope = createSymbolTable(table);
        analyzeSemanticBlock(elseNode->children, elseNode->childCount, elseScope);
    }
}
// -------------------------------------------
// analyzeSemanticLoop — לולאת while או repeat/until
// -------------------------------------------
static void analyzeSemanticLoop(ASTNode* node, SymbolTable* table) {
    ASTNode* condNode;
    ASTNode* bodyNode;
 
    if (node->type == AST_WHILE) {
        // children[0] = תנאי, children[1] = גוף
        condNode = node->children[0];
        bodyNode = node->children[1];
    } else {
        // REPEAT: children[0] = גוף, children[1] = תנאי (until)
        bodyNode = node->children[0];
        condNode = node->children[1];
    }
 
    // בדוק תנאי
    SymbolType condType = inferType(condNode, table);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN) {
        printf("Semantic Error at line %d: Loop condition must be boolean or numeric\n",
               node->token.line);
    }
 
    // צור היקף חדש לגוף הלולאה
    SymbolTable* loopScope = createSymbolTable(table);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, loopScope);
}
// -------------------------------------------
// analyzeSemanticFor — לולאת for מספרית (for i = start, limit, step do)
// -------------------------------------------
static void analyzeSemanticFor(ASTNode* node, SymbolTable* table) {
    // children[0] = שם משתנה הספירה
    // children[1] = התחלה, children[2] = סיום, children[3] = צעד, children[4] = גוף
    ASTNode* varNode   = node->children[0];
    ASTNode* startNode = node->children[1];
    ASTNode* limitNode = node->children[2];
    ASTNode* stepNode  = node->children[3];
    ASTNode* bodyNode  = node->children[4];
 
    // בדוק שההתחלה, הסיום והצעד הם מספריים
    SymbolType startType = inferType(startNode, table);
    SymbolType limitType = inferType(limitNode, table);
    SymbolType stepType  = (stepNode != NULL) ? inferType(stepNode, table) : TYPE_INT;
 
    bool startOk = (startType == TYPE_INT || startType == TYPE_DOUBLE || startType == TYPE_UNKNOWN);
    bool limitOk = (limitType == TYPE_INT || limitType == TYPE_DOUBLE || limitType == TYPE_UNKNOWN);
    bool stepOk  = (stepType  == TYPE_INT || stepType  == TYPE_DOUBLE || stepType  == TYPE_UNKNOWN);
 
    if (!startOk || !limitOk || !stepOk) {
        printf("Semantic Error at line %d: for loop bounds must be numeric\n",
               node->token.line);
    }
 
    // צור היקף חדש לגוף הלולאה
    SymbolTable* forScope = createSymbolTable(table);
 
    // הכנס את משתנה הספירה לתוך ההיקף החדש (הוא BLOCK_LOCAL)
    SymbolType iterType = (startType == TYPE_DOUBLE || limitType == TYPE_DOUBLE)
                          ? TYPE_DOUBLE : TYPE_INT;
    SymbolRecord* iterRecord = createVarRecord(
        varNode->token.value, iterType, SCOPE_BLOCK_LOCAL, true);
    insertSymbol(forScope, iterRecord);
 
    // נתח את גוף הלולאה
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, forScope);
}
// -------------------------------------------
// analyzeSemanticFunction — הגדרת פונקציה
// -------------------------------------------
static void analyzeSemanticFunction(ASTNode* node, SymbolTable* table) {
    // לפי הפארסר שלנו: שם הפונקציה נמצא בטוקן של הצומת עצמו!
    const char* funcName = node->token.value;
    
    // הילד האחרון הוא תמיד הגוף (הבלוק) של הפונקציה
    ASTNode* bodyNode = node->children[node->childCount - 1];
    
    // כל שאר הילדים (אם יש כאלו) הם הפרמטרים
    int paramCount = node->childCount - 1;
 
    // בנה מערך טיפוסי פרמטרים — כולם UNKNOWN בשלב זה
    SymbolType* paramTypes = NULL;
    if (paramCount > 0) {
        paramTypes = (SymbolType*)malloc(sizeof(SymbolType) * paramCount);
        for (int i = 0; i < paramCount; i++) {
            paramTypes[i] = TYPE_UNKNOWN;
        }
    }
 
    // שלב 1: הכנס רשומת פונקציה לטבלת הסמלים הנוכחית
    SymbolRecord* funcRecord = createFuncRecord(funcName, TYPE_UNKNOWN, paramTypes, paramCount);
    insertSymbol(table, funcRecord);
 
    // שלב 2: צור טבלת סמלים חדשה לגוף הפונקציה
    SymbolTable* funcScope = createSymbolTable(table);
 
    // שלב 3: הכנס את כל הפרמטרים כ-BLOCK_LOCAL בטבלת הפונקציה
    for (int i = 0; i < paramCount; i++) {
        const char* paramName = node->children[i]->token.value; // הפרמטרים הם הילדים הראשונים
        SymbolRecord* paramRecord = createVarRecord(paramName, TYPE_UNKNOWN, SCOPE_BLOCK_LOCAL, true);
        insertSymbol(funcScope, paramRecord);
    }
 
    // שלב 4: נתח את גוף הפונקציה
    SymbolRecord* prevFunc = currentFunctionScope;
    currentFunctionScope = funcRecord;
    
    // שליחת תוכן הבלוק לאנליזה
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, funcScope);
    
    currentFunctionScope = prevFunc;
 
    // שלב 5: עדכן את טיפוסי הפרמטרים ברשומת הפונקציה לפי מה שנלמד בתוך הבלוק
    if (paramCount > 0 && paramTypes != NULL) {
        for (int i = 0; i < paramCount; i++) {
            const char* paramName = node->children[i]->token.value;
            SymbolRecord* paramRecord = lookupSymbol(funcScope, paramName);
            if (paramRecord) {
                // המערך paramTypes כבר יושב בתוך funcRecord
                funcRecord->data.func_data.params.param_types[i] = paramRecord->type;
            }
        }
    }
    
    //THIS DEBUG DELETE
    if (paramCount > 0 && paramTypes != NULL) {
        for (int i = 0; i < paramCount; i++) {
            const char* paramName = node->children[i]->token.value;
            SymbolRecord* paramRecord = lookupSymbol(funcScope, paramName);
            if (paramRecord) {
                // המערך paramTypes כבר יושב בתוך funcRecord
                funcRecord->data.func_data.params.param_types[i] = paramRecord->type;
            }
        }
    }
    
    // ---> התיקון: שמירת הטבלה המקומית במערך במקום להדפיס אותה <---
    // שמירת הטבלה המקומית במערך במקום להדפיס אותה עכשיו
    if (scopeCount < 100) {
        allScopes[scopeCount] = funcScope;
        allScopeNames[scopeCount] = funcName;
        
        // ---> התיקון: שומרים את שמות הפרמטרים (כמו n) במערך החדש <---
        for (int i = 0; i < paramCount && i < 20; i++) {
            allFuncParamNames[scopeCount][i] = node->children[i]->token.value;
        }
        
        scopeCount++;
    }
    //END
}
 
// -------------------------------------------
// analyzeSemanticReturn — משפט return
// -------------------------------------------
// -------------------------------------------
// analyzeSemanticReturn — משפט return
// -------------------------------------------
static void analyzeSemanticReturn(ASTNode* node, SymbolTable* table) {
    if (node->childCount == 0 || node->children[0] == NULL) {
        // return ללא ערך — החזרה מסוג void
        return;
    }
    
 
    // שלב 1: הסק את הטיפוס של ביטוי ההחזרה
    SymbolType returnType = inferType(node->children[0], table);
 
    // שלב 2: עדכון הפונקציה הנוכחית (בלי לחפש בטבלה!)
    if (currentFunctionScope != NULL) {
        SymbolType currentRetType = currentFunctionScope->data.func_data.return_type;
        // מתעדכן רק אם עוד לא נקבע, או משתדרג ל-Double אם היה Int
        if (currentRetType == TYPE_UNKNOWN || currentRetType == TYPE_INT) {
            currentFunctionScope->data.func_data.return_type = returnType;
        }
    }
}
 
// -------------------------------------------
// analyzeSemanticCall — קריאת פונקציה
// -------------------------------------------
// -------------------------------------------
// analyzeSemanticCall — קריאת פונקציה
// -------------------------------------------
static void analyzeSemanticCall(ASTNode* node, SymbolTable* table) {
    const char* funcName = node->token.value;
 
    // שלב 1: חפש את הפונקציה בטבלת הסמלים
    SymbolRecord* funcRecord = lookupSymbol(table, funcName);
    if (funcRecord == NULL) {
        printf("Semantic Error at line %d: Call to undefined function '%s'\n",
               node->token.line, funcName);
        return;
    }
    if (funcRecord->type != TYPE_FUNCTION) {
        printf("Semantic Error at line %d: '%s' is not a function\n",
               node->token.line, funcName);
        return;
    }
 
    // שלב 2: בדוק שמספר הארגומנטים תואם
    int expectedParams = funcRecord->data.func_data.params.param_count;
    int givenArgs      = node->childCount;
 
    if (givenArgs != expectedParams) {
        printf("Semantic Error at line %d: Function '%s' expects %d arguments but got %d\n",
               node->token.line, funcName, expectedParams, givenArgs);
        return;
    }
 
    // שלב 3: עבור כל ארגומנט — הסק טיפוס ובדוק תאימות
    for (int i = 0; i < givenArgs; i++) {
        SymbolType argType = inferType(node->children[i], table);
        SymbolType expectedType = funcRecord->data.func_data.params.param_types[i];
 
        if (expectedType == TYPE_UNKNOWN) {
            // 1. עדכון הטיפוס ברשומת הפונקציה הגלובלית (מה שעשינו קודם)
            funcRecord->data.func_data.params.param_types[i] = argType;
            
            // 2. התיקון הקריטי: חזרה לטבלה המקומית של הפונקציה ועדכון המשתנה n עצמו!
            for (int j = 0; j < scopeCount; j++) {
                if (strcmp(allScopeNames[j], funcName) == 0) {
                    const char* paramName = allFuncParamNames[j][i];
                    SymbolRecord* paramRec = lookupSymbol(allScopes[j], paramName);
                    if (paramRec) {
                        paramRec->type = argType; // הקסם! n סוף סוף הופך ל-int
                    }
                    break;
                }
            }
            
        } else if (argType != expectedType && argType != TYPE_UNKNOWN) {
            printf("Semantic Error at line %d: Argument %d to function '%s' has wrong type\n",
                   node->token.line, i + 1, funcName);
        }
    }
}
// -------------------------------------------
// analyzeSemanticBlock — לב האנליזה, מעבד רשימת צמתים
// -------------------------------------------
static void analyzeSemanticBlock(ASTNode** nodes, int count, SymbolTable* table) {
    for (int i = 0; i < count; i++) {
        ASTNode* node = nodes[i];
        if (!node) continue;
 
        switch (node->type) {
            case AST_ASSIGNMENT:
                analyzeSemanticAssign(node, table);
                break;
 
            case AST_LOCAL_ASSIGN:
                analyzeSemanticLocal(node, table);
                break;
 
            case AST_IF:
                analyzeSemanticIf(node, table);
                break;
 
            case AST_WHILE:
            case AST_REPEAT:
                analyzeSemanticLoop(node, table);
                break;
 
            case AST_FUNCTION_DECL:
                analyzeSemanticFunction(node, table);
                break;
 
            case AST_FOR:
                analyzeSemanticFor(node, table);
                break;
 
            case AST_FUNCTION_CALL:

                analyzeSemanticCall(node, table);
                break;
 
            case AST_RETURN:
                analyzeSemanticReturn(node, table);
                break;
 
            default:
                // צמתים אחרים (הערות, שורות ריקות) — המשך
                break;
        }
    }
}
 
// -------------------------------------------
// analyzeSemantic — נקודת הכניסה הראשית
// -------------------------------------------
SymbolTable* analyzeSemantic(ASTNode* root) {
    if (!root) return NULL;
 
    //DEBUG
    scopeCount = 0;
    // שלב 1: צור טבלת סמלים גלובלית ריקה
    SymbolTable* globalTable = createSymbolTable(NULL);
 
    printf("\n--- Starting Semantic Analysis ---\n");

    // שלב 2: נתח את כל הצמתים מהשורש (השתמש בנתב הראשי או ב-Block)
    // במקרה שלך, הבלוק שמקבל את המערך והכמות:
    analyzeSemanticBlock(root->children, root->childCount, globalTable);
 
    printf("--- Semantic Analysis completed successfully! ---\n");

    // שלב 3: פשוט מחזירים את הטבלה למי שקרא לפונקציה!
    return globalTable;
}