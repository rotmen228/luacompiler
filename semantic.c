#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "error_handler.h"

//for return
static SymbolRecord* currentFunctionScope = NULL;


// Array collecting every function-scope table so codegen can retrieve them in O(1)
static SymbolTable* allScopes[100];
static const char* allScopeNames[100];
static const char* allFuncParamNames[100][20];
static int scopeCount = 0;

SymbolTable* getFuncScope(const char* funcName) {
    for (int i = 0; i < scopeCount; i++) {
        if (strcmp(allScopeNames[i], funcName) == 0) {
            return allScopes[i];
        }
    }
    return NULL;
}

// ==========================================
// Debug / print helpers
// ==========================================
static const char* getSymbolTypeName(SymbolType type) {
    switch(type) {
        case TYPE_INT:      return "int";
        case TYPE_DOUBLE:   return "double";
        case TYPE_STRING:   return "string";
        case TYPE_BOOL:     return "bool";
        case TYPE_FUNCTION: return "function";
        case TYPE_VOID:     return "void";
        case TYPE_UNKNOWN:  return "UNKNOWN";
        default:            return "???";
    }
}

static const char* getScopeTypeName(ScopeType scope) {
    switch(scope) {
        case SCOPE_GLOBAL:          return "GLOBAL";
        case SCOPE_GLOBAL_IMPLICIT: return "GLOBAL_IMPLICIT";
        case SCOPE_FILE_LOCAL:      return "FILE_LOCAL";
        case SCOPE_BLOCK_LOCAL:     return "BLOCK_LOCAL";
        default:                    return "???";
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
    if (isEmpty) printf("(Empty Table)\n");
    printf("==========================================================\n\n");
}

void printFinalSymbolTables(SymbolTable* globalScope) {
    printf("\n=== FINAL SYMBOL TABLES ===\n");
    for (int i = 0; i < scopeCount; i++) {
        printSymbolTable(allScopes[i], allScopeNames[i]);
    }
    if (globalScope != NULL) {
        printSymbolTable(globalScope, "Global Scope");
    }
}

// ==========================================
// Hash table helpers
// ==========================================

static unsigned int hash(const char* str) {
    unsigned int h = 5381;
    int c;
    while ((c = *str++)) h = ((h << 5) + h) + c;
    return h % HASH_TABLE_SIZE;
}

SymbolTable* createSymbolTable(SymbolTable* parent) {
    SymbolTable* table = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (!table) { printf("Memory allocation failed for SymbolTable\n"); exit(1); }
    for (int i = 0; i < HASH_TABLE_SIZE; i++) table->buckets[i] = NULL;
    table->parent_table  = parent;
    table->children = NULL;
    table->childCount = 0;
    table->childCapacity = 0;
    table->nextChild = 0;
    if (parent != NULL) {
        if (parent->childCount >= parent->childCapacity) {
            int newCap = (parent->childCapacity == 0) ? 4 : parent->childCapacity * 2;
            parent->children = (SymbolTable**)realloc(
                parent->children, newCap * sizeof(SymbolTable*));
            parent->childCapacity = newCap;
        }
        parent->children[parent->childCount++] = table;
    }
    return table;
}

SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init) {
    SymbolRecord* record = (SymbolRecord*)malloc(sizeof(SymbolRecord));
    record->name  = strdup(name);
    record->type  = type;
    record->scope = scope;
    record->data.var_data.is_initialized = is_init;
    return record;
}

SymbolRecord* createFuncRecord(const char* name, SymbolType returnType, SymbolType* paramTypes, int paramCount) {
    SymbolRecord* record = (SymbolRecord*)malloc(sizeof(SymbolRecord));
    record->name = strdup(name);
    record->type = TYPE_FUNCTION;
    record->scope = SCOPE_GLOBAL;
    record->data.func_data.return_type = returnType;
    record->data.func_data.params.param_types = paramTypes;
    record->data.func_data.params.param_count = paramCount;
    return record;
}

void insertSymbol(SymbolTable* table, SymbolRecord* record) {
    unsigned int index = hash(record->name);
    HashEntry* entry = (HashEntry*)malloc(sizeof(HashEntry));
    entry->record = record;
    entry->next = table->buckets[index];
    table->buckets[index] = entry;
}

