#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"


// פונקציית גיבוב (Hash) קלאסית ויעילה למחרוזות (djb2)
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



// ==========================================
// חלק 3: מערכת הסקת טיפוסים (Type Inference)
// ==========================================
SymbolType inferType(ASTNode* node, SymbolTable* table) {
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {
        case AST_NUMBER:
            if (strchr(node->token.value, '.')) return TYPE_DOUBLE;
            return TYPE_INT;

        case AST_STRING:
            return TYPE_STRING;

        case AST_NIL:
            return TYPE_VOID;

        case AST_IDENTIFIER: {
            // --- תיקון 1: טיפול בערכים בוליאניים ישירים ---
            if (strcmp(node->token.value, "true") == 0 || strcmp(node->token.value, "false") == 0) {
                return TYPE_BOOL;
            }

            // אם זה משתנה קיים, נשלוף את הטיפוס מהטבלה
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record) {
                return record->type;
            }
            return TYPE_UNKNOWN; // המשתנה עדיין לא הוגדר
        }

        case AST_BINOP: {
            SymbolType leftType = inferType(node->children[0], table);
            SymbolType rightType = inferType(node->children[1], table);
            TokenType op = node->token.type;

            // 1. אופרטורים לוגיים ויחסיים
            if (op == TOKEN_OP_EQ || op == TOKEN_OP_NEQ ||
                op == TOKEN_OP_LT || op == TOKEN_OP_GT ||
                op == TOKEN_OP_LTE || op == TOKEN_OP_GTE ||
                op == TOKEN_KW_AND || op == TOKEN_KW_OR) {
                
                // בדיקת תאימות: האם מנסים להשוות מחרוזת למספר? (אופציונלי אך מומלץ)
                if (leftType != rightType && leftType != TYPE_UNKNOWN && rightType != TYPE_UNKNOWN) {
                    // C תדע להתמודד עם השוואת Int ו-Double, אבל לא Int ו-String
                    if ((leftType == TYPE_STRING) != (rightType == TYPE_STRING)) {
                        printf("Semantic Error at line %d: Cannot compare string with non-string\n", node->token.line);
                    }
                }
                return TYPE_BOOL;
            }

            // 2. שרשור מחרוזות (..)
            if (op == TOKEN_OP_CONCAT) {
                // חייבים לוודא ששני הצדדים תואמים לפעולת שרשור
                if (leftType != TYPE_STRING || rightType != TYPE_STRING) {
                    printf("Semantic Error at line %d: Invalid operand types for concatenation (..). Both must be strings.\n", node->token.line);
                    return TYPE_UNKNOWN;
                }
                return TYPE_STRING;
            }

            // --- תיקון 2: בדיקת תאימות מחמירה לאופרטורים מתמטיים ---
            // חוקיות: מתמטיקה עושים רק על מספרים (Int או Double)
            if ((leftType != TYPE_INT && leftType != TYPE_DOUBLE && leftType != TYPE_UNKNOWN) || 
                (rightType != TYPE_INT && rightType != TYPE_DOUBLE && rightType != TYPE_UNKNOWN)) {
                
                printf("Semantic Error at line %d: Invalid operand types for math operation '%s'. Cannot use strings or booleans.\n", 
                       node->token.line, node->token.value);
                return TYPE_UNKNOWN; // עוצרים את הסקת הטיפוס כדי למנוע קריסה
            }

            // Type Promotion: אם עברנו את הבדיקה, ואחד מהם עשרוני - התוצאה עשרונית
            if (leftType == TYPE_DOUBLE || rightType == TYPE_DOUBLE) {
                return TYPE_DOUBLE;
            }
            return TYPE_INT; // ברירת המחדל למתמטיקה רגילה
        }

        case AST_FUNCTION_CALL: {
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
// חלק 4: הניתוח הסמנטי (AST Traversal)
// ==========================================
void analyzeSemanticFunction(ASTNode* funcNode, SymbolTable* currentTable) {
    // 1. צור רשומה בטבלת_סמלים_נוכחית עבור הפונקציה עם טיפוס UNKNOWN לפרמטרים וההחזרה
    SymbolRecord* funcRecord = createVarRecord(funcNode->token.value, TYPE_FUNCTION, SCOPE_GLOBAL, true);
    funcRecord->data.func_data.return_type = TYPE_UNKNOWN;
    
    // מספר הילדים פחות 1 (הילד האחרון הוא גוף הפונקציה) נותן לנו את כמות הפרמטרים
    int paramCount = funcNode->childCount - 1;
    funcRecord->data.func_data.params.param_count = paramCount;
    
    // הקצאת מערך דינמי לשמירת טיפוסי הפרמטרים
    if (paramCount > 0) {
        funcRecord->data.func_data.params.param_types = (SymbolType*)malloc(paramCount * sizeof(SymbolType));
        for(int i = 0; i < paramCount; i++) {
            funcRecord->data.func_data.params.param_types[i] = TYPE_UNKNOWN;
        }
    } else {
        funcRecord->data.func_data.params.param_types = NULL;
    }
    insertSymbol(currentTable, funcRecord);

    // 2. צור טבלת סמלים חדשה (טבלת_פונקציה)
    SymbolTable* funcTable = createSymbolTable(currentTable);

    // 3. עבור כל פרמטר, צור רשומה בטבלה החדשה
    for(int i = 0; i < paramCount; i++) {
        ASTNode* paramNode = funcNode->children[i];
        SymbolRecord* paramRecord = createVarRecord(paramNode->token.value, TYPE_UNKNOWN, SCOPE_BLOCK_LOCAL, true);
        insertSymbol(funcTable, paramRecord);
    }

    // 4. קרא לניתוח הבלוק על גוף הפונקציה
    // אנחנו מעבירים את funcRecord כדי שפקודות RETURN בתוך הבלוק ידעו את מי לעדכן
    ASTNode* bodyNode = funcNode->children[paramCount];
    analyzeNode(bodyNode, funcTable, funcRecord);

    // 5+6. שלוף את הטיפוסים המעודכנים מתוך טבלת הפונקציה ועדכן את חתימת הפונקציה
    for(int i = 0; i < paramCount; i++) {
        ASTNode* paramNode = funcNode->children[i];
        SymbolRecord* updatedParam = lookupSymbol(funcTable, paramNode->token.value);
        if (updatedParam && updatedParam->type != TYPE_UNKNOWN) {
            funcRecord->data.func_data.params.param_types[i] = updatedParam->type;
        }
    }
}
void analyzeSemanticCall(ASTNode* callNode, SymbolTable* currentTable, SymbolRecord* currentFunction) {
    const char* funcName = callNode->token.value;
    
    // 1+2. שלוף וחפש את שם הפונקציה
    SymbolRecord* funcRecord = lookupSymbol(currentTable, funcName);
    
    // 3. אם השם לא נמצא
    if (!funcRecord) {
        printf("Semantic Error at line %d: Attempt to call undefined function '%s'\n", callNode->token.line, funcName);
        return;
    }
    
    // 4. אם טיפוס לא FUNCTION
    if (funcRecord->type != TYPE_FUNCTION) {
        printf("Semantic Error at line %d: Attempt to call non-function identifier '%s'\n", callNode->token.line, funcName);
        return;
    }
    
    // 5. בדיקת התאמת מספר ארגומנטים
    int argCount = callNode->childCount;
    if (argCount != funcRecord->data.func_data.params.param_count) {
        printf("Semantic Error at line %d: Argument count mismatch for '%s'. Expected %d, got %d.\n", 
               callNode->token.line, funcName, funcRecord->data.func_data.params.param_count, argCount);
        return;
    }
    
    // 6. עבור כל ארגומנט
    for (int i = 0; i < argCount; i++) {
        ASTNode* argNode = callNode->children[i];
        
        // מנתחים את הארגומנט עצמו במקרה שהוא משוואה מורכבת
        analyzeNode(argNode, currentTable, currentFunction);
        
        // 6.1 הסקת הטיפוס של הארגומנט שנשלח
        SymbolType argType = inferType(argNode, currentTable);
        
        // 6.2 שלוף טיפוס מצופה מהרשומה של הפונקציה
        SymbolType expectedType = funcRecord->data.func_data.params.param_types[i];
        
        // 6.3 הסקת טיפוסים דינמית מזמן קריאה!
        if (expectedType == TYPE_UNKNOWN) {
            // 6.3.1 עדכון רשימת הפרמטרים
            funcRecord->data.func_data.params.param_types[i] = argType;
        } else {
            // 6.3.1 אם הטיפוס כבר ידוע - בצע בדיקת התאמה (Type Checking)
            if (argType != expectedType && argType != TYPE_UNKNOWN) {
                // המרה אוטומטית בין Int ל-Double
                if ((expectedType == TYPE_INT && argType == TYPE_DOUBLE) || 
                    (expectedType == TYPE_DOUBLE && argType == TYPE_INT)) {
                    funcRecord->data.func_data.params.param_types[i] = TYPE_DOUBLE;
                } else {
                    printf("Semantic Error at line %d: Type mismatch in argument %d of function '%s'\n", 
                           callNode->token.line, i+1, funcName);
                }
            }
        }
    }
}
void analyzeSemanticReturn(ASTNode* retNode, SymbolTable* currentTable, SymbolRecord* currentFunction) {
    // 1. אם לצומת_חזרה יש ביטוי
    if (retNode->childCount > 0) {
        ASTNode* exprNode = retNode->children[0];
        
        analyzeNode(exprNode, currentTable, currentFunction);
        
        // 1.1 קרא ל inferType
        SymbolType retType = inferType(exprNode, currentTable);
        
        // 1.2 שמור את הטיפוס שהוסק בתוך שדה טיפוס_החזרה של הפונקציה שעוטפת אותנו
        if (currentFunction && currentFunction->type == TYPE_FUNCTION) {
            SymbolType currentRetType = currentFunction->data.func_data.return_type;
            
            // מתעדכן אם עוד לא נקבע, או משודרג ל-Double אם יש החזרות שונות באותה פונקציה
            if (currentRetType == TYPE_UNKNOWN || currentRetType == TYPE_INT) {
                currentFunction->data.func_data.return_type = retType;
            } else if (currentRetType == TYPE_DOUBLE && retType == TYPE_INT) {
                // זה תקין, C יודעת להמיר Int ל-Double בחזרה
            } else if (currentRetType != retType && retType != TYPE_UNKNOWN) {
                printf("Semantic Error at line %d: Function has conflicting return types.\n", retNode->token.line);
            }
        }
    }
}
// ==========================================
// חלק 5: פונקציות ניתוח מבני בקרה (Control Flow)
// ==========================================

void analyzeSemanticIf(ASTNode* node, SymbolTable* currentTable, SymbolRecord* currentFunction) {
    // 1. ניתוח התנאי
    analyzeNode(node->children[0], currentTable, currentFunction);
    SymbolType condType = inferType(node->children[0], currentTable);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN) {
        printf("Semantic Error at line %d: Condition must be boolean or numeric.\n", node->token.line);
    }
    
    // 2. ניתוח בלוק ה-IF (אמת)
    analyzeSemanticBlock(node->children[1], currentTable, currentFunction);

    // 3. ניתוח בלוק ה-ELSE או ה-ELSEIF (שקר) - אם קיים
    if (node->childCount > 2) {
        ASTNode* elseNode = node->children[2];
        if (elseNode->type == AST_IF) {
            // שרשור elseif (קריאה רקורסיבית לפונקציית ה-IF)
            analyzeSemanticIf(elseNode, currentTable, currentFunction);
        } else {
            // בלוק else רגיל
            analyzeSemanticBlock(elseNode, currentTable, currentFunction);
        }
    }
}

void analyzeSemanticWhile(ASTNode* node, SymbolTable* currentTable, SymbolRecord* currentFunction) {
    // 1. ניתוח התנאי
    analyzeNode(node->children[0], currentTable, currentFunction);
    SymbolType condType = inferType(node->children[0], currentTable);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN) {
        printf("Semantic Error at line %d: Condition must be boolean or numeric.\n", node->token.line);
    }
    
    // 2. ניתוח גוף הלולאה
    analyzeSemanticBlock(node->children[1], currentTable, currentFunction);
}

void analyzeSemanticFor(ASTNode* node, SymbolTable* currentTable, SymbolRecord* currentFunction) {
    // יצירת טבלת סמלים מקומית ללולאה (משתנה האיטרציה חי רק בפנים)
    SymbolTable* forTable = createSymbolTable(currentTable);
    
    // ילד 0: משתנה הלולאה
    ASTNode* idNode = node->children[0];
    SymbolRecord* iteratorVar = createVarRecord(idNode->token.value, TYPE_INT, SCOPE_BLOCK_LOCAL, true);
    insertSymbol(forTable, iteratorVar);

    // ניתוח ביטויי הלולאה (התחלה, סיום, קפיצה) - רצים עד הילד לפני האחרון (שהוא הבלוק)
    for (int i = 1; i < node->childCount - 1; i++) {
        analyzeNode(node->children[i], currentTable, currentFunction);
    }

    // ניתוח הבלוק עצמו - אנו מעבירים לו את טבלת הלולאה שיצרנו
    ASTNode* bodyNode = node->children[node->childCount - 1];
    analyzeNode(bodyNode, forTable, currentFunction); 
}