SymbolRecord* lookupSymbol(SymbolTable* table, const char* name) {
    SymbolTable* current = table;
    while (current != NULL) {
        unsigned int index = hash(name);
        HashEntry* entry = current->buckets[index];
        while (entry != NULL) {
            if (strcmp(entry->record->name, name) == 0) return entry->record;
            entry = entry->next;
        }
        current = current->parent_table;
    }
    return NULL;
}

// Returns the next child scope of table in creation order, advancing the curser
SymbolTable* getNextChildScope(SymbolTable* table) {
    if (!table || table->nextChild >= table->childCount) return NULL;
    return table->children[table->nextChild++];
}

static SymbolTable* getGlobalTable(SymbolTable* table) {
    SymbolTable* current = table;
    while (current->parent_table != NULL) current = current->parent_table;
    return current;
}

static bool isGlobalTable(SymbolTable* table) {
    return table->parent_table == NULL;
}

// ==========================================
// Type inference
// ==========================================

SymbolType inferType(ASTNode* node, SymbolTable* table) {
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {
        case AST_NUMBER:
            return strchr(node->token.value, '.') ? TYPE_DOUBLE : TYPE_INT;

        case AST_STRING:
            return TYPE_STRING;

        case AST_NIL:
            return TYPE_UNKNOWN;

        case AST_IDENTIFIER: {
            if (strcmp(node->token.value, "true") == 0 ||
                strcmp(node->token.value, "false") == 0) return TYPE_BOOL;
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record) return record->type;
            reportError(PHASE_SEMANTIC, node->token.line, "Use of undeclared variable '%s'", node->token.value);
            return TYPE_UNKNOWN;
        }

        case AST_BINOP: {
            SymbolType leftType  = inferType(node->children[0], table);
            SymbolType rightType = inferType(node->children[1], table);
            TokenType op = node->token.type;
            if (op == TOKEN_OP_PLUS || op == TOKEN_OP_MINUS || op == TOKEN_OP_MUL ||
                op == TOKEN_OP_DIV || op == TOKEN_OP_MOD || op == TOKEN_OP_POW ||
                op == TOKEN_OP_LT || op == TOKEN_OP_GT || op == TOKEN_OP_LTE ||
                op == TOKEN_OP_GTE) {
                //type inference from other type in case its still not identified
                if (leftType == TYPE_UNKNOWN && node->children[0]->type == AST_IDENTIFIER) {
                    SymbolRecord* rec = lookupSymbol(table, node->children[0]->token.value);
                    if (rec) {
                        SymbolType g = (op == TOKEN_OP_MOD) ? TYPE_INT :
                                       (rightType == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
                        rec->type = g;
                        leftType  = g;
                    }
                }
                if (rightType == TYPE_UNKNOWN && node->children[1]->type == AST_IDENTIFIER) {
                    SymbolRecord* rec = lookupSymbol(table, node->children[1]->token.value);
                    if (rec) {
                        SymbolType g = (op == TOKEN_OP_MOD) ? TYPE_INT :
                                       (leftType == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
                        rec->type = g;
                        rightType = g;
                    }
                }
            }
            return checkTypeCompatibility(leftType, op, rightType, node->token.line);
        }

        case AST_FUNCTION_CALL: {
            analyzeSemanticCall(node, table);
            if (strcmp(node->token.value, "print") == 0) {
                return TYPE_VOID;
            }
            
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record && record->type == TYPE_FUNCTION)
                return record->data.func_data.return_type;
            return TYPE_UNKNOWN;
        }

        default:
            return TYPE_UNKNOWN;
    }
}

// ==========================================
// Type compatibility
// ==========================================

static SymbolType checkTypeCompatibility(SymbolType left, TokenType op, SymbolType right, int line) {
    if (op == TOKEN_OP_EQ  || op == TOKEN_OP_NEQ ||
        op == TOKEN_OP_LT  || op == TOKEN_OP_GT  ||
        op == TOKEN_OP_LTE || op == TOKEN_OP_GTE) {
        if (left != TYPE_UNKNOWN && right != TYPE_UNKNOWN) {
            if ((left == TYPE_STRING) != (right == TYPE_STRING))
                reportError(PHASE_SEMANTIC, line, "Cannot compare string with non-string");
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_KW_AND || op == TOKEN_KW_OR) return TYPE_BOOL;

    if (op == TOKEN_OP_CONCAT) {
        if (left != TYPE_STRING || right != TYPE_STRING) {
            reportError(PHASE_SEMANTIC, line, "Concatenation (..) requires both operands to be strings");
            return TYPE_UNKNOWN;
        }
        return TYPE_STRING;
    }

    bool leftValid  = (left  == TYPE_INT || left  == TYPE_DOUBLE || left  == TYPE_UNKNOWN);
    bool rightValid = (right == TYPE_INT || right == TYPE_DOUBLE || right == TYPE_UNKNOWN);
    if (!leftValid || !rightValid) {
        reportError(PHASE_SEMANTIC, line, "Cannot perform math operation on non-numeric types");
        return TYPE_UNKNOWN;
    }
    if (left == TYPE_DOUBLE || right == TYPE_DOUBLE) return TYPE_DOUBLE;
    return TYPE_INT;
}

// ==========================================
// Statement analyzers
// ==========================================

static void analyzeSemanticAssign(ASTNode* node, SymbolTable* table) {
    //left
    ASTNode* varNode = node->children[0];
    //right
    ASTNode* valNode = node->children[1];
    const char* varName = varNode->token.value;

    SymbolType inferred = inferType(valNode, table);
    SymbolRecord* existing = lookupSymbol(table, varName);

    if (existing != NULL) {
        if (existing->type == TYPE_UNKNOWN) {
            existing->type = inferred;
            existing->data.var_data.is_initialized = true;
        } else if (existing->type != inferred && inferred != TYPE_UNKNOWN) {
            reportError(PHASE_SEMANTIC, node->token.line, "Type mismatch for variable '%s'", varName);
        }
    } else {
        ScopeType scope;
        SymbolTable* targetTable;
        if (isGlobalTable(table)) {
            scope = SCOPE_GLOBAL;
            targetTable = table;
        } else {
            scope = SCOPE_GLOBAL_IMPLICIT;
            targetTable = getGlobalTable(table);
        }
        SymbolRecord* newRecord = createVarRecord(varName, inferred, scope, true);
        insertSymbol(targetTable, newRecord);
    }
}

static void analyzeSemanticLocal(ASTNode* node, SymbolTable* table) {
    ASTNode* varNode = node->children[0];
    ASTNode* valNode = node->children[1];
    const char* varName = varNode->token.value;

    SymbolType inferred = TYPE_UNKNOWN;
    bool initialized = false;

    if (valNode != NULL && valNode->type != AST_NIL) {
        inferred = inferType(valNode, table);
        initialized = true;
    }

    ScopeType scope = isGlobalTable(table) ? SCOPE_FILE_LOCAL : SCOPE_BLOCK_LOCAL;
    SymbolRecord* newRecord = createVarRecord(varName, inferred, scope, initialized);
    insertSymbol(table, newRecord);
}

static void analyzeSemanticIf(ASTNode* node, SymbolTable* table) {
    ASTNode* condNode = node->children[0];
    ASTNode* bodyNode = node->children[1];
    ASTNode* elseNode = (node->childCount > 2) ? node->children[2] : NULL;

    SymbolType condType = inferType(condNode, table);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN)
        reportError(PHASE_SEMANTIC, node->token.line, "if condition must be boolean or numeric");

    SymbolTable* ifScope = createSymbolTable(table);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, ifScope);

    if (elseNode != NULL) {
        // elseif is another AST_IF node; else is a plain block
        if (elseNode->type == AST_IF) {
            analyzeSemanticIf(elseNode, table);
        } else {
            SymbolTable* elseScope = createSymbolTable(table);
            analyzeSemanticBlock(elseNode->children, elseNode->childCount, elseScope);
        }
    }
}

static void analyzeSemanticLoop(ASTNode* node, SymbolTable* table) {
    ASTNode* condNode;
    ASTNode* bodyNode;

    if (node->type == AST_WHILE) {
        condNode = node->children[0];
        bodyNode = node->children[1];
    } else {
        bodyNode = node->children[0];
        condNode = node->children[1];
    }

    SymbolType condType = inferType(condNode, table);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN)
        reportError(PHASE_SEMANTIC, node->token.line, "Loop condition must be boolean or numeric");
    SymbolTable* loopScope = createSymbolTable(table);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, loopScope);
}

static void analyzeSemanticFor(ASTNode* node, SymbolTable* table) {
    ASTNode* varNode   = node->children[0];
    ASTNode* startNode = node->children[1];
    ASTNode* limitNode = node->children[2];
    ASTNode* stepNode  = (node->childCount == 5) ? node->children[3] : NULL;
    ASTNode* bodyNode  = node->children[node->childCount - 1];

    SymbolType startType = inferType(startNode, table);
    SymbolType limitType = inferType(limitNode, table);
    SymbolType stepType  = (stepNode != NULL) ? inferType(stepNode, table) : TYPE_INT;

    bool startOk = (startType == TYPE_INT || startType == TYPE_DOUBLE || startType == TYPE_UNKNOWN);
    bool limitOk = (limitType == TYPE_INT || limitType == TYPE_DOUBLE || limitType == TYPE_UNKNOWN);
    bool stepOk  = (stepType  == TYPE_INT || stepType  == TYPE_DOUBLE || stepType  == TYPE_UNKNOWN);

    if (!startOk || !limitOk || !stepOk)
        reportError(PHASE_SEMANTIC, node->token.line, "for loop bounds must be numeric");

    SymbolTable* forScope = createSymbolTable(table);
    SymbolType iterType = (startType == TYPE_DOUBLE || limitType == TYPE_DOUBLE)
                          ? TYPE_DOUBLE : TYPE_INT;
    SymbolRecord* iterRecord = createVarRecord(
        varNode->token.value, iterType, SCOPE_BLOCK_LOCAL, true);
    insertSymbol(forScope, iterRecord);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, forScope);
}

static void analyzeSemanticFunction(ASTNode* node, SymbolTable* table) {
    const char* funcName = node->token.value;
    ASTNode* bodyNode = node->children[node->childCount - 1];
    int paramCount = node->childCount - 1;

    SymbolType* paramTypes = NULL;
    if (paramCount > 0) {
        paramTypes = (SymbolType*)malloc(sizeof(SymbolType) * paramCount);
        for (int i = 0; i < paramCount; i++) paramTypes[i] = TYPE_UNKNOWN;
    }

    SymbolRecord* funcRecord = createFuncRecord(funcName, TYPE_UNKNOWN, paramTypes, paramCount);
    insertSymbol(table, funcRecord);

    SymbolTable* funcScope = createSymbolTable(table);
    for (int i = 0; i < paramCount; i++) {
        const char* paramName = node->children[i]->token.value;
        SymbolRecord* paramRecord = createVarRecord(paramName, TYPE_UNKNOWN, SCOPE_BLOCK_LOCAL, true);
        insertSymbol(funcScope, paramRecord);
    }

    SymbolRecord* prevFunc = currentFunctionScope;
    currentFunctionScope = funcRecord;
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, funcScope);
    currentFunctionScope = prevFunc;

    //update inferred param types into the function rec
    if (paramCount > 0 && paramTypes != NULL) {
        for (int i = 0; i < paramCount; i++) {
            const char* paramName = node->children[i]->token.value;
            SymbolRecord* paramRecord = lookupSymbol(funcScope, paramName);
            if (paramRecord) funcRecord->data.func_data.params.param_types[i] = paramRecord->type;
        }
    }

    // Save scope for codegen
    if (scopeCount < 100) {
        allScopes[scopeCount] = funcScope;
        allScopeNames[scopeCount] = funcName;
        for (int i = 0; i < paramCount && i < 20; i++)
            allFuncParamNames[scopeCount][i] = node->children[i]->token.value;
        scopeCount++;
    }
}

static void analyzeSemanticReturn(ASTNode* node, SymbolTable* table) {
    if (node->childCount == 0 || node->children[0] == NULL) {
        if (currentFunctionScope != NULL) {
            if (currentFunctionScope->data.func_data.return_type == TYPE_UNKNOWN)
                currentFunctionScope->data.func_data.return_type = TYPE_VOID;
            else if (currentFunctionScope->data.func_data.return_type != TYPE_VOID)
                reportError(PHASE_SEMANTIC, node->token.line, "Function '%s' returns both void and non-void values", currentFunctionScope->name);
        }
        return;
    }
    SymbolType returnType = inferType(node->children[0], table);
    if (currentFunctionScope != NULL) {
        SymbolType cur = currentFunctionScope->data.func_data.return_type;
        if (cur == TYPE_UNKNOWN) {
            currentFunctionScope->data.func_data.return_type = returnType;
        } else if (cur == TYPE_INT && returnType == TYPE_DOUBLE) {
            currentFunctionScope->data.func_data.return_type = TYPE_DOUBLE;
        } else if (cur == TYPE_DOUBLE && returnType == TYPE_INT) {
            // already wider — no change needed
        } else if (cur != returnType && returnType != TYPE_UNKNOWN) {
            reportError(PHASE_SEMANTIC, node->token.line, "Inconsistent return types in function '%s'", currentFunctionScope->name);
        }
    }
}

static void analyzeSemanticCall(ASTNode* node, SymbolTable* table) {
    const char* funcName = node->token.value;

    if (strcmp(funcName, "print") == 0) {
        for (int i = 0; i < node->childCount; i++) {
            inferType(node->children[i], table);
        }
        return;
    }

    SymbolRecord* funcRecord = lookupSymbol(table, funcName);

    //logic checks
    if (funcRecord == NULL) {
        reportError(PHASE_SEMANTIC, node->token.line, "Call to undefined function '%s'", funcName);
        return;
    }
    if (funcRecord->type != TYPE_FUNCTION) {
        reportError(PHASE_SEMANTIC, node->token.line, "'%s' is not a function", funcName);
        return;
    }
    int expectedParams = funcRecord->data.func_data.params.param_count;
    int givenArgs = node->childCount;

    if (givenArgs != expectedParams) {
        reportError(PHASE_SEMANTIC, node->token.line, "Function '%s' expects %d arguments but got %d", funcName, expectedParams, givenArgs);
        return;
    }

    //compare given types with wanted, if not equal then infer the type from them
    for (int i = 0; i < givenArgs; i++) {
        SymbolType argType = inferType(node->children[i], table);
        SymbolType expectedType = funcRecord->data.func_data.params.param_types[i];

        if (expectedType == TYPE_UNKNOWN) {
            funcRecord->data.func_data.params.param_types[i] = argType;
            for (int j = 0; j < scopeCount; j++) {
                if (strcmp(allScopeNames[j], funcName) == 0) {
                    const char* paramName = allFuncParamNames[j][i];
                    SymbolRecord* paramRec = lookupSymbol(allScopes[j], paramName);
                    if (paramRec) paramRec->type = argType;
                    break;
                }
            }
        } else if (argType != expectedType && argType != TYPE_UNKNOWN) {
            reportError(PHASE_SEMANTIC, node->token.line, "Argument %d to function '%s' has wrong type", i + 1, funcName);
        }
    }
}

static void analyzeSemanticBlock(ASTNode** nodes, int count, SymbolTable* table) {
    for (int i = 0; i < count; i++) {
        ASTNode* node = nodes[i];
        if (!node) continue;
        switch (node->type) {
            case AST_ASSIGNMENT:    analyzeSemanticAssign(node, table);   break;
            case AST_LOCAL_ASSIGN:  analyzeSemanticLocal(node, table);    break;
            case AST_IF:            analyzeSemanticIf(node, table);       break;
            case AST_WHILE:
            case AST_REPEAT:        analyzeSemanticLoop(node, table);     break;
            case AST_FUNCTION_DECL: analyzeSemanticFunction(node, table); break;
            case AST_FOR:           analyzeSemanticFor(node, table);      break;
            case AST_FUNCTION_CALL: analyzeSemanticCall(node, table);     break;
            case AST_RETURN:        analyzeSemanticReturn(node, table);   break;
            default: break;
        }
    }
}

//debug
SymbolTable* analyzeSemantic(ASTNode* root) {
    if (!root) return NULL;
    scopeCount = 0;
    SymbolTable* globalTable = createSymbolTable(NULL);
    printf("\n--- Starting Semantic Analysis ---\n");
    analyzeSemanticBlock(root->children, root->childCount, globalTable);
    printf("--- Semantic Analysis completed successfully! ---\n");
    return globalTable;
}